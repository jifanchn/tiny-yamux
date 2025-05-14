/**
 * @file yamux_wrapper.h
 * @brief Portable wrapper for the yamux protocol implementation
 * 
 * This header provides a portable interface to the yamux protocol
 * implementation, designed for easy porting to embedded systems.
 * It provides a simplified API that can be easily integrated into
 * various systems, including those without standard library support.
 */

#ifndef YAMUX_WRAPPER_H
#define YAMUX_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new yamux session wrapper
 *
 * @param read_cb Read callback function
 * @param write_cb Write callback function
 * @param io_ctx I/O context for callbacks
 * @param is_client True for client mode, false for server mode
 * @return Session context pointer or NULL on error
 */
void *yamux_wrapper_init(
    int (*read_cb)(void *ctx, void *buf, int len),
    int (*write_cb)(void *ctx, const void *buf, int len),
    void *io_ctx,
    int is_client);

/**
 * Destroy a yamux session wrapper
 *
 * @param context Session context returned by yamux_wrapper_init
 */
void yamux_wrapper_destroy(void *context);

/**
 * Process yamux protocol messages
 *
 * @param context Session context returned by yamux_wrapper_init
 * @return 0 on success, negative value on error
 */
int yamux_wrapper_process(void *context);

/**
 * Open a new stream
 *
 * @param context Session context returned by yamux_wrapper_init
 * @return Stream handle or NULL on error
 */
void *yamux_wrapper_open_stream(void *context);

/**
 * Accept a new stream (server only)
 *
 * @param context Session context returned by yamux_wrapper_init
 * @return Stream handle or NULL on error
 */
void *yamux_wrapper_accept_stream(void *context);

/**
 * Close a stream
 *
 * @param stream Stream handle returned by yamux_wrapper_open_stream or yamux_wrapper_accept_stream
 * @param reset True to reset the stream, false for normal close
 * @return 0 on success, negative value on error
 */
int yamux_wrapper_close_stream(void *stream, int reset);

/**
 * Read data from a stream
 *
 * @param stream Stream handle returned by yamux_wrapper_open_stream or yamux_wrapper_accept_stream
 * @param buf Buffer to store data
 * @param len Maximum number of bytes to read
 * @return Number of bytes read, 0 on EOF, negative value on error
 */
int yamux_wrapper_read(void *stream, void *buf, int len);

/**
 * Write data to a stream
 *
 * @param stream Stream handle returned by yamux_wrapper_open_stream or yamux_wrapper_accept_stream
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written, negative value on error
 */
int yamux_wrapper_write(void *stream, const void *buf, int len);

/**
 * Get the stream ID
 *
 * @param stream Stream handle returned by yamux_wrapper_open_stream or yamux_wrapper_accept_stream
 * @return Stream ID or 0 on error
 */
unsigned int yamux_wrapper_get_stream_id(void *stream);

/**
 * Ping remote endpoint
 *
 * @param context Session context returned by yamux_wrapper_init
 * @return 0 on success, negative value on error
 */
int yamux_wrapper_ping(void *context);

#ifdef __cplusplus
}
#endif

#endif // YAMUX_WRAPPER_H
