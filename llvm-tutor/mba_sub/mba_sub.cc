#include "mba_sub.h"

int main(int argc, char** argv) {
  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  auto owner = llvm::parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    llvm::errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  RunOnModule(*owner);

  if (llvm::verifyModule(*owner, &llvm::errs())) {
    llvm::errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(argv[1], ec, llvm::sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void RunOnModule(llvm::Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void RunOnFunction(llvm::Function& func) {
  for (auto& basic_block : func) RunOnBasicBlock(basic_block);
}

void RunOnBasicBlock(llvm::BasicBlock& basic_block) {
  using namespace llvm;
  // Loop over all instructions in the block. Replacing instructions requires
  // iterators, hence a for-range loop wouldn't be suitable.
  for (auto inst = basic_block.begin(); inst != basic_block.end(); ++inst) {
    // Skip non-binary (e.g. unary or compare) instruction.
    auto* bin_op = dyn_cast<BinaryOperator>(inst);
    if (bin_op == nullptr) continue;

    // Skip instructions other than integer sub.
    auto opcode = bin_op->getOpcode();
    if (opcode != Instruction::Sub || bin_op->getType()->isIntegerTy() == false)
      continue;

    // A uniform API for creating instructions and inserting them into basic
    // blocks.
    IRBuilder<> builder(bin_op);

    // Create an instruction representing (a + ~b) + 1
    Instruction* new_val = BinaryOperator::CreateAdd(
        builder.CreateAdd(bin_op->getOperand(0),
                          builder.CreateNot(bin_op->getOperand(1))),
        ConstantInt::get(bin_op->getType(), 1));

    dbgs() << *bin_op << " -> " << *new_val << "\n";

    // Replace `(a - b)` (original instructions) with `(a + ~b) + 1` (the new
    // instruction)
    ReplaceInstWithInst(basic_block.getInstList(), inst, new_val);
  }
}
