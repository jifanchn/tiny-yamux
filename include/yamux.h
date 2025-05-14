/**
 * @file yamux.h
 * @brief Yamux (Yet another Multiplexer) protocol implementation in C
 * 
 * This is a portable C implementation of the yamux protocol, based on
 * https://github.com/fatedier/yamux
 * 
 * The implementation is designed to be portable to embedded systems,
 * avoiding non-standard functions and providing a minimal API surface.
 */

#ifndef TINY_YAMUX_H
#define TINY_YAMUX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Protocol version
 */
#define YAMUX_PROTO_VERSION 0

/**
 * Frame types
 */
typedef enum {
    YAMUX_DATA          = 0x0,
    YAMUX_WINDOW_UPDATE = 0x1,
    YAMUX_PING          = 0x2,
    YAMUX_GO_AWAY       = 0x3
} yamux_type_t;

/**
 * Frame flags
 */
typedef enum {
    YAMUX_FLAG_NONE     = 0x0,
    YAMUX_FLAG_SYN      = 0x1,
    YAMUX_FLAG_ACK      = 0x2,
    YAMUX_FLAG_FIN      = 0x4,
    YAMUX_FLAG_RST      = 0x8
} yamux_flags_t;

/**
 * Go away errors
 */
typedef enum {
    YAMUX_NORMAL           = 0x0,
    YAMUX_PROTOCOL_ERROR   = 0x1,
    YAMUX_INTERNAL_ERROR   = 0x2
} yamux_error_t;

/**
 * Header structure
 */
typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t flags;
    uint32_t stream_id;
    uint32_t length;
} yamux_header_t;

/**
 * Configuration structure
 */
typedef struct {
    uint32_t accept_backlog;
    uint32_t enable_keepalive;
    uint32_t connection_write_timeout;
    uint32_t keepalive_interval;
    uint32_t max_stream_window_size;
} yamux_config_t;

/**
 * Session structure (opaque)
 */
typedef struct yamux_session yamux_session_t;

/**
 * Stream structure (opaque)
 */
typedef struct yamux_stream yamux_stream_t;

/**
 * Stream states
 */
typedef enum {
    YAMUX_STREAM_IDLE,       /* Initial state */
    YAMUX_STREAM_SYN_SENT,   /* Sent SYN frame */
    YAMUX_STREAM_SYN_RECV,   /* Received SYN frame */
    YAMUX_STREAM_ESTABLISHED, /* Stream established */
    YAMUX_STREAM_FIN_SENT,   /* Sent FIN frame */
    YAMUX_STREAM_FIN_RECV,   /* Received FIN frame */
    YAMUX_STREAM_CLOSED      /* Stream closed */
} yamux_stream_state_t;

/**
 * I/O function callbacks
 * 
 * @note PORTING REQUIRED: These are platform-dependent I/O callbacks that must be implemented
 * for your specific system (e.g., socket, UART, etc.).
 * - read: Should return number of bytes read, 0 for EOF, or -1 for error
 * - write: Should return number of bytes written or -1 for error
 */
typedef struct {
    int (*read)(void *ctx, uint8_t *buf, size_t len);
    int (*write)(void *ctx, const uint8_t *buf, size_t len);
    void *ctx;
} yamux_io_t;

/**
 * Error codes
 */
typedef enum {
    YAMUX_OK                  = 0,
    YAMUX_ERR_INVALID         = -1,
    YAMUX_ERR_NOMEM           = -2,
    YAMUX_ERR_IO              = -3,
    YAMUX_ERR_CLOSED          = -4,
    YAMUX_ERR_TIMEOUT         = -5,
    YAMUX_ERR_PROTOCOL        = -6,
    YAMUX_ERR_INTERNAL        = -7,
    YAMUX_ERR_INVALID_STREAM  = -8,
    YAMUX_ERR_WOULD_BLOCK     = -9
} yamux_result_t;

/**
 * Default configuration
 */
extern const yamux_config_t yamux_default_config;

/**
 * Initialize the Yamux library
 * 
 * @note PREFERRED INITIALIZATION FUNCTION: This is the recommended entry point for new applications.
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
    int is_client
);

/**
 * Destroy a yamux session created with yamux_init
 * 
 * @param session Session handle returned by yamux_init
 */
void yamux_destroy(void *session);

/**
 * Process incoming data for a session
 * 
 * @param session Session handle returned by yamux_init
 * @return 0 on success, negative value on error
 */
int yamux_process(void *session);

/**
 * Create a new yamux session (low-level API)
 * 
 * @note This is a low-level function. New applications should use yamux_init instead.
 * 
 * @param io I/O callbacks
 * @param client True if the session is a client, false if server
 * @param config Configuration or NULL for defaults
 * @param session Output parameter for the created session
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_session_create(
    yamux_io_t *io, 
    int client, 
    const yamux_config_t *config, 
    yamux_session_t **session
);

/**
 * Close a yamux session
 * 
 * @param session Session to close
 * @param err Error code to send (YAMUX_NORMAL for clean shutdown)
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_session_close(
    yamux_session_t *session, 
    yamux_error_t err
);

/**
 * Open a new stream
 * 
 * @param session Session
 * @param stream_id Stream ID (0 for auto-assign)
 * @param stream Output parameter for the created stream
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_open_detailed(
    yamux_session_t *session, 
    uint32_t stream_id, 
    yamux_stream_t **stream
);

/**
 * Accept a new stream (server only)
 * 
 * @param session Session
 * @param stream Output parameter for the accepted stream
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_accept(
    yamux_session_t *session, 
    yamux_stream_t **stream
);

/**
 * Close a stream
 * 
 * @param stream Stream to close
 * @param reset True to reset the stream, false for normal close
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_close(
    yamux_stream_t *stream, 
    int reset
);

/**
 * Read data from a stream
 * 
 * @param stream Stream to read from
 * @param buf Buffer to store data
 * @param len Maximum number of bytes to read
 * @param bytes_read Number of bytes actually read
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_read(
    yamux_stream_t *stream, 
    uint8_t *buf, 
    size_t len, 
    size_t *bytes_read
);

/**
 * Write data to a stream
 * 
 * @param stream Stream to write to
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @param bytes_written Number of bytes actually written
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_write(
    yamux_stream_t *stream, 
    const uint8_t *buf, 
    size_t len, 
    size_t *bytes_written
);

/**
 * Process incoming data
 * 
 * @param session Session
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_session_process(
    yamux_session_t *session
);

/**
 * Get the stream ID
 * 
 * @param stream Stream to get ID for
 * @return Stream ID
 */
uint32_t yamux_stream_get_id(
    yamux_stream_t *stream
);

/**
 * Get the current state of a stream
 *
 * @param stream Stream to query
 * @return The current stream state
 */
yamux_stream_state_t yamux_stream_get_state(
    yamux_stream_t *stream
);

/**
 * Get the current send window size for a stream
 *
 * @param stream Stream to query
 * @return The current send window size
 */
uint32_t yamux_stream_get_send_window(
    yamux_stream_t *stream
);

/**
 * Update the send window for a stream
 *
 * @param stream Stream to update
 * @param increment Amount to increment the window by
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_update_window(
    yamux_stream_t *stream, 
    uint32_t increment
);

/**
 * Ping the remote endpoint
 * 
 * @param session Session
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_session_ping(
    yamux_session_t *session
);

/*
 * ----- High-level stream API (for use with yamux_init) -----
 */

/**
 * Open a new stream
 * 
 * @param session Session handle returned by yamux_init
 * @return Stream handle, or NULL on error
 */
void* yamux_open_stream(void *session);

/**
 * Accept a new incoming stream (server only)
 * 
 * @param session Session handle returned by yamux_init
 * @return Stream handle, or NULL if no pending streams
 */
void* yamux_accept_stream(void *session);

/**
 * Close a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param reset True to forcibly reset the stream, false for normal close
 * @return 0 on success, negative value on error
 */
int yamux_close_stream(void *stream, int reset);

/**
 * Read data from a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param buf Buffer to read data into
 * @param len Maximum number of bytes to read
 * @return Number of bytes read, 0 on EOF, or negative value on error
 */
int yamux_read(void *stream, uint8_t *buf, size_t len);

/**
 * Write data to a stream
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written, or negative value on error
 */
int yamux_write(void *stream, const uint8_t *buf, size_t len);

/**
 * Get the stream ID
 * 
 * @param stream Stream handle returned by yamux_open_stream or yamux_accept_stream
 * @return Stream ID or 0 on error
 */
uint32_t yamux_get_stream_id(void *stream);

/**
 * Send a ping to the remote endpoint
 * 
 * @param session Session handle returned by yamux_init
 * @return 0 on success, negative value on error
 */
int yamux_ping(void *session);

#ifdef __cplusplus
}
#endif

#endif /* TINY_YAMUX_H */
