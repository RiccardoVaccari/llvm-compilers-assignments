#include "llvm/Transforms/Utils/LoopFusion.h"
#include <set>

using namespace llvm;

bool areControlFlowEquivalent(BasicBlock *BB1, BasicBlock *BB2, Function &F,
                              FunctionAnalysisManager &AM) {

  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);

  return (DT.dominates(BB1, BB2)) and (PDT.dominates(BB2, BB1));
}

BasicBlock *getGuard(Loop *L) {
  return L->getLoopPreheader()->getUniquePredecessor();
}

bool areAdjacent(Loop *L1, Loop *L2) {

  BasicBlock *BB2ToCheck = L2->isGuarded() ? getGuard(L2) : L2->getLoopPreheader();

  if (L1->isGuarded()) {
    // outs() << "Ha la guardia -> ";
    // L1->print(outs());

    BasicBlock *GuardBB = getGuard(L1);
    BranchInst *GuardBI = dyn_cast<BranchInst>(GuardBB->getTerminator());
    if (!GuardBI || GuardBI->isUnconditional())
      return false;

    return GuardBI->getSuccessor(1) == BB2ToCheck or GuardBI->getSuccessor(0) == BB2ToCheck;

  } else {
    return L1->getExitBlock() == BB2ToCheck;
  }
}

PreservedAnalyses LoopFusion::run(Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

  std::set<Loop *> topLevelLoops;

  for (auto *TopLevelLoop : LI)
    topLevelLoops.insert(TopLevelLoop);

  for (auto *L1 : topLevelLoops) {
    for (auto *L2: topLevelLoops){
    	if (L1 == L2)
    		continue;
    	outs() << "L1 => ";
    	L1->print(outs());

    	outs() << "L2 => ";
    	L2->print(outs());
    	outs() << (areAdjacent(L1, L2) ? "Sono adiacenti\n" : "Non sono adiacenti\n");
      outs() << "------------------------------\n";
    }
  }

  return PreservedAnalyses::all();
}