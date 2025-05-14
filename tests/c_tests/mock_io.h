/**
 * @file mock_io.h
 * @brief Mock IO functions for tests
 */

#ifndef MOCK_IO_H
#define MOCK_IO_H

#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Define attribute to silence unused function warnings */
#if defined(__GNUC__) || defined(__clang__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

/* Mock IO context */
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
} mock_io_t;

/* Read callback */
static MAYBE_UNUSED int mock_read(void *ctx, uint8_t *buf, size_t len) {
    mock_io_t *io = (mock_io_t *)ctx;
    
    if (io->should_fail_read) {
        return -1;
    }
    
    if (io->read_buf_used == 0 || io->read_pos >= io->read_buf_used) {
        return 0; /* No data available */
    }
    
    size_t available = io->read_buf_used - io->read_pos;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(buf, io->read_buf + io->read_pos, to_read);
    io->read_pos += to_read;
    
    return to_read;
}

/* Write callback */
static MAYBE_UNUSED int mock_write(void *ctx, const uint8_t *buf, size_t len) {
    mock_io_t *io = (mock_io_t *)ctx;
    
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

/* Initialize mock IO */
static MAYBE_UNUSED mock_io_t *mock_io_init(size_t buf_size) {
    mock_io_t *io = malloc(sizeof(mock_io_t));
    
    io->read_buf_size = buf_size;
    io->read_buf = malloc(io->read_buf_size);
    io->read_buf_used = 0;
    io->read_pos = 0;
    
    io->write_buf_size = buf_size;
    io->write_buf = malloc(io->write_buf_size);
    io->write_buf_used = 0;
    
    io->should_fail_read = 0;
    io->should_fail_write = 0;
    
    return io;
}

/* Free mock IO */
static MAYBE_UNUSED void mock_io_free(mock_io_t *io) {
    if (io) {
        free(io->read_buf);
        free(io->write_buf);
        free(io);
    }
}

/* Swap buffers between mock IOs */
static MAYBE_UNUSED void mock_io_swap_buffers(mock_io_t *io1, mock_io_t *io2) {
    /* io1's write buffer becomes io2's read buffer */
    uint8_t *temp_buf = io1->write_buf;
    size_t temp_size = io1->write_buf_size;
    size_t temp_used = io1->write_buf_used;
    
    io1->write_buf = io2->read_buf;
    io1->write_buf_size = io2->read_buf_size;
    io1->write_buf_used = 0;
    
    io2->read_buf = temp_buf;
    io2->read_buf_size = temp_size;
    io2->read_buf_used = temp_used;
    io2->read_pos = 0;
}

#endif /* MOCK_IO_H */
