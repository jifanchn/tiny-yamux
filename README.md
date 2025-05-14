# tiny-yamux

A portable C implementation of the [yamux](https://github.com/fatedier/yamux) multiplexing library, originally written in Go.

## Overview

The tiny-yamux project implements the yamux protocol in C that can be easily ported to embedded systems. The original yamux library is a Go implementation providing stream-oriented multiplexing capabilities over a single connection.

Key features of this port:
- Platform-agnostic with clear porting interfaces
- Avoids non-standard functions unsuitable for embedded systems
- Minimal memory footprint
- Simple API surface
- Clear separation between platform-specific I/O and protocol logic

## Features

- Multiplexing multiple streams over a single connection
- Bi-directional communication
- Flow control for backpressure management
- Keep-alive mechanism
- Graceful close and error handling
- Optimized for resource-constrained environments

## Project Status

**tiny-yamux** is currently functional and considered stable for its core C implementation.

- **Core Protocol**: All fundamental yamux protocol features (framing, session management, stream management, window updates, pings, go_away) are implemented.
- **Portability**: Designed with a clear porting layer (`yamux_port.c` and `yamux.h` facade) for easy adaptation to various platforms and I/O mechanisms.
- **Testing**: Passes a comprehensive suite of C-based unit and integration tests (`ctest`), covering various aspects including stream I/O, flow control, session lifecycle, and error handling.
- **Examples**: Includes a simple demo (`examples/simple_demo.c`) showcasing basic usage.
- **Build System**: Uses CMake for building the library, examples, and tests.

Future work might include further platform-specific examples, performance optimizations for specific use cases, or integration of CGO tests into the CMake build process.

## Directory Structure

```
tiny-yamux/
├── include/          # Public header files
├── src/              # Implementation source files
├── tests/            # Test files
│   ├── c_tests/      # Pure C tests
│   └── cgo_tests/    # CGO integration tests (run manually with 'go test')
├── examples/         # Example usage
├── build/            # Build output directory (created by CMake)
├── CMakeLists.txt    # CMake build system
├── LICENSE           # MIT License
├── README.md         # This file
└── docs/             # Protocol specification and related documents
```

## Building

### Using CMake

```bash
# Create a build directory
mkdir -p build && cd build

# Configure the build
cmake ..

# Build the library and examples
make

# Run tests
ctest

# Install the library (optional)
make install
```

## Usage

tiny-yamux provides a clear and simple API for integration with any platform:

### Basic Usage Example

```c
#include "yamux.h"

// Platform-specific I/O callbacks (PORTING REQUIRED)
int my_socket_read(void *ctx, uint8_t *buf, size_t len) {
    // Read from your transport layer (e.g., socket, UART, etc.)
    // Implementation details will vary by platform
    socket_t *sock = (socket_t *)ctx;
    return socket_read(sock, buf, len); // Return actual bytes read or -1 on error
}

int my_socket_write(void *ctx, const uint8_t *buf, size_t len) {
    // Write to your transport layer
    // Implementation details will vary by platform
    socket_t *sock = (socket_t *)ctx;
    return socket_write(sock, buf, len); // Return actual bytes written or -1 on error
}

// Initialize yamux session
socket_t *sock = create_socket(); // Platform-specific socket creation
void *session = yamux_init(my_socket_read, my_socket_write, sock, 1); // 1 for client mode

// Open a stream
void *stream = yamux_open_stream(session);

// Write data
int result = yamux_write(stream, data, data_len);

// Read data
int bytes_read = yamux_read(stream, buffer, buffer_size);

// Process protocol messages (call regularly to handle incoming data)
yamux_process(session);

// Close stream when done
yamux_close_stream(stream, 0); // 0 for normal close, 1 for reset

// Clean up
yamux_destroy(session);
```

### Event Loop Integration Example

```c
#include "yamux.h"

// Example of integrating yamux with an event loop
void event_loop(void *session, socket_t *sock) {
    fd_set readfds;
    struct timeval tv;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock->fd, &readfds);
        
        // Wait for data or timeout
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int result = select(sock->fd + 1, &readfds, NULL, NULL, &tv);
        
        if (result > 0) {
            // Data available, process it
            if (yamux_process(session) < 0) {
                // Handle error
                break;
            }
            
            // Accept any new streams
            void *stream = yamux_accept_stream(session);
            if (stream) {
                // Handle new stream
                // Typically you would add this to your managed streams list
                handle_new_stream(stream);
            }
            
            // Read from existing streams
            // ... implementation depends on your application
        }
        
        // Perform periodic tasks
        yamux_ping(session); // Optional keep-alive
    }
}

## Porting to Different Platforms

Tiny-Yamux is designed with clear platform abstraction to make it easy to port to different systems and environments. The key areas that require porting are:

### 1. I/O Callback Functions

The most critical porting requirement is implementing the I/O callbacks for your specific platform:

```c
// Read callback - Must be implemented
int my_read(void *ctx, uint8_t *buf, size_t len) {
    // Platform-specific read implementation
    // - Should return bytes read (>0) on success
    // - Should return 0 on end-of-stream
    // - Should return -1 on error
}

// Write callback - Must be implemented
int my_write(void *ctx, const uint8_t *buf, size_t len) {
    // Platform-specific write implementation
    // - Should return bytes written (>0) on success
    // - Should return -1 on error
}
```

### 2. Test Integration Guidelines

For testing on your platform, create a test infrastructure with these components:

1. **Socket Transport Layer**: Implement socket read/write for Linux environments
2. **Event Loop**: Create a platform-specific event loop that calls `yamux_process()` regularly
3. **Error Handling**: Map yamux error codes to meaningful platform-specific errors

### 3. Memory Considerations

- The library uses dynamic memory allocation for session and stream contexts
- Buffer sizes are configurable through the `yamux_config_t` structure
- For severely constrained systems, consider reducing buffer sizes and limiting the number of concurrent streams

## Testing

The library includes two types of tests:

1. **C Tests** - Pure C tests for individual components
2. **CGO Tests** - Tests that verify compatibility with the original Go implementation

To run the tests:

```bash
# C tests
make test

# CGO tests (requires Go)
cd tests/cgo_tests
go test
```

## Implementation Notes

- The implementation follows the yamux protocol specification closely
- Flow control is implemented using window updates similar to the original Go version
- Memory management is optimized for minimal footprint and fragmentation
- The code avoids dynamic memory allocation where possible in the embedded version

## Progress

See the [progress](./progress) file for current development status.

## License

This project is licensed under the MIT License - see the [LICENSE](./LICENSE) file for details.
