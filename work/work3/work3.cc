#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
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
InstPtrVector FindFAddsToConvert(BasicBlock& bb);
void ConvertFAdd(Instruction* fadd);

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
  auto* double_ty = Type::getDoubleTy(module.getContext());
  auto* hook_ty =
      FunctionType::get(double_ty, {double_ty, double_ty}, /*isVarArgs=*/false);
  module.getOrInsertFunction("Hook", hook_ty);
  for (auto& fn : module) {
    if (fn.isDeclaration()) continue;
    RunOnFunction(fn);
  }
}

void RunOnFunction(Function& fn) {
  auto fadds = InstPtrVector();
  for (auto& bb : fn) {
    auto new_fadds = FindFAddsToConvert(bb);
    fadds.insert(fadds.end(), new_fadds.begin(), new_fadds.end());
  }
  for (auto* fadd : fadds) ConvertFAdd(fadd);
}

InstPtrVector FindFAddsToConvert(BasicBlock& bb) {
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
  auto* hook_call =
      builder.CreateCall(fadd->getModule()->getFunction("Hook"),
                         {fadd->getOperand(0), fadd->getOperand(1)});
  ReplaceInstWithInst(fadd, hook_call);
}
