# Makefile for mini-esp32-iot-board-layout
# ESP32 IoT Board Layout Design Library
#
# Targets:
#   make          - Build static library
#   make test     - Build and run tests
#   make examples - Build all examples
#   make clean    - Remove build artifacts
#   make all      - Build library, tests, and examples

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -I./include
AR       = ar
ARFLAGS  = rcs

# Source files
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
LIB  = libesp32board.a

# Test files
TEST_SRCS = $(wildcard tests/*.c)
TEST_BINS = $(TEST_SRCS:.c=.test)

# Example files
EXAMPLE_SRCS = $(wildcard examples/*.c)
EXAMPLE_BINS = $(EXAMPLE_SRCS:.c=.ex)

.PHONY: all clean test examples lib

all: lib test examples

lib: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^
	@echo "  LIB  $@ built ($(words $(OBJS)) objects)"

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
	@echo "  CC   $<"

test: $(LIB) $(TEST_BINS)
	@echo "=== Running Tests ==="
	@passed=0; failed=0; \
	for t in $(TEST_BINS); do \
		if ./$$t; then passed=$$((passed+1)); \
		else failed=$$((failed+1)); fi; \
	done; \
	echo "=== Results: $$passed passed, $$failed failed ==="; \
	[ $$failed -eq 0 ] || exit 1

tests/%.test: tests/%.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lesp32board -lm -o $@
	@echo "  TEST $<"

examples: $(EXAMPLE_BINS)

examples/%.ex: examples/%.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lesp32board -lm -o $@
	@echo "  EX   $<"

run-examples: examples
	@for e in $(EXAMPLE_BINS); do echo "--- $$e ---"; ./$$e; done

clean:
	rm -f $(OBJS) $(LIB) $(TEST_BINS) $(EXAMPLE_BINS)
	rm -f tests/*.o examples/*.o
	@echo "  CLEAN complete"

help:
	@echo "Targets: all lib test examples run-examples clean help"
