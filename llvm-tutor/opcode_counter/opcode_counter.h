#ifndef LLVM_TUTOR_OPCODE_COUNTER_H_
#define LLVM_TUTOR_OPCODE_COUNTER_H_

#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/CommandLine.h"  // SMDiagnostic

void RunOnModule(llvm::Module& module);
void PrintOpcodeCounterResult(llvm::raw_ostream& out_stream,
                              const llvm::StringMap<unsigned>& opcode_map);

#endif  // LLVM_TUTOR_OPCODE_COUNTER_H_
