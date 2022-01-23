#include "duplicate_bb.h"

using namespace llvm;

int main(int argc, char** argv) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  duplicate_bb::RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

riv::RivResult riv::BuildRiv(Function& func, NodeType cfg_root) {
  RivResult result_map;

  // Initialise a double-ended queue that will be used to traverse all basic
  // blocks in `func`
  std::deque<NodeType> basic_blocks_to_process;
  basic_blocks_to_process.push_back(cfg_root);

  // STEP 1: For every basic block `basic_block` compute the set of integer
  // values defined in `basic_block`
  RivResult defined_values_map;
  for (auto& basic_block : func) {
    auto& values = defined_values_map[&basic_block];
    for (auto& inst : basic_block) {
      if (inst.getType()->isIntegerTy()) values.insert(&inst);
    }
  }

  // STEP 2: Compute the RIVs for the entry basic block. This will include
  // global variables and input arguments.
  auto& entry_basic_block_values = result_map[&(func.getEntryBlock())];

  for (auto& global : func.getParent()->getGlobalList()) {
    if (global.getType()->isIntegerTy())
      entry_basic_block_values.insert(&global);
  }

  for (auto& arg : func.args()) {
    if (arg.getType()->isIntegerTy()) entry_basic_block_values.insert(&arg);
  }

  // STEP 3: Traverse the CFG for every basic block in `func` calculate its RIVs
  while (basic_blocks_to_process.empty() == false) {
    auto* parent = basic_blocks_to_process.back();
    basic_blocks_to_process.pop_back();

    // Get the values defined in parent
    auto& parent_defs = defined_values_map[parent->getBlock()];
    // Get the RIV set of for `parent` (Since RivMap is updated on every
    // iteration, its contents are likely to be moved around when resizing. This
    // means that we need a copy of it (i.e. a reference is not sufficient).
    auto parent_rivs = result_map[parent->getBlock()];

    // Loop over all basic blocks that `parent` dominates and update their RIV
    // sets
    for (NodeType child : *parent) {
      basic_blocks_to_process.push_back(child);
      auto child_basic_block = child->getBlock();

      // Add values defined in `parent` to the current child's set of RIV
      result_map[child_basic_block].insert(parent_defs.begin(),
                                           parent_defs.end());

      // Add `parent`'s set of RIVs to the current child's RIV
      result_map[child_basic_block].insert(parent_defs.begin(),
                                           parent_defs.end());
    }
  }

  return result_map;
}

void duplicate_bb::RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void duplicate_bb::RunOnFunction(Function& func) {
  auto dominator_tree = DominatorTree(func);

  // Find BBs to duplicate
  auto targets = FindBBsToDuplicate(
      func, riv::BuildRiv(func, dominator_tree.getRootNode()));

  // This map is used to keep track of the new bindings. Otherwise, the
  // information from RIV will become obsolete.
  ValueToPhiMap re_mapper;

  // Duplicate
  for (auto& bb_ctx : targets)
    CloneBB(*std::get<0>(bb_ctx), std::get<1>(bb_ctx), re_mapper);
}

duplicate_bb::BBToSingleRivMap duplicate_bb::FindBBsToDuplicate(
    Function& func, const riv::RivResult& riv_result) {
  BBToSingleRivMap blocks_to_duplicate;

  // Get a random number generator. This will be used to choose a context value
  // for the injected `if-then-else` construct.
  std::random_device random_device;
  std::mt19937_64 rng(random_device());

  for (auto& bb : func) {
    // Basic blocks which are landing pads are used for handling exceptions.
    // That's out of scope of this pass.
    if (bb.isLandingPad()) continue;

    // Get the set of RIVs for this block
    const auto& reachable_values = riv_result.lookup(&bb);
    size_t reachable_values_count = reachable_values.size();

    // Are there any RIVs for this BB? We need at least one to be able to
    // duplicate this BB.
    if (reachable_values_count == 0) {
      errs() << "No context values for this BB\n";
      continue;
    }

    // Get a random context value from the RIV set
    auto iter = reachable_values.begin();
    std::uniform_int_distribution<> distribution(0, reachable_values_count - 1);
    std::advance(iter, distribution(rng));

    if (dyn_cast<GlobalValue>(*iter) != nullptr) {
      errs() << "Random context value is a global variable. Skipping this BB\n";
      continue;
    }

    errs() << "Random context value: " << **iter << "\n";

    // Store the binding between the current BB and the context variable that
    // will be used for the `if-then-else` construct.
    blocks_to_duplicate.emplace_back(&bb, *iter);
  }

  return blocks_to_duplicate;
}

void duplicate_bb::CloneBB(BasicBlock& bb, Value* context_value,
                           ValueToPhiMap& re_mapper) {
  static int duplicate_bb_count = 0;

  // Don't duplicate Phi nodes - start right after them
  auto* bb_head = bb.getFirstNonPHI();

  // Create the condition for 'if-then-else'
  IRBuilder<> builder(bb_head);
  auto* condition = builder.CreateIsNull(re_mapper.count(context_value)
                                             ? re_mapper[context_value]
                                             : context_value);

  // Create and insert the 'if-else' blocks. At this point both blocks are
  // trivial and contain only one terminator instruction branching to BB's tail,
  // which contains all the instructions from BBHead onwards.
  Instruction* then_term = nullptr;
  Instruction* else_term = nullptr;
  SplitBlockAndInsertIfThenElse(condition, bb_head, &then_term, &else_term);
  auto* tail = then_term->getSuccessor(0);

  assert(tail == else_term->getSuccessor(0) && "Inconsistent CFG");

  // Give the new basic blocks some meaningful names. This is not required, but
  // makes the output easier to read.
  std::string duplicate_bb_id = std::to_string(duplicate_bb_count);
  then_term->getParent()->setName("lt-clone-1-" + duplicate_bb_id);
  else_term->getParent()->setName("lt-clone-2-" + duplicate_bb_id);
  tail->setName("lt-tail-" + duplicate_bb_id);
  then_term->getParent()->getSinglePredecessor()->setName("lt-if-then-else-" +
                                                          duplicate_bb_id);

  // Variables to keep track of the new bindings
  ValueToValueMapTy tail_map, then_map, else_map;

  // The list of instructions in Tail that don't produce any values and thus can
  // be removed
  SmallVector<Instruction*, 8> to_remove;

  // Iterate through the original basic block and clone every instruction into
  // the 'if-then' and 'else' branches. Update the bindings/uses on the fly
  // (through ThenVMap, ElseVMap, TailVMap). At this stage, all instructions
  // apart from PHI nodes, are stored in Tail.
  for (auto iter = tail->begin(); iter != tail->end(); ++iter) {
    auto& inst = *iter;
    assert(isa<PHINode>(&inst) == false &&
           "Phi nodes have already been filtered out");

    // Skip terminators - duplicating them wouldn't make sense unless we want to
    // delete Tail completely.
    if (inst.isTerminator()) {
      RemapInstruction(&inst, tail_map, RemapFlags::RF_IgnoreMissingLocals);
      continue;
    }

    // Clone the instructions.
    auto *then_clone = inst.clone(), *else_clone = inst.clone();

    // Operands of ThenClone still hold references to the original BB.
    // Update/remap them.
    RemapInstruction(then_clone, then_map, RemapFlags::RF_IgnoreMissingLocals);
    then_clone->insertBefore(then_term);
    then_map[&inst] = then_clone;

    // Operands of ElseClone still hold references to the original BB.
    // Update/remap them.
    RemapInstruction(else_clone, else_map, RemapFlags::RF_IgnoreMissingLocals);
    else_clone->insertBefore(else_term);
    else_map[&inst] = else_clone;

    // Instructions that don't produce values can be safely removed from Tail
    if (then_clone->getType()->isVoidTy()) {
      to_remove.push_back(&inst);
      continue;
    }

    // Instruction that produce a value should not require a slot in the TAIL
    // *but* they can be used from the context, so just always generate a PHI,
    // and let further optimization do the cleaning
    auto* phi = PHINode::Create(then_clone->getType(), 2);
    phi->addIncoming(then_clone, then_term->getParent());
    phi->addIncoming(else_clone, else_term->getParent());
    tail_map[&inst] = phi;

    re_mapper[&inst] = phi;

    // Instructions are modified as we go, use the iterator version of
    // ReplaceInstWithInst.
    ReplaceInstWithInst(tail->getInstList(), iter, phi);
  }

  // Purge instructions that don't produce any value
  for (auto inst : to_remove) inst->eraseFromParent();

  ++duplicate_bb_count;
}
