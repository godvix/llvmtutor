#include "merge_bb.h"

using namespace llvm;

// Number of basic blocks merged
static int num_deduplicate_bbs = 0;
// Number of updated branch targets
static int overall_num_of_updated_branch_targets = 0;

static int GetNumNonDbgInstInBB(BasicBlock* bb);

int main(int argc, char** argv) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  merge_bb::RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

bool merge_bb::CanRemoveInst(const Instruction* inst) {
  assert(inst->hasOneUse() && "`inst` needs to have exactly one use");

  auto* phi_node_use = dyn_cast<PHINode>(*(inst->use_begin()));
  auto* succ = inst->getParent()->getTerminator()->getSuccessor(0);
  auto* user = cast<Instruction>(*(inst->user_begin()));

  bool same_parent_bb = (user->getParent() == inst->getParent());
  bool used_in_phi =
      (phi_node_use != nullptr && phi_node_use->getParent() == succ &&
       phi_node_use->getIncomingValueForBlock(inst->getParent()) == inst);

  return used_in_phi || same_parent_bb;
}

bool merge_bb::CanMergeInstructions(ArrayRef<Instruction*> insts) {
  const auto* inst1 = insts[0];
  const auto* inst2 = insts[1];
  dbgs() << *inst1 << "\n";
  dbgs() << *inst2 << "\n";

  if (inst1->isSameOperationAs(inst2) == false) return false;

  // Each instruction must have exactly zero or one use.
  bool has_use = !(inst1->user_empty());
  for (auto* iter : insts) {
    if (has_use && iter->hasOneUse() == false) return false;
    if (has_use == false && iter->user_empty() == false) return false;
  }

  // Not all instructions that have one use can be merged. Make sure that
  // instructions that have one use can be safely deleted.
  if (has_use) {
    if (CanRemoveInst(inst1) == false || CanRemoveInst(inst2) == false)
      return false;
  }

  // Make sure that `inst1` and `inst2` have identical operands.
  assert(inst2->getNumOperands() == inst1->getNumOperands());
  int num_opnds = inst1->getNumOperands();
  for (int opnd_idx = 0; opnd_idx < num_opnds; ++opnd_idx) {
    if (inst2->getOperand(opnd_idx) != inst1->getOperand(opnd_idx))
      return false;
  }

  return true;
}

int merge_bb::UpdateBranchTargets(BasicBlock* bb_to_erase,
                                  BasicBlock* bb_to_retain) {
  SmallVector<BasicBlock*, 8> bb_to_update(predecessors(bb_to_erase));

  dbgs() << "DEDUPLICATE BB: merging duplicated blocks ("
         << bb_to_erase->getName() << " into " << bb_to_retain->getName()
         << ")\n";

  int updated_targets_count = 0;
  for (auto* bb0 : bb_to_update) {
    // The terminator is either a branch (conditional or unconditional) or a
    // switch statement. One of its targets should be `bb_to_erase`. Replace
    // that target with `bb_to_retain`.
    auto* term = bb0->getTerminator();
    for (int op_idx = 0, num_opnds = term->getNumOperands(); op_idx < num_opnds;
         ++op_idx) {
      if (term->getOperand(op_idx) == bb_to_erase) {
        term->setOperand(op_idx, bb_to_retain);
        ++updated_targets_count;
      }
    }
  }

  return updated_targets_count;
}

bool merge_bb::MergeDuplicatedBlock(BasicBlock* bb1,
                                    SmallPtrSet<BasicBlock*, 8>& delete_list) {
  // Do not optimize the entry block
  if (bb1 == &(bb1->getParent()->getEntryBlock())) return false;

  // Only merge CFG edges of unconditional branch
  auto* bb1_term = dyn_cast<BranchInst>(bb1->getTerminator());
  if (bb1_term == nullptr || bb1_term->isConditional()) return false;

  // Do not optimize non-branch and non-switch CFG edges (to keep things
  // relatively simple)
  for (auto* block : predecessors(bb1)) {
    if (isa<BranchInst>(block->getTerminator()) == false &&
        isa<SwitchInst>(block->getTerminator()) == false)
      return false;
  }

  auto* bb_succ = bb1_term->getSuccessor(0);

  BasicBlock::iterator inst_iter = bb_succ->begin();
  const auto* pn = dyn_cast<PHINode>(inst_iter);
  Value* in_val_bb1 = nullptr;
  Instruction* in_inst_bb1 = nullptr;
  if (pn != nullptr) {
    // Do not optimize if multiple PHI instructions exist in the successor (to
    // keep things relatively simple)
    if (++inst_iter != bb_succ->end() && isa<PHINode>(inst_iter)) return false;

    in_val_bb1 = pn->getIncomingValueForBlock(bb1);
    in_inst_bb1 = dyn_cast<Instruction>(in_val_bb1);
  }

  int bb1_num_inst = GetNumNonDbgInstInBB(bb1);
  for (auto* bb2 : predecessors(bb_succ)) {
    // Do not optimize the entry block
    if (bb2 == &(bb2->getParent()->getEntryBlock())) continue;

    // Only merge CFG edges of unconditional branch
    auto* bb2_term = dyn_cast<BranchInst>(bb2->getTerminator());
    if (bb2_term == nullptr || bb2_term->isConditional()) continue;

    // Do not optimize non-branch and non-switch CFG edges (to keep things
    // relatively simple)
    for (auto* block : predecessors(bb2)) {
      if (isa<BranchInst>(block->getTerminator()) == false &&
          isa<SwitchInst>(block->getTerminator()) == false)
        continue;
    }

    // Skip basic blocks that have already been marked for merging
    if (delete_list.find(bb2) != delete_list.end()) continue;

    // Make sure that `bb2` != `bb1`
    if (bb2 == bb1) continue;

    // `bb1` and `bb2` are definitely different if the number of instructions is
    // not identical
    if (bb1_num_inst != GetNumNonDbgInstInBB(bb2)) continue;

    // Control flow can be merged if incoming values the PHI node at the
    // successor are same values of both defined in the BBs to merge. For the
    // latter case, `CanMergeInstructions` executes further analysis.
    if (pn != nullptr) {
      auto* in_val_bb2 = pn->getIncomingValueForBlock(bb2);
      auto* in_inst_bb2 = dyn_cast<Instruction>(in_val_bb2);

      bool are_values_similar = (in_val_bb1 == in_val_bb2);
      bool both_values_defined_in_parent =
          ((in_inst_bb1 != nullptr && in_inst_bb1->getParent() == bb1) &&
           (in_inst_bb2 != nullptr && in_inst_bb2->getParent() == bb2));
      if (are_values_similar == false && both_values_defined_in_parent == false)
        continue;
    }

    // Finally, check that all instructions in `bb1` and `bb2` are identical
    LockstepReverseIterator lockstep_reverse_iter(bb1, bb2);
    while (lockstep_reverse_iter.IsValid() &&
           CanMergeInstructions(*lockstep_reverse_iter))
      --lockstep_reverse_iter;

    // Valid iterator means that a mismatch was found in middle of BB
    if (lockstep_reverse_iter.IsValid()) continue;

    // It is safe to de-duplicate - do so.
    int updated_targets = UpdateBranchTargets(bb1, bb2);
    assert(updated_targets != 0 && "No branch target was updated");
    overall_num_of_updated_branch_targets += updated_targets;
    delete_list.insert(bb1);
    ++num_deduplicate_bbs;

    return true;
  }

  return false;
}

void merge_bb::RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void merge_bb::RunOnFunction(Function& func) {
  SmallPtrSet<BasicBlock*, 8> delete_list;

  for (auto& bb : func) MergeDuplicatedBlock(&bb, delete_list);

  for (auto* bb : delete_list) DeleteDeadBlock(bb);
}

//------------------------------------------------------------------------------
// Helper data structures
//------------------------------------------------------------------------------
merge_bb::LockstepReverseIterator::LockstepReverseIterator(BasicBlock* bb1_in,
                                                           BasicBlock* bb2_in)
    : bb1_(bb1_in), bb2_(bb2_in), fail_(false) {
  this->insts_.clear();

  auto* inst_bb1 = GetLastNonDbgInst(this->bb1_);
  if (inst_bb1 == nullptr) this->fail_ = true;

  auto* inst_bb2 = GetLastNonDbgInst(this->bb2_);
  if (inst_bb2 == nullptr) this->fail_ = true;

  this->insts_.push_back(inst_bb1);
  this->insts_.push_back(inst_bb2);
}

Instruction* merge_bb::LockstepReverseIterator::GetLastNonDbgInst(
    BasicBlock* bb) {
  auto* inst = bb->getTerminator();

  do {
    inst = inst->getPrevNode();
  } while (inst != nullptr && isa<DbgInfoIntrinsic>(inst));

  return inst;
}

bool merge_bb::LockstepReverseIterator::IsValid() const {
  return this->fail_ == false;
}

merge_bb::LockstepReverseIterator&
merge_bb::LockstepReverseIterator::operator--() {
  if (this->fail_) return *this;

  for (auto*& inst : this->insts_) {
    do {
      inst = inst->getPrevNode();
    } while (inst != nullptr && isa<DbgInfoIntrinsic>(inst));

    if (inst == nullptr) {
      // Already at the beginning of BB
      this->fail_ = true;
      return *this;
    }
  }

  return *this;
}

ArrayRef<Instruction*> merge_bb::LockstepReverseIterator::operator*() const {
  return this->insts_;
}

static int GetNumNonDbgInstInBB(BasicBlock* bb) {
  int count = 0;
  for (auto& inst : *bb) {
    if (isa<DbgInfoIntrinsic>(inst) == false) ++count;
  }
  return count;
}
