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
CORE_OBJS	= $(patsubst lib/core/src/%.c, build/$(BUILD)/obj/core/%.o, $(CORE_SRCS))
CORE_LIB	= build/$(BUILD)/obj/core/libcore.a

# Utils
UTILS		= scr_init scroller
UTIL_SRCS	= $(foreach util,$(UTILS), $(wildcard src/$(util)/*.c))
UTIL_OBJS	= $(patsubst src/%.c, build/$(BUILD)/obj/%.o, $(UTIL_SRCS))
UTIL_BINS	= $(addprefix build/$(BUILD)/bin/, $(UTILS))

# Tests
TEST_SRCS 	= $(wildcard lib/core/test/test_*.c)
TEST_OBJS 	= $(patsubst lib/core/test/%.c, build/$(BUILD)/obj/test/%.o, $(TEST_SRCS))
TEST_BINS 	= $(patsubst lib/core/test/%.c, build/$(BUILD)/bin/test_%, $(TEST_SRCS))

# Object files for each utilty
UTIL_OBJS_scr_init  = $(filter build/$(BUILD)/obj/scr_init/%, $(UTIL_OBJS))
UTIL_OBJS_scroller  = $(filter build/$(BUILD)/obj/scroller/%, $(UTIL_OBJS))
# Append your new utility's object files to the list above

#
#
# Rules
#
#

all: $(UTIL_BINS)

#
# Util rules
#

# scr_init linkage
build/$(BUILD)/bin/scr_init: $(UTIL_OBJS_scr_init) $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

# scroller linkage
build/$(BUILD)/bin/scroller: $(UTIL_OBJS_scroller) $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

# Append your new utility's linkage rules the list above

$(UTIL_OBJS): build/$(BUILD)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

#
# Core lib rules
#

$(CORE_LIB): $(CORE_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(CORE_OBJS): build/$(BUILD)/obj/core/%.o: lib/core/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

#
# Test rules
#

$(TEST_BINS): build/$(BUILD)/bin/test_%: build/$(BUILD)/obj/test/%.o $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(CORE_LIB) -o $@

$(TEST_OBJS): build/$(BUILD)/obj/test/%.o: lib/core/test/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

#
# Include generated dependencies
#

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

