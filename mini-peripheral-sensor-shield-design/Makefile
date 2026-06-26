CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm

LIB_SRCS = src/sensor_types.c            src/signal_conditioning.c            src/shield_interface.c            src/sensor_calibration.c            src/sensor_filter.c            src/sensor_fusion.c

LIB_OBJS = $(LIB_SRCS:.c=.o)

TEST_SRC   = tests/test_shield.c
TEST_BIN   = tests/test_shield

EX_SRC1 = examples/example_thermistor_shield.c
EX_SRC2 = examples/example_imu_shield.c
EX_SRC3 = examples/example_gas_sensor_shield.c
EX_BIN1 = examples/example_thermistor_shield
EX_BIN2 = examples/example_imu_shield
EX_BIN3 = examples/example_gas_sensor_shield

.PHONY: all clean test examples

all: test examples

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_SRC) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(EX_BIN1): $(EX_SRC1) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN2): $(EX_SRC2) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN3): $(EX_SRC3) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

examples: $(EX_BIN1) $(EX_BIN2) $(EX_BIN3)

clean:
	rm -f src/*.o
	rm -f $(TEST_BIN) $(TEST_BIN).exe
	rm -f $(EX_BIN1) $(EX_BIN1).exe
	rm -f $(EX_BIN2) $(EX_BIN2).exe
	rm -f $(EX_BIN3) $(EX_BIN3).exe
