#include "opcode_counter.h"

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
  for (auto& function : module) {
    StringMap<unsigned> opcode_map;
    for (auto& basic_block : function) {
      for (auto& instruction : basic_block) {
        StringRef name = instruction.getOpcodeName();
        if (opcode_map.find(name) == opcode_map.end()) {
          opcode_map[name] = 1;
        } else {
          ++opcode_map[name];
        }
      }
    }
    errs() << "Printing analysis 'OpcodeCounter Pass' for function '"
           << function.getName() << "':\n";
    PrintOpcodeCounterResult(errs(), opcode_map);
  }
}

void PrintOpcodeCounterResult(llvm::raw_ostream& out_stream,
                              const llvm::StringMap<unsigned>& opcode_map) {
  using namespace llvm;
  out_stream << "================================================="
             << "\n";
  out_stream << "LLVM-TUTOR: OpcodeCounter results\n";
  out_stream << "=================================================\n";
  const char* str1 = "OPCODE";
  const char* str2 = "#TIMES USED";
  out_stream << format("%-20s %-10s\n", str1, str2);
  out_stream << "-------------------------------------------------"
             << "\n";
  for (auto& instruction : opcode_map) {
    out_stream << format("%-20s %-10lu\n", instruction.first().str().c_str(),
                         instruction.second);
  }
  out_stream << "-------------------------------------------------"
             << "\n\n";
}
