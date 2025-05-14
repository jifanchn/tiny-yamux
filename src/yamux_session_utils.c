/**
 * @file yamux_session_utils.c
 * @brief Utility functions for yamux session management
 */

#include "../include/yamux.h"
#include "yamux_internal.h"
#include "yamux_defs.h"
#include <stdlib.h>
#include <string.h>

/**
 * Handle ping frame
 *
 * @param session Session
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_ping(
    yamux_session_t *session, 
    const yamux_header_t *header)
{
    yamux_header_t response;
    uint8_t frame[8];
    
    if (!session || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if it's a ping request (SYN) or response (ACK) */
    if (header->flags & YAMUX_FLAG_SYN) {
        /* This is a ping request, send a response */
        memset(&response, 0, sizeof(response));
        response.version = YAMUX_PROTO_VERSION;
        response.type = YAMUX_PING;
        response.flags = YAMUX_FLAG_ACK;  /* ACK flag for response */
        response.stream_id = 0;
        response.length = 0;
        
        /* Encode header */
        yamux_encode_header(&response, frame);
        
        /* Send frame */
        if (session->io.write(session->io.ctx, frame, sizeof(frame)) != sizeof(frame)) {
            return YAMUX_ERR_IO;
        }
    }
    
    /* For ping response (ACK), do nothing */
    
    return YAMUX_OK;
}

/**
 * Handle go-away frame
 *
 * @param session Session
 * @param header Frame header
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_handle_go_away(
    yamux_session_t *session, 
    const yamux_header_t *header)
{
    uint8_t data[4];
    
    if (!session || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Read reason code */
    if (header->length != 4) {
        /* Go-away reason must be 4 bytes */
        return YAMUX_ERR_PROTOCOL;
    }
    
    /* Read reason code */
    if (session->io.read(session->io.ctx, data, sizeof(data)) != sizeof(data)) {
        return YAMUX_ERR_IO;
    }
    
    /* Decode reason (big-endian) - we don't use it currently */
    /* yamux_error_t err = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]; */
    
    /* Mark session as shut down */
    session->go_away_received = 1;
    
    return YAMUX_OK;
}
