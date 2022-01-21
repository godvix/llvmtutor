#include "riv.h"
using namespace llvm;

static void PrintRivResult(raw_ostream& out_stream,
                           const riv::RivResult& riv_map);

int main(int argc, char** argv) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  riv::RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void riv::RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void riv::RunOnFunction(Function& func) {
  auto dominator_tree = DominatorTree(func);
  RivResult res = BuildRiv(func, dominator_tree.getRootNode());
  PrintRivResult(errs(), res);
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

static void PrintRivResult(raw_ostream& out_stream,
                           const riv::RivResult& riv_map) {
  out_stream << "=================================================\n";
  out_stream << "LLVM-TUTOR: RIV analysis results\n";
  out_stream << "=================================================\n";

  const char* str1 = "BB id";
  const char* str2 = "Reachable Ineger Values";
  out_stream << format("%-10s %-30s\n", str1, str2);
  out_stream << "-------------------------------------------------\n";

  const char* empty_str = "";

  for (auto const& key_value : riv_map) {
    std::string dummy_str;
    raw_string_ostream basic_block_id_stream(dummy_str);
    key_value.first->printAsOperand(basic_block_id_stream, false);
    out_stream << format("BB %-12s %-30s\n",
                         basic_block_id_stream.str().c_str(), empty_str);
    for (auto const* IntegerValue : key_value.second) {
      std::string dummy_str;
      raw_string_ostream inst_stream(dummy_str);
      IntegerValue->print(inst_stream);
      out_stream << format("%-12s %-30s\n", empty_str,
                           inst_stream.str().c_str());
    }
  }

  out_stream << "\n\n";
}
