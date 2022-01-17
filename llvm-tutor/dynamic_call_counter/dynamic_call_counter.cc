#include "dynamic_call_counter.h"

int main(int argc, char** argv) {
  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  auto owner = llvm::parseIRFile(argv[1], err, context);
  if (owner == nullptr) {
    llvm::outs() << "ParseIRFile failed\n" << err.getMessage() << "\n";
    return 1;
  }

  RunOnModule(*owner);

  if (llvm::verifyModule(*owner, &llvm::outs())) {
    llvm::outs() << "Generated module is not correct!\n";
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(argv[1], ec, llvm::sys::fs::F_None);
  owner->print(out, nullptr);
  return 0;
}

llvm::Constant* CreateGlobalCounter(llvm::Module& module,
                                    llvm::StringRef global_var_name) {
  using namespace llvm;
  auto& context = module.getContext();

  // This will insert a declaration into module
  Constant* new_global_var = module.getOrInsertGlobal(
      global_var_name, IntegerType::getInt32Ty(context));

  // This will change the declaration into definition (and initialise to 0)
  GlobalVariable* new_global_variable = module.getNamedGlobal(global_var_name);
  new_global_variable->setLinkage(GlobalValue::CommonLinkage);
  new_global_variable->setAlignment(MaybeAlign(4));
  new_global_variable->setInitializer(ConstantInt::get(context, APInt(32, 0)));

  return new_global_var;
}

void RunOnModule(llvm::Module& module) {
  using namespace llvm;
  bool instrumented = false;

  // Function name <--> IR variable that holds the call counter
  StringMap<Constant*> call_counter_map;
  // Function name <--> IR variable that holds the function name
  StringMap<Constant*> func_name_map;

  auto& context = module.getContext();

  // STEP 1: For each function in the module, inject a call-counting code
  // --------------------------------------------------------------------
  for (auto& func : module) {
    if (func.isDeclaration()) continue;

    // Get an IR builder. Sets the insertion point to the top of the function
    IRBuilder<> builder(&*func.getEntryBlock().getFirstInsertionPt());

    // Create a global variable to count the calls to this function
    std::string counter_name = "counter_for_" + std::string(func.getName());
    Constant* var = CreateGlobalCounter(module, counter_name);
    call_counter_map[func.getName()] = var;

    // Create a global variable to hold the name of this function
    auto func_name = builder.CreateGlobalStringPtr(func.getName());
    func_name_map[func.getName()] = func_name;

    // Inject instruction to increment the call count each time this function
    // executes
    LoadInst* new_load =
        builder.CreateLoad(IntegerType::getInt32Ty(context), var);
    Value* new_instruction = builder.CreateAdd(builder.getInt32(1), new_load);
    builder.CreateStore(new_instruction, var);

    dbgs() << " Instrumented: " << func.getName() << "\n";

    instrumented = true;
  }

  // Stop here if there are no function definitions in this module
  if (instrumented == false) return;

  // STEP 2: Inject the declaration of printf
  // ----------------------------------------
  // Create (or _get_ in cases where it's already available) the following
  // declaration in the IR module:
  //    declare i32 @printf(i8*, ...)
  // It corresponds to the following C declaration:
  //    int printf(char *, ...)
  PointerType* printf_arg_type =
      PointerType::getUnqual(Type::getInt8Ty(context));
  FunctionType* printf_type = FunctionType::get(
      IntegerType::getInt32Ty(context), printf_arg_type, /*isVarArg=*/true);

  FunctionCallee printf_callee =
      module.getOrInsertFunction("printf", printf_type);

  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  Function* printf_function = dyn_cast<Function>(printf_callee.getCallee());
  printf_function->setDoesNotThrow();
  printf_function->addParamAttr(0, Attribute::NoCapture);
  printf_function->addParamAttr(0, Attribute::ReadOnly);

  // STEP 3: Inject a global variable that will hold the printf format string
  // ------------------------------------------------------------------------
  Constant* result_format_str =
      ConstantDataArray::getString(context, "%-20s %-10lu\n");

  Constant* result_format_str_var = module.getOrInsertGlobal(
      "result_format_str_ir", result_format_str->getType());
  dyn_cast<GlobalVariable>(result_format_str_var)
      ->setInitializer(result_format_str);

  std::string out = "";
  out += "=================================================\n";
  out += "LLVM-TUTOR: dynamic analysis results\n";
  out += "=================================================\n";
  out += "NAME                 #N DIRECT CALLS\n";
  out += "-------------------------------------------------\n";

  Constant* result_header_str =
      ConstantDataArray::getString(context, out.c_str());

  Constant* result_header_str_var = module.getOrInsertGlobal(
      "result_header_str_ir", result_header_str->getType());
  dyn_cast<GlobalVariable>(result_header_str_var)
      ->setInitializer(result_header_str);

  // STEP 4: Define a printf wrapper that will print the results
  // -----------------------------------------------------------
  // Define `printf_wrapper` that will print the results stored in FuncNameMap
  // and CallCounterMap.  It is equivalent to the following C++ function:
  // ```
  //    void printf_wrapper() {
  //      for (auto &item : Functions)
  //        printf("llvm-tutor): Function %s was called %d times. \n",
  //        item.name, item.count);
  //    }
  // ```
  // (item.name comes from FuncNameMap, item.count comes from
  // CallCounterMap)
  FunctionType* printf_wrapper_type =
      FunctionType::get(Type::getVoidTy(context), {}, /*isVarArg=*/false);
  Function* printf_wrapper_func = dyn_cast<Function>(
      module.getOrInsertFunction("printf_wrapper", printf_wrapper_type)
          .getCallee());

  // Create the entry basic block for printf_wrapper ...
  BasicBlock* ret_block =
      BasicBlock::Create(context, "enter", printf_wrapper_func);
  IRBuilder<> builder(ret_block);

  // ... and start inserting calls to printf
  // (printf requires i8*, so cast the input strings accordingly)
  Value* result_header_str_ptr =
      builder.CreatePointerCast(result_header_str_var, printf_arg_type);
  Value* result_format_str_ptr =
      builder.CreatePointerCast(result_format_str_var, printf_arg_type);

  builder.CreateCall(printf_callee, {result_header_str_ptr});

  LoadInst* load_counter;
  for (auto& item : call_counter_map) {
    load_counter =
        builder.CreateLoad(IntegerType::getInt32Ty(context), item.second);
    // load_counter = builder.CreateLoad(item.second);
    builder.CreateCall(
        printf_callee,
        {result_format_str_ptr, func_name_map[item.first()], load_counter});
  }

  // Finally, insert return instruction
  builder.CreateRetVoid();

  // STEP 5: Call `printf_wrapper` at the very end of this module
  // ------------------------------------------------------------
  appendToGlobalDtors(module, printf_wrapper_func, /*Priority=*/0);
}
