# Makefile for TecnicoFS tests

CC = gcc
CFLAGS = -g -Wall -std=gnu99 -I..

# Test binaries
TEST_STATE = test_state
TEST_OPERATIONS = test_operations

# Object files from main project (without main.o)
STATE_OBJ = ../fs/state.o
OPERATIONS_OBJ = ../fs/operations.o

.PHONY: all clean test unit-tests integration-tests

all: $(TEST_STATE) $(TEST_OPERATIONS)

# Build test for state module
$(TEST_STATE): test_state.c $(STATE_OBJ)
	$(CC) $(CFLAGS) -o $@ test_state.c $(STATE_OBJ)

# Build test for operations module
$(TEST_OPERATIONS): test_operations.c $(STATE_OBJ) $(OPERATIONS_OBJ)
	$(CC) $(CFLAGS) -o $@ test_operations.c $(STATE_OBJ) $(OPERATIONS_OBJ)

# Build object files from main project if needed
$(STATE_OBJ):
	$(MAKE) -C .. fs/state.o

$(OPERATIONS_OBJ):
	$(MAKE) -C .. fs/operations.o

# Run all tests
test: unit-tests integration-tests concurrency-tests

# Run unit tests
unit-tests: $(TEST_STATE) $(TEST_OPERATIONS)
	@echo ""
	@echo "Running unit tests..."
	@echo ""
	./$(TEST_STATE)
	./$(TEST_OPERATIONS)

# Run integration tests
integration-tests:
	@echo ""
	@echo "Running integration tests..."
	./run_integration_tests.sh

# Run concurrency tests
concurrency-tests:
	@echo ""
	@echo "Running concurrency tests..."
	./run_concurrency_tests.sh

clean:
	rm -f $(TEST_STATE) $(TEST_OPERATIONS)
	rm -rf fixtures output
