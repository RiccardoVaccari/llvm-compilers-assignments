#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/LoopFusion.h"

using namespace llvm;
PreservedAnalyses LoopFusion::run(Function &F, FunctionAnalysisManager &AM) {
	LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

	for (auto *TopLevelLoop : LI){
		for (auto *L : depth_first(TopLevelLoop)){
			if (L->isInnermost())
				L->getHeader()->print(outs());
			
		}
	// Capire se i due loop iterano allo stesso modo
	// Capire se i due loop sono adiacenti

	}
	
	return PreservedAnalyses::all();
}