#ifndef LLVM_TUTOR_STATIC_CALL_COUNTER_H_
#define LLVM_TUTOR_STATIC_CALL_COUNTER_H_

#include "llvm/ADT/MapVector.h"  // MapVector
#include "llvm/IR/InstrTypes.h"  //CallBase
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/CommandLine.h"  // SMDiagnostic

using ResultStaticCallCounter =
    llvm::MapVector<const llvm::Function*, unsigned>;
void RunOnModule(llvm::Module& module);

#endif  // LLVM_TUTOR_STATIC_CALL_COUNTER_H_
