/**
 * @file yamux_port.c
 * @brief Primary API for the Yamux protocol with portable interface
 * 
 * This file implements the main Yamux API, providing a simplified interface
 * that's easy to port to different platforms. It separates platform-specific
 * I/O operations from the protocol implementation.
 */

#include "../include/yamux.h"
#include "yamux_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Stream context structure
 */
typedef struct {
    yamux_stream_t *stream;       /* Internal Yamux stream */
    yamux_context_t *context;     /* Parent session context */
} yamux_stream_context_t;

/**
 * Initialize the Yamux library
 * 
 * This is the primary entry point for applications using the Yamux library.
 * It creates a new Yamux session with the provided I/O callbacks and returns
 * a session handle.
 * 
 * @param read_cb Platform-specific read function callback
 * @param write_cb Platform-specific write function callback
 * @param io_ctx Context to be passed to I/O callbacks
 * @param is_client True if client mode, false if server mode
 * @return Session handle, or NULL on error
 */
void* yamux_init(
    int (*read_cb)(void *ctx, uint8_t *buf, size_t len),
    int (*write_cb)(void *ctx, const uint8_t *buf, size_t len),
    void *io_ctx,
    int is_client)
{
    yamux_context_t *ctx;
    yamux_result_t result;
    
    /* Allocate context structure */
    ctx = (yamux_context_t *)malloc(sizeof(yamux_context_t));
    if (!ctx) {
        return NULL;
    }
    
    /* Initialize context */
    memset(ctx, 0, sizeof(yamux_context_t));
    ctx->is_client = is_client;
    ctx->config = yamux_default_config;
    
    /* Set up I/O callbacks */
    ctx->io.read = (int (*)(void *, uint8_t *, size_t))read_cb;
    ctx->io.write = (int (*)(void *, const uint8_t *, size_t))write_cb;
    ctx->io.ctx = io_ctx;
    
    /* Create internal Yamux session */
    result = yamux_session_create(&ctx->io, is_client, &ctx->config, &ctx->session);
    if (result != YAMUX_OK) {
        free(ctx);
        return NULL;
    }

    fprintf(stderr, "DEBUG: yamux_init (in yamux_port.c): created ctx = %p, ctx->session = %p\n", (void*)ctx, (void*)ctx->session);
    fflush(stderr);
    
    return ctx;
}

/**
 * Destroy a Yamux session
 * 
 * @param session Session handle returned by yamux_init
 */
void yamux_destroy(void *session)
{
    yamux_context_t *ctx = (yamux_context_t *)session;
    
    if (!ctx) {
        return;
    }
    
    /* Close internal session */
    if (ctx->session) {
        yamux_session_close(ctx->session, YAMUX_NORMAL);
    }
    
    /* Free resources */
    free(ctx);
}

/**
 * Process incoming data for a session
 * 
 * This function should be called regularly to process incoming Yamux frames.
 * It reads data from the I/O callbacks and processes any complete frames.
 * 
 * @param session Session handle returned by yamux_init
 * @return 0 on success, negative value on error
 */
int yamux_process(void *session_handle)
{
    yamux_context_t *ctx = (yamux_context_t *)session_handle;
    yamux_result_t result;

    // Added debug prints
    fprintf(stderr, "DEBUG: yamux_process (in yamux_port.c): received session_handle = %p\n", session_handle);
    fprintf(stderr, "DEBUG: yamux_process (in yamux_port.c): cast to ctx = %p\n", (void*)ctx);
    if (ctx) {
        fprintf(stderr, "DEBUG: yamux_process (in yamux_port.c): ctx->session = %p\n", (void*)ctx->session);
    } else {
        fprintf(stderr, "DEBUG: yamux_process (in yamux_port.c): ctx is NULL\n");
    }
    fflush(stderr);
    
    if (!ctx || !ctx->session) {
        return -1; // Should be YAMUX_ERR_INVALID or similar
    }
    
    /* Process incoming data */
    result = yamux_session_process(ctx->session);
    
    /* Map result to simple error code */
    return (result == YAMUX_OK) ? 0 : (int)result;
}

/**
 * Open a new stream
 * 
 * @param session Session handle returned by yamux_init
 * @return Stream handle, or NULL on error
 */
void* yamux_open_stream(void *session)
{
    yamux_context_t *ctx = (yamux_context_t *)session;
    yamux_stream_context_t *stream_ctx;
    yamux_stream_t *stream = NULL;
    yamux_result_t result;
    
    if (!ctx || !ctx->session) {
        return NULL;
    }
    
    /* Allocate stream context */
    stream_ctx = (yamux_stream_context_t *)malloc(sizeof(yamux_stream_context_t));
    if (!stream_ctx) {
        return NULL;
    }
    
    /* Open stream */
    result = yamux_stream_open_detailed(ctx->session, 0, &stream);
    if (result != YAMUX_OK) {
        free(stream_ctx);
        return NULL;
    }
    
    /* Initialize stream context */
    stream_ctx->stream = stream;
    stream_ctx->context = ctx;
    
    return stream_ctx;
}

/**
 * Accept a new incoming stream (server only)
 * 
 * @param session Session handle returned by yamux_init
 * @return Stream handle, or NULL if no pending streams
 */
void* yamux_accept_stream(void *session)
{
    yamux_context_t *ctx = (yamux_context_t *)session;
    yamux_stream_context_t *stream_ctx;
    yamux_stream_t *stream;
    yamux_result_t result;
    
    if (!ctx || !ctx->session) {
        return NULL;
    }
    
    /* Allocate stream context */
    stream_ctx = (yamux_stream_context_t *)malloc(sizeof(yamux_stream_context_t));
    if (!stream_ctx) {
        return NULL;
    }
    
    /* Accept stream */
    result = yamux_stream_accept(ctx->session, &stream);
    if (result != YAMUX_OK) {
        free(stream_ctx);
        return NULL;
    }
    
    /* Initialize stream context */
    stream_ctx->stream = stream;
    stream_ctx->context = ctx;
    
    return stream_ctx;
}

/**
 * Close a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param reset True to forcibly reset the stream, false for normal close
 * @return 0 on success, negative value on error
 */
int yamux_close_stream(void *stream, int reset)
{
    yamux_stream_context_t *stream_ctx = (yamux_stream_context_t *)stream;
    yamux_result_t result;
    
    if (!stream_ctx || !stream_ctx->stream) {
        return -1;
    }
    
    /* Close stream */
    result = yamux_stream_close(stream_ctx->stream, reset);
    
    /* Free stream context */
    free(stream_ctx);
    
    return (result == YAMUX_OK) ? 0 : (int)result;
}

/**
 * Read data from a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param buf Buffer to read data into
 * @param len Maximum number of bytes to read
 * @return Number of bytes read, 0 on EOF, or negative value on error
 */
int yamux_read(void *stream, uint8_t *buf, size_t len)
{
    yamux_stream_context_t *stream_ctx = (yamux_stream_context_t *)stream;
    yamux_result_t result;
    size_t bytes_read;
    
    if (!stream_ctx || !stream_ctx->stream || !buf || len == 0) {
        return -1;
    }
    
    /* Read from stream */
    result = yamux_stream_read(stream_ctx->stream, buf, len, &bytes_read);
    
    if (result != YAMUX_OK) {
        return (int)result;
    }
    
    return (int)bytes_read;
}

/**
 * Write data to a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written, or negative value on error
 */
int yamux_write(void *stream, const uint8_t *buf, size_t len)
{
    yamux_stream_context_t *stream_ctx = (yamux_stream_context_t *)stream;
    yamux_result_t result;
    size_t bytes_written;
    
    if (!stream_ctx || !stream_ctx->stream || !buf || len == 0) {
        return -1;
    }
    
    /* Write to stream */
    result = yamux_stream_write(stream_ctx->stream, buf, len, &bytes_written);
    
    if (result != YAMUX_OK) {
        return (int)result;
    }
    
    return (int)bytes_written;
}

/**
 * Get the stream ID
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @return Stream ID or 0 on error
 */
uint32_t yamux_get_stream_id(void *stream)
{
    yamux_stream_context_t *stream_ctx = (yamux_stream_context_t *)stream;
    
    if (!stream_ctx || !stream_ctx->stream) {
        return 0;
    }
    
    return yamux_stream_get_id(stream_ctx->stream);
}

/**
 * Send a ping to the remote endpoint
 * 
 * @param session Session handle returned by yamux_init
 * @return 0 on success, negative value on error
 */
int yamux_ping(void *session)
{
    yamux_context_t *ctx = (yamux_context_t *)session;
    yamux_result_t result;
    
    if (!ctx || !ctx->session) {
        return -1;
    }
    
    /* Send ping */
    result = yamux_session_ping(ctx->session);
    
    return (result == YAMUX_OK) ? 0 : (int)result;
}
