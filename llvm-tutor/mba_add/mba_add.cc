#include "mba_add.h"
using namespace llvm;

static constexpr double kRatio = 1.;

int main(int argc, char** argv) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void RunOnFunction(Function& func) {
  for (auto& basic_block : func) RunOnBasicBlock(basic_block);
}

void RunOnBasicBlock(BasicBlock& basic_block) {
  // Get a (rather naive) random number generator that will be used to decide
  // whether to replace the current instruction or not.
  std::mt19937_64 rng;
  rng.seed(1234);
  std::uniform_real_distribution<double> dist(0., 1.);

  // Loop over all instructions in the block. Replacing instructions requires
  // iterators, hence a for-range loop wouldn't be suitable
  for (auto inst = basic_block.begin(); inst != basic_block.end(); ++inst) {
    // Skip non-binary (e.g. unary or compare) instructions
    auto* bin_op = dyn_cast<BinaryOperator>(inst);
    if (bin_op == nullptr) continue;

    // Skip instructions other than add
    if (bin_op->getOpcode() != Instruction::Add) continue;

    // Skip if the result is not 8-bit wide (this implies that the operands are
    // also 8-bit wide)
    if (bin_op->getType()->isIntegerTy() == false ||
        bin_op->getType()->getIntegerBitWidth() != 8)
      continue;

    // Use `kRatio` and `rng` to decide whether to substitude this particular
    // 'add'
    if (dist(rng) > kRatio) continue;

    // A uniform API for creating instructions and inserting them into basic
    // blocks
    IRBuilder<> builder(bin_op);

    // Constants used in building the instruction for substitution
    auto val_2 = ConstantInt::get(bin_op->getType(), 2);
    auto val_39 = ConstantInt::get(bin_op->getType(), 39);
    auto val_23 = ConstantInt::get(bin_op->getType(), 23);
    auto val_151 = ConstantInt::get(bin_op->getType(), 151);
    auto val_111 = ConstantInt::get(bin_op->getType(), 111);

    // Build an instruction representing `(((a ^ b) + 2 * (a & b)) * 39 + 23) *
    // 151 + 111`
    auto* new_inst =
        // E = e5 + 111
        BinaryOperator::CreateAdd(
            // e5 = e4 * 151
            builder.CreateMul(
                // e4 = e2 + 23
                builder.CreateAdd(
                    // e3 = e2 * 39
                    builder.CreateMul(
                        // e2 = e0 + e1
                        builder.CreateAdd(
                            // e0 = a ^ b
                            builder.CreateXor(bin_op->getOperand(0),
                                              bin_op->getOperand(1)),
                            // e1 = 2 * (a & b)
                            builder.CreateMul(
                                val_2,
                                builder.CreateAnd(bin_op->getOperand(0),
                                                  bin_op->getOperand(1)))),
                        val_39),  // e3 = e2 * 39
                    val_23),      // e4 = e2 + 23
                val_151),         // e5 = e4 * 151
            val_111);             // E = e5 + 111

    dbgs() << *bin_op << " -> " << *new_inst << "\n";

    // Replace `(a + b)` (original instructions) with `(((a ^ b) + 2 * (a & b))
    // * 39 + 23) * 151 + 111` (the new instruction)
    ReplaceInstWithInst(basic_block.getInstList(), inst, new_inst);
  }
}
