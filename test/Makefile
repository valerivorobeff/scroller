BIN = basic_test

# Flags
CFLAGS += -I../src

SRC = basic.c \
	../src/grid.c \
	../src/ihash.c

OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
BIN := $(BIN).exe
endif

all: $(BIN)

$(BIN): clean
	$(CC) $(SRC) $(CFLAGS) -o $(BIN)

clean:
	rm -f $(OBJ)
