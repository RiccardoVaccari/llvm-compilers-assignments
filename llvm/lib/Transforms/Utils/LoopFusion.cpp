#include "llvm/Transforms/Utils/LoopFusion.h"
#include <set>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

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
  if (!SE.hasLoopInvariantBackedgeTakenCount(L1) or !SE.hasLoopInvariantBackedgeTakenCount(L2))
    return false;
  return getTripCount(L1, SE) == getTripCount(L2, SE);
}

BasicBlock * getBody(Loop *L){
  BranchInst *HeaderBI = dyn_cast<BranchInst>(L->getHeader()->getTerminator());
  if (!HeaderBI)
    return nullptr;
  
  if (L->contains(HeaderBI->getSuccessor(0)))
    return HeaderBI->getSuccessor(0);

  if (L->contains(HeaderBI->getSuccessor(1)))
    return HeaderBI->getSuccessor(1);

  return nullptr;
}

bool haveNegativeDistance(Loop *L1, Loop *L2, DependenceInfo &DI){
  /*
  BasicBlock *Loop1Body = getBody(L1);
  BasicBlock *Loop2Body = getBody(L2);

  if(!Loop1Body or !Loop2Body)
    return true;
    
  for(auto &instLoop1 : *Loop1Body)
    for(auto &instLoop2 : *Loop2Body){
        if (DI.depends(&instLoop1, &instLoop2, true)){
          outs() << "Istruzioni " << instLoop1 << " e " << instLoop2 << " sono dipendenti\n";
         //return true;
        }
    }
  
  */
  return false;

}


bool isOkForFusion(Loop *L){
  
  if (!L->getLoopPreheader() or !L->getHeader() or !L->getLoopLatch() or !L->getExitingBlock() or !L->getExitBlock())
    return false;
  
  
  if (!L->isLoopSimplifyForm())
    return false;
  

  // if (!L->isRotatedForm())
  //   return false;

  return true;
}

bool replaceUsesIV(Loop *L1, Loop *L2){
  PHINode* ivL1 = L1->getCanonicalInductionVariable();
  PHINode* ivL2 = L2->getCanonicalInductionVariable();

  if(!ivL1 || !ivL2)
    return false;
  
  
  ivL2->replaceAllUsesWith(ivL1);
  ivL2->eraseFromParent();

  return true;
}

bool loopFuse(Loop *L1, Loop *L2, LoopInfo &LI){
  if(!replaceUsesIV(L1, L2)){
    outs() << "Errore nella modifica degli usi della Induction Varible nel Loop2.\n";
    return false;
  }
  
  // Gets all the needed blocks. 
  BasicBlock *L2PreHeader = getEntryBlock(L2);

  BasicBlock *L1Header = L1->getHeader();
  BasicBlock *L2Header = L2->getHeader();

  BasicBlock *L1Body = getBody(L1);
  BasicBlock *L2Body = getBody(L2);

  BasicBlock *L1Latch = L1->getLoopLatch();
  BasicBlock *L2Latch = L2->getLoopLatch();

  BasicBlock *L2Exit = L2->getExitBlock();

  // Change 
  //  L1Header 
  //    -> (True) L1Body 
  //    -> (False) L2Preheader
  // To  
  //  L1Header 
  //    -> (True) L1Body 
  //    -> (False) L2Exit
  L1Header->getTerminator()->replaceSuccessorWith(L2PreHeader, L2Exit);

  // Change 
  //  L1Body 
  //    -> L1Latch 
  // To  
  //  L1Body 
  //    -> L2Body
  L1Body->getTerminator()->replaceSuccessorWith(L1Latch, L2Body);

  // Change 
  //  L2Body 
  //    -> L2Latch 
  // To  
  //  L2Body 
  //    -> L1Latch
  L2Body->getTerminator()->replaceSuccessorWith(L2Latch, L1Latch);

  // Change 
  //  L2Header 
  //    -> L2Body 
  // To  
  //  L2Header 
  //    -> L2Latch
  L2Header->getTerminator()->replaceSuccessorWith(L2Body, L2Latch);

  for(auto blocksL2 : L2->blocks()){
    if(blocksL2 != L2Header && blocksL2 != L2Latch){
      L1->addBasicBlockToLoop(blocksL2, LI);
    }
  }

  return true;

}

std::list<Loop *> getTopLevelLoops(LoopInfo &LI, bool verbose = false){
  std::list<Loop *> topLevelLoops;

  for (auto *TopLevelLoop : LI){
    TopLevelLoop->print(outs());

    if (isOkForFusion(TopLevelLoop)){
      outs() << "OK\n";
      topLevelLoops.push_front(TopLevelLoop);
    }
  }

  for(auto L : topLevelLoops){
    L->print(outs());
  }
  return topLevelLoops;
}

bool tryLoopFusion(std::list<Loop *> topLevelLoops, LoopInfo &LI, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE, DependenceInfo &DI){
  bool areFusable = true;

  for (auto it1 = topLevelLoops.begin(); it1 != topLevelLoops.end(); ++it1) {
    for (auto it2 = std::next(it1); it2 != topLevelLoops.end(); ++it2) {
      areFusable = true;

      Loop *L1 = *it1;
      Loop *L2 = *it2;

      outs() << "\nL1 => ";
      L1->print(outs());

      outs() << "L2 => ";
      L2->print(outs());

      bool cfeCheck = areControlFlowEquivalent(getEntryBlock(L1), getEntryBlock(L2), DT, PDT);
      outs() << (cfeCheck ? "Sono CFE\n" : "Non sono CFE\n");

      bool adjCheck = areAdjacent(L1, L2);
      outs() << (adjCheck ? "Sono adiacenti\n" : "Non sono adiacenti\n");

      bool tcCheck = isSameTripCount(L1, L2, SE);
      outs() << (tcCheck ? "Hanno lo stesso trip count\n" : "Non hanno lo stesso trip count\n");

      bool negdisCheck = haveNegativeDistance(L1, L2, DI);
      outs() << (!negdisCheck ? "Non ci sono dipendenze\n" : "");

      areFusable = cfeCheck && adjCheck && tcCheck && !negdisCheck;

      if(areFusable){
        if(loopFuse(L1, L2, LI)){
          outs() << "Loop fusi correttamente.\n\n";
          LI.erase(L2);
          return true;

        }
        else
          outs() << "Errore durante loop fusion.\n\n";
      }
      outs() << "\n\n";
    }
  }
  return false;

}

PreservedAnalyses LoopFusion::run(Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
  ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
  DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
  
  bool programChanged = false;
  std::list<Loop *> topLevelLoops;


  do{
    outs() << "----------- Loop da analizzare -----------\n";
   
    topLevelLoops = getTopLevelLoops(LI, true);

    programChanged = (topLevelLoops.size() >= 2) ? tryLoopFusion(topLevelLoops, LI, DT, PDT, SE, DI) : false;
    if(programChanged)
      EliminateUnreachableBlocks(F);
    else
      outs() << "Analisi dei loop completata.\n";
    
  }while(programChanged);

  return PreservedAnalyses::all();
}