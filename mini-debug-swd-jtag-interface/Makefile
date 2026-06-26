# Makefile - mini-debug-swd-jtag-interface
CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude
LDFLAGS  = -lm
SRC_DIR  = src
TST_DIR  = tests
EX_DIR   = examples
BLD_DIR  = build
SRCS = $(SRC_DIR)/swd_protocol.c $(SRC_DIR)/jtag_tap.c $(SRC_DIR)/debug_port.c $(SRC_DIR)/debug_transport.c $(SRC_DIR)/debug_security.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BLD_DIR)/%.o)
LIB = $(BLD_DIR)/libdebug_interface.a
TEST_SRC = $(TST_DIR)/test_swd_protocol.c $(TST_DIR)/test_jtag_tap.c $(TST_DIR)/test_debug_port.c
TEST_BINS = $(BLD_DIR)/test_swd_protocol $(BLD_DIR)/test_jtag_tap $(BLD_DIR)/test_debug_port
EX_SRCS = $(EX_DIR)/ex1_swd_connect.c $(EX_DIR)/ex2_jtag_scan.c $(EX_DIR)/ex3_flash_program.c $(EX_DIR)/ex4_debug_halt.c
EX_BINS = $(BLD_DIR)/ex1_swd_connect $(BLD_DIR)/ex2_jtag_scan $(BLD_DIR)/ex3_flash_program $(BLD_DIR)/ex4_debug_halt

.PHONY: all test examples clean

all: $(LIB) $(TEST_BINS)

$(BLD_DIR):
	mkdir -p $(BLD_DIR)

$(LIB): $(BLD_DIR) $(OBJS)
	ar rcs $@ $(OBJS)

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BLD_DIR)/test_swd_protocol: $(LIB) $(TST_DIR)/test_swd_protocol.c
	$(CC) $(CFLAGS) $(INCLUDES) $(TST_DIR)/test_swd_protocol.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/test_jtag_tap: $(LIB) $(TST_DIR)/test_jtag_tap.c
	$(CC) $(CFLAGS) $(INCLUDES) $(TST_DIR)/test_jtag_tap.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/test_debug_port: $(LIB) $(TST_DIR)/test_debug_port.c
	$(CC) $(CFLAGS) $(INCLUDES) $(TST_DIR)/test_debug_port.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/ex1_swd_connect: $(LIB) $(EX_DIR)/ex1_swd_connect.c
	$(CC) $(CFLAGS) $(INCLUDES) $(EX_DIR)/ex1_swd_connect.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/ex2_jtag_scan: $(LIB) $(EX_DIR)/ex2_jtag_scan.c
	$(CC) $(CFLAGS) $(INCLUDES) $(EX_DIR)/ex2_jtag_scan.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/ex3_flash_program: $(LIB) $(EX_DIR)/ex3_flash_program.c
	$(CC) $(CFLAGS) $(INCLUDES) $(EX_DIR)/ex3_flash_program.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

$(BLD_DIR)/ex4_debug_halt: $(LIB) $(EX_DIR)/ex4_debug_halt.c
	$(CC) $(CFLAGS) $(INCLUDES) $(EX_DIR)/ex4_debug_halt.c -L$(BLD_DIR) -ldebug_interface $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@echo "Running test suite..."
	$(BLD_DIR)/test_swd_protocol
	$(BLD_DIR)/test_jtag_tap
	$(BLD_DIR)/test_debug_port
	@echo "All tests passed."

examples: $(EX_BINS)

clean:
	rm -rf $(BLD_DIR)
