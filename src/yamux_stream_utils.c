/**
 * @file yamux_stream_utils.c
 * @brief Utility functions for yamux stream management
 */

#include "../include/yamux.h"
#include "yamux_internal.h"
#include "yamux_defs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Get the stream ID
 *
 * @param stream Stream
 * @return Stream ID
 */
uint32_t yamux_stream_get_id(
    yamux_stream_t *stream)
{
    if (!stream) {
        return 0;
    }
    
    return stream->id;
}

/**
 * Find a stream by ID
 *
 * @param session Session
 * @param stream_id Stream ID to find
 * @return Stream pointer or NULL if not found
 */
yamux_stream_t *yamux_get_stream(
    yamux_session_t *session, 
    uint32_t stream_id)
{
    size_t i;
    
    if (!session) {
        return NULL;
    }
    
    /* Search for stream */
    for (i = 0; i < session->stream_count; i++) {
        if (session->streams[i] && session->streams[i]->id == stream_id) {
            return session->streams[i];
        }
    }
    
    return NULL;
}

/**
 * Add a stream to a session
 *
 * @param session Session
 * @param stream Stream to add
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_add_stream(
    yamux_session_t *session, 
    yamux_stream_t *stream)
{
    yamux_stream_t **new_streams;
    size_t new_capacity;
    size_t i;
    
    if (!session || !stream) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if session is shut down */
    if (session->go_away_received) {
        return YAMUX_ERR_CLOSED;
    }
    
    /* Check if stream already exists */
    if (yamux_get_stream(session, stream->id)) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Find an empty slot */
    for (i = 0; i < session->stream_count; i++) {
        if (!session->streams[i]) {
            session->streams[i] = stream;
            return YAMUX_OK;
        }
    }
    
    /* Check if we need to resize the streams array */
    if (session->stream_count >= session->stream_capacity) {
        /* Double the capacity */
        new_capacity = session->stream_capacity * 2;
        new_streams = (yamux_stream_t **)realloc(
            session->streams, 
            new_capacity * sizeof(yamux_stream_t *)
        );
        
        if (!new_streams) {
            return YAMUX_ERR_NOMEM;
        }
        
        /* Initialize new slots */
        for (i = session->stream_capacity; i < new_capacity; i++) {
            new_streams[i] = NULL;
        }
        
        /* Update session */
        session->streams = new_streams;
        session->stream_capacity = new_capacity;
    }
    
    /* Add stream to the first empty slot */
    session->streams[session->stream_count] = stream;
    session->stream_count++;
    
    return YAMUX_OK;
}

/**
 * Remove a stream from a session
 *
 * @param session Session
 * @param stream_id Stream ID to remove
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_remove_stream(
    yamux_session_t *session, 
    uint32_t stream_id)
{
    size_t i;
    
    if (!session) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Search for stream */
    for (i = 0; i < session->stream_count; i++) {
        if (session->streams[i] && session->streams[i]->id == stream_id) {
            /* Remove stream */
            session->streams[i] = NULL;
            
            /* If it's the last stream, update stream count */
            if (i == session->stream_count - 1) {
                session->stream_count--;
            }
            
            return YAMUX_OK;
        }
    }
    
    return YAMUX_ERR_INVALID;
}

/**
 * Add a stream to the accept queue
 *
 * @param session Session
 * @param stream Stream to add
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_enqueue_stream(
    yamux_session_t *session, 
    yamux_stream_t *stream)
{
    if (!session || !stream) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if session is shut down */
    if (session->go_away_received) {
        return YAMUX_ERR_CLOSED;
    }
    
    /* Check accept backlog limit - not using accept_queue_size since it's not in the structure */
    if (session->config.accept_backlog == 0) {
        return YAMUX_ERR_TIMEOUT;
    }
    
    /* Add to accept queue */
    stream->next = session->accept_queue;
    session->accept_queue = stream;
    
    /* Accept queue size is managed by the queue itself */
    
    return YAMUX_OK;
}

/**
 * Enqueue a stream to the session's accept queue.
 * This is typically called when a server-side stream enters SYN_RECV state.
 *
 * @param session The session.
 * @param stream The stream to enqueue.
 * @return YAMUX_OK on success, or an error code.
 */
yamux_result_t yamux_enqueue_stream_for_accept(yamux_session_t *session, yamux_stream_t *stream) {
    if (!session || !stream) {
        return YAMUX_ERR_INVALID;
    }

    // Ensure stream is not already in a queue or linked elsewhere inappropriately
    stream->next = NULL;

    if (!session->accept_queue) {
        // Queue is empty, this stream becomes the head
        session->accept_queue = stream;
    } else {
        // Add to the end of the queue
        yamux_stream_t *current = session->accept_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = stream;
    }
    
    // TODO: Potentially signal or notify that a stream is ready for accept if using blocking accept.
    // For now, yamux_accept_stream will just pick it up on the next call.

    printf("DEBUG: Enqueued stream %u for acceptance.\n", stream->id);
    return YAMUX_OK;
}

/**
 * Note: yamux_handle_data and yamux_handle_window_update functions have been moved to yamux_handlers.c
 * to avoid duplicate symbols. Function declarations remain in yamux_internal.h
 */
