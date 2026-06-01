# FreeBSD configuration — auto-detect compiler
CC  := $(or $(shell command -v clang 2>/dev/null),$(shell command -v gcc 2>/dev/null),$(shell command -v cc 2>/dev/null),clang)
CXX := $(or $(shell command -v clang++ 2>/dev/null),$(shell command -v g++ 2>/dev/null),$(shell command -v c++ 2>/dev/null),clang++)

STD := $(shell echo 'int main(){}' | $(CC) -std=c23 -x c - -o /dev/null 2>/dev/null && echo c23 || echo c17)
LLVM_CONFIG ?= $(shell command -v llvm-config 2>/dev/null)
ifneq ($(LLVM_CONFIG),)
  CFLAGS   := -std=$(STD) -Iinclude/ $(shell $(LLVM_CONFIG) --cflags)
  LDFLAGS  := $(shell $(LLVM_CONFIG) --ldflags) -lstdc++ $(shell $(LLVM_CONFIG) --libs core)
else
  CFLAGS   := -std=$(STD) -Iinclude/
  LDFLAGS  := -lLLVM
endif

DEBUG_FLAGS    := -g -fsanitize=undefined,address -Og -DDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
RELEASE_FLAGS  := -O3
EXE      := haste
