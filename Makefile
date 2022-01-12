PREFIX      ?= `pwd`
BIN_PATH     = $(PREFIX)/bin
LLVM_CONFIG ?= llvm-config

CXX             =   clang++
CXXFLAGS        +=  -O0 -g -Wall `$(LLVM_CONFIG) --cxxflags` -fno-rtti -fpic
LDFLAGS         +=  `$(LLVM_CONFIG) --ldflags` `$(LLVM_CONFIG) --libs bitreader bitwriter interpreter core irreader mcjit native option support`

PROGS = parsetest

all: before_build $(PROGS)
	@echo $(GREEN)[+] All done!"\e[0m"

before_build: 
	mkdir -p $(BIN_PATH)

%: %.cc
	$(CXX) $(CXXFLAGS) $< -o $(BIN_PATH)/$@ $(LDFLAGS) 

.NOTPARALLEL: clean

clean:
	rm -rf $(BIN_PATH)