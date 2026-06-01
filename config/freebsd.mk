# FreeBSD configuration
# LLVM is typically in /usr/local — set LLVM_CONFIG if needed
LLVM_CONFIG ?= llvm-config
CC       := clang
CXX      := clang++
CFLAGS   := -std=c23 -Iinclude/ $(shell $(LLVM_CONFIG) --cflags)
LDFLAGS  := $(shell $(LLVM_CONFIG) --ldflags) -lstdc++ $(shell $(LLVM_CONFIG) --libs core)
DEBUG_FLAGS    := -g -fsanitize=undefined,address -Og -DDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
RELEASE_FLAGS  := -O3
EXE      := haste
