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

bool runOnBasicBlock(BasicBlock &B) {
	for (auto &inst : B){
		BinaryOperator *mul = dyn_cast<BinaryOperator>(&inst);
		if (not mul or mul->getOpcode() != BinaryOperator::Mul)
			continue;
		
		int pos = 0;
		for (auto op = inst.op_begin(); op != inst.op_end(); op++, pos++){
      	ConstantInt *C = dyn_cast<ConstantInt>(op);
			if (C) {
				APInt value = C->getValue();
				if(value.isPowerOf2()){
					int shift_count = C->getValue().exactLogBase2();
					Instruction *shiftInst = BinaryOperator::Create(BinaryOperator::Shl, inst.getOperand(!pos), ConstantInt::get(C->getType(), shift_count));
    			shiftInst->insertAfter(&inst);
    			inst.replaceAllUsesWith(shiftInst);
          outs() <<"Instruction:\n\t"<< inst << "\nReplaced with:\n\t" << *shiftInst << "\n";
				}
			}
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

