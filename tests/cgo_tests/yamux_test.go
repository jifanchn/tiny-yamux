package cgo_tests

// This file implements tests to verify compatibility between
// the C implementation (tiny-yamux) and the original Go implementation

// #cgo CFLAGS: -I../../include
// #cgo LDFLAGS: -L../../build/lib -ltiny_yamux
// #include "yamux.h"
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
//
// // IO callbacks and context for C yamux implementation
// typedef struct {
//     int read_fd;
//     int write_fd;
// } pipe_ctx_t;
//
// int test_read(void *ctx, uint8_t *buf, size_t len) {
//     pipe_ctx_t *pipe_ctx = (pipe_ctx_t*)ctx;
//     ssize_t bytes = read(pipe_ctx->read_fd, buf, len);
//     if (bytes < 0) {
//         return -1;
//     }
//     return (int)bytes;
// }
//
// int test_write(void *ctx, const uint8_t *buf, size_t len) {
//     pipe_ctx_t *pipe_ctx = (pipe_ctx_t*)ctx;
//     ssize_t bytes = write(pipe_ctx->write_fd, buf, len);
//     if (bytes < 0) {
//         return -1;
//     }
//     return (int)bytes;
// }
//
// // Init C yamux session
// yamux_session_t* init_c_session(int read_fd, int write_fd, int is_client) {
//     pipe_ctx_t *ctx = malloc(sizeof(pipe_ctx_t));
//     if (!ctx) {
//         return NULL;
//     }
//     
//     ctx->read_fd = read_fd;
//     ctx->write_fd = write_fd;
//     
//     yamux_io_t io;
//     io.read = test_read;
//     io.write = test_write;
//     io.ctx = ctx;
//     
//     yamux_session_t *session;
//     if (yamux_session_create(&io, is_client, NULL, &session) != 0) {
//         free(ctx);
//         return NULL;
//     }
//     
//     return session;
// }
//
// // Process C yamux messages
// int process_c_messages(yamux_session_t *session) {
//     return yamux_session_process(session);
// }
//
// // Open a stream in C yamux
// yamux_stream_t* open_c_stream(yamux_session_t *session) {
//     yamux_stream_t *stream;
//     if (yamux_stream_open(session, 0, &stream) != 0) {
//         return NULL;
//     }
//     return stream;
// }
//
// // Accept a stream in C yamux
// yamux_stream_t* accept_c_stream(yamux_session_t *session) {
//     yamux_stream_t *stream;
//     if (yamux_stream_accept(session, &stream) != 0) {
//         return NULL;
//     }
//     return stream;
// }
//
// // Write to a stream in C yamux
// int write_c_stream(yamux_stream_t *stream, const void *buf, size_t len) {
//     size_t bytes_written;
//     if (yamux_stream_write(stream, buf, len, &bytes_written) != 0) {
//         return -1;
//     }
//     return (int)bytes_written;
// }
//
// // Read from a stream in C yamux
// int read_c_stream(yamux_stream_t *stream, void *buf, size_t len) {
//     size_t bytes_read;
//     if (yamux_stream_read(stream, buf, len, &bytes_read) != 0) {
//         return -1;
//     }
//     return (int)bytes_read;
// }
//
// // Close a stream in C yamux
// int close_c_stream(yamux_stream_t *stream, int reset) {
//     return yamux_stream_close(stream, reset);
// }
//
// // Close a session in C yamux
// int close_c_session(yamux_session_t *session) {
//     return yamux_session_close(session, 0); // Normal close
// }
import "C"

import (
	"fmt"
	"io"
	"net"
	"sync"
	"syscall"
	"testing"
	"time"
	"unsafe"

	"github.com/fatedier/yamux"
)

// PipeConn adapts io.ReadWriteCloser to net.Conn
type PipeConn struct {
	io.ReadWriteCloser
}

func (p PipeConn) LocalAddr() net.Addr {
	return &net.UnixAddr{Name: "pipe", Net: "unix"}
}

func (p PipeConn) RemoteAddr() net.Addr {
	return &net.UnixAddr{Name: "pipe", Net: "unix"}
}

func (p PipeConn) SetDeadline(t time.Time) error {
	return nil
}

func (p PipeConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (p PipeConn) SetWriteDeadline(t time.Time) error {
	return nil
}

// CreateOSPipe creates a pair of file descriptors using os.Pipe
func CreateOSPipe() (readFd, writeFd int, err error) {
	var p [2]int
	err = syscall.Pipe(p[:])
	if err != nil {
		return 0, 0, err
	}
	return p[0], p[1], nil
}

// TestBasicCompatibility tests basic compatibility between Go and C implementations
func TestBasicCompatibility(t *testing.T) {
	// Skip for now as we need to build the C library first
	t.Skip("Skipping CGO test until C library is fully implemented and built")

	// Create pipes for C->Go communication
	cReadFd, goWriteFd, err := CreateOSPipe()
	if err != nil {
		t.Fatalf("Failed to create C->Go pipe: %v", err)
	}
	defer syscall.Close(cReadFd)
	defer syscall.Close(goWriteFd)

	// Create pipes for Go->C communication
	goReadFd, cWriteFd, err := CreateOSPipe()
	if err != nil {
		t.Fatalf("Failed to create Go->C pipe: %v", err)
	}
	defer syscall.Close(goReadFd)
	defer syscall.Close(cWriteFd)

	// Initialize C session (server mode)
	cSession := C.init_c_session(C.int(cReadFd), C.int(cWriteFd), 0) // 0 for server mode
	if cSession == nil {
		t.Fatalf("Failed to create C server session")
	}
	defer C.close_c_session(cSession)

	// Create Go session (client mode)
	goReader := os.NewFile(uintptr(goReadFd), "goReader")
	goWriter := os.NewFile(uintptr(goWriteFd), "goWriter")
	pipeConn := PipeConn{
		ReadWriteCloser: struct {
			io.Reader
			io.Writer
			io.Closer
		}{
			Reader: goReader,
			Writer: goWriter,
			Closer: goReader,
		},
	}

	goConfig := yamux.DefaultConfig()
	goConfig.LogOutput = nil // Disable logging
	goSession, err := yamux.Client(pipeConn, goConfig)
	if err != nil {
		t.Fatalf("Failed to create Go client session: %v", err)
	}
	defer goSession.Close()

	// Start C session processor
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for i := 0; i < 10; i++ { // Process a few times
			res := C.process_c_messages(cSession)
			if res != 0 {
				t.Errorf("C session processing failed: %d", res)
				break
			}
			time.Sleep(100 * time.Millisecond)
		}
	}()

	// Open a stream from Go side
	goStream, err := goSession.OpenStream()
	if err != nil {
		t.Fatalf("Failed to open Go stream: %v", err)
	}
	defer goStream.Close()

	// Give time for C side to process
	time.Sleep(500 * time.Millisecond)

	// Accept the stream on C side
	cStream := C.accept_c_stream(cSession)
	if cStream == nil {
		t.Fatalf("Failed to accept stream on C side")
	}
	defer C.close_c_stream(cStream, 0)

	// Send data from Go to C
	testData := []byte("Hello from Go!")
	n, err := goStream.Write(testData)
	if err != nil || n != len(testData) {
		t.Fatalf("Failed to write data from Go: %v, %d", err, n)
	}

	// Give time for C side to process
	time.Sleep(500 * time.Millisecond)

	// Read data on C side
	cBuf := C.malloc(C.size_t(1024))
	defer C.free(cBuf)
	cBytesRead := C.read_c_stream(cStream, cBuf, C.size_t(1024))
	if cBytesRead < 0 {
		t.Fatalf("Failed to read data on C side")
	}

	// Verify data received on C side
	cData := C.GoBytes(cBuf, C.int(cBytesRead))
	if string(cData) != string(testData) {
		t.Fatalf("Data mismatch: got %q, want %q", string(cData), string(testData))
	}

	// Send data from C to Go
	cResponse := C.CString("Hello from C!")
	defer C.free(unsafe.Pointer(cResponse))
	cBytesWritten := C.write_c_stream(cStream, unsafe.Pointer(cResponse), C.size_t(C.strlen(cResponse)))
	if cBytesWritten < 0 {
		t.Fatalf("Failed to write data from C")
	}

	// Read data on Go side
	goBuf := make([]byte, 1024)
	n, err = goStream.Read(goBuf)
	if err != nil {
		t.Fatalf("Failed to read data on Go side: %v", err)
	}
	goBuf = goBuf[:n]

	// Verify data received on Go side
	expectedResponse := "Hello from C!"
	if string(goBuf) != expectedResponse {
		t.Fatalf("Data mismatch: got %q, want %q", string(goBuf), expectedResponse)
	}

	// Close the stream from Go side
	if err := goStream.Close(); err != nil {
		t.Fatalf("Failed to close Go stream: %v", err)
	}

	// Wait for C processor to finish
	wg.Wait()

	t.Log("Basic compatibility test completed successfully")
}

// TestMultipleStreams tests multiple streams between Go and C implementations
func TestMultipleStreams(t *testing.T) {
	// Skip for now as we need to build the C library first
	t.Skip("Skipping multiple streams test until C library is fully implemented and built")

	// Test implementation would be similar to TestBasicCompatibility
	// but with multiple stream operations
	t.Log("Multiple streams test would go here")
}

// TestPing tests ping functionality between Go and C implementations
func TestPing(t *testing.T) {
	// Skip for now as we need to build the C library first
	t.Skip("Skipping ping test until C library is fully implemented and built")

	// Test implementation would verify ping/pong functionality
	t.Log("Ping test would go here")
}

// TestGoAway tests GoAway functionality between Go and C implementations
func TestGoAway(t *testing.T) {
	// Skip for now as we need to build the C library first
	t.Skip("Skipping GoAway test until C library is fully implemented and built")

	// Test implementation would verify graceful shutdown
	t.Log("GoAway test would go here")
}
