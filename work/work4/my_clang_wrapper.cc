#include <unistd.h>  // access

#include <cstdio>   // perror
#include <cstdlib>  // getenv, rand, system
#include <cstring>  // strlen
#include <string>
#include <vector>

#include "llvm/IR/Constants.h"         // ConstantInt
#include "llvm/IR/DerivedTypes.h"      // IntegerType, PointerType
#include "llvm/IR/GlobalValue.h"       // GlobalValue
#include "llvm/IR/GlobalVariable.h"    // GlobalVariable
#include "llvm/IR/IRBuilder.h"         // IRBuilder
#include "llvm/IR/LLVMContext.h"       // LLVMContext
#include "llvm/IR/Module.h"            // Module
#include "llvm/IR/Verifier.h"          // verifyModule
#include "llvm/IRReader/IRReader.h"    // parseIRFile
#include "llvm/Support/Casting.h"      // cast
#include "llvm/Support/SourceMgr.h"    // SMDiagnostic
#include "llvm/Support/raw_ostream.h"  // raw_fd_ostream

static constexpr int kLogMapSize = 16;
static constexpr int kMapSize = (1 << kLogMapSize);

using namespace llvm;

int Execute(const int argc, const char** argv);
bool IsSourceFile(const char* filename);
bool GenerateIr(const char* filename, const char* output);
bool IsIrFile(const char* filename);
bool Instrument(const char* filename, const char* output);
void RunOnModule(Module& module);

int main(int argc, char** argv) {
  bool* to_remove = new bool[argc + 1];
  memset(to_remove, 0, (argc + 1) * sizeof(bool));
  char** new_argv = new char*[argc + 1];
  memset(new_argv, 0, (argc + 1) * sizeof(char*));
  for (int i = 0; i < argc; ++i) new_argv[i] = argv[i];
  for (int i = 1; i < argc; ++i) {
    if (IsSourceFile(new_argv[i]) == false) continue;
    int len = strlen(new_argv[i]);
    char* ll_filename = new char[len + 2];
    strcpy(ll_filename, new_argv[i]);
    strcpy(ll_filename + len - 1, "ll");
    if (GenerateIr(new_argv[i], ll_filename) == false) {
      delete[] ll_filename;
      continue;
    }
    to_remove[i] = true;
    new_argv[i] = ll_filename;
  }
  for (int i = 1; i < argc; ++i) {
    if (IsIrFile(new_argv[i]) == false) continue;
    Instrument(new_argv[i], new_argv[i]);
  }

  char* mcw_lib_path = getenv("MCW_LIB");
  if (mcw_lib_path == nullptr)
    mcw_lib_path = "/mnt/d/projects/llvmtutor/work/work4/runtime_lib.o";

  new_argv[0] = "clang";
  new_argv[argc++] = mcw_lib_path;
  Execute(argc, (const char**)new_argv);

  char* rm[2];
  rm[0] = "rm";
  for (int i = 1; i < argc; ++i) {
    if (to_remove[i]) {
      rm[1] = new_argv[i];
      Execute(2, (const char**)rm);
    }
  }

  delete[] new_argv;
  return 0;
}

int Execute(const int argc, const char** argv) {
  int tot_len = 0;
  for (int i = 0; i < argc; ++i) tot_len += strlen(argv[i]);
  tot_len += argc + 4;
  char* cmd = new char[tot_len];
  memset(cmd, '\0', tot_len * sizeof(char));
  for (int i = 0; i < argc; ++i) {
    strcat(cmd, argv[i]);
    strcat(cmd, " ");
  }
  puts(cmd);
  int ret = system(cmd);
  delete[] cmd;
  return ret;
}

bool IsSourceFile(const char* filename) {
  int len = strlen(filename);
  return (len >= 2) && (filename[len - 2] == '.') &&
         (filename[len - 1] == 'c') && (access(filename, R_OK) == 0);
}

bool GenerateIr(const char* filename, const char* output) {
  const char* argv[8] = {nullptr};
  argv[0] = "clang";
  argv[1] = "-S";
  argv[2] = "-emit-llvm";
  argv[3] = filename;
  argv[4] = "-o";
  argv[5] = output;
  return Execute(6, argv) == 0;
}

bool IsIrFile(const char* filename) {
  int len = strlen(filename);
  return (len >= 3) && (filename[len - 3] == '.') &&
         (filename[len - 2] == 'l') && (filename[len - 1] == 'l') &&
         (access(filename, R_OK) == 0);
}

bool Instrument(const char* filename, const char* output) {
  LLVMContext context;
  SMDiagnostic err;
  auto owner = parseIRFile(filename, err, context);
  if (owner == nullptr) {
    outs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return false;
  }
  RunOnModule(*owner);
  if (verifyModule(*owner, &outs())) {
    outs() << "Generated module is not correct!\n";
    return false;
  }
  std::error_code ec;
  raw_fd_ostream out(output, ec, sys::fs::F_None);
  owner->print(out, nullptr);
  return true;
}

void RunOnModule(Module& module) {
  int inst_blocks = 0;

  auto* int8_ty = IntegerType::getInt8Ty(module.getContext());
  auto* int32_ty = IntegerType::getInt32Ty(module.getContext());
  auto* int8_ptr_ty = PointerType::getInt8PtrTy(module.getContext());

  auto* mcw_map_ptr = new GlobalVariable(
      /*M=*/module, /*Ty=*/int8_ptr_ty,
      /*isConstant=*/false, /*Linkage=*/GlobalValue::ExternalLinkage,
      /*Initializer=*/nullptr, /*Name=*/"__mcw_area_ptr");
  auto* mcw_prev_loc = new GlobalVariable(
      /*M=*/module, /*Ty=*/int32_ty, /*isConstant=*/false,
      /*Linkage=*/GlobalValue::ExternalLinkage,
      /*Initializer=*/nullptr,
      /*Name=*/"__mcw_prev_loc",
      /*InsertBefore=*/nullptr,
      /*ThreadLocalMode=*/GlobalVariable::GeneralDynamicTLSModel);

  for (auto& fn : module) {
    if (fn.isDeclaration()) continue;
    for (auto& bb : fn) {
      auto insertion_pt = bb.getFirstInsertionPt();
      auto builder = IRBuilder<>(&*insertion_pt);

      // Make up `cur_loc`
      int cur_loc_real = rand() % kMapSize;
      auto* cur_loc = ConstantInt::get(int32_ty, cur_loc_real);

      // Load `prev_loc`
      auto* prev_loc = builder.CreateLoad(mcw_prev_loc);

      // Load SHM pointer
      auto* map_ptr = builder.CreateLoad(mcw_map_ptr);
      auto* map_ptr_idx =
          builder.CreateGEP(map_ptr, builder.CreateXor(prev_loc, cur_loc));

      // update bitmap
      auto* counter = builder.CreateLoad(map_ptr_idx);
      auto* increase = builder.CreateAdd(counter, ConstantInt::get(int8_ty, 1));
      builder.CreateStore(increase, map_ptr_idx);

      // Set `prev_loc` to `cur_loc >> 1`
      builder.CreateStore(ConstantInt::get(int32_ty, cur_loc_real >> 1),
                          mcw_prev_loc);

      ++inst_blocks;
    }
  }

  outs() << "Instrumented " << inst_blocks << " locations.\n";
}
