#include "llvm/Transforms/Utils/LoopInvariantCodeMotion.h"
#include <set>

using namespace llvm;

bool isInstructionLoopInvariant(Instruction &Inst, Loop &L);

bool isValueLoopInvariant(Value *Val, Loop &L) {
  /*
  Funzione che controlla che l'operando sia loop invariant.
  */

  // Se l'operando è una costante oppure è un argument della function allora restituisco true. 
  if (isa<ConstantInt>(Val) or isa<Argument>(Val))
    return true;

  // Trovo la reaching definition dell'operando e...
  Instruction *I = dyn_cast<Instruction>(Val);

  //... se è un phinode restituisco falso.
  if (!I or isa<PHINode>(Val))
    return false;

  //... se la reaching definition si trova al di fuori del loop allora l'instruzione sarà loop invariant. 
  if (!L.contains(I))
    return true;

  //... altrimenti dovrò controllare a sua volta che l'istruzione sia loop invariant o no. 
  // Se infatti a sua volta l'operando ha una reaching definition nel loop che è loop invariant allora
  // sarà loop invariant altrimenti no. 
  return isInstructionLoopInvariant(*I, L);
}

bool isInstructionLoopInvariant(Instruction &Inst, Loop &L) {
  /*
  Funzione che restituisce true se l'istruzione è loop invariant, false altrimenti. 
  */
  if (isa<PHINode>(Inst)) // Un phi node non è mai loop invariant. 
    return false;

  // Controllo che ogni operando sia loop invariant, se anche solo uno non lo è l'istruzione NON è loop invariant. 
  for (auto OI = Inst.op_begin(); OI != Inst.op_end(); OI++) {
    Value *Val = *OI;
    if (!isValueLoopInvariant(Val, L))
      return false;
  }

  return true;
}

bool dominatesAllExits(Instruction &Inst, std::set<BasicBlock *> LoopExitBB,
                       DominatorTree &DT) {
  /*
  Funzione che restituisce true se il blocco a cui appartiene domina tutte le uscite, altrimenti false. 
  */
  for (BasicBlock *BB : LoopExitBB) 
    if (!DT.dominates(Inst.getParent(), BB)) // Se anche solo un blocco d'uscita non è dominato dal blocco padre allora si retuisce false
      return false;
  return true;
}

bool isLoopDead(Instruction &Inst, Loop &L) {
  /*
  Funzione che, scorrendo gli usi, determina se la variabile è dead o no in base a che l'uso si trovi fuori dal loop. 
  */
  for (auto UI = Inst.user_begin(); UI != Inst.user_end(); UI++) {
    Instruction *I = dyn_cast<Instruction>(*UI);
    if (!L.contains(I))
      return false;
  }
  return true;
}

PreservedAnalyses LoopInvariantCodeMotion::run(Loop &L, LoopAnalysisManager &LAM,
                                LoopStandardAnalysisResults &LAR,
                                LPMUpdater &LU) {

  if (!L.isLoopSimplifyForm()) {
    outs() << "\nIl loop non è in forma NORMALE.\n";
    return PreservedAnalyses::all();
  }

  outs() << "\nIl loop è in forma NORMALE, procediamo.\n";

  BasicBlock *PreHeader = L.getLoopPreheader(); // Pre header
  Instruction &FinalInst = PreHeader->back(); // Branch del pre header

  std::set<BasicBlock *> LoopExitBB;
  std::set<Instruction *> InstructionsLICM;

  outs() << "********** LOOP **********\n";
  for (auto BI = L.block_begin(); BI != L.block_end(); ++BI) {
    BasicBlock *BB = *BI;

    // Trovo tutti gli Exit Basic Blocks 
    // (successori di dei blocchi del Loop che non appartengono al loop)
    for (BasicBlock *Succ : successors(BB))
      if (!L.contains(Succ))
        LoopExitBB.insert(BB);


    // Trovo tutte le Loop Invariant instructions
    for (auto II = BB->begin(); II != BB->end(); II++) {
      Instruction &Inst = *II;

      // Controllo tutte le condizioni affinchè possa avvenire la code motion. 
      
      // Controllo se l'istruzione è loop invariant. 
      if (isInstructionLoopInvariant(Inst, L)){
        outs() << "LOOP INVARIANT -> ";
        Inst.print(outs());
        // Controllo che si trovi in un blocco che domina tutte le uscite
        if (dominatesAllExits(Inst, LoopExitBB, LAR.DT)){
          InstructionsLICM.insert(&Inst);
          outs() << "\t[ DOMINA LE USCITE ]";
        }
        // OPPURE
        // Controllo che non abbia usi dopo il loop.
        if (isLoopDead(Inst, L)) {
          InstructionsLICM.insert(&Inst);
          outs() << "\t[ DEAD LOOP ]";
        }

        outs() << "\n";
      } 
    }
  }

  // Code motion
  for (Instruction *Inst : InstructionsLICM) {
    Inst->removeFromParent();
    Inst->insertBefore(&FinalInst);
  }


  return PreservedAnalyses::all();
}