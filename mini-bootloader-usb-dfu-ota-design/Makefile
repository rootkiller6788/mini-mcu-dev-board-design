# mini-bootloader-usb-dfu-ota-design Makefile
# USB DFU Bootloader and OTA Update Design for MCU Dev Boards
#
# Targets:
#   make          — build library
#   make test     — build and run test suite
#   make examples — build and run all examples
#   make clean    — remove build artifacts
#   make check    — run SKILL.md safety reviews

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -g -O2 \
           -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS  = -lm
INCLUDES = -Iinclude

SRC_C    = src/usb_dfu_core.c src/usb_descriptors.c src/firmware_image.c \
           src/flash_manager.c src/ota_transport.c \
           src/crypto_verify.c src/boot_sequence.c

OBJ      = $(SRC_C:.c=.o)
LIB      = build/libbootloader_usb_dfu_ota.a
TEST_SRC = tests/test_bootloader.c
TEST_BIN = build/test_bootloader
EXAMPLES_SRC = $(wildcard examples/*.c)
EXAMPLES_BIN = $(EXAMPLES_SRC:examples/%.c=build/example_%)

.PHONY: all test examples clean check

all: $(LIB) examples

build:
	mkdir -p build

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB): $(OBJ) | build
	ar rcs $@ $(OBJ)

$(TEST_BIN): $(OBJ) $(TEST_SRC) | build
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) $(OBJ) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "=== Running test suite ==="
	$(TEST_BIN)
	@echo "=== All tests passed ==="

build/example_%: examples/%.c $(OBJ) | build
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJ) $(LDFLAGS) -o $@

examples: $(EXAMPLES_BIN)
	@for exe in $(EXAMPLES_BIN); do echo "--- $$exe ---"; $$exe; echo ""; done

clean:
	rm -rf build/ src/*.o

check:
	@echo "=== Safety Review ==="
	@grep -rn "_fn[0-9]" --include="*.c" --include="*.h" . || echo "  PASS: No filler"
	@grep -rn "_aux[0-9]" --include="*.c" --include="*.h" . || echo "  PASS: No aux"
	@grep -rn "algorithm variant" --include="*.c" . || echo "  PASS: No variant"
	@grep -rn "\bsorry\b" --include="*.lean" . || echo "  PASS: No sorry"
	@echo -n "  include/ + src/ lines: "
	@find . \( -path "*/include/*.h" -o -path "*/src/*.c" \) -exec cat {} + 2>/dev/null | wc -l
	@echo "  Threshold: 3000"

help:
	@echo "make | make test | make examples | make clean | make check"
