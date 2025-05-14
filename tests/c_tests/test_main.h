#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include <stddef.h>
#include <stdint.h>
#include "../../include/yamux.h" // For yamux_io_t

// Default buffer size for mock I/O
#define MOCK_IO_BUFFER_SIZE 4096

// Mock I/O structure
typedef struct {
    yamux_io_t io_if;       // The I/O interface for yamux lib (must be first for context casting)
    uint8_t *read_buf;      // Buffer for data to be read by yamux
    size_t read_buf_size;   // Total size of read_buf
    size_t read_buf_len;    // Current amount of data in read_buf
    size_t read_pos;        // Current position in read_buf for next read

    uint8_t *write_buf;     // Buffer for data written by yamux
    size_t write_buf_size;  // Total size of write_buf
    size_t write_buf_len;   // Current amount of data in write_buf
} stream_io_mock_t;

// Mock I/O callback functions (implementations will be in test_main.c or similar)
int mock_io_read(void *ctx, uint8_t *buf, size_t len);
int mock_io_write(void *ctx, const uint8_t *buf, size_t len);

// Mock I/O helper functions
stream_io_mock_t* stream_io_mock_init(void);
void stream_io_mock_destroy(stream_io_mock_t* mock_io);

/**
 * @brief Connects the write buffer of one mock I/O to the read buffer of another.
 * This simulates data being sent from src and becoming available for reading at dest.
 * @param src The source mock I/O (its write_buf content will be moved).
 * @param dest The destination mock I/O (its read_buf will be populated).
 */
void stream_io_connect(stream_io_mock_t* src, stream_io_mock_t* dest);

// Assertion helper function declarations (implementations in test_main.c or similar)
void assert_true(int condition, const char *message);
void assert_int_equal(int a, int b, const char *message);
void assert_string_equal(const char *a, const char *b, const char *message);

#endif // TEST_MAIN_H
