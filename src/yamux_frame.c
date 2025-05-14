/**
 * @file yamux_frame.c
 * @brief Implementation of yamux frame encoding and decoding
 */

#include "../include/yamux.h"
#include "yamux_internal.h"

/**
 * Encode a yamux header into a binary buffer
 * 
 * Header format (12 bytes):
 * +---------------------------------------------------------------+
 * | Version(8) | Type(8) | Flags(16) | StreamID(32) | Length(32) |
 * +---------------------------------------------------------------+
 * 
 * @param header Header structure
 * @param buffer Output buffer (must be at least 12 bytes)
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_encode_header(const yamux_header_t *header, uint8_t *buffer)
{
    if (!header || !buffer) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Version */
    buffer[0] = header->version;
    
    /* Type */
    buffer[1] = header->type;
    
    /* Flags (big-endian) */
    buffer[2] = (header->flags >> 8) & 0xFF;
    buffer[3] = header->flags & 0xFF;
    
    /* Stream ID (big-endian) */
    buffer[4] = (header->stream_id >> 24) & 0xFF;
    buffer[5] = (header->stream_id >> 16) & 0xFF;
    buffer[6] = (header->stream_id >> 8) & 0xFF;
    buffer[7] = header->stream_id & 0xFF;
    
    /* Length (stored after the header in the following 4 bytes) */
    buffer[8] = (header->length >> 24) & 0xFF;
    buffer[9] = (header->length >> 16) & 0xFF;
    buffer[10] = (header->length >> 8) & 0xFF;
    buffer[11] = header->length & 0xFF;
    
    return YAMUX_OK;
}

/**
 * Decode a binary buffer into a yamux header
 * 
 * @param buffer Input buffer (must be at least 12 bytes)
 * @param buffer_len Length of the input buffer
 * @param header Output header structure
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_decode_header(const uint8_t *buffer, size_t buffer_len, yamux_header_t *header)
{
    if (!buffer || !header) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check buffer size - actual header is 8 bytes, not 12 */
    if (buffer_len < 8) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Version */
    header->version = buffer[0];
    
    /* Check version */
    if (header->version != YAMUX_PROTO_VERSION) {
        return YAMUX_ERR_PROTOCOL;
    }
    
    /* Type */
    header->type = buffer[1];
    
    /* Check type */
    if (header->type > YAMUX_GO_AWAY) {
        return YAMUX_ERR_PROTOCOL;
    }
    
    /* Flags (big-endian) */
    header->flags = (buffer[2] << 8) | buffer[3];
    
    /* Stream ID (big-endian) */
    header->stream_id = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
    
    /* Length (big-endian) */
    header->length = (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11];
    
    return YAMUX_OK;
}
