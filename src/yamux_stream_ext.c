/**
 * @file yamux_stream_ext.c
 * @brief Additional stream utility functions for Yamux
 */

#include "../include/yamux.h"
#include "yamux_internal.h"

/**
 * Get the current send window size for a stream
 *
 * @param stream Stream to query
 * @return The current send window size
 */
uint32_t yamux_stream_get_send_window(yamux_stream_t *stream) {
    if (!stream) {
        return 0;
    }
    
    return stream->send_window;
}

/**
 * Get the current state of a stream
 *
 * @param stream Stream to query
 * @return The current stream state
 */
yamux_stream_state_t yamux_stream_get_state(yamux_stream_t *stream) {
    if (!stream) {
        return YAMUX_STREAM_CLOSED;
    }
    
    return stream->state;
}

/**
 * Update the send window for a stream
 *
 * @param stream Stream to update
 * @param increment Amount to increment the window by
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_stream_update_window(yamux_stream_t *stream, uint32_t increment) {
    if (!stream) {
        return YAMUX_ERR_INVALID;
    }
    
    stream->send_window += increment;
    
    return YAMUX_OK;
}

/* yamux_stream_get_id is already defined in yamux_stream_utils.c */
