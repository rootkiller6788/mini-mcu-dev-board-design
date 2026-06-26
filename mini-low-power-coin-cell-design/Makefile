CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lm

SRC = src/coin_cell_battery.c src/low_power_mcu.c src/power_budget.c src/voltage_regulation.c src/battery_life.c src/energy_harvesting.c
OBJ = $(SRC:.c=.o)
TESTS = tests/test_coin_cell tests/test_power_budget tests/test_battery_life
EXAMPLES = examples/example_cr2032_logger examples/example_ble_beacon examples/example_sensor_node

.PHONY: all clean test examples lib

all: lib test examples

lib: $(OBJ)
	ar rcs libcoin_cell.a $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/test_coin_cell: tests/test_coin_cell.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

tests/test_power_budget: tests/test_power_budget.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

tests/test_battery_life: tests/test_battery_life.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

examples/example_cr2032_logger: examples/example_cr2032_logger.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

examples/example_ble_beacon: examples/example_ble_beacon.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

examples/example_sensor_node: examples/example_sensor_node.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

test: $(TESTS)
	@echo "=== Running Tests ==="
	@./tests/test_coin_cell
	@./tests/test_power_budget
	@./tests/test_battery_life
	@echo "=== All tests passed ==="

examples: $(EXAMPLES)
	@echo "=== Running Examples ==="
	@./examples/example_cr2032_logger
	@./examples/example_ble_beacon
	@./examples/example_sensor_node

clean:
	rm -f src/*.o libcoin_cell.a
	rm -f tests/test_coin_cell tests/test_power_budget tests/test_battery_life
	rm -f examples/example_cr2032_logger examples/example_ble_beacon examples/example_sensor_node
