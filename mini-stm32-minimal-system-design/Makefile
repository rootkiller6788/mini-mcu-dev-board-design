# Makefile for mini-stm32-minimal-system-design
# Knowledge: STM32 minimal system hardware design library

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -O2 -g
INCLUDES = -Iinclude
SRCDIR   = src
OBJDIR   = build
LIBNAME  = libstm32_minimal_sys
LIB      = $(OBJDIR)/$(LIBNAME).a

SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

TEST_SOURCES = $(wildcard tests/*.c)
TEST_BINS    = $(patsubst tests/%.c,$(OBJDIR)/test_%,$(TEST_SOURCES))

EXAMPLE_SOURCES = $(wildcard examples/*.c)
EXAMPLE_BINS    = $(patsubst examples/%.c,$(OBJDIR)/example_%,$(EXAMPLE_SOURCES))

.PHONY: all clean test examples lib

all: lib

lib: $(LIB)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB): $(OBJECTS)
	ar rcs $@ $^

$(OBJDIR)/test_%: tests/%.c $(LIB) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJDIR) -lstm32_minimal_sys -lm -o $@

$(OBJDIR)/example_%: examples/%.c $(LIB) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJDIR) -lstm32_minimal_sys -lm -o $@

test: $(TEST_BINS)
	@echo "Running tests..."
	@passed=0; failed=0; \
	for t in $(TEST_BINS); do \
		echo "  $$t"; \
		if ./$$t; then passed=$$((passed+1)); \
		else failed=$$((failed+1)); fi; \
	done; \
	echo "Results: $$passed passed, $$failed failed"

examples: $(EXAMPLE_BINS)
	@echo "Running examples..."
	@for e in $(EXAMPLE_BINS); do \
		echo "=== $$e ==="; \
		./$$e; \
	done

clean:
	rm -rf $(OBJDIR)

# Quick build and test
check: clean lib test
	@echo "All checks passed"
