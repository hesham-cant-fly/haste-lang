# OS detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
  include config/linux.mk
else ifeq ($(UNAME_S),FreeBSD)
  include config/freebsd.mk
else ifneq (,$(filter MSYS_NT% MINGW64_NT% MINGW32_NT% CYGWIN_NT%,$(UNAME_S)))
  include config/windows.mk
else
  # Fallback: try linux.mk (covers Darwin, etc.)
  include config/linux.mk
endif

BUILD_DIR := .build/
SRC_DIR   := source/
SRCS      := $(wildcard $(SRC_DIR)*.c)
OBJS      := $(addprefix $(BUILD_DIR),$(notdir $(SRCS:.c=.o)))
INCLUDES  := $(wildcard include/*.h)

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
	@echo "$(CC) -o $@ $^ (link flags...)"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): $(SRC_DIR)haste.h
$(BUILD_DIR)%.o: $(SRC_DIR)%.c | $(BUILD_DIR)
	@echo "$(CC) -o $@ $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

-include $(OBJS:.o=.d)

$(SRC_DIR)haste.h: $(INCLUDES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(all)
	./$(EXE)

test: $(EXE)
	@cd test && python3 ./run_tests.py

test-brief: $(EXE)
	@cd test && python3 ./run_tests.py --brief

clean-test:
	rm -f test/**/*.got test/**/*.tokens test/**/*.ll test/**/*.json

clean: clean-test
	rm -rdf $(OBJS) $(EXE) $(BUILD_DIR) TAGS
