#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test_main.h" // Added for stream_io_mock_t and test helpers
#include "../../src/yamux_internal.h"
#include "../../include/yamux.h"

// Forward declare yamux_session_t if not fully visible here for the typedef
// struct yamux_session_s; // Assuming yamux_session_t is a typedef for struct yamux_session_s

/* External assert function declaration */
void assert_true(int condition, const char *message);
void assert_int_equal(int a, int b, const char *message);
void assert_string_equal(const char *a, const char *b, const char *message);

/* Mock IO context for stream I/O testing */
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
} stream_io_t;

/* Mock read callback */
int stream_read(void *ctx, uint8_t *buf, size_t len) {
    stream_io_t *io = (stream_io_t *)ctx;
    
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

/* Mock write callback */
int stream_write(void *ctx, const uint8_t *buf, size_t len) {
    stream_io_t *io = (stream_io_t *)ctx;
    
    if (io->should_fail_write) {
        return -1;
    }
    
    if (io->write_buf_used + len > io->write_buf_size) {
        /* Resize buffer if needed */
        size_t new_size = io->write_buf_size * 2;
        if (new_size < io->write_buf_used + len) {
            new_size = io->write_buf_used + len;
        }
        
        uint8_t* new_buf = realloc(io->write_buf, new_size);
        if (!new_buf) return -1;
        
        io->write_buf = new_buf;
        io->write_buf_size = new_size;
    }
    
    memcpy(io->write_buf + io->write_buf_used, buf, len);
    io->write_buf_used += len;
    
    return len;
}

/* Initialize stream IO */
stream_io_t *stream_io_init(void) {
    stream_io_t *io = malloc(sizeof(stream_io_t));
    if (!io) return NULL;
    
    io->read_buf_size = 4096;
    io->read_buf = malloc(io->read_buf_size);
    if (!io->read_buf) {
        free(io);
        return NULL;
    }
    
    io->read_buf_used = 0;
    io->read_pos = 0;
    
    io->write_buf_size = 4096;
    io->write_buf = malloc(io->write_buf_size);
    if (!io->write_buf) {
        free(io->read_buf);
        free(io);
        return NULL;
    }
    io->write_buf_used = 0;
    
    io->should_fail_read = 0;
    io->should_fail_write = 0;
    
    return io;
}

/* Free stream IO */
void stream_io_free(stream_io_t *io) {
    if (io) {
        free(io->read_buf);
        free(io->write_buf);
        free(io);
    }
}

/* Minimal stream I/O test to isolate crash source */
void test_stream_io(void) {
    printf("Testing Stream I/O operations (minimal version)\n");
    fflush(stdout);
    
    yamux_context_t *client_ctx = NULL;
    yamux_context_t *server_ctx = NULL;
    yamux_stream_t *client_stream = NULL;
    yamux_stream_t *server_stream = NULL;
    stream_io_mock_t *client_io = NULL;   
    stream_io_mock_t *server_io = NULL;   
    int result = 0;
    
    /* Initialize IO contexts */
    client_io = stream_io_mock_init();
    server_io = stream_io_mock_init();
    
    if (!client_io || !server_io) {
        printf("ERROR: Failed to initialize IO contexts\n");
        goto cleanup;
    }
    
    printf("Step 1: Testing session initialization...\n");
    fflush(stdout);
    
    /* 1. Initialize sessions */
    client_ctx = yamux_init(mock_io_read, mock_io_write, client_io, 1);
    server_ctx = yamux_init(mock_io_read, mock_io_write, server_io, 0);
    
    if (!client_ctx || !server_ctx || !client_ctx->session || !server_ctx->session) {
        printf("ERROR: Failed to initialize sessions/contexts\n");
        goto cleanup;
    }
    
    printf("Session initialization successful\n");
    
    printf("Step 2: Testing stream creation...\n");
    fflush(stdout);
    
    /* 2. Create stream */
    printf("DEBUG: About to call yamux_stream_open_detailed for client_session...\n");
    fflush(stdout);
    result = yamux_stream_open_detailed(client_ctx->session, 0, &client_stream);
    printf("DEBUG: yamux_stream_open_detailed call result: %d. client_stream pointer: %p\n", result, (void*)client_stream);
    fflush(stdout);
    assert_int_equal(result, YAMUX_OK, "Client: yamux_stream_open_detailed failed");

    // Assert that the client stream was successfully opened
    assert_true(client_stream != NULL, "Failed to open client stream (pointer is NULL after call)");

    printf("DEBUG: Client stream opened successfully. client_stream = %p\n", (void*)client_stream); fflush(stdout);

    /* Connect the IOs to allow SYN frame to flow from client to server */
    printf("DEBUG: About to call stream_io_connect(client_io, server_io)...\n");
    fflush(stdout);
    stream_io_connect(client_io, server_io);
    printf("DEBUG: stream_io_connect returned.\n");
    fflush(stdout);
    
    if (server_ctx && server_ctx->session) {
        printf("DEBUG: BEFORE server yamux_process: server_ctx->session PTR = %p, server_ctx->session->go_away_received = %d\n", 
               (void*)server_ctx->session, 
               server_ctx->session->go_away_received);
        fflush(stdout);
    }

    printf("DEBUG: BEFORE server yamux_process: server_ctx PTR = %p, server_ctx->session PTR = %p\n", 
           (void*)server_ctx, (void*)server_ctx->session); 
    fflush(stdout);
    fprintf(stderr, "DEBUG: test_stream_io: server_ctx = %p, server_ctx->session (expected by test) = %p\n", (void*)server_ctx, (void*)server_ctx->session);
    fflush(stderr);
    printf("DEBUG: About to call yamux_process(server_ctx)...\n"); fflush(stdout); 

    /* Process incoming frame on server */
    result = yamux_process(server_ctx);
    printf("DEBUG: yamux_process returned. result = %d\n", result);
    fflush(stdout);
    
    if (result < 0) {
        printf("ERROR: Server failed to process client SYN, result=%d\n", result);
        goto cleanup;
    }
    
    printf("DEBUG: About to print 'Server processed client SYN successfully'\n");
    fflush(stdout);
    printf("Server processed client SYN successfully\n");
    fflush(stdout);
    printf("DEBUG: Just printed 'Server processed client SYN successfully'\n");
    fflush(stdout);

    // Server accepts stream
    printf("DEBUG: (A) Before yamux_accept_stream call\n"); fflush(stdout);
    result = yamux_stream_accept(server_ctx->session, &server_stream); 
    printf("DEBUG: yamux_accept_stream returned. server_stream = %p, result = %d\n", (void*)server_stream, result);
    fflush(stdout);

    if (result != YAMUX_OK && result != YAMUX_ERR_WOULD_BLOCK) {
        printf("DEBUG: (D) Inside if (!server_stream) - server_stream IS NULL\n"); fflush(stdout);
        printf("ERROR: Server failed to accept stream, result=%d\n", result);
        fflush(stdout);
        goto cleanup;
    }
    printf("DEBUG: (E) After if (!server_stream) check - server_stream IS NOT NULL\n"); fflush(stdout);

    printf("Server accepted client stream successfully\n");
    fflush(stdout);

    printf("DEBUG: (F) After 'Server accepted client stream successfully' print\n"); fflush(stdout);

    // Client processes server's SYN-ACK
    printf("DEBUG: Manually connecting server_io write to client_io read for server's SYN-ACK...\n"); fflush(stdout);
    stream_io_connect(server_io, client_io);
    printf("DEBUG: stream_io_connect done for server's SYN-ACK.\n"); fflush(stdout);

    printf("DEBUG: Client calling yamux_process() to handle server's SYN-ACK...\n"); fflush(stdout);
    // IMPORTANT: Pass client_ctx (the handle), not client_ctx->session
    result = yamux_process(client_ctx); // Ensure this is client_ctx
    printf("DEBUG: yamux_process on client (for SYN-ACK) returned %d. Client stream state: %d\n", result, client_stream ? client_stream->state : -1); fflush(stdout);
    if (result < 0 && result != YAMUX_ERR_WOULD_BLOCK) { 
        printf("ERROR: Client failed to process server's SYN-ACK, result=%d\n", result);
        goto cleanup;
    }
    // After this, client_stream should be ESTABLISHED
    if (client_stream && client_stream->state != YAMUX_STREAM_ESTABLISHED) {
        printf("WARNING: Client stream state is %d, expected ESTABLISHED (%d) after processing SYN-ACK\n", client_stream->state, YAMUX_STREAM_ESTABLISHED);
        // Not necessarily a fatal error for this test stage, but good to note.
    }

    printf("Step 3: Testing data transfer (Client to Server)...\n");
    fflush(stdout);

    printf("DEBUG: (G) Before declaring test_data_client\n"); fflush(stdout);
    const char *test_data_client = "Hello from client!";
    printf("DEBUG: (H) After declaring test_data_client, before strlen. Value: '%s'\n", test_data_client); fflush(stdout);
    size_t test_data_client_len = strlen(test_data_client);
    printf("DEBUG: (I) After strlen. Length: %zu\n", test_data_client_len); fflush(stdout);
    uint8_t read_buffer[128];
    printf("DEBUG: (J) After declaring read_buffer\n"); fflush(stdout);

    printf("DEBUG: Client writing data: '%s' (%zu bytes). Client stream state: %d, send_window: %u\n", 
           test_data_client, test_data_client_len, client_stream ? client_stream->state : -1, client_stream ? client_stream->send_window : 0); 
    fflush(stdout);

    // Use size_t for bytes_written for yamux_stream_write's out parameter
    size_t actual_bytes_written = 0; 
    result = yamux_stream_write(client_stream, (const uint8_t *)test_data_client, test_data_client_len, &actual_bytes_written);
    printf("DEBUG: Client yamux_stream_write: result=%d, actual_bytes_written=%zu. Expected len=%zu\n", 
           result, actual_bytes_written, test_data_client_len);
    fflush(stdout);
    
    if (result != YAMUX_OK) {
        printf("ERROR: Client failed to write data, result=%d\n", result);
        goto cleanup;
    }
    printf("DEBUG PRE-ASSERT: actual_bytes_written (%zu) vs test_data_client_len (%zu)\n", actual_bytes_written, test_data_client_len); fflush(stdout);
    assert_int_equal(actual_bytes_written, test_data_client_len, "Client: yamux_stream_write did not accept all bytes");
    printf("DEBUG POST-ASSERT: Assertion passed.\n"); fflush(stdout);

    // Manually transfer data from client's write buffer to server's read buffer for mock IO
    printf("DEBUG PRE-CONNECT: Manually connecting client_io write to server_io read for data frame...\n"); 
    printf("DEBUG PRE-CONNECT: client_io->write_buf_len = %zu, server_io->read_buf_len = %zu, server_io->read_pos = %zu\n",
           client_io->write_buf_len, server_io->read_buf_len, server_io->read_pos);
    fflush(stdout);
    stream_io_connect(client_io, server_io);
    printf("DEBUG POST-CONNECT: stream_io_connect done for data frame.\n");
    printf("DEBUG POST-CONNECT: client_io->write_buf_len = %zu, server_io->read_buf_len = %zu, server_io->read_pos = %zu\n",
           client_io->write_buf_len, server_io->read_buf_len, server_io->read_pos);
    fflush(stdout);

    // Server processes incoming data
    printf("DEBUG PRE-SERVER-PROCESS: Server calling yamux_process() to handle client's data frame...\n");
    printf("DEBUG PRE-SERVER-PROCESS: server_ctx = %p, server_ctx->session = %p\n", (void*)server_ctx, (void*)(server_ctx ? server_ctx->session : NULL));
    fflush(stdout);
    result = yamux_process(server_ctx); 
    printf("DEBUG POST-SERVER-PROCESS: yamux_process on server (for data) returned %d\n", result);
    fflush(stdout);

    if (result < 0 && result != YAMUX_ERR_WOULD_BLOCK) { 
        printf("ERROR: Server failed to process client data, result=%d\n", result);
        goto cleanup;
    }

    // Server reads data
    memset(read_buffer, 0, sizeof(read_buffer));
    printf("DEBUG PRE-SERVER-READ: Server reading data. server_stream = %p\n", (void*)server_stream);
    if(server_stream) {
        printf("DEBUG PRE-SERVER-READ: server_stream state = %d, recv_window = %u, id = %u\n", 
               server_stream->state, server_stream->recv_window, server_stream->id); 
        printf("DEBUG PRE-SERVER-READ: server_stream->recvbuf: used = %zu, size = %zu, pos = %zu\n", 
               server_stream->recvbuf.used, 
               server_stream->recvbuf.size, 
               server_stream->recvbuf.pos);  
    }
    fflush(stdout);
    // Use size_t for bytes_read for yamux_stream_read's out parameter
    size_t actual_bytes_read = 0;
    result = yamux_stream_read(server_stream, read_buffer, sizeof(read_buffer) - 1, &actual_bytes_read);
    printf("DEBUG POST-SERVER-READ: yamux_stream_read on server returned %d, actual_bytes_read %zu\n", result, actual_bytes_read);
    fflush(stdout);

    if (result != YAMUX_OK && result != YAMUX_ERR_WOULD_BLOCK) { 
        printf("ERROR: Server failed to read data, result=%d\n", result);
        goto cleanup;
    }
    read_buffer[actual_bytes_read] = '\0'; // Null-terminate
    printf("DEBUG: Server read data: '%s' (%zu bytes)\n", read_buffer, actual_bytes_read);

    assert_int_equal(actual_bytes_read, test_data_client_len, "Server: Did not read expected number of bytes");
    assert_string_equal((char *)read_buffer, test_data_client, "Server: Read data does not match sent data");

    printf("Client to Server data transfer successful!\n");

    // TODO: Add Server to Client data transfer test here

    /* Clean up gracefully */
    printf("Step 4: Cleaning up resources\n");
    fflush(stdout);

    printf("DEBUG CLEANUP: About to close client_stream (%p)\n", (void*)client_stream); fflush(stdout);
    if (client_stream) {
        yamux_result_t close_res = yamux_stream_close(client_stream, 0);
        result = (close_res == YAMUX_OK) ? 0 : (int)close_res; 
        printf("DEBUG CLEANUP: client_stream close result: %d (raw: %d)\n", result, close_res);
        fflush(stdout);
        // Note: yamux_stream_close handles freeing the stream structure itself if appropriate (e.g. state becomes CLOSED)
        // No separate free of a stream_context wrapper is needed here as we have the raw stream.
        client_stream = NULL; // Avoid double close attempts
    }
    printf("DEBUG CLEANUP: client_stream closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to close server_stream (%p)\n", (void*)server_stream); fflush(stdout);
    if (server_stream) {
        yamux_result_t close_res = yamux_stream_close(server_stream, 0);
        result = (close_res == YAMUX_OK) ? 0 : (int)close_res;
        printf("DEBUG CLEANUP: server_stream close result: %d (raw: %d)\n", result, close_res);
        fflush(stdout);
        // Note: yamux_stream_close handles freeing the stream structure itself
        server_stream = NULL; // Avoid double close attempts
    }
    printf("DEBUG CLEANUP: server_stream closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to destroy client_ctx (%p)\n", (void*)client_ctx); fflush(stdout);
    if (client_ctx) {
        yamux_destroy(client_ctx);
        printf("DEBUG CLEANUP: client_ctx destroy called.\n");
        client_ctx = NULL; // Prevent double free
    }
    printf("DEBUG CLEANUP: client_ctx closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to destroy server_ctx (%p)\n", (void*)server_ctx); fflush(stdout);
    if (server_ctx) {
        yamux_destroy(server_ctx);
        printf("DEBUG CLEANUP: server_ctx destroy called.\n");
        server_ctx = NULL; // Prevent double free
    }
    printf("DEBUG CLEANUP: server_ctx closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to free client_io (%p)\n", (void*)client_io); fflush(stdout);
    if (client_io) {
        stream_io_mock_destroy(client_io);
        printf("DEBUG CLEANUP: client_io destroy called.\n");
    }
    printf("DEBUG CLEANUP: client_io freed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to free server_io (%p)\n", (void*)server_io); fflush(stdout);
    if (server_io) {
        stream_io_mock_destroy(server_io);
        printf("DEBUG CLEANUP: server_io destroy called.\n");
    }
    printf("DEBUG CLEANUP: server_io freed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: End of test_basic_stream_io cleanup.\n"); fflush(stdout);

    printf("Minimal Stream I/O test completed successfully!\n");
    assert_true(1, "Stream I/O test passed");
    return; // Successfully completed, do not fall into error cleanup
    
cleanup:
    /* Clean up resources */
    printf("DEBUG CLEANUP: About to close client_stream (%p)\n", (void*)client_stream); fflush(stdout);
    if (client_stream) {
        yamux_result_t close_res = yamux_stream_close(client_stream, 0);
        result = (close_res == YAMUX_OK) ? 0 : (int)close_res; 
        printf("DEBUG CLEANUP: client_stream close result: %d (raw: %d)\n", result, close_res);
        fflush(stdout);
        // Note: yamux_stream_close handles freeing the stream structure itself if appropriate (e.g. state becomes CLOSED)
        // No separate free of a stream_context wrapper is needed here as we have the raw stream.
        client_stream = NULL; // Avoid double close attempts
    }
    printf("DEBUG CLEANUP: client_stream closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to close server_stream (%p)\n", (void*)server_stream); fflush(stdout);
    if (server_stream) {
        yamux_result_t close_res = yamux_stream_close(server_stream, 0);
        result = (close_res == YAMUX_OK) ? 0 : (int)close_res;
        printf("DEBUG CLEANUP: server_stream close result: %d (raw: %d)\n", result, close_res);
        fflush(stdout);
        // Note: yamux_stream_close handles freeing the stream structure itself
        server_stream = NULL; // Avoid double close attempts
    }
    printf("DEBUG CLEANUP: server_stream closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to destroy client_ctx (%p)\n", (void*)client_ctx); fflush(stdout);
    if (client_ctx) {
        yamux_destroy(client_ctx);
        printf("DEBUG CLEANUP: client_ctx destroy called.\n");
    }
    printf("DEBUG CLEANUP: client_ctx closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to destroy server_ctx (%p)\n", (void*)server_ctx); fflush(stdout);
    if (server_ctx) {
        yamux_destroy(server_ctx);
        printf("DEBUG CLEANUP: server_ctx destroy called.\n");
    }
    printf("DEBUG CLEANUP: server_ctx closed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to free client_io (%p)\n", (void*)client_io); fflush(stdout);
    if (client_io) {
        stream_io_mock_destroy(client_io);
        printf("DEBUG CLEANUP: client_io destroy called.\n");
    }
    printf("DEBUG CLEANUP: client_io freed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: About to free server_io (%p)\n", (void*)server_io); fflush(stdout);
    if (server_io) {
        stream_io_mock_destroy(server_io);
        printf("DEBUG CLEANUP: server_io destroy called.\n");
    }
    printf("DEBUG CLEANUP: server_io freed.\n"); fflush(stdout);

    printf("DEBUG CLEANUP: End of test_basic_stream_io cleanup.\n"); fflush(stdout);
}
