/**
 * @file test_common.h
 * @brief Common utilities for tests
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

/* Define attribute to silence unused function warnings */
#if defined(__GNUC__) || defined(__clang__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

#include "yamux.h"
#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Buffer for pipe IO */
struct pipe_buffer {
    uint8_t *data;
    size_t capacity;
    size_t used;
    size_t pos;
};

/* Pipe IO context */
typedef struct {
    struct pipe_buffer *read_buf;
    struct pipe_buffer *write_buf;
} pipe_io_context_t;

/* Initialize a pipe buffer */
static struct pipe_buffer *pipe_buffer_init(size_t capacity) {
    struct pipe_buffer *buf = (struct pipe_buffer *)malloc(sizeof(struct pipe_buffer));
    if (!buf) {
        return NULL;
    }
    
    buf->data = (uint8_t *)malloc(capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    
    buf->capacity = capacity;
    buf->used = 0;
    buf->pos = 0;
    
    return buf;
}

/* Free a pipe buffer */
static void pipe_buffer_free(struct pipe_buffer *buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

/* Read from a pipe buffer */
static int pipe_buffer_read(struct pipe_buffer *buf, uint8_t *data, size_t len) {
    size_t available;
    
    if (!buf || !data || len == 0) {
        return -1;
    }
    
    available = buf->used - buf->pos;
    if (available == 0) {
        return 0;
    }
    
    if (len > available) {
        len = available;
    }
    
    memcpy(data, buf->data + buf->pos, len);
    buf->pos += len;
    
    return len;
}

/* Write to a pipe buffer */
static int pipe_buffer_write(struct pipe_buffer *buf, const uint8_t *data, size_t len) {
    size_t available;
    
    if (!buf || !data || len == 0) {
        return -1;
    }
    
    available = buf->capacity - buf->used;
    if (available == 0) {
        return 0;
    }
    
    if (len > available) {
        len = available;
    }
    
    memcpy(buf->data + buf->used, data, len);
    buf->used += len;
    
    return len;
}

/* Pipe read callback for yamux */
static MAYBE_UNUSED int pipe_read(void *ctx, uint8_t *buf, size_t len) {
    pipe_io_context_t *io_ctx = (pipe_io_context_t *)ctx;
    return pipe_buffer_read(io_ctx->read_buf, buf, len);
}

/* Pipe write callback for yamux */
static MAYBE_UNUSED int pipe_write(void *ctx, const uint8_t *buf, size_t len) {
    pipe_io_context_t *io_ctx = (pipe_io_context_t *)ctx;
    return pipe_buffer_write(io_ctx->write_buf, buf, len);
}

/* Create a pipe IO context */
static MAYBE_UNUSED pipe_io_context_t *pipe_io_context_create(size_t buffer_size) {
    pipe_io_context_t *ctx = (pipe_io_context_t *)malloc(sizeof(pipe_io_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->read_buf = pipe_buffer_init(buffer_size);
    if (!ctx->read_buf) {
        free(ctx);
        return NULL;
    }
    
    ctx->write_buf = pipe_buffer_init(buffer_size);
    if (!ctx->write_buf) {
        pipe_buffer_free(ctx->read_buf);
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

/* Free a pipe IO context */
static MAYBE_UNUSED void pipe_io_context_free(pipe_io_context_t *ctx) {
    if (ctx) {
        pipe_buffer_free(ctx->read_buf);
        pipe_buffer_free(ctx->write_buf);
        free(ctx);
    }
}

/* Swap read and write buffers */
static MAYBE_UNUSED void pipe_io_context_swap_buffers(pipe_io_context_t *ctx1, pipe_io_context_t *ctx2) {
    struct pipe_buffer *temp;
    
    /* Connect ctx1's write buffer to ctx2's read buffer */
    temp = ctx1->write_buf;
    ctx1->write_buf = ctx2->read_buf;
    ctx2->read_buf = temp;
}

#endif /* TEST_COMMON_H */
