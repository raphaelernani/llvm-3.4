//===- ProfilingUtils.cpp - Helper functions shared by profilers ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a few helper functions which are used by profile
// instrumentation code to instrument the code.  This allows the profiler pass
// to worry about *what* to insert, and these functions take care of *how* to do
// it.
//
//===----------------------------------------------------------------------===//

#include "ProfilingUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

void llvm::InsertProfilingInitCall(Function *MainFn, const char *FnName,
                                   GlobalValue *Array,
                                   PointerType *arrayType) {
  LLVMContext &Context = MainFn->getContext();
  Type *ArgVTy =
    PointerType::getUnqual(Type::getInt8PtrTy(Context));
  PointerType *UIntPtr = arrayType ? arrayType :
		Type::getInt32PtrTy(Context);
  Module &M = *MainFn->getParent();
  Constant *InitFn = M.getOrInsertFunction(FnName, Type::getInt32Ty(Context),
                                           Type::getInt32Ty(Context),
                                           ArgVTy, UIntPtr,
                                           Type::getInt32Ty(Context),
                                           (Type *)0);

  // This could force argc and argv into programs that wouldn't otherwise have
  // them, but instead we just pass null values in.
  std::vector<Value*> Args(4);
  Args[0] = Constant::getNullValue(Type::getInt32Ty(Context));
  Args[1] = Constant::getNullValue(ArgVTy);

  // Skip over any allocas in the entry block.
  BasicBlock *Entry = MainFn->begin();
  BasicBlock::iterator InsertPos = Entry->begin();
  while (isa<AllocaInst>(InsertPos)) ++InsertPos;

  std::vector<Constant*> GEPIndices(2,
                             Constant::getNullValue(Type::getInt32Ty(Context)));
  llvm::ArrayRef<llvm::Constant *> arGEPIndices(GEPIndices);
  unsigned NumElements = 0;
  if (Array) {
    Args[2] = ConstantExpr::getGetElementPtr(Array, arGEPIndices);
    NumElements =
      cast<ArrayType>(Array->getType()->getElementType())->getNumElements();
  } else {
    // If this profiling instrumentation doesn't have a constant array, just
    // pass null.
    Args[2] = ConstantPointerNull::get(UIntPtr);
  }
  Args[3] = ConstantInt::get(Type::getInt32Ty(Context), NumElements);
  llvm::ArrayRef<llvm::Value *> arArgs(Args);

  CallInst *InitCall = CallInst::Create(InitFn, arArgs, "newargc", InsertPos);

  // If argc or argv are not available in main, just pass null values in.
  Function::arg_iterator AI;
  switch (MainFn->arg_size()) {
  default:
  case 2:
    AI = MainFn->arg_begin(); ++AI;
    if (AI->getType() != ArgVTy) {
      Instruction::CastOps opcode = CastInst::getCastOpcode(AI, false, ArgVTy,
                                                            false);
      InitCall->setArgOperand(1,
          CastInst::Create(opcode, AI, ArgVTy, "argv.cast", InitCall));
    } else {
      InitCall->setArgOperand(1, AI);
    }
    /* FALL THROUGH */

  case 1:
    AI = MainFn->arg_begin();
    // If the program looked at argc, have it look at the return value of the
    // init call instead.
    if (!AI->getType()->isIntegerTy(32)) {
      Instruction::CastOps opcode;
      if (!AI->use_empty()) {
        opcode = CastInst::getCastOpcode(InitCall, true, AI->getType(), true);
        AI->replaceAllUsesWith(
          CastInst::Create(opcode, InitCall, AI->getType(), "", InsertPos));
      }
      opcode = CastInst::getCastOpcode(AI, true,
                                       Type::getInt32Ty(Context), true);
      InitCall->setArgOperand(0,
          CastInst::Create(opcode, AI, Type::getInt32Ty(Context),
                           "argc.cast", InitCall));
    } else {
      AI->replaceAllUsesWith(InitCall);
      InitCall->setArgOperand(0, AI);
    }
    break;

  case 0: break;
  }
}

void llvm::IncrementCounterInBlock(BasicBlock *BB, unsigned CounterNum,
                                   GlobalValue *CounterArray, bool beginning) {
  // Insert the increment after any alloca or PHI instructions...
  BasicBlock::iterator InsertPos = beginning ? BB->getFirstNonPHI() :
		BB->getTerminator();
  while (isa<AllocaInst>(InsertPos))
    ++InsertPos;

  LLVMContext &Context = BB->getContext();

  // Create the getelementptr constant expression
  std::vector<Constant*> Indices(2);
  Indices[0] = Constant::getNullValue(Type::getInt32Ty(Context));
  Indices[1] = ConstantInt::get(Type::getInt32Ty(Context), CounterNum);
  llvm::ArrayRef<llvm::Constant *> arGEPIndices(Indices);
  Constant *ElementPtr =
    ConstantExpr::getGetElementPtr(CounterArray, arGEPIndices);

  // Load, increment and store the value back.
  Value *OldVal = new LoadInst(ElementPtr, "OldFuncCounter", InsertPos);
  Value *NewVal = BinaryOperator::Create(Instruction::Add, OldVal,
                                 ConstantInt::get(Type::getInt32Ty(Context), 1),
                                         "NewFuncCounter", InsertPos);
  new StoreInst(NewVal, ElementPtr, InsertPos);
}
