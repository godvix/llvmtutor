#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"  // parseIRFile
#include "llvm/Support/Casting.h"    // cast
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Support/CommandLine.h"  // SMDiagnostic

#include <iostream>
#include <list>
#include <string>

void instrument(llvm::Module& M) {
    using namespace llvm;
    Function& F = *M.getFunction("main");
    for (BasicBlock& BB : F) {  // iterate every basic blocks in the `F` function
        for (Instruction& Ins : BB) {  // iterate every instruction in the `BB` basic blocks
            unsigned opcode = Ins.getOpcode();
            switch (opcode) {
                case Instruction::Add:
                case Instruction::Sub: {
                    Value* operand1 = Ins.getOperand(1);  // get the second operand

                    // notice: `Value` is the basic type in llvm, just like
                    // `Object` in Java or Python. So before using a `Value`,
                    // you're better to downcast it.
                    if (!isa<ConstantInt>(operand1))
                        continue;

                    // If you dont know what the `Value` is. You can print it!
                    // llvm::outs() << "what the value? : " << operand1 << "\n";

                    ConstantInt* constint = cast<ConstantInt>(operand1);
                    int v = constint->getSExtValue();
                    ConstantInt* newConstint = ConstantInt::get(constint->getType(), -v);
                    Ins.setOperand(1, newConstint);  // replace the second operand.
                    break;
                }
                default:
                    break;
            }
        }
    }
}


int main(int argc, char** argv) {
    llvm::LLVMContext Context;
    llvm::SMDiagnostic Err;
    auto Owner = llvm::parseIRFile(argv[1], Err, Context);
    if (!Owner) {
        llvm::errs() << "ParseIRFile failed\n" << Err.getMessage() << "\n";
        return 1;
    }

    instrument(*Owner);

    if (llvm::verifyModule(*Owner, &llvm::errs())) {
        llvm::errs() << "Generated module is not correct!\n";
        return 1;
    }
    std::error_code ec;
    llvm::raw_fd_ostream out(argv[1], ec, llvm::sys::fs::F_None);
    Owner->print(out, nullptr);
    return 0;
}
