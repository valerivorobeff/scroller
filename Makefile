CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11
INCLUDES = -Ilib/core/src

BUILD ?= debug

# Build modes 
ifeq ($(BUILD), debug)
    CFLAGS += -O0 -g -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

#
#
# Vars
#
#

# Core
CORE_SRCS	= $(wildcard lib/core/src/*.c)
CORE_OBJS	= $(patsubst lib/core/src/%.c, build/$(BUILD)/core/%.o, $(CORE_SRCS))
CORE_LIB	= build/$(BUILD)/libcore.a

# Utils
UTILS		= scr_init
UTIL_SRCS 	= $(foreach util,$(UTILS), src/$(util)/main.c)
UTIL_OBJS 	= $(patsubst src/%/main.c, build/$(BUILD)/%.o, $(UTIL_SRCS))
UTIL_BINS 	= $(patsubst src/%/main.c, build/$(BUILD)/%, $(UTIL_SRCS))

# Tests
TEST_SRCS 	= $(wildcard lib/core/test/test_*.c)
TEST_OBJS 	= $(patsubst lib/core/test/%.c, build/$(BUILD)/test/%.o, $(TEST_SRCS))
TEST_BINS 	= $(TEST_OBJS:.o=)

#
#
# Rules
#
#

all: $(UTIL_BINS)

# Utils
$(UTIL_BINS): build/$(BUILD)/%: build/$(BUILD)/%.o $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(UTIL_OBJS): build/$(BUILD)/%.o: src/%/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Core lib
$(CORE_LIB): $(CORE_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(CORE_OBJS): build/$(BUILD)/core/%.o: lib/core/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Tests
$(TEST_BINS): %: %.o $(CORE_LIB)
	$(CC) $(CFLAGS) $< $(CORE_LIB) -o $@

$(TEST_OBJS): build/$(BUILD)/test/%.o: lib/core/test/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include generated dependencies
DEPS = $(CORE_OBJS:.o=.d) $(UTIL_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
-include $(DEPS)

clean:
	rm -rf build/

test: all $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "Running test '$$t'"; \
		$$t || { echo "x $$? tests failed in test $$t"; exit 1; } \
	done; \
	echo "✅ All tests passed!"

.PHONY: all clean test

