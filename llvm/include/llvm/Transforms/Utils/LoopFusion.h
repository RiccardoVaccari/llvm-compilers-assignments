#ifndef LLVM_TRANSFORMS_LOOPFUSION_H
#define LLVM_TRANSFORMS_LOOPFUSION_H

#include "llvm/IR/PassManager.h"
// #include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"

namespace llvm {
	class LoopFusion : public PassInfoMixin<LoopFusion> {
		public:
		PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
	};
} // namespace llvm
#endif // LLVM_TRANSFORMS_LOOPFUSION_H