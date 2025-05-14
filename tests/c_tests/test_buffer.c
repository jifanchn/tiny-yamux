#include "yamux.h"
#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test buffer functionality */
void test_buffer(void) {
    yamux_buffer_t buffer;
    yamux_result_t result;
    uint8_t data[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t read_data[16];
    size_t bytes_read;
    
    printf("Testing buffer functionality...\n");
    
    /* Initialize buffer */
    result = yamux_buffer_init(&buffer, 8);
    assert(result == YAMUX_OK);
    assert(buffer.size == 8);
    assert(buffer.used == 0);
    assert(buffer.pos == 0);
    
    /* Write to buffer (with resize) */
    result = yamux_buffer_write(&buffer, data, sizeof(data));
    assert(result == YAMUX_OK);
    assert(buffer.size >= 16);
    assert(buffer.used == 16);
    assert(buffer.pos == 0);
    
    /* Read from buffer */
    result = yamux_buffer_read(&buffer, read_data, 8, &bytes_read);
    assert(result == YAMUX_OK);
    assert(bytes_read == 8);
    assert(buffer.pos == 8);
    assert(memcmp(read_data, data, 8) == 0);
    
    /* Read more from buffer */
    result = yamux_buffer_read(&buffer, read_data + 8, 8, &bytes_read);
    assert(result == YAMUX_OK);
    assert(bytes_read == 8);
    assert(buffer.pos == 16);
    assert(memcmp(read_data + 8, data + 8, 8) == 0);
    
    /* Buffer should be empty now */
    result = yamux_buffer_read(&buffer, read_data, 8, &bytes_read);
    assert(result == YAMUX_OK);
    assert(bytes_read == 0);
    
    /* Compact buffer */
    result = yamux_buffer_compact(&buffer);
    assert(result == YAMUX_OK);
    assert(buffer.used == 0);
    assert(buffer.pos == 0);
    
    /* Free buffer */
    yamux_buffer_free(&buffer);
    
    printf("Buffer tests passed!\n");
}

/* Test runner moved to test_main.c */
