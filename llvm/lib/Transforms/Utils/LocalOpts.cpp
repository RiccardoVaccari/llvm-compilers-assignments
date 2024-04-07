//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

// clang -emit-llvm -S -c file.c .o file.ll
// opt

enum opType { MUL, ADD, DIV, SUB };

bool strenghtReduction(Instruction &inst, opType opT) {
  int pos = 0;

  for (auto operand = inst.op_begin(); operand != inst.op_end();
       operand++, pos++) {
    ConstantInt *C = dyn_cast<ConstantInt>(operand);
    if (C) {
      APInt value = C->getValue();
      if (value.isPowerOf2()) {
        int shift_count = C->getValue().exactLogBase2();

        Instruction::BinaryOps shiftT;

        if (opT == MUL)
          shiftT = Instruction::Shl;
        else if (pos == 1)
          shiftT = Instruction::LShr;
        else 
          return false;

        Instruction *shiftInst =
            BinaryOperator::Create(shiftT, inst.getOperand(!pos),
                                   ConstantInt::get(C->getType(), shift_count));

        shiftInst->insertAfter(&inst);
        inst.replaceAllUsesWith(shiftInst);
        outs() << "Strength Reduction\n\tInstruction:\n\t\t" << inst
               << "\n\tReplaced with:\n\t\t" << *shiftInst << "\n\n";
        return true;
      }
    }
  }
  return false;
}

bool advStrenghtReduction(Instruction &inst) {
  int pos = 0;

  for (auto operand = inst.op_begin(); operand != inst.op_end();
       operand++, pos++) {
    ConstantInt *C = dyn_cast<ConstantInt>(operand);
    if (C) {
      APInt value = C->getValue();
      /*
      x * 15 = 15 * x = (x << 4) - x
      x * 17 = 17 * x = (x << 4) + x
      */
      Instruction::BinaryOps sumType;
      int shift_count = 0;

      if ((value + 1).isPowerOf2()) {
        shift_count = (value + 1).exactLogBase2();
        sumType = Instruction::Sub;
      } else if ((value - 1).isPowerOf2()) {
        shift_count = (value - 1).exactLogBase2();
        sumType = Instruction::Add;
      } else
        continue;

      Instruction *shiftInst =
          BinaryOperator::Create(BinaryOperator::Shl, inst.getOperand(!pos),
                                 ConstantInt::get(C->getType(), shift_count));

      Instruction *sumInst =
          BinaryOperator::Create(sumType, inst.getOperand(!pos), shiftInst);

      shiftInst->insertAfter(&inst);
      sumInst->insertAfter(shiftInst);
      inst.replaceAllUsesWith(sumInst);

      outs() << "Advanced Strength Reduction\n\tInstruction:\n\t\t" << inst
             << "\n\tReplaced with:\n\t\t" << *shiftInst << " and " << *sumInst
             << "\n\n";
      return true;
    }
  }
  return false;
}

bool algebraicIdentity(Instruction &inst, opType opT) {
  int pos = 0;
  for (auto operand = inst.op_begin(); operand != inst.op_end();
       operand++, pos++) {
    ConstantInt *C = dyn_cast<ConstantInt>(operand);
    if (C) {
      APInt value = C->getValue();

      if ((value.isZero() && opT == ADD) || (value.isOne() && opT == MUL)) {
        inst.replaceAllUsesWith(inst.getOperand(!pos));
        outs() << "Algebraic Identity\n\tInstruction:\n\t" << inst
               << "\n\thas a " << value << " in " << pos << " position."
               << "\n\n";
        return true;
      }
    }
  }
  return false;
}

bool multiInstOpt(Instruction &inst, opType opT) {
  int pos = 0;
  for (auto operandUser = inst.op_begin(); operandUser != inst.op_end();
       operandUser++, pos++) {
    ConstantInt *CUser = dyn_cast<ConstantInt>(operandUser);

    if (CUser) {

      APInt valueToFind = CUser->getValue();
      Instruction::BinaryOps opToFind =
          opT == SUB ? Instruction::Add : Instruction::Sub;

      for (auto iter = inst.user_begin(); iter != inst.user_end(); ++iter) {

        User *instUser = *iter;
        BinaryOperator *opUsee = dyn_cast<BinaryOperator>(instUser);

        if (not opUsee)
          continue;

        for (auto operandUsee = instUser->op_begin();
             operandUsee != instUser->op_end(); operandUsee++) {
          ConstantInt *CUsee = dyn_cast<ConstantInt>(operandUsee);
          if (CUsee && opUsee->getOpcode() == opToFind &&
              CUsee->getValue() == valueToFind) {

            outs() << "Multi-Instruction Optimization\n\t" << inst << " and "
                   << *instUser << "\n ";
            instUser->replaceAllUsesWith(inst.getOperand(!pos));

            return true;
          }
        }
      }
    }
  }
  return false;
}

bool runOnBasicBlock(BasicBlock &B) {

  for (auto &inst : B) {
    BinaryOperator *op = dyn_cast<BinaryOperator>(&inst);

    if (not op)
      continue;

    switch (op->getOpcode()) {
    case BinaryOperator::Mul:
      if (!algebraicIdentity(inst, MUL) and !strenghtReduction(inst, MUL))
        advStrenghtReduction(inst);
      break;
    case BinaryOperator::Add:
      if (!algebraicIdentity(inst, ADD))
        multiInstOpt(inst, ADD);
      break;

    case BinaryOperator::Sub:
      multiInstOpt(inst, SUB);
      break;

    case (BinaryOperator::UDiv):
    case (BinaryOperator::SDiv):
      strenghtReduction(inst, DIV);
      break;

    default:
      break;
    }
  }

  for(auto instItr = B.begin(); instItr != B.end();){
    BinaryOperator *op = dyn_cast<BinaryOperator>(instItr);
    if(op and instItr->hasNUses(0))
      instItr = instItr->eraseFromParent();
    else
      instItr++;
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

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}
