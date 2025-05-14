/**
 * @file yamux_handlers.c
 * @brief Implementation of yamux frame handlers
 */

#include "../include/yamux.h"
#include "yamux_internal.h"
#include "yamux_defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Handle a DATA frame
 * 
 * @param session Session context
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_data(yamux_session_t *session, const yamux_header_t *header) {
    yamux_stream_t *stream;
    yamux_result_t result;
    int bytes_read;
    
    /* Validate session and header */
    if (!session || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Find the stream */
    stream = yamux_get_stream(session, header->stream_id);
    if (!stream) {
        return YAMUX_ERR_INVALID_STREAM;
    }
    
    /* Check if the stream is readable */
    if (stream->state == YAMUX_STREAM_CLOSED || 
        stream->state == YAMUX_STREAM_FIN_RECV) {
        return YAMUX_ERR_CLOSED;
    }
    
    /* Check for FIN flag */
    if (header->flags & YAMUX_FLAG_FIN) {
        if (stream->state == YAMUX_STREAM_ESTABLISHED) {
            stream->state = YAMUX_STREAM_FIN_RECV;
        } else if (stream->state == YAMUX_STREAM_FIN_SENT) {
            stream->state = YAMUX_STREAM_CLOSED;
        }
    }
    
    /* If there's no data, we're done */
    if (header->length == 0) {
        return YAMUX_OK;
    }
    
    /* Allocate a buffer for the data if needed */
    if (header->length > session->recv_buf_size) {
        uint8_t *new_buf = realloc(session->recv_buf, header->length);
        if (!new_buf) {
            return YAMUX_ERR_NOMEM;
        }
        session->recv_buf = new_buf;
        session->recv_buf_size = header->length;
    }
    
    /* Read the data from the connection */
    bytes_read = session->io.read(session->io.ctx, session->recv_buf, header->length);
    if (bytes_read < 0) {
        return YAMUX_ERR_IO;
    }
    
    /* Check if we got all the data */
    if ((size_t)bytes_read < header->length) {
        /* Partial read, not enough data */
        return YAMUX_ERR_IO;
    }
    
    /* Write the data to the stream's receive buffer */
    result = yamux_buffer_write(&stream->recvbuf, session->recv_buf, bytes_read);
    if (result != YAMUX_OK) {
        return result;
    }
    
    /* Update the receive window */
    stream->recv_window -= bytes_read;
    
    /* Send a window update if needed */
    if (stream->recv_window < YAMUX_WINDOW_UPDATE_THRESHOLD) {
        /* Implement window update inline since the function might not be defined yet */
        yamux_header_t update;
        uint8_t frame[12];  /* 8-byte header + 4-byte window size */
        yamux_session_t *sess = stream->session;
        
        /* Create window update frame */
        memset(&update, 0, sizeof(update));
        update.version = YAMUX_PROTO_VERSION;
        update.type = YAMUX_WINDOW_UPDATE;
        update.flags = 0;
        update.stream_id = stream->id;
        update.length = 4;  /* Window update is a 32-bit value */
        
        /* Encode header */
        yamux_encode_header(&update, frame);
        
        /* Encode window update (big-endian) */
        frame[8] = (YAMUX_DEFAULT_WINDOW_SIZE >> 24) & 0xFF;
        frame[9] = (YAMUX_DEFAULT_WINDOW_SIZE >> 16) & 0xFF;
        frame[10] = (YAMUX_DEFAULT_WINDOW_SIZE >> 8) & 0xFF;
        frame[11] = YAMUX_DEFAULT_WINDOW_SIZE & 0xFF;
        
        /* Send frame */
        if (sess->io.write(sess->io.ctx, frame, sizeof(frame)) != sizeof(frame)) {
            return YAMUX_ERR_IO;
        }
        
        /* Update the window */
        stream->recv_window = YAMUX_DEFAULT_WINDOW_SIZE;
    }
    
    return YAMUX_OK;
}

/**
 * Handle a WINDOW_UPDATE frame
 * 
 * @param session Session context
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_window_update(yamux_session_t *session, const yamux_header_t *header) {
    printf("DEBUG (yamux_handle_window_update): Handling WINDOW_UPDATE for stream %u, flags: 0x%x, length: %u\n", header->stream_id, header->flags, header->length);

    if (header->length != 4) {
        printf("ERROR (yamux_handle_window_update): Invalid header length %u for WINDOW_UPDATE, expected 4.\n", header->length);
        // Optionally send GO_AWAY or RST stream
        return YAMUX_ERR_PROTOCOL;
    }

    uint8_t payload_buf[4];
    int read_len = session->io.read(session->io.ctx, payload_buf, 4);
    if (read_len != 4) {
        printf("ERROR (yamux_handle_window_update): Failed to read 4-byte payload for WINDOW_UPDATE. Read %d bytes.\n", read_len);
        return YAMUX_ERR_IO;
    }
    uint32_t window_val_payload;
    memcpy(&window_val_payload, payload_buf, sizeof(uint32_t));
    window_val_payload = ntohl(window_val_payload);
    printf("DEBUG (yamux_handle_window_update): Read payload_window_value: %u\n", window_val_payload);

    yamux_stream_t *stream = yamux_get_stream(session, header->stream_id);

    if (header->flags & YAMUX_FLAG_SYN) {
        printf("DEBUG (yamux_handle_window_update): SYN flag set.\n");
        if (!session->client) { // Server side: received SYN from client
            printf("DEBUG (yamux_handle_window_update): Server received SYN for stream %u\n", header->stream_id);
            if (stream) {
                printf("ERROR (yamux_handle_window_update): Stream %u already exists on server.\n", header->stream_id);
                // This might be a duplicate SYN, could RST or ignore.
                return YAMUX_ERR_PROTOCOL; 
            }

            // Create a new stream structure for the incoming client stream
            stream = (yamux_stream_t *)malloc(sizeof(yamux_stream_t));
            if (!stream) return YAMUX_ERR_NOMEM;
            memset(stream, 0, sizeof(yamux_stream_t));

            stream->session = session;
            stream->id = header->stream_id;
            stream->state = YAMUX_STREAM_SYN_RECV;
            stream->send_window = window_val_payload; // Client's initial recv_window is our initial send_window
            stream->recv_window = session->config.max_stream_window_size; // Our initial recv_window for the client

            printf("DEBUG (yamux_handle_window_update): New stream %u created (server). send_window: %u, recv_window: %u\n", 
                   stream->id, stream->send_window, stream->recv_window);

            if (yamux_buffer_init(&stream->recvbuf, YAMUX_INITIAL_BUFFER_SIZE) != YAMUX_OK) {
                free(stream);
                return YAMUX_ERR_NOMEM;
            }
            if (yamux_add_stream(session, stream) != YAMUX_OK) {
                yamux_buffer_free(&stream->recvbuf);
                free(stream);
                return YAMUX_ERR_INTERNAL;
            }

            // Send SYN-ACK back to client
            yamux_header_t resp_header;
            resp_header.version = YAMUX_PROTO_VERSION;
            resp_header.type = YAMUX_WINDOW_UPDATE;
            resp_header.flags = YAMUX_FLAG_SYN | YAMUX_FLAG_ACK;
            resp_header.stream_id = stream->id;
            resp_header.length = 4; // Payload is our initial window size (4 bytes)

            uint8_t frame_buf[YAMUX_HEADER_SIZE + 4];
            yamux_encode_header(&resp_header, frame_buf);
            
            uint32_t net_recv_window = htonl(stream->recv_window);
            memcpy(frame_buf + YAMUX_HEADER_SIZE, &net_recv_window, sizeof(net_recv_window));

            printf("DEBUG (yamux_handle_window_update): Server sending SYN-ACK for stream %u, payload_window: %u\n", stream->id, stream->recv_window);
            if (session->io.write(session->io.ctx, frame_buf, sizeof(frame_buf)) != sizeof(frame_buf)) {
                printf("ERROR (yamux_handle_window_update): io.write failed for SYN-ACK\n");
                // Error sending SYN-ACK, cleanup stream?
                yamux_remove_stream(session, stream->id); // This will free buffer and stream
                return YAMUX_ERR_IO;
            }
            stream->state = YAMUX_STREAM_ESTABLISHED; // Server considers it established after sending SYN-ACK
            printf("DEBUG (yamux_handle_window_update): Server stream %u ESTABLISHED after sending SYN-ACK.\n", stream->id);

            // Enqueue for accept by application if not already handled by a direct accept call
            // This logic might need refinement based on how yamux_accept_stream is used
            if (yamux_enqueue_stream_for_accept(session, stream) != YAMUX_OK) {
                // Handle error if necessary, though unlikely here
            }

        } else { // Client side: This case should not happen if SYN is only sent by client opening stream
                 // However, if it's a SYN-ACK (SYN|ACK), it will be handled below.
            printf("WARN (yamux_handle_window_update): Client received a frame with only SYN flag. This is unexpected.\n");
        }
    }

    // Handle ACK flag (part of SYN-ACK for client, or standalone ACK for other purposes if defined)
    if (header->flags & YAMUX_FLAG_ACK) {
        printf("DEBUG (yamux_handle_window_update): ACK flag set.\n");
        if (stream) {
            if (session->client && stream->state == YAMUX_STREAM_SYN_SENT && (header->flags & YAMUX_FLAG_SYN)) { // Client received SYN-ACK
                printf("DEBUG (yamux_handle_window_update): Client received SYN-ACK for stream %u\n", stream->id);
                stream->send_window = window_val_payload; // Server's initial recv_window is our send_window
                stream->state = YAMUX_STREAM_ESTABLISHED;
                printf("DEBUG (yamux_handle_window_update): Client stream %u ESTABLISHED. send_window updated to %u.\n", stream->id, stream->send_window);
            } else if (stream->state == YAMUX_STREAM_FIN_SENT && (header->flags & YAMUX_FLAG_FIN)) {
                 // Handle FIN-ACK for stream closing
                 printf("DEBUG (yamux_handle_window_update): FIN-ACK received for stream %u. Changing state to CLOSED.\n", stream->id);
                 stream->state = YAMUX_STREAM_CLOSED;
                 // yamux_remove_stream might be called later by a cleanup task or when all data is read
            } else {
                // Other ACK scenarios, if any (e.g., ACK for data, though Yamux doesn't use explicit data ACKs like TCP)
                printf("DEBUG (yamux_handle_window_update): Received ACK for stream %u in state %d. No specific action taken.\n", stream->id, stream->state);
            }
        } else {
            printf("WARN (yamux_handle_window_update): Received ACK for non-existent stream %u\n", header->stream_id);
            // Potentially send RST if an ACK is for an unknown stream
        }
    }

    // Handle regular window update (no SYN, no ACK or ACK already processed)
    // This is when the remote party is granting more send window to us.
    if (!(header->flags & YAMUX_FLAG_SYN) && !(header->flags & YAMUX_FLAG_ACK)) {
        if (stream) {
            stream->send_window += window_val_payload;
            printf("DEBUG (yamux_handle_window_update): Stream %u send_window increased by %u to %u\n", 
                   stream->id, window_val_payload, stream->send_window);
        } else {
            printf("WARN (yamux_handle_window_update): Window update for non-existent stream %u\n", header->stream_id);
            // Potentially send RST
        }
    }
    
    // If FIN flag is set (and not part of SYN-ACK or other combined flags handled above)
    if (header->flags & YAMUX_FLAG_FIN && !(header->flags & YAMUX_FLAG_ACK) && !(header->flags & YAMUX_FLAG_SYN)) {
        printf("DEBUG (yamux_handle_window_update): FIN flag set (standalone).\n");
        if (stream) {
            stream->state = YAMUX_STREAM_FIN_RECV;
            printf("DEBUG (yamux_handle_window_update): Stream %u received FIN. State changed to FIN_RECV.\n", stream->id);
            // Application should see EOF on read. Send FIN-ACK back.
            yamux_header_t resp_header;
            resp_header.version = YAMUX_PROTO_VERSION;
            resp_header.type = YAMUX_WINDOW_UPDATE; // Or PING type with FIN|ACK as per some implementations
            resp_header.flags = YAMUX_FLAG_FIN | YAMUX_FLAG_ACK;
            resp_header.stream_id = stream->id;
            resp_header.length = 0; // FIN-ACK typically has no payload

            uint8_t frame_buf[YAMUX_HEADER_SIZE];
            yamux_encode_header(&resp_header, frame_buf);

            printf("DEBUG (yamux_handle_window_update): Sending FIN-ACK for stream %u\n", stream->id);
            if (session->io.write(session->io.ctx, frame_buf, sizeof(frame_buf)) != sizeof(frame_buf)) {
                printf("ERROR (yamux_handle_window_update): io.write failed for FIN-ACK\n");
                return YAMUX_ERR_IO;
            }
            // If we also sent FIN previously, and now received FIN, then can move to CLOSED.
            // This part of state machine needs careful review with yamux_close behavior.
        } else {
            printf("WARN (yamux_handle_window_update): FIN for non-existent stream %u\n", header->stream_id);
        }
    }

    // Handle RST flag
    if (header->flags & YAMUX_FLAG_RST) {
        printf("DEBUG (yamux_handle_window_update): RST flag set.\n");
        if (stream) {
            printf("DEBUG (yamux_handle_window_update): Stream %u received RST. Closing stream.\n", stream->id);
            stream->state = YAMUX_STREAM_CLOSED; // Or a specific RST state
            // Notify application, cleanup stream resources.
            // Consider calling yamux_remove_stream or marking for removal.
            // For now, just change state. The test might not cover RST fully.
            yamux_remove_stream(session, stream->id); // Proactively remove and free
        } else {
            printf("WARN (yamux_handle_window_update): RST for non-existent stream %u\n", header->stream_id);
        }
    }

    return YAMUX_OK;
}

/**
 * Handle a PING frame
 * 
 * @param session Session context
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_ping(yamux_session_t *session, const yamux_header_t *header) {
    uint8_t ping_data[8] = {0};
    int bytes_read;
    
    /* Validate session and header */
    if (!session || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if it's a ping request or response */
    if (header->flags & YAMUX_FLAG_ACK) {
        /* Ping response, nothing to do */
        return YAMUX_OK;
    }
    
    /* Ping request, read the data if any */
    if (header->length > 0) {
        bytes_read = session->io.read(session->io.ctx, ping_data, (header->length < 8) ? header->length : 8);
        if (bytes_read < 0) {
            return YAMUX_ERR_IO;
        }
    }
    
    /* Send a ping response */
    yamux_header_t response = {
        .version = YAMUX_PROTO_VERSION,
        .type = YAMUX_PING,
        .flags = YAMUX_FLAG_ACK,
        .stream_id = 0,
        .length = header->length
    };
    
    /* Encode the header */
    yamux_encode_header(&response, session->recv_buf);
    
    /* Send the header */
    if (session->io.write(session->io.ctx, session->recv_buf, 8) != 8) {
        return YAMUX_ERR_IO;
    }
    
    /* Send the ping data if any */
    if (header->length > 0) {
        ssize_t written = session->io.write(session->io.ctx, ping_data, header->length);
        if (written < 0 || (size_t)written != header->length) {
            return YAMUX_ERR_IO;
        }
    }
    
    return YAMUX_OK;
}

/**
 * Handle a GO_AWAY frame
 * 
 * @param session Session context
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_go_away(yamux_session_t *session, const yamux_header_t *header) {
    int bytes_read;
    /* Parse reason code but we don't need to use it */
    
    /* Validate session and header */
    if (!session || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check frame size */
    if (header->length != 4) {
        return YAMUX_ERR_PROTOCOL;
    }
    
    /* Read the reason */
    bytes_read = session->io.read(session->io.ctx, session->recv_buf, 4);
    if (bytes_read < 0) {
        return YAMUX_ERR_IO;
    }
    
    /* Check if we got all the data */
    if (bytes_read < 4) {
        /* Partial read, not enough data */
        return YAMUX_ERR_IO;
    }
    
    /* Parse the reason (big-endian), but we don't need to store it
     * We just need to read the reason bytes from the socket */
    
    /* Mark the session as going away */
    session->go_away_received = 1;
    
    return YAMUX_OK;
}
