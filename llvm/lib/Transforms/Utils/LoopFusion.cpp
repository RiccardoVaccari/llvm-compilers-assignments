#include "llvm/Transforms/Utils/LoopFusion.h"
#include <set>

using namespace llvm;

bool areControlFlowEquivalent(BasicBlock *BB0, BasicBlock *BB1, DominatorTree &DT,PostDominatorTree &PDT) {
  return BB0 == BB1 or (DT.dominates(BB0, BB1) && PDT.dominates(BB1, BB0));
}

BasicBlock *getGuard(Loop *L) {
  return L->getLoopPreheader()->getUniquePredecessor();
}

BasicBlock *getEntryBlock(Loop *L) {
  return (L->isGuarded() ? getGuard(L) : L->getLoopPreheader());
}

bool areAdjacent(Loop *L1, Loop *L2) {

  BasicBlock *BB2ToCheck = getEntryBlock(L2);

  if (L1->isGuarded()) {
    // outs() << "Ha la guardia -> ";
    // L1->print(outs());

    BasicBlock *GuardBB = getGuard(L1);
    BranchInst *GuardBI = dyn_cast<BranchInst>(GuardBB->getTerminator());
    if (!GuardBI || GuardBI->isUnconditional())
      return false;

    return GuardBI->getSuccessor(1) == BB2ToCheck or
           GuardBI->getSuccessor(0) == BB2ToCheck;

  } else {
    return L1->getExitBlock() == BB2ToCheck;
  }
}

const SCEV* getTripCount(Loop *L, ScalarEvolution &SE){
  return SE.getTripCountFromExitCount(SE.getExitCount(L, L->getExitingBlock()));
}

bool isSameTripCount(Loop *L1, Loop *L2, ScalarEvolution &SE){
  return getTripCount(L1, SE) == getTripCount(L2, SE);
}

PreservedAnalyses LoopFusion::run(Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
  ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

  std::set<Loop *> topLevelLoops;

  for (auto *TopLevelLoop : LI)
    topLevelLoops.insert(TopLevelLoop);

  for (auto *L1 : topLevelLoops) {
    for (auto *L2 : topLevelLoops) {
      if (L1 == L2)
        continue;
      outs() << "\nL1 => ";
      L1->print(outs());

      outs() << "L2 => ";
      L2->print(outs());

      outs() << (areControlFlowEquivalent(getEntryBlock(L1), getEntryBlock(L2), DT, PDT) ? "Sono CFE\n" : "Non sono CFE\n");
      outs() << (areAdjacent(L1, L2) ? "Sono adiacenti\n" : "Non sono adiacenti\n");
      outs() << (isSameTripCount(L1, L2, SE) ? "Hanno lo stesso trip count\n" : "Non hanno lo stesso trip count\n");

      outs() << "------------------------------\n";
    }
  }

  return PreservedAnalyses::all();
}