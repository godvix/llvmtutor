#ifndef LLVM_TUTOR_INJECT_FUNC_CALL_H_
#define LLVM_TUTOR_INJECT_FUNC_CALL_H_

#include "llvm/IR/Constant.h"   // ConstantDataArray
#include "llvm/IR/IRBuilder.h"  // IRBuilder
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/CommandLine.h"  // SMDiagnostic
#include "llvm/Support/Debug.h"        // LLVM_DEBUG

void RunOnModule(llvm::Module& module);

#endif  // LLVM_TUTOR_INJECT_FUNC_CALL_H_
