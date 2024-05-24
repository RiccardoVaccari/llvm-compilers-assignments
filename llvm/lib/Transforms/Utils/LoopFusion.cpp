#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/LoopFusion.h"

using namespace llvm;
PreservedAnalyses LoopFusion::run(Function &F, FunctionAnalysisManager &AM) {
	outs() << "Ciao sto facendo loop-fusion!\n";
	return PreservedAnalyses::all();
}