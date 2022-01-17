#include "inject_func_call.h"

int main(int argc, char **argv) {
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

void RunOnModule(llvm::Module &module) {
  using namespace llvm;
  auto &context = module.getContext();
  PointerType *printf_arg_type_ptr =
      PointerType::getUnqual(Type::getInt8Ty(context));

  // STEP 1: Inject the declaration of printf
  // ----------------------------------------
  // Create (or _get_ in cases where it's already available) the
  // following declaration in the IR module:
  //    declare i32 @printf(i8*, ...)
  // It corresponds to the following C declaration:
  //    int printf(char *, ...)
  FunctionType *printf_type_ptr =
      FunctionType::get(IntegerType::getInt32Ty(context), printf_arg_type_ptr,
                        /*isVarArgs=*/true);

  FunctionCallee printf_callee =
      module.getOrInsertFunction("printf", printf_type_ptr);

  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  Function *printf_function_ptr = dyn_cast<Function>(printf_callee.getCallee());
  printf_function_ptr->setDoesNotThrow();
  printf_function_ptr->addParamAttr(0, Attribute::NoCapture);
  printf_function_ptr->addParamAttr(0, Attribute::ReadOnly);

  // STEP 2: Inject a global variable that will hold the printf format string
  // ------------------------------------------------------------------------
  llvm::Constant *printf_format_str = ConstantDataArray::getString(
      context,
      "(llvm-tutor) Hello from: %s\n(llvm-tutor)   number of arguments: %d\n");

  Constant *printf_format_str_var = module.getOrInsertGlobal(
      "printf_format_str", printf_format_str->getType());
  dyn_cast<GlobalVariable>(printf_format_str_var)
      ->setInitializer(printf_format_str);

  // STEP 3: For each function in the module, inject a call to printf
  // ----------------------------------------------------------------
  for (auto &function : module) {
    if (function.isDeclaration()) continue;

    // Get an IR builder. Sets the insertion point to the top of the function
    IRBuilder<> builder(&*function.getEntryBlock().getFirstInsertionPt());

    // Inject a global variable that contains the function name
    auto FuncName = builder.CreateGlobalStringPtr(function.getName());

    // printf_callee requires i8*, but printf_format_str_var is an array: [n x
    // i8]. Add a cast: [n x i8] -> i8*
    llvm::Value *format_str_ptr = builder.CreatePointerCast(
        printf_format_str_var, printf_arg_type_ptr, "formatStr");

    // The following is visible only if you pass -debug on the command line
    // *and* you have an assert build.
    dbgs() << " Injecting call to printf inside " << function.getName() << "\n";

    // Finally, inject a call to printf
    builder.CreateCall(printf_callee, {format_str_ptr, FuncName,
                                       builder.getInt32(function.arg_size())});
  }
}
