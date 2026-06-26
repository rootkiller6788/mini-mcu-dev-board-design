# Makefile for mini-power-clock-reset-bring-up
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -g -Iinclude -D_USE_MATH_DEFINES
LDFLAGS := -lm

SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:.c=.o)
HDRS    := $(wildcard include/*.h)
TESTS   := $(wildcard tests/*.c)
TESTBIN := $(TESTS:.c=)
EXAMPLES := $(wildcard examples/*.c)
EXAMPLEBIN := $(EXAMPLES:.c=)
TARGET  := libpowerclockreset.a

.PHONY: all clean test examples lines

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $^

src/%.o: src/%.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TESTBIN)
	@failed=0; for t in $(TESTBIN); do echo "Running $$t..."; ./$$t && echo "  PASS" || { echo "  FAIL"; failed=1; }; done; exit $$failed

tests/%: tests/%.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $< -L. -lpowerclockreset $(LDFLAGS)

examples: $(EXAMPLEBIN)

examples/%: examples/%.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $< -L. -lpowerclockreset $(LDFLAGS)

lines:
	@echo "Total include+src:"; @(cat include/*.h src/*.c 2>/dev/null) | wc -l

clean:
	rm -f src/*.o $(TARGET) $(TESTBIN) $(EXAMPLEBIN)
