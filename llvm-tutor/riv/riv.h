#ifndef LLVM_TUTOR_RIV_H_
#define LLVM_TUTOR_RIV_H_

#include "llvm/ADT/MapVector.h"    // MapVector
#include "llvm/ADT/SmallPtrSet.h"  // SmallPtrSet
#include "llvm/IR/Constant.h"      // ConstantDataArray
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"  // IRBuilder
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"                 // parseIRFile
#include "llvm/Support/CommandLine.h"               // SMDiagnostic
#include "llvm/Transforms/Utils/BasicBlockUtils.h"  // ReplaceInstWithInst

namespace riv {

using RivResult = llvm::MapVector<const llvm::BasicBlock*,
                                  llvm::SmallPtrSet<llvm::Value*, 8> >;
using NodeType = llvm::DomTreeNodeBase<llvm::BasicBlock>*;

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);
RivResult BuildRiv(llvm::Function& func, NodeType cfg_root);

}  // namespace riv

#endif  // LLVM_TUTOR_RIV_H_
