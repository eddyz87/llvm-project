#ifndef LLVM_ANALYSIS_TRACEPOINT_PARAMS_H
#define LLVM_ANALYSIS_TRACEPOINT_PARAMS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

class TracepointParams : public PassInfoMixin<TracepointParams> {
public:
  explicit TracepointParams() = default;

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

} // namespace llvm

#endif
