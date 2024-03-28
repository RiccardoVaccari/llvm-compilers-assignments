//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

enum opType{
    MUL,
    ADD,
    DIV
};

bool strenghtReduction(Instruction &inst, opType opT){
  int pos = 0;

  for (auto operand = inst.op_begin(); operand != inst.op_end(); operand++, pos++)
  {
    ConstantInt *C = dyn_cast<ConstantInt>(operand);
    if (C) 
    {
      APInt value = C->getValue();
      if(value.isPowerOf2())
      {
        int shift_count = C->getValue().exactLogBase2();

        Instruction *shiftInst = nullptr;

        if(opT == MUL)
            shiftInst = BinaryOperator::Create(
              BinaryOperator::Shl, 
              inst.getOperand(!pos), 
              ConstantInt::get(C->getType(), shift_count)
              );        
        else
            shiftInst = BinaryOperator::Create(
              BinaryOperator::LShr, 
              inst.getOperand(!pos), 
              ConstantInt::get(C->getType(), shift_count)
              );    


        shiftInst->insertAfter(&inst);
        inst.replaceAllUsesWith(shiftInst);
        outs() <<"Instruction:\n\t"<< inst << "\nReplaced with:\n\t" << *shiftInst << "\n\n";
        return true;
      }
    }
  }
  return false;
}

bool algebraicIdentity(Instruction &inst, opType opT){
  int pos = 0;
  for (auto operand = inst.op_begin(); operand != inst.op_end(); operand++, pos++)
  {
    ConstantInt *C = dyn_cast<ConstantInt>(operand);
    if (C) 
    {
      APInt value = C->getValue();
      
      if((value.isZero() && opT == ADD) || (value.isOne() && opT == MUL))
      {
        inst.replaceAllUsesWith(inst.getOperand(!pos));
        outs() <<"Instruction:\n\t"<< inst << "\nhas a "<< value << " in "<<pos<<" position"<<"\n\n";
        return true;
      }
    }
  }
  return false;

}




bool runOnBasicBlock(BasicBlock &B) {


	for (auto &inst : B)
  {

    BinaryOperator *op = dyn_cast<BinaryOperator>(&inst);

    if(not op)
      continue;   
    
    switch(op->getOpcode()){
      case BinaryOperator::Mul:
        if(!algebraicIdentity(inst, MUL))
          strenghtReduction(inst,MUL);

        break;
      case (BinaryOperator::Add):
        algebraicIdentity(inst,ADD);
        break;

      case (BinaryOperator::SDiv):
        strenghtReduction(inst,DIV);
        break;

      default:

        break;
    } 
    
    
    
     
	 }
  return true;
}


bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}


PreservedAnalyses LocalOpts::run(Module &M,
                                      ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}

