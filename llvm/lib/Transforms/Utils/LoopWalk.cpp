#include "llvm/Transforms/Utils/LoopWalk.h"
#include <set>

using namespace llvm;

bool isInstructionLoopInvariant(Instruction &Inst, Loop &L);

bool isValueLoopInvariant(Value *Val, Loop &L) {
  if (isa<ConstantInt>(Val) or isa<Argument>(Val))
    return true;

  Instruction *I = dyn_cast<Instruction>(Val);
  if (!I or isa<PHINode>(Val))
    return false;

  if (!L.contains(I))
    return true;

  return isInstructionLoopInvariant(*I, L);
}

bool isInstructionLoopInvariant(Instruction &Inst, Loop &L) {

  if (isa<PHINode>(Inst))
    return false;

  for (auto OI = Inst.op_begin(); OI != Inst.op_end(); OI++) {
    Value *Val = *OI;
    if (!isValueLoopInvariant(Val, L))
      return false;
  }

  return true;
}

bool dominatesAllExits(Instruction &Inst, std::set<BasicBlock *> LoopExitBB,
                       DominatorTree &DT) {

  for (BasicBlock *BB : LoopExitBB) {
    // if (BB == Inst.getParent())
    //   continue;

    if (!DT.dominates(Inst.getParent(), BB))
      return false;
  }

  return true;
}

bool isLoopDead(Instruction &Inst, Loop &L) {
  for (auto UI = Inst.user_begin(); UI != Inst.user_end(); UI++) {
    Instruction *I = dyn_cast<Instruction>(*UI);
    if (!L.contains(I))
      return false;
  }
  return true;
}

PreservedAnalyses LoopWalk::run(Loop &L, LoopAnalysisManager &LAM,
                                LoopStandardAnalysisResults &LAR,
                                LPMUpdater &LU) {

  if (!L.isLoopSimplifyForm()) {
    outs() << "\nIl loop non è in forma NORMALE.\n";
    return PreservedAnalyses::all();
  }

  outs() << "\nIl loop è in forma NORMALE, procediamo.\n";
  BasicBlock *PreHeader = L.getLoopPreheader();
  Instruction &FinalInst = PreHeader->back();

  // BasicBlock *head = L.getHeader();
  // Function *F = head->getParent();

  // outs() << "********** CFG **********\n";
  // for (auto iter = F->begin(); iter != F->end(); iter++) {
  //   BasicBlock &BB = *iter;
  //   outs() << BB << "\n";
  // }

  std::set<BasicBlock *> LoopExitBB;
  std::set<Instruction *> InstructionsLICM;

  outs() << "********** LOOP **********\n";
  for (auto BI = L.block_begin(); BI != L.block_end(); ++BI) {
    BasicBlock *BB = *BI;

    // Find the Exit Basic Blocks
    for (BasicBlock *Succ : successors(BB))
      if (!L.contains(Succ))
        LoopExitBB.insert(BB);

    // BB->print(outs());
    // outs() << "\n------------------------------------------\n";

    // Find the Loop Invariant instructions
    for (auto II = BB->begin(); II != BB->end(); II++) {
      Instruction &Inst = *II;
      if (isInstructionLoopInvariant(Inst, L)){// and dominatesAllExits(Inst, LoopExitBB, LAR.DT)) {

        outs() << "LOOP INVARIANT -> ";
        Inst.print(outs());

        if (dominatesAllExits(Inst, LoopExitBB, LAR.DT)){
          InstructionsLICM.insert(&Inst);
          outs() << "\t[ DOMINA LE USCITE ]";
        }

        if (isLoopDead(Inst, L)) {
          InstructionsLICM.insert(&Inst);
          outs() << "\t[ DEAD LOOP ]";
        }

        outs() << "\n";
      } 
    }
  }

  for (Instruction *Inst : InstructionsLICM) {
    Inst->removeFromParent();
    Inst->insertBefore(&FinalInst);
  }

  return PreservedAnalyses::all();
}