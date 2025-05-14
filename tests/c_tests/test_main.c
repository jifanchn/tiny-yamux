#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_main.h"

/* Function prototypes for tests */
void test_buffer(void);
void test_frame_encoding(void);
void test_frame_decoding(void);
void test_stream_io(void);
void test_session_creation(void);
void test_session_ping(void);
void test_flow_control(void);
void test_stream_lifecycle(void);
void test_concurrent_streams(void);
void test_error_handling(void);

/* Test runner */
typedef struct {
    const char *name;
    void (*func)(void);
} test_case_t;

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

int main(void) {
    test_case_t tests[] = {
        {"Buffer Management", test_buffer},
        {"Frame Encoding", test_frame_encoding},
        {"Frame Decoding", test_frame_decoding},
        {"Stream I/O", test_stream_io},
        {"Session Creation", test_session_creation},
        {"Session Ping", test_session_ping},
        {"Flow Control", test_flow_control},
        {"Stream Lifecycle", test_stream_lifecycle},
        {"Concurrent Streams", test_concurrent_streams},
        {"Error Handling", test_error_handling}
    };
    
    int num_tests = sizeof(tests) / sizeof(test_case_t);
    int passed = 0;
    
    printf("Running %d tests...\n\n", num_tests);
    
    for (int i = 0; i < num_tests; i++) {
        printf("Test %d/%d: %s...", i + 1, num_tests, tests[i].name);
        fflush(stdout);
        
        /* Run the test */
        tests[i].func();
        
        /* If we get here, the test passed (failed tests exit) */
        printf(" %sPASSED%s\n", GREEN, RESET);
        passed++;
    }
    
    printf("\n%d/%d tests passed\n", passed, num_tests);
    
    return (passed == num_tests) ? 0 : 1;
}

/* Utility function for test assertions */
void assert_true(int condition, const char *message) {
    if (!condition) {
        printf(" %sFAILED%s: %s\n", RED, RESET, message);
        exit(1);
    }
}

void assert_int_equal(int a, int b, const char *message) {
    if (a != b) {
        printf(" %sFAILED%s: %s (Expected: %d, Got: %d)\n", RED, RESET, message, b, a);
        exit(1);
    }
}

void assert_string_equal(const char *a, const char *b, const char *message) {
    if (a == NULL && b == NULL) return;
    if (a == NULL || b == NULL || strcmp(a, b) != 0) {
        printf(" %sFAILED%s: %s (Expected: '%s', Got: '%s')\n", RED, RESET, message, b ? b : "NULL", a ? a : "NULL");
        exit(1);
    }
}

// Mock I/O callback functions
int mock_io_read(void *ctx, uint8_t *buf, size_t len) {
    stream_io_mock_t *mock = (stream_io_mock_t *)ctx;
    if (!mock || !mock->read_buf || mock->read_pos >= mock->read_buf_len) {
        return YAMUX_ERR_WOULD_BLOCK; // Or 0 if indicating EOF and no error
    }

    size_t bytes_to_read = mock->read_buf_len - mock->read_pos;
    if (bytes_to_read > len) {
        bytes_to_read = len;
    }

    memcpy(buf, mock->read_buf + mock->read_pos, bytes_to_read);
    mock->read_pos += bytes_to_read;
    return (int)bytes_to_read;
}

int mock_io_write(void *ctx, const uint8_t *buf, size_t len) {
    stream_io_mock_t *mock = (stream_io_mock_t *)ctx;

    printf("DEBUG: mock_io_write: Entered. ctx(mock)=%p, len=%zu\n", (void*)mock, len);
    if (mock) {
        printf("DEBUG: mock_io_write: mock is not NULL. mock->write_buf=%p, mock->write_buf_size=%zu\n", (void*)mock->write_buf, mock->write_buf_size);
    }

    if (!mock || !mock->write_buf) {
        printf("ERROR: mock_io_write: Condition '!mock || !mock->write_buf' is TRUE. !mock=%d, !mock->write_buf=%d\n",
               !mock, (mock ? !mock->write_buf : -1)); // Avoid dereferencing NULL mock
        return YAMUX_ERR_INVALID;
    }

    if (mock->write_buf_len + len > mock->write_buf_size) {
        // Buffer overflow, try to reallocate or return error
        size_t new_size = mock->write_buf_size + len + MOCK_IO_BUFFER_SIZE; // Grow by len + some extra
        uint8_t *new_buf = (uint8_t *)realloc(mock->write_buf, new_size);
        if (!new_buf) {
            fprintf(stderr, "mock_io_write: FAILED to realloc write_buf\n");
            return YAMUX_ERR_INTERNAL; // Indicate an internal error
        }
        mock->write_buf = new_buf;
        mock->write_buf_size = new_size;
    }

    memcpy(mock->write_buf + mock->write_buf_len, buf, len);
    mock->write_buf_len += len;
    return (int)len;
}

// Mock I/O helper functions implementations
stream_io_mock_t* stream_io_mock_init(void) {
    stream_io_mock_t *mock = (stream_io_mock_t *)calloc(1, sizeof(stream_io_mock_t));
    if (!mock) {
        perror("stream_io_mock_init: calloc failed");
        return NULL;
    }

    mock->read_buf = (uint8_t *)malloc(MOCK_IO_BUFFER_SIZE);
    mock->write_buf = (uint8_t *)malloc(MOCK_IO_BUFFER_SIZE);

    if (!mock->read_buf || !mock->write_buf) {
        perror("stream_io_mock_init: malloc for buffers failed");
        free(mock->read_buf);
        free(mock->write_buf);
        free(mock);
        return NULL;
    }

    mock->read_buf_size = MOCK_IO_BUFFER_SIZE;
    mock->write_buf_size = MOCK_IO_BUFFER_SIZE;
    mock->read_buf_len = 0;
    mock->read_pos = 0;
    mock->write_buf_len = 0;

    // Setup the yamux_io_t interface
    mock->io_if.read = mock_io_read;
    mock->io_if.write = mock_io_write;
    mock->io_if.ctx = mock; // Pass the mock instance as context

    return mock;
}

void stream_io_mock_destroy(stream_io_mock_t* mock_io) {
    if (mock_io) {
        free(mock_io->read_buf);
        free(mock_io->write_buf);
        free(mock_io);
    }
}

void stream_io_connect(stream_io_mock_t* src, stream_io_mock_t* dest) {
    if (!src || !dest || !src->write_buf || !dest->read_buf) {
        fprintf(stderr, "stream_io_connect: Invalid arguments\n");
        return;
    }

    if (src->write_buf_len > 0) {
        // Ensure dest read buffer is large enough
        if (dest->read_buf_len + src->write_buf_len > dest->read_buf_size) {
            size_t new_size = dest->read_buf_len + src->write_buf_len + MOCK_IO_BUFFER_SIZE;
            uint8_t *new_buf = (uint8_t *)realloc(dest->read_buf, new_size);
            if (!new_buf) {
                fprintf(stderr, "stream_io_connect: FAILED to realloc dest->read_buf\n");
                // Decide how to handle this error. For tests, maybe an assert or print.
                return; // Or abort, or set an error flag.
            }
            dest->read_buf = new_buf;
            dest->read_buf_size = new_size;
        }
        
        // Append src's write_buf to dest's read_buf
        memcpy(dest->read_buf + dest->read_buf_len, src->write_buf, src->write_buf_len);
        dest->read_buf_len += src->write_buf_len;
        // dest->read_pos should remain unchanged as new data is appended.
        // If dest->read_pos was at the end of old data, it's still at the start of new data from this connect.

        // Clear src's write buffer after transfer
        src->write_buf_len = 0;
    }
}
