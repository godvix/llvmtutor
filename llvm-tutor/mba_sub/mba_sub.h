#ifndef LLVM_TUTOR_MBA_SUB_H_
#define LLVM_TUTOR_MBA_SUB_H_

#include "llvm/IR/Constant.h"   // ConstantDataArray
#include "llvm/IR/IRBuilder.h"  // IRBuilder
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"                 // parseIRFile
#include "llvm/Support/CommandLine.h"               // SMDiagnostic
#include "llvm/Support/Debug.h"                     // LLVM_DEBUG
#include "llvm/Transforms/Utils/BasicBlockUtils.h"  // ReplaceInstWithInst

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);
void RunOnBasicBlock(llvm::BasicBlock& basic_block);

#endif  // LLVM_TUTOR_MBA_SUB_H_
