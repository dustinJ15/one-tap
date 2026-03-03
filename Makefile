# one-tap Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Ilib/raylib/src
LDFLAGS = -Llib/raylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC     = $(shell find src -name '*.c')
BIN     = one-tap

RAYLIB_A = lib/raylib/src/libraylib.a

.PHONY: all clean raylib

all: raylib $(BIN)

raylib: $(RAYLIB_A)

$(RAYLIB_A):
	$(MAKE) -C lib/raylib/src PLATFORM=PLATFORM_DESKTOP

$(BIN): $(SRC) $(RAYLIB_A)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)

clean-all: clean
	$(MAKE) -C lib/raylib/src clean
