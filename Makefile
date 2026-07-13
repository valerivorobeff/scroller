###############################################################################
#                                                                             #
#                            scroller makefile                                #
#                                                                             #
#   * If you want to add a new utility                                        #
#       add its features to this file to the lists marked with Do-Util:       #
#                                                                             #
#   * If you want to add a new lib                                           #
#       add its features to this file to the lists marked with Do-Lib:        #
#                                                                             #
###############################################################################

LEX = flex
LEXFLAGS =

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11
INCLUDES = -Ilib/core/src
# Do-Lib: Add your lib include directory to the list above

BUILD ?= debug

# Build modes 
ifeq ($(BUILD), debug)
    CFLAGS += -O0 -g -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

#####################
#                   #
#       Vars        #
#                   #
#####################

#
# Libs
#

# Core
CORE_SRCS	= $(wildcard lib/core/src/*.c)
CORE_OBJS	= $(patsubst lib/core/src/%.c, build/$(BUILD)/obj/core/%.o, $(CORE_SRCS))
CORE_LIB	= build/$(BUILD)/obj/core/libcore.a

# Do-Lib: Add your lib to the list above
# Example (uncomment and modify):
# MY_SRCS	= $(wildcard lib/my/src/*.c)
# MY_OBJS	= $(patsubst lib/my/src/%.c, build/$(BUILD)/obj/my/%.o, $(MY_SRCS))
# MY_LIB	= build/$(BUILD)/obj/my/libmy.a

#
# Utils
#

UTILS			= scr_init scroller
UTIL_LEX		= $(foreach util,$(UTILS), $(wildcard src/$(util)/*.l))
UTIL_LEX_SRCS	= $(patsubst src/%.l, build/$(BUILD)/gen/%.c, $(UTIL_LEX))
UTIL_LEX_OBJS	= $(patsubst build/$(BUILD)/gen/%.c, build/$(BUILD)/obj/%.o, $(UTIL_LEX_SRCS))
UTIL_SRCS		= $(foreach util,$(UTILS), $(wildcard src/$(util)/*.c))
UTIL_OBJS		= $(patsubst src/%.c, build/$(BUILD)/obj/%.o, $(UTIL_SRCS))
UTIL_BINS		= $(addprefix build/$(BUILD)/bin/, $(UTILS))

# Object files for each utilty
UTIL_OBJS_scr_init  = $(filter build/$(BUILD)/obj/scr_init/%, $(UTIL_OBJS) $(UTIL_LEX_OBJS))
UTIL_OBJS_scroller  = $(filter build/$(BUILD)/obj/scroller/%, $(UTIL_OBJS) $(UTIL_LEX_OBJS))
# Do-Util: Add your new utility's object files to the list above

#
# Tests
#

# Core tests
TEST_SRCS_core   = $(wildcard lib/core/test/test_*.c)
TEST_OBJS_core   = $(patsubst lib/core/test/%.c, build/$(BUILD)/obj/test/core/%.o, $(TEST_SRCS_core))
TEST_BINS_core   = $(patsubst lib/core/test/%.c, build/$(BUILD)/bin/test_core_%, $(TEST_SRCS_core))

# Do-Lib: Add your lib tests to the list above
# Example (uncomment and modify):
# TEST_SRCS_my     = $(wildcard lib/my/test/test_*.c)
# TEST_OBJS_my     = $(patsubst lib/my/test/%.c, build/$(BUILD)/obj/test/my/%.o, $(TEST_SRCS_my))
# TEST_BINS_my     = $(patsubst lib/my/test/%.c, build/$(BUILD)/bin/test_my_%, $(TEST_SRCS_my))

# All tests
TEST_SRCS = $(TEST_SRCS_core)
TEST_OBJS = $(TEST_OBJS_core)
TEST_BINS = $(TEST_BINS_core)

#####################
#                   #
#       Rules       #
#                   #
#####################

all: $(UTIL_BINS)

#
# Lex rules

$(UTIL_LEX_SRCS): build/$(BUILD)/gen/%.c: src/%.l
	@mkdir -p $(dir $@)
	$(LEX) $(LEXFLAGS) -o $@ $<

$(UTIL_LEX_OBJS): build/$(BUILD)/obj/%.o: build/$(BUILD)/gen/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(UTIL_LEX_OBJS): $(UTIL_LEX_SRCS)

#
# Lib rules
#

# Core lib
$(CORE_LIB): $(CORE_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(CORE_OBJS): build/$(BUILD)/obj/core/%.o: lib/core/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# DoLib: Add your lib dependencies to the list above
# Example (uncomment and modify):
# $(MY_LIB): $(MY_OBJS)
# 	@mkdir -p $(dir $@)
# 	ar rcs $@ $^
#
# $(MY_OBJS): build/$(BUILD)/obj/my/%.o: lib/my/src/%.c
# 	@mkdir -p $(dir $@)
# 	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

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

# Do-Util: Add your new utility's linkage rules to the list above
# Example (uncomment and modify):
# build/$(BUILD)/bin/my_util: $(UTIL_OBJS_my_util) $(CORE_LIB) $(MY_LIB)
# 	@mkdir -p $(dir $@)
# 	$(CC) $(CFLAGS) $^ -o $@

$(UTIL_OBJS): build/$(BUILD)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

#
# Test rules
#

# Core tests
$(TEST_BINS_core): build/$(BUILD)/bin/test_core_%: build/$(BUILD)/obj/test/core/%.o $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(CORE_LIB) -o $@

$(TEST_OBJS_core): build/$(BUILD)/obj/test/core/%.o: lib/core/test/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Do-Lib: Add your lib test rules to the list above
# Example (uncomment and modify):
# $(TEST_BINS_my): build/$(BUILD)/bin/test_my_%: build/$(BUILD)/obj/test/my/%.o $(MY_LIB)
# 	@mkdir -p $(dir $@)
# 	$(CC) $(CFLAGS) $< $(MY_LIB) -o $@
#
# $(TEST_OBJS_my): build/$(BUILD)/obj/test/my/%.o: lib/my/test/%.c
# 	@mkdir -p $(dir $@)
# 	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

#
# Include generated dependencies
#

DEPS = $(CORE_OBJS:.o=.d) \
	$(UTIL_OBJS:.o=.d) \
	$(UTIL_LEX_OBJS:.o=.d) \
	$(TEST_OBJS:.o=.d)
# Do-Lib: Do-Util: Add your lib or util objs to the list above
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

