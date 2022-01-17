PREFIX      ?= $(PWD)
BIN_PATH     = $(PREFIX)/bin
LLVM_CONFIG ?= llvm-config

CXX             =   clang++
CXXFLAGS        +=  -O0 -g -Wall `$(LLVM_CONFIG) --cxxflags` -fno-rtti -fpic
LDFLAGS         +=  `$(LLVM_CONFIG) --ldflags` `$(LLVM_CONFIG) --libs bitreader bitwriter interpreter core irreader mcjit native option support`

PROGS = mba_add
TARGET = input_for_mba

all: before_build $(PROGS) IR
	$(BIN_PATH)/$(PROGS) $(TARGET).ll
	clang $(TARGET).ll -o $(BIN_PATH)/$(TARGET)

before_build:
	mkdir -p $(BIN_PATH)

%: %.cc
	$(CXX) $(CXXFLAGS) $< -o $(BIN_PATH)/$@ $(LDFLAGS)

.NOTPARALLEL: clean

clean:
	rm -rf $(BIN_PATH)

normal: before_build
	clang $(TARGET).c -o $(BIN_PATH)/$(TARGET)

IR:
	clang -S -emit-llvm $(TARGET).c -o $(TARGET).ll
