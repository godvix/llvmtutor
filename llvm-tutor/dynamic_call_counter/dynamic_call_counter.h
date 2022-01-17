#ifndef LLVM_TUTOR_STATIC_CALL_COUNTER_H_
#define LLVM_TUTOR_STATIC_CALL_COUNTER_H_

#include "llvm/ADT/MapVector.h"    // MapVector
#include "llvm/IR/IRBuilder.h"     // IRBuilder
#include "llvm/IR/InstrTypes.h"    // CallBase
#include "llvm/IR/Instructions.h"  // LoadInst
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"             // parseIRFile
#include "llvm/Support/CommandLine.h"           // SMDiagnostic
#include "llvm/Transforms/Utils/ModuleUtils.h"  // appendToGlobalDtors

using ResultStaticCallCounter =
    llvm::MapVector<const llvm::Function*, unsigned>;
void RunOnModule(llvm::Module& module);
llvm::Constant* CreateGlobalCounter(llvm::Module& module,
                                    llvm::StringRef global_var_name);

#endif  // LLVM_TUTOR_STATIC_CALL_COUNTER_H_
