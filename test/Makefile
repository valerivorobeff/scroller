BIN = basic_test

# Flags
CFLAGS = -I../src -Wall -Wextra
LDFLAGS =

# Debug/Release
ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG -g -O0 -DIHASH_DEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

SRC = basic.c \
    ../src/grid.c \
    ../src/ilist2.c \
    ../src/ihash.c

OBJ = $(SRC:.c=.o)

# OS special
ifeq ($(OS),Windows_NT)
    BIN := $(BIN).exe
    RM = del /Q
else
    RM = rm -f
endif

# Targets
all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) DEBUG=1 all

release:
	$(MAKE) DEBUG=0 all

clean:
	$(RM) $(OBJ) $(BIN)

.PHONY: all clean debug release

