PREFIX      ?= $(PWD)
BIN_PATH     = $(PREFIX)/bin
LLVM_CONFIG ?= llvm-config

CXX             =   clang++
CXXFLAGS        +=  -O0 -g -Wall `$(LLVM_CONFIG) --cxxflags` -fno-rtti -fpic
LDFLAGS         +=  `$(LLVM_CONFIG) --ldflags` `$(LLVM_CONFIG) --libs bitreader bitwriter interpreter core irreader mcjit native option support`

PROGS = work1

all: before_build $(PROGS) IR
	$(BIN_PATH)/$(PROGS) main.ll
	clang -O0 main.ll -o "$(BIN_PATH)/main"
	@echo $(GREEN)[+] All done!"\e[0m"

before_build:
	mkdir -p $(BIN_PATH)

%: %.cc
	$(CXX) $(CXXFLAGS) $< -o $(BIN_PATH)/$@ $(LDFLAGS)

.NOTPARALLEL: clean

clean:
	rm -rf $(BIN_PATH)

normal:
	clang ../main.c -o $(BIN_PATH)/main

IR:
	clang -S -emit-llvm ../main.c -o main.ll
