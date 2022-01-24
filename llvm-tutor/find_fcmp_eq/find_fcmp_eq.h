#ifndef LLVM_TUTOR_FIND_FCMP_EQ_H_
#define LLVM_TUTOR_FIND_FCMP_EQ_H_

#include <random>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"  // ConstantDataArray
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"     // IRBuilder
#include "llvm/IR/InstIterator.h"  // instructions()
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"  // parseIRFile
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"               // SMDiagnostic
#include "llvm/Support/Debug.h"                     // LLVM_DEBUG
#include "llvm/Transforms/Utils/BasicBlockUtils.h"  // ReplaceInstWithInst
#include "llvm/Transforms/Utils/ValueMapper.h"      // ValueToValueMapTy

namespace find_fcmp_eq {

using Result = std::vector<llvm::FCmpInst*>;

void RunOnModule(llvm::Module& module);
void RunOnFunction(llvm::Function& func);

}  // namespace find_fcmp_eq

#endif  // LLVM_TUTOR_FIND_FCMP_EQ_H_
