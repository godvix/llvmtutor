#ifndef LLVM_TUTOR_MERGE_BB_H_
#define LLVM_TUTOR_MERGE_BB_H_

#include <random>

#include "llvm/ADT/ArrayRef.h"
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

namespace merge_bb {

// Checks whether the input instruction `inst` (that has exactly one use) can be
// removed. This is the case when its only user is either:
// 1) a PHI (it can be easily updated if `inst` is removed), or
// 2) located in the same block in as `inst` (if that block is removed then the
// user will also be removed)
bool CanRemoveInst(const llvm::Instruction* inst);

// Instructions in `insts` belong to different blocks that unconditionally
// branch to a common successor. Analyze them and return true if it would be
// possible to merge them, i.e. replace `inst1` with `inst2` (or vice-versa).
bool CanMergeInstructions(llvm::ArrayRef<llvm::Instruction*> insts);

// Replace the destination of incoming edges of `bb_to_erase` by `bb_to_retain`
int UpdateBranchTargets(llvm::BasicBlock* bb_to_erase,
                        llvm::BasicBlock* bb_to_retain);

// If `bb` is duplicated, then merges `bb` with its duplicate and adds `bb` to
// `delete_list`. `delete_list` contains the list of blocks to be deleted.
bool MergeDuplicatedBlock(llvm::BasicBlock* bb,
                          llvm::SmallPtrSet<llvm::BasicBlock*, 8>& delete_list);

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);

//------------------------------------------------------------------------------
// Helper data structures
//------------------------------------------------------------------------------
// Iterates through intructions in `bb1` and `bb2` in reverse order from the
// first non-debug instruction. For example (assume all blocks have size `n`):
//   LockstepReverseIterator iter(bb1, bb2);
//   *(iter--) = [bb1[n], bb2[n]];
//   *(iter--) = [bb1[n - 1], bb2[n - 1]];
//   *(iter--) = [bb1[n - 2], bb2[n - 2]];
//   ...
class LockstepReverseIterator {
 public:
  LockstepReverseIterator(llvm::BasicBlock* bb1_in, llvm::BasicBlock* bb2_in);

  llvm::Instruction* GetLastNonDbgInst(llvm::BasicBlock* bb);
  bool IsValid() const;

  LockstepReverseIterator& operator--();

  llvm::ArrayRef<llvm::Instruction*> operator*() const;

 private:
  llvm::BasicBlock *bb1_, *bb2_;

  llvm::SmallVector<llvm::Instruction*, 2> insts_;
  bool fail_;
};

}  // namespace merge_bb

#endif  // LLVM_TUTOR_MERGE_BB_H_
