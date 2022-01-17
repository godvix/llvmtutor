#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/CommandLine.h"  // SMDiagnostic

void Visit(llvm::Module& module);

int main(int argc, char** argv) {
  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  auto owner = llvm::parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    llvm::errs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  Visit(*owner);

  if (llvm::verifyModule(*owner, &llvm::errs())) {
    llvm::errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(argv[1], ec, llvm::sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void Visit(llvm::Module& module) {
  using namespace llvm;
  for (auto& function : module) {
    errs() << "(llvm-tutor) Hello from: " << function.getName() << "\n";
    errs() << "(llvm-tutor)   number of arguments: " << function.arg_size()
           << "\n";
  }
}
