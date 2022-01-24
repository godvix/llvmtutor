#include "find_fcmp_eq.h"

using namespace llvm;

namespace {

static void PrintFCmpEqInstructions(
    raw_ostream& ostream, Function& func,
    const find_fcmp_eq::Result& fcmp_eq_insts) noexcept;

static void PrintFCmpEqInstructions(
    raw_ostream& ostream, Function& func,
    const find_fcmp_eq::Result& fcmp_eq_insts) noexcept {
  if (fcmp_eq_insts.empty()) return;

  ostream << "Floating-point equality comparions in \"" << func.getName()
          << "\":\n";

  // Using a ModuleSlotTracker for printing makes it so full function analysis
  // for slot numbering only occurs once instead of every time an instruction is
  // printed.
  ModuleSlotTracker tracker(func.getParent());

  for (auto* fcmp_eq : fcmp_eq_insts) {
    fcmp_eq->print(ostream, tracker);
    ostream << "\n";
  }
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

  find_fcmp_eq::RunOnModule(*owner);

  if (verifyModule(*owner, &errs())) {
    errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  raw_fd_ostream out(argv[1], ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void find_fcmp_eq::RunOnModule(Module& module) {
  for (auto& func : module) RunOnFunction(func);
}

void find_fcmp_eq::RunOnFunction(Function& func) {
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

  PrintFCmpEqInstructions(errs(), func, comparisons);
}
