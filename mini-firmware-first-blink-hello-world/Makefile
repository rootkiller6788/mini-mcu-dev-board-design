# Makefile — mini-firmware-first-blink-hello-world
# Builds host-side tests (not MCU cross-compilation)
# These tests verify logic correctness, math, and data structures.

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2 -g
LDFLAGS  =

# Source directories
INC_DIR  = include
SRC_DIR  = src
TST_DIR  = tests
EXA_DIR  = examples

# Header files
HEADERS  = $(wildcard $(INC_DIR)/*.h)

# Source files
SRCS     = $(SRC_DIR)/gpio.c $(SRC_DIR)/uart.c $(SRC_DIR)/timer.c \
           $(SRC_DIR)/bootloader.c $(SRC_DIR)/cortex_m.c \
           $(SRC_DIR)/adc.c $(SRC_DIR)/watchdog.c $(SRC_DIR)/startup.c

# Test files
TESTS    = $(TST_DIR)/test_gpio $(TST_DIR)/test_uart $(TST_DIR)/test_bootloader $(TST_DIR)/test_core
TEST_SRCS = $(TST_DIR)/test_gpio.c $(TST_DIR)/test_uart.c $(TST_DIR)/test_bootloader.c $(TST_DIR)/test_core.c

# Example files
EXAMPLES = $(EXA_DIR)/blink_hello $(EXA_DIR)/uart_echo $(EXA_DIR)/pwm_led

.PHONY: all clean test examples

# Default target
all: test

# ──────────────────────────────────────────────
# Test targets
# ──────────────────────────────────────────────

test: $(TESTS)
	@echo "=== Running all tests ==="
	@for t in $(TESTS); do \
		echo ""; \
		echo "--- $$t ---"; \
		./$$t || exit 1; \
	done
	@echo ""
	@echo "=== ALL TESTS PASSED ==="

$(TST_DIR)/test_gpio: $(TST_DIR)/test_gpio.c $(SRC_DIR)/gpio.c $(INC_DIR)/gpio.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TST_DIR)/test_gpio.c $(SRC_DIR)/gpio.c $(LDFLAGS)

$(TST_DIR)/test_uart: $(TST_DIR)/test_uart.c $(SRC_DIR)/uart.c $(SRC_DIR)/cortex_m.c $(INC_DIR)/uart.h $(INC_DIR)/cortex_m.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TST_DIR)/test_uart.c $(SRC_DIR)/uart.c $(SRC_DIR)/cortex_m.c $(LDFLAGS)

$(TST_DIR)/test_bootloader: $(TST_DIR)/test_bootloader.c $(SRC_DIR)/bootloader.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c $(INC_DIR)/bootloader.h $(INC_DIR)/cortex_m.h $(INC_DIR)/uart.h $(INC_DIR)/arch_compat.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TST_DIR)/test_bootloader.c $(SRC_DIR)/bootloader.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c $(LDFLAGS)

$(TST_DIR)/test_core: $(TST_DIR)/test_core.c $(SRC_DIR)/timer.c $(SRC_DIR)/watchdog.c $(SRC_DIR)/adc.c $(SRC_DIR)/startup.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c $(INC_DIR)/timer.h $(INC_DIR)/watchdog.h $(INC_DIR)/adc.h $(INC_DIR)/startup.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TST_DIR)/test_core.c $(SRC_DIR)/timer.c $(SRC_DIR)/watchdog.c $(SRC_DIR)/adc.c $(SRC_DIR)/startup.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c $(LDFLAGS)

# ──────────────────────────────────────────────
# Example targets
# ──────────────────────────────────────────────

examples: $(EXAMPLES)

$(EXA_DIR)/blink_hello: $(EXA_DIR)/blink_hello.c $(INC_DIR)/gpio.h $(SRC_DIR)/gpio.c $(INC_DIR)/startup.h $(SRC_DIR)/startup.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(EXA_DIR)/blink_hello.c $(SRC_DIR)/gpio.c $(SRC_DIR)/startup.c $(SRC_DIR)/cortex_m.c $(SRC_DIR)/uart.c $(LDFLAGS)

$(EXA_DIR)/uart_echo: $(EXA_DIR)/uart_echo.c $(INC_DIR)/uart.h $(SRC_DIR)/uart.c $(INC_DIR)/gpio.h $(SRC_DIR)/gpio.c $(SRC_DIR)/cortex_m.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(EXA_DIR)/uart_echo.c $(SRC_DIR)/uart.c $(SRC_DIR)/gpio.c $(SRC_DIR)/cortex_m.c $(LDFLAGS)

$(EXA_DIR)/pwm_led: $(EXA_DIR)/pwm_led.c $(INC_DIR)/timer.h $(SRC_DIR)/timer.c $(INC_DIR)/gpio.h $(SRC_DIR)/gpio.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(EXA_DIR)/pwm_led.c $(SRC_DIR)/timer.c $(SRC_DIR)/gpio.c -lm $(LDFLAGS)

# ──────────────────────────────────────────────
# Utility
# ──────────────────────────────────────────────

clean:
	rm -f $(TESTS) $(EXAMPLES)
	rm -f $(TST_DIR)/*.o $(SRC_DIR)/*.o $(EXA_DIR)/*.o

# Show line counts
lines:
	@echo "Headers:"
	@wc -l $(INC_DIR)/*.h
	@echo ""
	@echo "Sources:"
	@wc -l $(SRC_DIR)/*.c $(SRC_DIR)/*.lean
	@echo ""
	@echo "Tests:"
	@wc -l $(TST_DIR)/*.c
	@echo ""
	@echo "Total include/ + src/:"
	@find . \( -path "*/include/*.h" -o -path "*/src/*.c" -o -path "*/src/*.lean" \) -exec cat {} + | wc -l

# Safety checks per SKILL.md §十
safety:
	@echo "=== Safety Review (SKILL.md §十) ==="
	@echo ""
	@echo "--- Filler detection ---"
	@grep -rn "_fn[0-9]" --include="*.c" --include="*.h" . || echo "  PASS: no _fnN patterns"
	@grep -rn "_aux[0-9]" --include="*.c" --include="*.h" . || echo "  PASS: no _auxN patterns"
	@grep -rn "_ext[0-9][^a-z]" --include="*.c" --include="*.h" . || echo "  PASS: no _extN patterns"
	@grep -rn "algorithm variant" --include="*.c" . || echo "  PASS: no filler text"
	@grep -rn "extension point" --include="*.c" . || echo "  PASS: no filler text"
	@grep -rn "supplemental assert" --include="*.c" . || echo "  PASS: no filler text"
	@echo ""
	@echo "--- Lean filler detection ---"
	@grep -rn "SystemMetric" --include="*.lean" . || echo "  PASS: no SystemMetric"
	@grep -rn "traceability_matrix" --include="*.lean" . || echo "  PASS: no traceability_matrix"
	@grep -rn "by trivial" --include="*.lean" . || echo "  PASS: no by trivial"
	@grep -rn "\bsorry\b" --include="*.lean" . || echo "  PASS: no sorry"
	@echo ""
	@echo "--- Stub detection ---"
	@find . -name "*.c" -size -200c -exec echo "SUSPECT: {}" \; || echo "  PASS: no files < 200 bytes"
	@echo ""
	@echo "--- TODO/FIXME/stub/placeholder ---"
	@grep -rn "TODO\|FIXME\|stub\|placeholder" --include="*.c" --include="*.h" --include="*.lean" . || echo "  PASS: no TODO/FIXME/stub/placeholder"
	@echo ""
	@echo "=== Safety review complete ==="
