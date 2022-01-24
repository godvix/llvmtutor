#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/Casting.h"      // cast
#include "llvm/Support/CommandLine.h"  // SMDiagnostic
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;
using InstPtrVector = std::vector<Instruction*>;

void RunOnModule(Module& module);
void RunOnFunction(Function& fn);
InstPtrVector FindInstsToConvert(BasicBlock& bb);
void ConvertFAdd(Instruction* fadd);

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

void RunOnModule(Module& module) {
  for (auto& fn : module) RunOnFunction(fn);
}

void RunOnFunction(Function& fn) {
  auto fadds = InstPtrVector();
  for (auto& bb : fn) {
    auto new_fadds = FindInstsToConvert(bb);
    fadds.insert(fadds.end(), new_fadds.begin(), new_fadds.end());
  }
  for (auto* fadd : fadds) ConvertFAdd(fadd);
}

InstPtrVector FindInstsToConvert(BasicBlock& bb) {
  auto res = InstPtrVector();
  for (auto& inst : bb) {
    switch (inst.getOpcode()) {
      case Instruction::FAdd: {
        res.push_back(&inst);
        break;
      }
      default: {
        break;
      }
    }
  }
  return res;
}

void ConvertFAdd(Instruction* fadd) {
  auto builder = IRBuilder<>(fadd);
  auto* fadd_clone = fadd->clone();
  auto* val_100 = ConstantFP::get(fadd->getType(), 100.);
  auto* condition =
      dyn_cast<FCmpInst>(builder.CreateFCmpOGT(fadd_clone, val_100));
  Instruction* then_term = nullptr;
  Instruction* else_term = nullptr;
  SplitBlockAndInsertIfThenElse(condition, fadd, &then_term, &else_term);
  fadd_clone->insertBefore(condition);
  auto* phi = PHINode::Create(fadd->getType(), 2);
  phi->addIncoming(val_100, then_term->getParent());
  phi->addIncoming(fadd_clone, else_term->getParent());
  ReplaceInstWithInst(fadd, phi);
}
