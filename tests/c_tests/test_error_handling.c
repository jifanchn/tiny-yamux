#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* External assert function declaration */
void assert_true(int condition, const char *message);

/* Mock IO context for testing error conditions */
typedef struct {
    uint8_t *read_buf;
    size_t read_buf_size;
    size_t read_buf_used;
    size_t read_pos;
    
    uint8_t *write_buf;
    size_t write_buf_size;
    size_t write_buf_used;
    
    int should_fail_read;
    int should_fail_write;
} error_io_t;

/* Read callback with error simulation */
int error_read(void *ctx, uint8_t *buf, size_t len) {
    error_io_t *io = (error_io_t *)ctx;
    
    if (io->should_fail_read) {
        return -1;
    }
    
    if (io->read_buf_used == 0 || io->read_pos >= io->read_buf_used) {
        return 0;
    }
    
    size_t available = io->read_buf_used - io->read_pos;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(buf, io->read_buf + io->read_pos, to_read);
    io->read_pos += to_read;
    
    return to_read;
}

/* Write callback with error simulation */
int error_write(void *ctx, const uint8_t *buf, size_t len) {
    error_io_t *io = (error_io_t *)ctx;
    
    if (io->should_fail_write) {
        return -1;
    }
    
    if (io->write_buf_used + len > io->write_buf_size) {
        /* Resize buffer if needed */
        size_t new_size = io->write_buf_size * 2;
        if (new_size < io->write_buf_used + len) {
            new_size = io->write_buf_used + len;
        }
        
        io->write_buf = realloc(io->write_buf, new_size);
        io->write_buf_size = new_size;
    }
    
    memcpy(io->write_buf + io->write_buf_used, buf, len);
    io->write_buf_used += len;
    
    return len;
}

/* Initialize error IO */
error_io_t *error_io_init(void) {
    error_io_t *io = malloc(sizeof(error_io_t));
    
    io->read_buf_size = 4096;
    io->read_buf = malloc(io->read_buf_size);
    io->read_buf_used = 0;
    io->read_pos = 0;
    
    io->write_buf_size = 4096;
    io->write_buf = malloc(io->write_buf_size);
    io->write_buf_used = 0;
    
    io->should_fail_read = 0;
    io->should_fail_write = 0;
    
    return io;
}

/* Free error IO */
void error_io_free(error_io_t *io) {
    if (io) {
        free(io->read_buf);
        free(io->write_buf);
        free(io);
    }
}

/* Test error handling */
void test_error_handling(void) {
    printf("Testing error handling...\n");
    yamux_session_t *session = NULL;
    yamux_stream_t *stream = NULL;
    yamux_io_t io;
    error_io_t *error_io = NULL;
    yamux_config_t config;
    yamux_result_t result;
    
    /* TEST 1: NULL parameter validation */
    result = yamux_session_create(NULL, 1, NULL, &session);
    assert_true(result == YAMUX_ERR_INVALID, "Should fail with NULL IO");
    
    result = yamux_session_create(&io, 1, NULL, NULL);
    assert_true(result == YAMUX_ERR_INVALID, "Should fail with NULL session pointer");
    
    /* Initialize error IO */
    error_io = error_io_init();
    
    /* Set up IO callbacks */
    io.read = error_read;
    io.write = error_write;
    io.ctx = error_io;
    
    /* Set default config */
    memset(&config, 0, sizeof(config));
    config.accept_backlog = 128;
    
    /* Create session */
    result = yamux_session_create(&io, 1, &config, &session);
    assert_true(result == YAMUX_OK, "Failed to create session");
    
    /* TEST 2: Invalid stream ID - testing our fix */
    result = yamux_stream_open_detailed(session, 0xFFFFFFFF, &stream);
    assert_true(result == YAMUX_ERR_INVALID, "Should fail with invalid stream ID 0xFFFFFFFF");
    
    /* Clean up IO resources */
    if (error_io) {
        error_io_free(error_io);
        error_io = NULL;
    }
}
