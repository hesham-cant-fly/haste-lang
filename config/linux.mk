# Linux configuration
CC       := clang
CXX      := clang++
CFLAGS   := -std=c23 -Iinclude/ $(shell llvm-config --cflags)
LDFLAGS  := $(shell llvm-config --ldflags) -lstdc++ $(shell llvm-config --libs core)
DEBUG_FLAGS    := -g -fsanitize=undefined,address -Og -DDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
RELEASE_FLAGS  := -O3
EXE      := haste
