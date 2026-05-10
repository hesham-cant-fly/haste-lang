LDFLAGS:=-lstdc++ $(LLVM_LDFLAGS)
CFLAGS:=-std=c11 -Iinclude/ $(LLVM_CFLAGS)
CC:=clang
CXX:=clang++

DEBUG_FLAGS:=-g -fsanitize=undefined,address -Og -DDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
RELEASE_FLAGS:=-O2

EXE:=haste

BUILD_DIR:=.build/
SRC_DIR:=source/
SRCS:=$(wildcard $(SRC_DIR)*.c)
OBJS:=$(addprefix $(BUILD_DIR),$(notdir $(SRCS:.c=.o)))
INCLUDES:=$(wildcard include/*.h)

.PHONY: all gen_compile_flags run clean debug release test test-clean

all: debug

debug: CFLAGS += $(DEBUG_FLAGS)
debug: CFLAGS += -MMD -MP
debug: gen_compile_flags $(EXE)

gen_compile_flags:
	@echo -xc $(CFLAGS) $(LDFLAGS) | tr ' ' '\n' > compile_flags.txt

release: CFLAGS += $(RELEASE_FLAGS)
release: $(EXE)

$(EXE): $(OBJS)
	@echo $(CC) -o $@ $^ "(link flags...)"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): $(SRC_DIR)haste.h
$(BUILD_DIR)%.o: $(SRC_DIR)%.c | $(BUILD_DIR)
	@echo $(CC) -o $@ $<
	@$(CC) $(CFLAGS) -c -o $@ $<

-include $(OBJS:.o=.d)

$(SRC_DIR)haste.h: $(INCLUDES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(all)
	./$(EXE)

test: $(EXE)
	@cd test && python3 ./run_tests.py

test-clean:
	rm -f test/*.got test/lexing/*.got test/integration/*.got

clean: test-clean
	rm -rdf $(OBJS) $(EXE) $(BUILD_DIR) TAGS
