#ifndef LLVM_TUTOR_RIV_H_
#define LLVM_TUTOR_RIV_H_

#include <random>

#include "llvm/ADT/MapVector.h"  // MapVector
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constant.h"  // ConstantDataArray
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"  // IRBuilder
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"  // parseIRFile
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"               // SMDiagnostic
#include "llvm/Support/Debug.h"                     // LLVM_DEBUG
#include "llvm/Transforms/Utils/BasicBlockUtils.h"  // ReplaceInstWithInst

using RivResult = llvm::MapVector<const llvm::BasicBlock*,
                                  llvm::SmallPtrSet<llvm::Value*, 8> >;
using NodeType = llvm::DomTreeNodeBase<llvm::BasicBlock>*;

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);
void RunOnBasicBlock(llvm::BasicBlock& basic_block);
RivResult BuildRiv(llvm::Function& func, NodeType cfg_root);

#endif  // LLVM_TUTOR_RIV_H_
