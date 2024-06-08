#include "llvm/Transforms/Utils/LoopFusion.h"
#include <set>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/DependenceAnalysis.h"
using namespace llvm;

bool areControlFlowEquivalent(BasicBlock *BB0, BasicBlock *BB1, DominatorTree &DT,PostDominatorTree &PDT) {
  /*
  Due loop sono CFE se L0 domina L1 e L1 postdomina L0 allora i due loop sono CFE equivalenti
  */
  return BB0 == BB1 or (DT.dominates(BB0, BB1) && PDT.dominates(BB1, BB0));
}

BasicBlock *getGuard(Loop *L) {
  /*
  La guardia corrisponde al blocco prima dell'header. 
  */
  return L->getLoopPreheader()->getUniquePredecessor();
}

BasicBlock *getEntryBlock(Loop *L) {
  /*
  L'entry block corrisponderà alla guardia se il loop ne ha una, altrimenti al pre-header
  */
  return (L->isGuarded() ? getGuard(L) : L->getLoopPreheader());
}

bool areAdjacent(Loop *L1, Loop *L2) {
  /*
  Due loop sono adiacenti se non ci sono basic blocks aggiuntivi nel CFG tra l'uscita del primo
  e l'inizio dell'altro. 

  - Se i loop sono guarded il successore non loop del guard branch di L0 deve essere l'entry Block
  - Se i loop NON sono guarded l'exit block di L0 deve essere il pre-header di L1 
  */
  BasicBlock *BB2ToCheck = getEntryBlock(L2);

  if (L1->isGuarded()) {
    // Se ha la guardia controllo che l'ultima istruzione di Branch 
    // della guardia jumpi all'entry block del loop 2. 

    BasicBlock *GuardBB = getGuard(L1);
    BranchInst *GuardBI = dyn_cast<BranchInst>(GuardBB->getTerminator());

    if (!GuardBI /*|| GuardBI->isUnconditional()*/)
      return false;

    return GuardBI->getSuccessor(1) == BB2ToCheck or
           GuardBI->getSuccessor(0) == BB2ToCheck;

  } else {
    // Se non ha la guardia l'exit block di L1 deve corrispondere all'entry block di L2 
    // (che può essere la sua guardia o il pre header)
    return L1->getExitBlock() == BB2ToCheck;
  }
}

const SCEV* getTripCount(Loop *L, ScalarEvolution &SE){
  /*
  Restituisce il numero di backedge che viene eseguito prima dell'exiting block, 
  se non è esattamente calcolabile, restituisce SCEVCouldNotCompute da cui poi, con
  getTripCountFromExitCount verrà computato il trip count. 
  */
  return SE.getTripCountFromExitCount(SE.getExitCount(L, L->getExitingBlock()));
} 

bool isSameTripCount(Loop *L1, Loop *L2, ScalarEvolution &SE){
  /*
  Funzione che controlla che i due loop abbiano lo stesso trip count.
  */
  return getTripCount(L1, SE) == getTripCount(L2, SE);
}

BasicBlock * getBody(Loop *L){
  /*
  Funzione che recupera il body del loop che corrisponderà al blocco successivo all'header 
  e che, allo stesso tempo, apparterà al loop. 
  */
  BranchInst *HeaderBI = dyn_cast<BranchInst>(L->getHeader()->getTerminator());
  if (!HeaderBI)
    return nullptr;
  
  // in pratica uno dei due casi restituerà l'exiting block
  // e l'altro invece il body. 

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
  
  std::list<Instruction *> storeInstLoop1;

  for(auto &instLoop1 : *Loop1Body)
  {
    if(isa<StoreInst>(instLoop1))
    {
      for (auto operandStore = instLoop1.op_begin(); operandStore != instLoop1.op_end(); operandStore++)
      {
        Instruction *IStore = dyn_cast<Instruction>(*operandStore);
        if (auto *GEPStore = dyn_cast<GetElementPtrInst>(IStore))
        {
          Value *ptrStore = GEPStore->getPointerOperand();
          outs() << instLoop1 << " scrive nell'array ";
          ptrStore->print(outs());
          outs() << "\n";

          for(auto &instLoop2 : *Loop2Body)
          {
            if(isa<LoadInst>(instLoop2))
            {
              for (auto operandLoad = instLoop2.op_begin(); operandLoad != instLoop2.op_end(); operandLoad++)
              {
                Instruction *ILoad = dyn_cast<Instruction>(*operandLoad);
                if (auto *GEPLoad = dyn_cast<GetElementPtrInst>(ILoad))
                {
                  Value *ptrLoad = GEPLoad->getPointerOperand();
                  outs() << instLoop2 << " legge dall'array ";
                  ptrLoad->print(outs());
                  outs() << "\n";
                  if(ptrLoad == ptrStore)
                  {
                    outs() << " Scrivono e leggono dallo stesso array\n";
                    if(std::unique_ptr< Dependence > DIobj = DI.depends(&instLoop2, &instLoop1,true))
                    {

                      outs() << "Dipendono";
                    }
                                       
                  }
                }
              }
            }
          }
        }
      }
    }
    */
   return false;
}

bool isOkForFusion(Loop *L){
  /*
  Controllo che il loop possa essere controllato per la fusione in modo tale da non dover
  controllare dopo che esistano tutti questi blocchi. 
  */
  if (!L->getLoopPreheader() or !L->getHeader() or !L->getLoopLatch() or !L->getExitingBlock() or !L->getExitBlock())
    return false;
  
  /*
  Controllo che il loop sia in forma normale
  */
  if (!L->isLoopSimplifyForm())
    return false;

  return true;
}

bool replaceUsesIV(Loop *L1, Loop *L2){
  /*
  Rimpiazza gli usi dell'iteration variable del loop 2 con quella del loop 1.
  */
  PHINode* ivL1 = L1->getCanonicalInductionVariable();
  PHINode* ivL2 = L2->getCanonicalInductionVariable();

  if(!ivL1 || !ivL2)
    return false;
  
  
  ivL2->replaceAllUsesWith(ivL1);
  ivL2->eraseFromParent();

  return true;
}

bool loopFuse(Loop *L1, Loop *L2, LoopInfo &LI){
  /*
  Funzione che fonde effettivamente i loop
  */
  if(!replaceUsesIV(L1, L2)){
    outs() << "Errore nella modifica degli usi della Induction Varible nel Loop2.\n";
    return false;
  }
  
  // Recupero tutti i blocchi necessari
  BasicBlock *L2PreHeader = getEntryBlock(L2);

  BasicBlock *L1Header = L1->getHeader();
  BasicBlock *L2Header = L2->getHeader();

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

  // Tutti i predecessori del Latch di L1 (i blocchi che formavano il body di L1)
  // dovranno avere un link verso al body di L2. Così facendo il body di L2 verrà eseguito
  // dopo il body di L1
  for(auto *L1LatchPred : predecessors(L1Latch))
    L1LatchPred->getTerminator()->replaceSuccessorWith(L1Latch, L2Body);


  // Tutti i predecessori del Latch di L2 (i blocchi che formavano il body di L2)
  // dovranno avere un link verso il latch di L1. Così facendo, una volta terminato il body di L2
  // verrà eseguito il Latch di L1. 
  for(auto *L2LatchPred : predecessors(L2Latch))
    L2LatchPred->getTerminator()->replaceSuccessorWith(L2Latch, L1Latch);

  // Change 
  //  L2Header 
  //    -> L2Body 
  // To  
  //  L2Header 
  //    -> L2Latch
  L2Header->getTerminator()->replaceSuccessorWith(L2Body, L2Latch);


  // Questa parte serve per aggiornare la composizione dei blocchi del Loop appena fuso, 
  // in pratica si dice quali blocchi fanno parte del nuovo loop e quali no. 
  // I blocchi del body del Loop2 verranno aggiunti al nuovo Loop ottenuto dalla fusione, 
  // questi blocchi corrisponderanno a tutti i blocchi TRANNE l'header e il latch. 
  for(auto blocksL2 : L2->blocks()){
    if(blocksL2 != L2Header && blocksL2 != L2Latch){
      L1->addBasicBlockToLoop(blocksL2, LI);
    }
  }
  return true;

}

std::list<Loop *> getTopLevelLoops(LoopInfo &LI, bool verbose = false){
  /*
  Funzione che crea e restituisce una lista dei TopLevelLoops presenti nel programma, 
  solo dopo avere controllato che fossero Ok For Fusion. 
  */
  std::list<Loop *> topLevelLoops;

  for (auto *TopLevelLoop : LI){
    if (isOkForFusion(TopLevelLoop))
      topLevelLoops.push_front(TopLevelLoop);
  }

  if(verbose)
    for(auto L : topLevelLoops)
      L->print(outs());
    
  return topLevelLoops;
}

bool tryLoopFusion(std::list<Loop *> topLevelLoops, LoopInfo &LI, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE, DependenceInfo &DI){
  // Funzione che "prova" a fare una loop fusion provando tutte le coppie possibili
  // di loop presenti nel programma. 

  // Se la funzione termina restituendo falso allora l'analisi è completata, non ci sono loop da fondere. 
  // Se la funzione restitusice true vuol dire che c'è stata una fusione e quindi questa funzione verrà richiamata. 
  
  bool areFusable = true;

  for (auto it1 = topLevelLoops.begin(); it1 != topLevelLoops.end(); ++it1) {
    for (auto it2 = std::next(it1); it2 != topLevelLoops.end(); ++it2) {
      // Scorro la lista dei toop level loops con due iteratori. 
  
      areFusable = true;

      Loop *L1 = *it1;
      Loop *L2 = *it2;

      outs() << "\nL1 => ";
      L1->print(outs());

      outs() << "L2 => ";
      L2->print(outs());

      // Eseguo tutti i controlli
      bool cfeCheck = areControlFlowEquivalent(getEntryBlock(L1), getEntryBlock(L2), DT, PDT);
      outs() << (cfeCheck ? "Sono CFE\n" : "Non sono CFE\n");

      bool adjCheck = areAdjacent(L1, L2);
      outs() << (adjCheck ? "Sono adiacenti\n" : "Non sono adiacenti\n");

      bool tcCheck = isSameTripCount(L1, L2, SE);
      outs() << (tcCheck ? "Hanno lo stesso trip count\n" : "Non hanno lo stesso trip count\n");

      bool negdisCheck = haveNegativeDistance(L1, L2, DI);
      outs() << (!negdisCheck ? "Non ci sono dipendenze\n" : "");

      // Tutti i controlli devono essere verificati affinchè possa avvenire la loop fuse. 
      areFusable = cfeCheck && adjCheck && tcCheck && !negdisCheck;

      if(areFusable){
        if(loopFuse(L1, L2, LI)){ // Fondo i loop
          outs() << "Loop fusi correttamente.\n\n";
          LI.erase(L2); // Rimuovo dal loop info L2 in modo tale che non esista più. 
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

  // Dichiaro e creo tutti gli strumenti di analisi che andrò ad utilizzare e passare alle funzioni. 
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
  ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
  DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
  
  bool programChanged = false; // Variabile che, tracka se il programma cambia, continua a provare la loop fuse. 
  std::list<Loop *> topLevelLoops;

  do{
    outs() << "----------- Loop da analizzare -----------\n";
    topLevelLoops = getTopLevelLoops(LI, true); // Recupero i top level loops
    // Se ci sono meno di due loop allora l'analisi termina
    // Se la loop fusion ha restituito falso allora termino, altrimenti continuo. 
    programChanged = (topLevelLoops.size() >= 2) ? tryLoopFusion(topLevelLoops, LI, DT, PDT, SE, DI) : false;
    if(programChanged)
      EliminateUnreachableBlocks(F); // Eliminazione blocchi irragiungibili
    else
      outs() << "Analisi dei loop completata.\n";
    
  }while(programChanged);

  return PreservedAnalyses::all();
}