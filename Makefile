CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -std=c11 -march=native -O2 -g
LDFLAGS = -g

ifeq ($(OS),Windows_NT)
	TARGET = ./build/test.exe
else
	TARGET = ./build/test
endif

SRC = photjson.c test.c
OBJ = $(SRC:.c=.o)
OBJ := $(addprefix build/,$(OBJ))

build: $(TARGET)

test: build
	$(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: %.c | dir
	$(CC) $(CFLAGS) -c -o $@ $<

dir:
	@if [ ! -d build ]; then mkdir build; fi

clean:
	rm -rf build/*

.PHONY: build test dir clean
