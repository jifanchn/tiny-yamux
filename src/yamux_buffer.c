/**
 * @file yamux_buffer.c
 * @brief Implementation of buffer management for yamux
 */

#include "yamux_internal.h"
#include <stdlib.h>
#include <string.h>

/**
 * Initialize a buffer with an initial size
 *
 * @param buffer Buffer to initialize
 * @param initial_size Initial size of the buffer
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_buffer_init(yamux_buffer_t *buffer, size_t initial_size)
{
    if (!buffer || initial_size == 0) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Allocate buffer */
    buffer->data = (uint8_t *)malloc(initial_size);
    if (!buffer->data) {
        return YAMUX_ERR_NOMEM;
    }
    
    /* Initialize buffer */
    buffer->size = initial_size;
    buffer->used = 0;
    buffer->pos = 0;
    
    return YAMUX_OK;
}

/**
 * Free a buffer
 *
 * @param buffer Buffer to free
 */
void yamux_buffer_free(yamux_buffer_t *buffer)
{
    if (buffer) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->size = 0;
        buffer->used = 0;
        buffer->pos = 0;
    }
}

/**
 * Write data to a buffer
 *
 * @param buffer Buffer to write to
 * @param data Data to write
 * @param len Length of data
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_buffer_write(yamux_buffer_t *buffer, const uint8_t *data, size_t len)
{
    uint8_t *new_data;
    size_t new_size;
    
    if (!buffer || !data || len == 0) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if buffer needs to be resized */
    if (buffer->used + len > buffer->size) {
        /* Calculate new size (double the current size or add enough for the new data) */
        new_size = buffer->size * 2;
        if (new_size < buffer->used + len) {
            new_size = buffer->used + len;
        }
        
        /* Resize buffer */
        new_data = (uint8_t *)realloc(buffer->data, new_size);
        if (!new_data) {
            return YAMUX_ERR_NOMEM;
        }
        
        /* Update buffer */
        buffer->data = new_data;
        buffer->size = new_size;
    }
    
    /* Copy data to buffer */
    memcpy(buffer->data + buffer->used, data, len);
    buffer->used += len;
    
    return YAMUX_OK;
}

/**
 * Read data from a buffer
 *
 * @param buffer Buffer to read from
 * @param data Buffer to store data
 * @param len Maximum number of bytes to read
 * @param bytes_read Number of bytes actually read
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_buffer_read(yamux_buffer_t *buffer, uint8_t *data, size_t len, size_t *bytes_read)
{
    size_t available;
    
    if (!buffer || !data || len == 0 || !bytes_read) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Calculate available bytes */
    available = buffer->used - buffer->pos;
    if (available == 0) {
        *bytes_read = 0;
        return YAMUX_OK;
    }
    
    /* Adjust read size if necessary */
    if (len > available) {
        len = available;
    }
    
    /* Copy data from buffer */
    memcpy(data, buffer->data + buffer->pos, len);
    buffer->pos += len;
    *bytes_read = len;
    
    return YAMUX_OK;
}

/**
 * Compact a buffer by removing already read data
 *
 * @param buffer Buffer to compact
 * @return YAMUX_OK on success, error code otherwise
 */
yamux_result_t yamux_buffer_compact(yamux_buffer_t *buffer)
{
    if (!buffer) {
        return YAMUX_ERR_INVALID;
    }
    
    /* Check if there's anything to compact */
    if (buffer->pos == 0) {
        return YAMUX_OK;
    }
    
    /* Move data to the beginning of the buffer */
    if (buffer->pos < buffer->used) {
        memmove(buffer->data, buffer->data + buffer->pos, buffer->used - buffer->pos);
    }
    
    /* Update buffer */
    buffer->used -= buffer->pos;
    buffer->pos = 0;
    
    return YAMUX_OK;
}
