#include "llvm/Analysis/TracepointParams.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/LazyValueInfo.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "tracepoint_params"

using namespace llvm;

static cl::opt<std::string> TracepointParamsOut("tracepoint-params-out", cl::Hidden,
  cl::init(""), cl::desc("JSON file with info about nullness of tracepoint params"));

namespace {

class DISubprogsInfo {
  llvm::DenseMap<StringRef, DISubprogram *> SubprogsByName;
  llvm::DenseMap<DISubprogram *, SmallVector<DILocalVariable *, 4>> ParamsBySubprog;
public:
  void collect(Module &M);
  SmallVectorImpl<DILocalVariable *> *lookupParams(StringRef Name);
};

SmallVectorImpl<DILocalVariable *> *DISubprogsInfo::lookupParams(StringRef Name) {
  if (!SubprogsByName.contains(Name))
    return nullptr;
  DISubprogram *DSub = SubprogsByName[Name];
  if (!ParamsBySubprog.contains(DSub))
    return nullptr;
  return &ParamsBySubprog[DSub];
}

void DISubprogsInfo::collect(Module &M) {
  auto VisitMD = [&](std::function<void (MDNode *)> Fn) {
    SmallVector<Metadata *, 128> WorkList;
    DenseSet<Metadata *> Visited;
    for (DICompileUnit *DCU: M.debug_compile_units())
      WorkList.push_back(DCU);
    while (!WorkList.empty()) {
      auto *MD = dyn_cast_or_null<MDNode>(WorkList.pop_back_val());
      if (!MD)
        continue;
      if (Visited.contains(MD))
        continue;
      Visited.insert(MD);
      Fn(MD);
      for (const MDOperand &MDO: MD->operands())
        WorkList.push_back(MDO.get());
    }
  };
  VisitMD([&](MDNode *MD) {
    auto *DSub = dyn_cast<DISubprogram>(MD);
    if (DSub)
      SubprogsByName[DSub->getName()] = DSub;
  });
  VisitMD([&](MDNode *MD) {
    auto *DVar = dyn_cast<DILocalVariable>(MD);
    if (!DVar)
      return;
    auto *DSub = dyn_cast<DISubprogram>(DVar->getScope());
    if (!DSub)
      return;
    if (!DVar->isParameter())
      return;
    ParamsBySubprog[DSub].push_back(DVar);
  });
  for (auto &Pair: ParamsBySubprog) {
    SmallVectorImpl<DILocalVariable *> &Vec = Pair.second;
    llvm::sort(Vec, [=](DILocalVariable *A, DILocalVariable *B) {
      return A->getArg() - B->getArg();
    });
  }
}

static const char *TP_RAW_EVENT_PFX = "trace_event_raw_event_";
static const char *TP_DISPATCH_PFX = "__SCT__tp_func_";

enum Nullness {
  DefNotNull,
  CanBeNull,
  DontKnowIfNull,
};

static Nullness nullnessFromLVI(LazyValueInfo &LVI, Value *V, Instruction *I) {
  PointerType *Type = dyn_cast<PointerType>(V->getType());
  auto *Pred = LVI.getPredicateAt(ICmpInst::ICMP_EQ, V, ConstantPointerNull::get(Type), I, false);
  auto *EqNull = dyn_cast_or_null<ConstantInt>(Pred);
  if (EqNull && EqNull->isZero())
    return DefNotNull;
  if (EqNull && EqNull->isOne())
    return CanBeNull;
  return DontKnowIfNull;
}

static bool implicitNonNull(FunctionAnalysisManager &FAM, Function &F, Value *V, bool CheckNonNull = true) {
  LazyValueInfo &LVI = FAM.getResult<LazyValueAnalysis>(F);
  for (Use &U: V->uses()) {
    if (CheckNonNull) {
      Instruction *I = dyn_cast_or_null<Instruction>(U.getUser());
      if (!I)
        continue;
      if (nullnessFromLVI(LVI, V, I) == DefNotNull)
        continue;
    }
    bool R = false;
    if (auto *GEP = dyn_cast<GetElementPtrInst>(U.getUser()))
      R = implicitNonNull(FAM, F, GEP, false);
    else if (auto *Load = dyn_cast<LoadInst>(U.getUser()))
      R = Load->getPointerOperand() == V;
    else if (auto *Store = dyn_cast<StoreInst>(U.getUser()))
      R = Store->getPointerOperand() == V;
    else if (auto *Call = dyn_cast<CallInst>(U.getUser());
             Call && isa<Function>(Call->getCalledOperand())) {
      Function *Fn = cast<Function>(Call->getCalledOperand());
      Intrinsic::ID FnId = Fn->getIntrinsicID();
      if (FnId == Intrinsic::memcpy ||
          FnId == Intrinsic::memset ||
          FnId == Intrinsic::memmove) {
        R = true;
      } else {
        Argument *A = Fn->getArg(Call->getArgOperandNo(&U));
        R = implicitNonNull(FAM, *Fn, A);
      }
    }
    if (R)
      return true;
  }
  return false;
}

static bool comparedWithNull(Value *V) {
  PointerType *Type = dyn_cast<PointerType>(V->getType());
  Value *Null = ConstantPointerNull::get(Type);
  for (User *U: V->users()) {
    if (auto *Cmp = dyn_cast<ICmpInst>(U);
        Cmp && (Cmp->getOperand(0) == Null || Cmp->getOperand(1) == Null))
      return true;
  }
  return false;
}

static bool findDominatingNonNullUse(DominatorTree &Dom, Value *V, Instruction *At) {
  // llvm::dbgs() << "findDominatingNonNullUse:\n";
  // llvm::dbgs() << "  V=" << *V << "\n";
  for (User *U: V->users()) {
    // llvm::dbgs() << "  U=" << *U << "\n";
    if (!Dom.dominates(U, At))
      continue;
    // llvm::dbgs() << "    dominates\n";
    if (auto *GEP = dyn_cast<GetElementPtrInst>(U);
        GEP && GEP->getPointerOperand() == V &&
        findDominatingNonNullUse(Dom, GEP, At))
      return true;
    if (auto *Load = dyn_cast<LoadInst>(U);
        Load && Load->getPointerOperand() == V)
      return true;
    if (auto *Store = dyn_cast<StoreInst>(U);
        Store && Store->getPointerOperand() == V)
      return true;
    if (auto *Call = dyn_cast<CallInst>(U)) {
      if (Call->getCalledOperand() == V)
        return true;
      if (auto *Fn = dyn_cast<Function>(Call->getCalledOperand())) {
        Intrinsic::ID FnId = Fn->getIntrinsicID();
        if (FnId == Intrinsic::memcpy ||
            FnId == Intrinsic::memset ||
            FnId == Intrinsic::memmove)
          return true;
      }
    }
  }
  return false;
}

// Parameter properties at tracepoint call:
// - non-null at call (or):
//   - value tracking says its not null
//   - is dominated by implicit non-null access
// - maybe null at call (or):
//   - value tracking says its null
//   - value tracking does not know if it is null *and*
//     there is a null check for parameter somewhere in the calling function
static Nullness inferArgNullness(FunctionAnalysisManager &FAM, CallInst *Call, unsigned ArgNo) {
  Function *Fn = Call->getFunction();
  LazyValueInfo &LVI = FAM.getResult<LazyValueAnalysis>(*Fn);
  Value *V = Call->getArgOperand(ArgNo);
  Nullness N = nullnessFromLVI(LVI, V, Call);
  // llvm::dbgs() << *Fn << "\n";
  // llvm::dbgs() << "inferArgNullness:\n";
  // llvm::dbgs() << "  V: " << *V << "\n";
  // llvm::dbgs() << "  N: " << N << "\n";
  if (N == DefNotNull || N == CanBeNull)
    return N;
  if (N == DontKnowIfNull && comparedWithNull(V))
    return CanBeNull;
  if (isa<GetElementPtrInst>(V))
    return DefNotNull;
  DominatorTree &Dom = FAM.getResult<DominatorTreeAnalysis>(*Fn);
  if (findDominatingNonNullUse(Dom, V, Call))
    return DefNotNull;
  return DontKnowIfNull;
}

static enum Nullness inferArgNullness(FunctionAnalysisManager &FAM, Function &F, unsigned ArgNo) {
  // llvm::dbgs() << F << "\n";
  // llvm::dbgs() << "ArgNo: " << ArgNo << "\n";
  Nullness AccN = DontKnowIfNull;
  for (User *U: F.users()) {
    auto *Call = dyn_cast<CallInst>(U);
    if (!Call)
      continue;
    // llvm::dbgs() << "Call: " << *Call << "\n";
    Nullness N = inferArgNullness(FAM, Call, ArgNo);
    if (N == DontKnowIfNull || AccN == N)
      continue;
    if (AccN == DontKnowIfNull) {
      AccN = N;
      continue;
    }
    if ((AccN == DefNotNull && N == CanBeNull) ||
        (AccN == CanBeNull  && N == DefNotNull)) {
      AccN = CanBeNull;
      continue;
    }
  }
  return AccN;
}

StringRef slice(StringRef S, unsigned Start) {
  return S.slice(Start, S.size());
}

struct TpParamInfo {
  StringRef TpName = "";
  StringRef ArgName = "";
  int ArgNo = -1;
  Nullness N = DontKnowIfNull;
  bool CallSite = false;
};

static void printJson(raw_ostream &OS, TpParamInfo &Info, bool &First) {
  OS << (First ? " " : ",");
  OS << "{";
  OS << "\"tp\": \"" << Info.TpName << "\", ";
  OS << left_justify("", std::max(0, 40 - (int)Info.TpName.size()));
  OS << "\"arg\": \"" << Info.ArgName << "\", ";
  OS << left_justify("", std::max(0, 16 - (int)Info.ArgName.size()));
  OS << "\"argNo\": " << Info.ArgNo << ", ";
  OS << "\"notNull\": " << (Info.N == DefNotNull) << ", ";
  OS << "\"canBeNull\": " << (Info.N == CanBeNull) << ", ";
  OS << "\"cs\": " << Info.CallSite;
  OS << "}\n";
  First = false;
}

static void inspectTracepointParams(raw_ostream &OS, Function &F, FunctionAnalysisManager &FAM,
                                    bool &First) {
  StringRef TpName = slice(F.getName(), strlen(TP_RAW_EVENT_PFX));
  SmallString<128> Tmp;
  unsigned ArgNo = 1;
  for (auto *AI = F.arg_begin() + 1/* skip __data */; AI != F.arg_end(); ++AI, ++ArgNo) {
    Argument &A = *AI;
    if (!A.getType()->isPointerTy())
      continue;
    bool ImplicitNonNull = implicitNonNull(FAM, F, &A);
    bool ComparedWithNull = comparedWithNull(&A);
    TpParamInfo PI;
    PI.TpName = TpName;
    PI.ArgName = A.getName();
    PI.ArgNo = ArgNo - 1;
    if (ImplicitNonNull)
      PI.N = DefNotNull;
    else if (ComparedWithNull)
      PI.N = CanBeNull;
    else
      PI.N = DontKnowIfNull;
    printJson(OS, PI, First);
  }
}

static void inspectTracepointSimpleCalls(raw_ostream &OS, Function &F,
                                         FunctionAnalysisManager &FAM, DISubprogsInfo &DInfo,
                                         bool &First) {
  StringRef TpName = slice(F.getName(), strlen("trace_"));
  unsigned ArgNo = 0;
  for (auto *AI = F.arg_begin(); AI != F.arg_end(); ++AI, ++ArgNo) {
    Argument &A = *AI;
    if (!A.getType()->isPointerTy())
      continue;
    Nullness N = inferArgNullness(FAM, F, ArgNo);
    TpParamInfo PI;
    PI.TpName = TpName;
    PI.ArgName = A.getName();
    PI.ArgNo = ArgNo;
    PI.N = N;
    PI.CallSite = true;
    printJson(OS, PI, First);
  }
}

static void inspectTracepointDispatchParams(raw_ostream &OS, Function &F,
                                            FunctionAnalysisManager &FAM, DISubprogsInfo &DInfo,
                                            bool &First) {
  StringRef TpName = slice(F.getName(), strlen(TP_DISPATCH_PFX));
  SmallString<128> Tmp;
  Tmp.append("trace_");
  Tmp.append(TpName);
  SmallVectorImpl<DILocalVariable *> *DIParams = DInfo.lookupParams(Tmp);
  if (DIParams && (DIParams->size() + 1 != F.arg_size()))
    DIParams = nullptr;
  unsigned ArgNo = 1;
  for (auto *AI = F.arg_begin() + 1 /* skip __data */; AI != F.arg_end(); ++AI, ++ArgNo) {
    Argument &A = *AI;
    if (!A.getType()->isPointerTy())
      continue;
    Nullness N = inferArgNullness(FAM, F, ArgNo);
    TpParamInfo PI;
    PI.TpName = TpName;
    if (DIParams)
      PI.ArgName = (*DIParams)[ArgNo - 1]->getName();
    PI.ArgNo = ArgNo - 1;
    PI.N = N;
    PI.CallSite = true;
    printJson(OS, PI, First);
  }
}

static bool isTracepointRawEvent(Function &F) {
  return !F.isDeclaration() &&
         F.getName().starts_with(TP_RAW_EVENT_PFX) &&
         F.arg_size() > 0 &&
         F.getArg(0)->getName() == "__data" &&
         F.getArg(0)->getType()->isPointerTy();
}

static bool isTracepointDispatch(Function &F) {
  return F.isDeclaration() &&
         F.getName().starts_with(TP_DISPATCH_PFX)
         //&& F.getName().ends_with("add_device_to_group")
         ;
}

} /* Anonymous namespace */

PreservedAnalyses TracepointParams::run(Module &M, ModuleAnalysisManager &MAM) {
  if (TracepointParamsOut == "")
    return PreservedAnalyses::all();
  std::error_code EC;
  raw_fd_stream OS = TracepointParamsOut == "-"
                     ? raw_fd_stream(1, false)
                     : raw_fd_stream(TracepointParamsOut, EC);
  FunctionAnalysisManager &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  DISubprogsInfo DInfo;
  bool First = true;
  DInfo.collect(M);
  OS << "[\n";
  for (Function &F: M) {
    if (isTracepointRawEvent(F))
      inspectTracepointParams(OS, F, FAM, First);
    if (!F.getName().starts_with(TP_RAW_EVENT_PFX) &&
        !F.getName().starts_with("trace_raw_output_") &&
        F.getName().starts_with("trace_") &&
        !F.use_empty())
      inspectTracepointSimpleCalls(OS, F, FAM, DInfo, First);
    if (isTracepointDispatch(F) && !F.use_empty())
      inspectTracepointDispatchParams(OS, F, FAM, DInfo, First);
  }

  for (GlobalVariable &GV: M.globals()) {
    if (!GV.hasInitializer())
      continue;
    auto *CS = dyn_cast<ConstantStruct>(GV.getInitializer());
    if (!CS)
      continue;
    if (CS->getType()->isLiteral() ||
        CS->getType()->getName() != "struct.trace_event_call")
      continue;
    if (CS->getNumOperands() < 2)
      continue;
    auto *Template = CS->getOperand(1);
    if (First) {
      OS << " ";
      First = false;
    } else {
      OS << ",";
    }
    OS << "{\"type\": \"event\"";
    OS << ", name: \"" << GV.getName() << "\",";
    OS << left_justify("", std::max(0, 40 - (int)GV.getName().size()));
    OS << "\"template\": \"" << Template->getName() << "\"";
    OS << "}\n";
  }
  OS << "]\n";

  return PreservedAnalyses::all();
}
