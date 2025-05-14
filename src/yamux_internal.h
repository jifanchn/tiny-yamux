/**
 * @file yamux_internal.h
 * @brief Internal definitions for yamux implementation
 */

#ifndef YAMUX_INTERNAL_H
#define YAMUX_INTERNAL_H

// #include "../include/yamux.h" // Removed to avoid opaque type conflict
#include "yamux_defs.h"

/* Forward declarations */
struct yamux_stream;

/* Session structure */
struct yamux_session {
    yamux_io_t io;                  /* I/O callbacks */
    int client;                     /* 1 if client mode, 0 if server mode */
    
    uint32_t next_stream_id;        /* Next stream ID to use */
    uint32_t remote_window;         /* Remote receive window size */
    uint32_t go_away_received;      /* Whether go away has been received */
    
    yamux_stream_t **streams;       /* Array of active streams */
    size_t stream_count;            /* Number of active streams */
    size_t stream_capacity;         /* Capacity of streams array */
    
    yamux_stream_t *accept_queue;   /* Queue of streams pending accept */
    
    yamux_config_t config;          /* Session configuration */
    uint32_t last_ping_id;          /* ID of the last ping sent */
    int keepalive_enabled;          /* Whether keepalive is enabled */
    uint32_t keepalive_interval;    /* Keepalive interval in milliseconds */
    
    uint8_t *recv_buf;              /* Temporary receive buffer */
    size_t recv_buf_size;           /* Size of receive buffer */
};

/* Yamux context structure (exposed via opaque pointer in public API) */
/* This is the actual definition needed by yamux_port.c and tests */
typedef struct yamux_context {
    yamux_session_t *session;     /* Internal Yamux session */
    yamux_io_t io;                /* I/O callbacks */
    int is_client;                /* Client or server mode */
    yamux_config_t config;        /* Configuration */
} yamux_context_t;

/* Stream structure */

/* Use stream state from yamux_defs.h */

/* Buffer structure */
typedef struct {
    uint8_t *data;                /* Buffer data */
    size_t size;                  /* Total size of the buffer */
    size_t used;                  /* Used bytes in the buffer */
    size_t pos;                   /* Current read position */
} yamux_buffer_t;

/* Stream structure */
struct yamux_stream {
    struct yamux_session *session;  /* Parent session */
    uint32_t id;                    /* Stream ID */
    yamux_stream_state_t state;     /* Stream state */
    
    yamux_buffer_t recvbuf;        /* Receive buffer */
    uint32_t send_window;          /* Send window size */
    uint32_t recv_window;          /* Receive window size */
    
    struct yamux_stream *next;     /* Next stream in accept queue */
};

/* Frame encoding/decoding functions */
yamux_result_t yamux_encode_header(const yamux_header_t *header, uint8_t *buffer);
yamux_result_t yamux_decode_header(const uint8_t *buffer, size_t buffer_len, yamux_header_t *header);

/* Frame handling functions */
yamux_result_t yamux_handle_data(struct yamux_session *session, const yamux_header_t *header);
yamux_result_t yamux_handle_window_update(struct yamux_session *session, const yamux_header_t *header);
yamux_result_t yamux_handle_ping(struct yamux_session *session, const yamux_header_t *header);
yamux_result_t yamux_handle_go_away(struct yamux_session *session, const yamux_header_t *header);

/* Core session processing function */
yamux_result_t yamux_session_process(yamux_session_t *session);

/* Stream management functions */
yamux_stream_t *yamux_get_stream(struct yamux_session *session, uint32_t stream_id);
yamux_result_t yamux_add_stream(struct yamux_session *session, yamux_stream_t *stream);
yamux_result_t yamux_remove_stream(struct yamux_session *session, uint32_t stream_id);
yamux_result_t yamux_enqueue_stream(struct yamux_session *session, yamux_stream_t *stream);
yamux_result_t yamux_enqueue_stream_for_accept(struct yamux_session *session, yamux_stream_t *stream);

/* Buffer management functions */
yamux_result_t yamux_buffer_init(yamux_buffer_t *buffer, size_t initial_size);
void yamux_buffer_free(yamux_buffer_t *buffer);
yamux_result_t yamux_buffer_write(yamux_buffer_t *buffer, const uint8_t *data, size_t len);
yamux_result_t yamux_buffer_read(yamux_buffer_t *buffer, uint8_t *data, size_t len, size_t *bytes_read);
yamux_result_t yamux_buffer_compact(yamux_buffer_t *buffer);

#endif /* YAMUX_INTERNAL_H */
