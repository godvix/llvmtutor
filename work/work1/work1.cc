#include <iostream>
#include <list>
#include <string>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/Casting.h"      // cast
#include "llvm/Support/CommandLine.h"  // SMDiagnostic
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

void Instrument(llvm::Module& module);

int main(int argc, char** argv) {
  llvm::LLVMContext Context;
  llvm::SMDiagnostic Err;
  auto owner = llvm::parseIRFile(argv[1], Err, Context);
  if (owner == nullptr) {
    llvm::errs() << "ParseIRFile failed\n" << Err.getMessage() << "\n";
    return 1;
  }

  Instrument(*owner);

  if (llvm::verifyModule(*owner, &llvm::errs())) {
    llvm::errs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(argv[1], ec, llvm::sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

void Instrument(llvm::Module& module) {
  using namespace llvm;
  Function* function = module.getFunction("main");
  for (auto& basic_block : *function) {
    for (auto instruction_iterator = basic_block.begin();
         instruction_iterator != basic_block.end(); ++instruction_iterator) {
      switch (instruction_iterator->getOpcode()) {
        case Instruction::FAdd: {
          auto* new_instruction =
              BinaryOperator::CreateFSub(instruction_iterator->getOperand(0),
                                         instruction_iterator->getOperand(1));
          ReplaceInstWithInst(basic_block.getInstList(), instruction_iterator,
                              new_instruction);
          break;
        }
        default: {
          break;
        }
      }
    }
  }
}
