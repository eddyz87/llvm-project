#ifndef LLVM_LIB_TARGET_BPF_BPFTARGETLOWERINGOBJECTFILE
#define LLVM_LIB_TARGET_BPF_BPFTARGETLOWERINGOBJECTFILE

#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {
class BPFTargetLoweringObjectFileELF : public TargetLoweringObjectFileELF {
  MCSection *JTSection = nullptr;

public:
  virtual void Initialize(MCContext &ctx, const TargetMachine &TM) override;
  virtual MCSection *
  getSectionForJumpTable(const Function &F, const TargetMachine &TM,
                         const MachineJumpTableEntry *JTE) const override;
};
}

#endif // LLVM_LIB_TARGET_BPF_BPFTARGETLOWERINGOBJECTFILE
