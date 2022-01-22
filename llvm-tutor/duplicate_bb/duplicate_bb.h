#ifndef LLVM_TUTOR_DUPlICATE_BB_H_
#define LLVM_TUTOR_DUPlICATE_BB_H_

#include <random>

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"  // ConstantDataArray
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"  // IRBuilder
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"  // parseIRFile
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"               // SMDiagnostic
#include "llvm/Support/Debug.h"                     // LLVM_DEBUG
#include "llvm/Transforms/Utils/BasicBlockUtils.h"  // ReplaceInstWithInst
#include "llvm/Transforms/Utils/ValueMapper.h"      // ValueToValueMapTy

namespace riv {

using RivResult = llvm::MapVector<const llvm::BasicBlock*,
                                  llvm::SmallPtrSet<llvm::Value*, 8> >;
using NodeType = llvm::DomTreeNodeBase<llvm::BasicBlock>*;

RivResult BuildRiv(llvm::Function& func, NodeType cfg_root);

}  // namespace riv

namespace duplicate_bb {

// Maps BB, a BasicBlock, to one integer value (defined in a different
// BasicBlock) that's reachable in BB. The Value that BB is mapped to is used in
// the `if-then-else` construct when cloning BB.
using BBToSingleRivMap =
    std::vector<std::tuple<llvm::BasicBlock*, llvm::Value*> >;
// Maps a Value before duplication to a Phi node that merges the corresponding
// values after duplication/cloning.
using ValueToPhiMap = std::map<llvm::Value*, llvm::Value*>;

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);

// Creates a BBToSingleRIVMap of BasicBlocks that are suitable for cloning.
BBToSingleRivMap FindBBsToDuplicate(llvm::Function& func,
                                    const riv::RivResult& riv_result);
// Clones the input basic block:
//  * injects an `if-then-else` construct using ContextValue
//  * duplicates BB
//  * adds PHI nodes as required
void CloneBB(llvm::BasicBlock& BB, llvm::Value* ContextValue,
             ValueToPhiMap& ReMapper);

}  // namespace duplicate_bb

#endif  // LLVM_TUTOR_DUPlICATE_BB_H_
