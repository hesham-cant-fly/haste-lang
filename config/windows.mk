# Windows configuration (MinGW / MSYS2)
# Requires: mingw-w64-x86_64-llvm (provides llvm-config)
# Override LLVM_CONFIG if llvm-config is not in PATH
LLVM_CONFIG ?= llvm-config
CC       := gcc
CXX      := g++
CFLAGS   := -std=c17 -Iinclude/
LDFLAGS  := -lLLVM
DEBUG_FLAGS    := -g -Og -DDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
RELEASE_FLAGS  := -O3
EXE      := haste.exe
