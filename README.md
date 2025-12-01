# TecnicoFS

A multi-threaded client-server filesystem implementation using UNIX domain sockets. Developed for the Operating Systems course at DEI/IST/ULisboa.

## Features

- **Multi-threaded server** with configurable thread pool
- **Client-server architecture** using UNIX domain sockets (UDP/datagram)
- **File operations**: create, delete, lookup, move, print tree
- **Thread-safe** with read-write locks for concurrent access
- **Automated test suite** with unit, integration, and concurrency tests

## Building

```bash
# Build server
make

# Build client
make -C client

# Build everything and run tests
make test
```

## Usage

### Start the server
```bash
./tecnicofs <num_threads> <socket_name>
# Example:
./tecnicofs 4 server_socket
```

### Run the client
```bash
cd client
./tecnicofs-client <input_file> <socket_name>
# Example:
./tecnicofs-client commands.txt server_socket
```

### Command format (input file)
```
c /path/to/file f    # Create file
c /path/to/dir d     # Create directory
l /path/to/node      # Lookup (search)
d /path/to/node      # Delete
m /source /dest      # Move/rename
p /output/file.txt   # Print filesystem tree to file
```

## Testing

```bash
# Run all tests
make test

# Run specific test suites
cd tests
make unit-tests        # Unit tests for state.c and operations.c
make integration-tests # End-to-end client-server tests
make concurrency-tests # Multi-client stress tests
```

## Project Structure

```
├── main.c                    # Server main program
├── Makefile                  # Server build configuration
├── tecnicofs-api-constants.h # Shared constants and types
├── fs/
│   ├── state.c               # Inode table management
│   ├── state.h
│   ├── operations.c          # Filesystem operations
│   └── operations.h
├── client/
│   ├── tecnicofs-client.c    # Client main program
│   ├── tecnicofs-client-api.c # Client socket API
│   ├── tecnicofs-client-api.h
│   └── Makefile
└── tests/
    ├── minunit.h             # Minimal unit test framework
    ├── test_state.c          # Unit tests for state module
    ├── test_operations.c     # Unit tests for operations module
    ├── run_integration_tests.sh
    ├── run_concurrency_tests.sh
    └── Makefile
```

## Architecture

- **Server**: Multi-threaded, uses a global read-write lock (`pthread_rwlock_t`) for synchronization
- **Communication**: UNIX domain sockets with datagram protocol
- **Filesystem**: In-memory inode table with fixed size (50 inodes, 20 entries per directory)

## License

Educational project - IST/ULisboa 2020-21
