/**
 * @file yamux_session.c
 * @brief Implementation of the yamux session functionality
 */

#include "../include/yamux.h"
#include "yamux_internal.h"
#include "yamux_defs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Default configuration values */
const yamux_config_t yamux_default_config = {
    .accept_backlog = 256,
    .enable_keepalive = 1,
    .connection_write_timeout = 30000, /* 30 seconds */
    .keepalive_interval = 60000,      /* 60 seconds */
    .max_stream_window_size = 256 * 1024  /* 256 KB */
};

/* Add some fields to the session structure that weren't in yamux_internal.h */
static int yamux_session_is_shutdown(yamux_session_t *session) {
    return session->go_away_received;
}

static void yamux_session_set_shutdown(yamux_session_t *session, yamux_error_t reason) {
    (void)reason; /* Unused parameter */
    session->go_away_received = 1;
}

/* Initialize a new session */
yamux_result_t yamux_session_create(
    yamux_io_t *io, 
    int client, 
    const yamux_config_t *config, 
    yamux_session_t **session)
{
    yamux_session_t *s;
    
    /* Validate parameters */
    if (!io || !session) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Allocate session structure */
    s = (yamux_session_t *)malloc(sizeof(yamux_session_t));
    if (!s) {
        return YAMUX_ERR_NOMEM;
    }
    
    /* Initialize session */
    memset(s, 0, sizeof(yamux_session_t));
    
    /* Copy I/O callbacks */
    s->io = *io;
    
    /* Set client mode */
    s->client = client;
    
    /* Set configuration */
    if (config) {
        s->config = *config;
    } else {
        s->config = yamux_default_config;
    }
    
    /* Initialize stream ID based on client/server mode */
    /* Client uses odd IDs, server uses even IDs */
    s->next_stream_id = client ? 1 : 2;
    
    /* Initialize streams array */
    s->stream_capacity = 16;  /* Initial capacity */
    s->streams = (yamux_stream_t **)malloc(s->stream_capacity * sizeof(yamux_stream_t *));
    if (!s->streams) {
        free(s);
        return YAMUX_ERR_NOMEM;
    }
    
    /* Initialize accept queue */
    s->accept_queue = NULL;
    
    /* Set session pointer */
    *session = s;
    
    return YAMUX_OK;
}

/* Close a session */
yamux_result_t yamux_session_close(
    yamux_session_t *session, 
    yamux_error_t err)
{
    size_t i;
    
    /* Validate parameters */
    if (!session) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if already shut down */
    if (yamux_session_is_shutdown(session)) {
        return YAMUX_OK;
    }
    
    /* Mark as shut down */
    yamux_session_set_shutdown(session, err);
    
    /* Send GoAway frame if possible */
    yamux_header_t header;
    memset(&header, 0, sizeof(header));
    header.version = YAMUX_PROTO_VERSION;
    header.type = YAMUX_GO_AWAY;
    header.flags = 0;
    header.stream_id = 0;
    header.length = 4;  /* Error code is a 32-bit value */
    
    uint8_t frame[12];  /* 8-byte header + 4-byte error code */
    yamux_encode_header(&header, frame);
    
    /* Encode error code (big-endian) */
    frame[8] = (err >> 24) & 0xFF;
    frame[9] = (err >> 16) & 0xFF;
    frame[10] = (err >> 8) & 0xFF;
    frame[11] = err & 0xFF;
    
    /* Send frame (ignore errors, we're shutting down anyway) */
    session->io.write(session->io.ctx, frame, sizeof(frame));
    
    /* Close all streams */
    for (i = 0; i < session->stream_count; i++) {
        if (session->streams[i]) {
            yamux_stream_close(session->streams[i], 1);
        }
    }
    
    /* Free streams array */
    free(session->streams);
    session->streams = NULL;
    session->stream_count = 0;
    session->stream_capacity = 0;
    
    return YAMUX_OK;
}

/* Process incoming data */
yamux_result_t yamux_session_process(
    yamux_session_t *session)
{
    fprintf(stderr, "\n*** ULTRA DEBUG: ENTERING yamux_session_process - VERSION CHECKPOINT 05-15-A ***\n\n");
    fflush(stderr);
    uint8_t header_buf[YAMUX_HEADER_SIZE]; /* 12 bytes for header */
    yamux_header_t header;
    yamux_result_t result;
    
    // ---- ADDED DEBUG ----
    printf("DEBUG: INSIDE yamux_session_process: received session ptr = %p\n", (void*)session);
    if (session) {
        printf("DEBUG: INSIDE yamux_session_process: session->go_away_received = %d\n", session->go_away_received);
    }
    fflush(stdout);
    // ---- END ADDED DEBUG ----

    /* Validate parameters */
    if (!session) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if shut down */
    if (session->go_away_received) {
        return YAMUX_ERR_CLOSED;
    }
    
    /* Read header - only read YAMUX_HEADER_SIZE bytes for the actual header */
    int read_result = session->io.read(session->io.ctx, header_buf, YAMUX_HEADER_SIZE);
    printf("DEBUG: Header read result: %d, expected: %d\n", read_result, YAMUX_HEADER_SIZE);
    if (read_result != YAMUX_HEADER_SIZE) {
        fprintf(stderr, "DEBUG (yamux_session_process): Header read failed/short (read_result=%d), returning YAMUX_ERR_IO (%d)\n", read_result, YAMUX_ERR_IO);
        fflush(stderr);
        return YAMUX_ERR_IO;
    }
    
    /* Decode header */
    fprintf(stderr, "DEBUG: --- PRE-HEX DUMP MARKER ---\n");
    fflush(stderr);
    fprintf(stderr, "DEBUG: Raw header_buf (%d bytes): ", read_result);
    for (int i = 0; i < read_result; i++) {
        fprintf(stderr, "%02x ", header_buf[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "DEBUG: --- POST-HEX DUMP MARKER ---\n");
    fflush(stderr);

    fprintf(stderr, "MANUAL CHECK: header_buf[0] (Version) = %u | header_buf[1] (Type) = %u\n", header_buf[0], header_buf[1]);
    fflush(stderr);

    /* Decode header */
    result = yamux_decode_header(header_buf, YAMUX_HEADER_SIZE, &header);
    fprintf(stderr, "DEBUG: Decode header result: %d, frame type: %d, flags: 0x%x, stream_id: %u, length: %u\n", 
           result, header.type, header.flags, header.stream_id, header.length);
    fflush(stderr);
    if (result != YAMUX_OK) {
        return result;
    }
    
    /* Process frame based on type */
    printf("DEBUG: Processing frame type: %d\n", header.type);
    switch (header.type) {
        case YAMUX_DATA:
            printf("DEBUG: Handling DATA frame\n");
            result = yamux_handle_data(session, &header);
            printf("DEBUG: DATA frame result: %d\n", result);
            break;
        case YAMUX_WINDOW_UPDATE:
            printf("DEBUG: Handling WINDOW_UPDATE frame\n");
            result = yamux_handle_window_update(session, &header);
            printf("DEBUG: WINDOW_UPDATE frame result: %d\n", result);
            break;
        case YAMUX_PING:
            printf("DEBUG: Handling PING frame\n");
            result = yamux_handle_ping(session, &header);
            printf("DEBUG: PING frame result: %d\n", result);
            break;
        case YAMUX_GO_AWAY:
            printf("DEBUG: Handling GO_AWAY frame\n");
            result = yamux_handle_go_away(session, &header);
            printf("DEBUG: GO_AWAY frame result: %d\n", result);
            break;
        default:
            /* Invalid frame type */
            printf("DEBUG: Invalid frame type: %d\n", header.type);
            return YAMUX_ERR_PROTOCOL;
    }
    
    return result;
}

/* Ping the remote endpoint */
yamux_result_t yamux_session_ping(
    yamux_session_t *session)
{
    yamux_header_t header;
    uint8_t frame[8];
    
    /* Validate parameters */
    if (!session) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if shut down */
    if (session->go_away_received) {
        return YAMUX_ERR_CLOSED;
    }
    
    /* Create ping frame */
    memset(&header, 0, sizeof(header));
    header.version = YAMUX_PROTO_VERSION;
    header.type = YAMUX_PING;
    header.flags = YAMUX_FLAG_SYN;  /* SYN indicates request, ACK indicates response */
    header.stream_id = 0;
    header.length = 0;
    
    /* Encode header */
    yamux_encode_header(&header, frame);
    
    /* Send frame */
    if (session->io.write(session->io.ctx, frame, sizeof(frame)) != sizeof(frame)) {
        return YAMUX_ERR_IO;
    }
    
    return YAMUX_OK;
}

/* 
 * Note: The actual implementations for these functions are now in yamux_frame.c and yamux_handlers.c
 * These are just declarations to satisfy external references
 */
extern yamux_result_t yamux_encode_header(const yamux_header_t *header, uint8_t *buffer);
extern yamux_result_t yamux_decode_header(const uint8_t *buffer, size_t buffer_len, yamux_header_t *header);
extern yamux_result_t yamux_handle_data(yamux_session_t *session, const yamux_header_t *header);
extern yamux_result_t yamux_handle_window_update(yamux_session_t *session, const yamux_header_t *header);
extern yamux_result_t yamux_handle_ping(yamux_session_t *session, const yamux_header_t *header);
extern yamux_result_t yamux_handle_go_away(yamux_session_t *session, const yamux_header_t *header);
