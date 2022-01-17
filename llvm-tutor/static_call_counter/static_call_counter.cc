#include "static_call_counter.h"

static void PrintStaticCallCounterResult(
    llvm::raw_ostream& out_stream, const ResultStaticCallCounter& direct_calls);

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

void RunOnModule(llvm::Module& module) {
  using namespace llvm;
  ResultStaticCallCounter res;
  for (auto& func : module) {
    for (auto& basic_block : func) {
      for (auto& instruction : basic_block) {
        // If this is a call instruction then call_base_ptr will be not null.
        auto* call_base_ptr = dyn_cast<CallBase>(&instruction);
        if (call_base_ptr == nullptr) continue;

        // if call_base_ptr is a direct function call then direct_invoc_ptr will
        // be not null.
        auto direct_invoc_ptr = call_base_ptr->getCalledFunction();
        if (direct_invoc_ptr == nullptr) continue;

        // We have a direct function call - update the count for the function
        // being called.
        auto call_count = res.find(direct_invoc_ptr);
        if (call_count == res.end())
          call_count = res.insert(std::make_pair(direct_invoc_ptr, 0)).first;
        ++call_count->second;
      }
    }
  }
  PrintStaticCallCounterResult(errs(), res);
}

static void PrintStaticCallCounterResult(
    llvm::raw_ostream& out_stream,
    const ResultStaticCallCounter& direct_calls) {
  using namespace llvm;
  out_stream << "================================================="
             << "\n";
  out_stream << "LLVM-TUTOR: static analysis results\n";
  out_stream << "=================================================\n";
  const char* str1 = "NAME";
  const char* str2 = "#N DIRECT CALLS";
  out_stream << format("%-20s %-10s\n", str1, str2);
  out_stream << "-------------------------------------------------"
             << "\n";

  for (auto& call_count : direct_calls) {
    out_stream << format("%-20s %-10lu\n",
                         call_count.first->getName().str().c_str(),
                         call_count.second);
  }

  out_stream << "-------------------------------------------------"
             << "\n\n";
}
