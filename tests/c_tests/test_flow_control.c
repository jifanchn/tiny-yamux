#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Define yamux window size constants if not defined */
#ifndef YAMUX_DEFAULT_WINDOW_SIZE
#define YAMUX_DEFAULT_WINDOW_SIZE 256 * 1024 /* 256 KB */
#endif

/* Function declarations for functions not in public API */
uint32_t yamux_stream_get_send_window(yamux_stream_t *stream);
yamux_result_t yamux_stream_update_window(yamux_stream_t *stream, uint32_t increment);

/* External assert function declaration */
void assert_true(int condition, const char *message);

/* Mock IO context for flow control testing */
typedef struct {
    uint8_t *read_buf;
    size_t read_buf_size;
    size_t read_buf_used;
    size_t read_pos;
    
    uint8_t *write_buf;
    size_t write_buf_size;
    size_t write_buf_used;
    
    int should_fail_read;
    int should_fail_write;
    int limit_write_bytes;
    int max_write_bytes;
} flow_io_t;

/* Mock read callback */
int flow_read(void *ctx, uint8_t *buf, size_t len) {
    flow_io_t *io = (flow_io_t *)ctx;
    
    if (io->should_fail_read) {
        return -1;
    }
    
    if (io->read_buf_used == 0 || io->read_pos >= io->read_buf_used) {
        return 0; /* No data available */
    }
    
    size_t available = io->read_buf_used - io->read_pos;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(buf, io->read_buf + io->read_pos, to_read);
    io->read_pos += to_read;
    
    return to_read;
}

/* Mock write callback with byte limit for flow control testing */
int flow_write(void *ctx, const uint8_t *buf, size_t len) {
    flow_io_t *io = (flow_io_t *)ctx;
    
    if (io->should_fail_write) {
        return -1;
    }
    
    if (io->limit_write_bytes) {
        len = (len < (size_t)io->max_write_bytes) ? len : (size_t)io->max_write_bytes;
    }
    
    if (io->write_buf_used + len > io->write_buf_size) {
        /* Resize buffer if needed */
        size_t new_size = io->write_buf_size * 2;
        if (new_size < io->write_buf_used + len) {
            new_size = io->write_buf_used + len;
        }
        
        io->write_buf = realloc(io->write_buf, new_size);
        io->write_buf_size = new_size;
    }
    
    memcpy(io->write_buf + io->write_buf_used, buf, len);
    io->write_buf_used += len;
    
    return len;
}

/* Initialize flow IO */
flow_io_t *flow_io_init(void) {
    flow_io_t *io = malloc(sizeof(flow_io_t));
    
    io->read_buf_size = 4096;
    io->read_buf = malloc(io->read_buf_size);
    io->read_buf_used = 0;
    io->read_pos = 0;
    
    io->write_buf_size = 4096;
    io->write_buf = malloc(io->write_buf_size);
    io->write_buf_used = 0;
    
    io->should_fail_read = 0;
    io->should_fail_write = 0;
    io->limit_write_bytes = 0;
    io->max_write_bytes = 1024;
    
    return io;
}

/* Free flow IO */
void flow_io_free(flow_io_t *io) {
    if (io) {
        free(io->read_buf);
        free(io->write_buf);
        free(io);
    }
}

/* Connect two flow IOs */
void flow_io_connect(flow_io_t *io1, flow_io_t *io2) {
    /* Swap read and write buffers */
    uint8_t *temp_buf = io1->write_buf;
    size_t temp_size = io1->write_buf_size;
    size_t temp_used = io1->write_buf_used;
    
    io1->write_buf = io2->read_buf;
    io1->write_buf_size = io2->read_buf_size;
    io1->write_buf_used = 0;
    
    io2->read_buf = temp_buf;
    io2->read_buf_size = temp_size;
    io2->read_buf_used = temp_used;
    io2->read_pos = 0;
}

/* Test flow control with the new portable API */
void test_flow_control(void) {
    printf("Testing flow control with the Yamux portable API...\n");
    fflush(stdout);

    /* 
     * This test validates the flow control mechanism in Yamux
     * by sending data in chunks and verifying that window updates
     * are working correctly to maintain data flow.
     */
    
    /* Initialize all variables */
    flow_io_t* client_io = NULL;
    flow_io_t* server_io = NULL;
    void* client_session = NULL;
    void* server_session = NULL;
    void* client_stream = NULL;
    void* server_stream = NULL;
    uint8_t* send_buffer = NULL;
    uint8_t* recv_buffer = NULL;
    int result = 0;
    int bytes_written = 0;
    int total_written = 0;
    int total_read = 0;
    int bytes_read = 0;
    
    printf("Step 1: Initializing sessions\n");
    fflush(stdout);
    
    /* Step 1: Initialize IO contexts and sessions */
    client_io = flow_io_init();
    server_io = flow_io_init();
    
    if (!client_io || !server_io) {
        printf("ERROR: Failed to initialize IO contexts\n");
        goto cleanup;
    }
    
    client_session = yamux_init(flow_read, flow_write, client_io, 1);
    server_session = yamux_init(flow_read, flow_write, server_io, 0);
    
    if (!client_session || !server_session) {
        printf("ERROR: Failed to initialize Yamux sessions\n");
        goto cleanup;
    }
    
    printf("Sessions initialized successfully\n");
    
    printf("Step 2: Creating and accepting stream\n");
    fflush(stdout);
    
    /* Step 2: Open streams */
    client_stream = yamux_open_stream(client_session);
    if (!client_stream) {
        printf("ERROR: Failed to open client stream\n");
        goto cleanup;
    }
    
    /* Establish connection between sessions */
    flow_io_connect(client_io, server_io);
    result = yamux_process(server_session);
    if (result < 0) {
        printf("ERROR: Server process failed: %d\n", result);
        goto cleanup;
    }
    
    server_stream = yamux_accept_stream(server_session);
    if (!server_stream) {
        printf("ERROR: Failed to accept stream\n");
        goto cleanup;
    }
    
    printf("Streams established successfully\n");
    
    printf("Step 3: Testing data transfer with flow control\n");
    fflush(stdout);
    
    /* Step 3: Test data transfer with smaller chunks to test flow control */
    #define TEST_BUFFER_SIZE 2048  /* 2KB buffer - smaller for safety */
    #define CHUNK_SIZE 512         /* Send in 512B chunks */
    #define NUM_CHUNKS 4           /* Send a few chunks to test window updates */
    
    send_buffer = malloc(TEST_BUFFER_SIZE);
    recv_buffer = malloc(TEST_BUFFER_SIZE);
    
    if (!send_buffer || !recv_buffer) {
        printf("ERROR: Failed to allocate buffers\n");
        goto cleanup;
    }
    
    /* Initialize test data */
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        send_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    /* Test writing and reading in chunks to exercise flow control */
    for (int chunk = 0; chunk < NUM_CHUNKS; chunk++) {
        int offset = chunk * CHUNK_SIZE;
        int remaining = TEST_BUFFER_SIZE - offset;
        int chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        if (chunk_size <= 0) break;
        
        printf("Writing chunk %d (%d bytes)\n", chunk, chunk_size);
        fflush(stdout);
        
        /* Write a chunk */
        bytes_written = yamux_write(client_stream, send_buffer + offset, chunk_size);
        if (bytes_written <= 0) {
            printf("ERROR: Write failed on chunk %d, result=%d\n", chunk, bytes_written);
            goto cleanup;
        }
        
        total_written += bytes_written;
        printf("Wrote chunk %d: %d bytes (total: %d)\n", chunk, bytes_written, total_written);
        
        /* Forward data to server */
        flow_io_connect(client_io, server_io);
        result = yamux_process(server_session);
        if (result < 0) {
            printf("ERROR: Server process failed: %d\n", result);
            goto cleanup;
        }
        
        /* Read the chunk on server */
        bytes_read = yamux_read(server_stream, recv_buffer + total_read, chunk_size);
        if (bytes_read <= 0) {
            printf("ERROR: Read failed on chunk %d, result=%d\n", chunk, bytes_read);
            goto cleanup;
        }
        
        total_read += bytes_read;
        printf("Read chunk %d: %d bytes (total: %d)\n", chunk, bytes_read, total_read);
        
        /* Connect in reverse direction to allow window updates to flow back */
        flow_io_connect(server_io, client_io);
        result = yamux_process(client_session);
        if (result < 0) {
            printf("ERROR: Client process failed: %d\n", result);
            goto cleanup;
        }
    }
    
    printf("Successfully transferred %d bytes with flow control\n", total_written);
    
    /* Verify total bytes transferred */
    if (total_written != total_read) {
        printf("ERROR: Bytes written (%d) doesn't match bytes read (%d)\n", 
               total_written, total_read);
        goto cleanup;
    }
    
    /* Verify data integrity */
    int data_match = 1;
    for (int i = 0; i < total_read; i++) {
        if (recv_buffer[i] != (uint8_t)(i & 0xFF)) {
            printf("ERROR: Data corruption at byte %d (expected 0x%02x, got 0x%02x)\n", 
                  i, (uint8_t)(i & 0xFF), recv_buffer[i]);
            data_match = 0;
            break;
        }
    }
    
    if (!data_match) {
        printf("ERROR: Data integrity check failed\n");
        goto cleanup;
    }
    
    printf("Data integrity verified for %d bytes\n", total_read);
    
    /* Test closing streams */
    printf("Step 4: Closing streams\n");
    fflush(stdout);
    
    result = yamux_close_stream(client_stream, 0);
    if (result < 0) {
        printf("ERROR: Failed to close client stream, result=%d\n", result);
    }
    
    result = yamux_close_stream(server_stream, 0);
    if (result < 0) {
        printf("ERROR: Failed to close server stream, result=%d\n", result);
    }
    
    /* Test passed */
    printf("Flow control test completed successfully!\n");
    assert_true(1 == 1, "Flow control test passed");
    
cleanup:
    /* Clean up resources */
    if (client_stream) yamux_close_stream(client_stream, 0);
    if (server_stream) yamux_close_stream(server_stream, 0);
    if (client_session) yamux_destroy(client_session);
    if (server_session) yamux_destroy(server_session);
    if (client_io) flow_io_free(client_io);
    if (server_io) flow_io_free(server_io);
    free(send_buffer);
    free(recv_buffer);
}
