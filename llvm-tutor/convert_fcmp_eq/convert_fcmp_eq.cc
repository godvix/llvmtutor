#include "convert_fcmp_eq.h"

using namespace llvm;

// Unnamed namespace for private function
namespace {

FCmpInst* ConvertFCmpEqInstruction(FCmpInst* fcmp) noexcept;

FCmpInst* ConvertFCmpEqInstruction(FCmpInst* fcmp) noexcept {
  assert(fcmp != nullptr && "The given fcmp instruction is null");

  if (fcmp->isEquality() == false) {
    // We're only interested in equality-based comparions, so return null if
    // this comparion isn't equality-based.
    return nullptr;
  }

  auto* lhs = fcmp->getOperand(0);
  auto* rhs = fcmp->getOperand(1);
  // Determine the new floating-point comparion predicate based on the current
  // one.
  CmpInst::Predicate cmp_pred = [fcmp]() -> auto {
    switch (fcmp->getPredicate()) {
      case CmpInst::Predicate::FCMP_OEQ: {
        return CmpInst::Predicate::FCMP_OLT;
      }
      case CmpInst::Predicate::FCMP_UEQ: {
        return CmpInst::Predicate::FCMP_ULT;
      }
      case CmpInst::Predicate::FCMP_ONE: {
        return CmpInst::Predicate::FCMP_OGE;
      }
      case CmpInst::Predicate::FCMP_UNE: {
        return CmpInst::Predicate::FCMP_UGE;
      }
      default: {
        llvm_unreachable("Unsupported fcmp predicate");
      }
    }
  }
  ();

  // Create the objects and values needed to perform the equality comparions
  // conversion.
  auto* module = fcmp->getModule();
  assert(module != nullptr &&
         "The given fcmp instruction does not belong to a module");
  auto& ctx = module->getContext();
  auto* i64_ty = IntegerType::get(ctx, 64);
  auto* double_ty = Type::getDoubleTy(ctx);

  // Define the sign-mask and double-precision machine epsilon constants.
  ConstantInt* sign_mask = ConstantInt::get(i64_ty, ~(1L << 63));
  // The machine epsilon value for IEEE 754 double-precision values is 2 ^ -52
  // or (b / 2) * b ^ -(p - 1) where b (base) = 2 and p (precision) = 53.
  auto epsilon_bits = APInt(64, 0x3cb0000000000000);
  auto* epsilon_value = ConstantFP::get(double_ty, epsilon_bits.bitsToDouble());

  // Create an IRBuilder with an insertion point set to the given fcmp
  // instruction.
  auto builder = IRBuilder<>(fcmp);
  // Create the subtraction, casting, absolute value, and new comparison
  // instructions one at a time.
  // %0 = fsub double %a, %b
  auto* fsub_inst = builder.CreateFSub(lhs, rhs);
  // %1 = bitcast double %0 to i64
  auto* cast_to_i64 = builder.CreateBitCast(fsub_inst, i64_ty);
  // %2 = and i64 %1, 0x7fffffffffffffff
  auto* abs_value = builder.CreateAnd(cast_to_i64, sign_mask);
  // %3 = bitcast i64 %2 to double
  auto* cast_to_double = builder.CreateBitCast(abs_value, double_ty);
  // %4 = fcmp <olt/ult/oge/uge> double %3, 0x3cb0000000000000
  // Rather than creating a new instruction, we'll just change the predicate and
  // operands of the existing fcmp instruction to match what we want.
  fcmp->setPredicate(cmp_pred);
  fcmp->setOperand(0, cast_to_double);
  fcmp->setOperand(1, epsilon_value);
  return fcmp;
}

}  // namespace

int main(int argc, char** argv) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  convert_fcmp_eq::RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

find_fcmp_eq::Result find_fcmp_eq::RunOnFunction(Function& func) {
  Result comparisons;
  for (auto& inst : instructions(func)) {
    // We're only looking for 'fcmp' instructions here.
    auto* fcmp = dyn_cast<FCmpInst>(&inst);
    if (fcmp != nullptr) {
      // We've found an 'fcmp' instruction; we need to make sure it's an
      // equality comparions.
      if (fcmp->isEquality()) comparisons.push_back(fcmp);
    }
  }

  return comparisons;
}

void convert_fcmp_eq::RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void convert_fcmp_eq::RunOnFunction(Function& func) {
  auto comparisons = find_fcmp_eq::RunOnFunction(func);
  // Functions marked explicitly 'optnone' should be ignored since we shouldn't
  // be changing anything in them anyway.
  if (func.hasFnAttribute(Attribute::OptimizeNone)) {
    dbgs() << "Ignoring optnone-marked function \"" << func.getName() << "\"\n";
  } else {
    for (auto* fcmp : comparisons) ConvertFCmpEqInstruction(fcmp);
  }
}
