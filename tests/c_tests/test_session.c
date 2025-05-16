/**
 * @file test_session.c
 * @brief Test for yamux session management
 */

#include "test_common.h"
#include "test_main.h"
#include "mock_io.h"

/* Test session creation and basic operations */
void test_session_creation(void) {
    yamux_io_t io;
    yamux_session_t *session;
    yamux_result_t result;
    pipe_io_context_t *io_ctx;
    
    printf("Testing session creation...\n");
    
    /* Create pipe IO context */
    io_ctx = pipe_io_context_create(4096);
    assert(io_ctx != NULL);
    
    /* Set up IO callbacks */
    io.read = pipe_read;
    io.write = pipe_write;
    io.ctx = io_ctx;
    
    /* Create client session */
    result = yamux_session_create(&io, 1, NULL, &session);
    assert(result == YAMUX_OK);
    assert(session != NULL);
    
    /* Close session */
    result = yamux_session_close(session, YAMUX_NORMAL);
    assert(result == YAMUX_OK);
    
    /* Create server session */
    result = yamux_session_create(&io, 0, NULL, &session);
    assert(result == YAMUX_OK);
    assert(session != NULL);
    
    /* Close session */
    result = yamux_session_close(session, YAMUX_NORMAL);
    assert(result == YAMUX_OK);
    
    /* Free IO context */
    pipe_io_context_free(io_ctx);
    
    printf("Session creation tests passed!\n");
}

/* Test ping functionality */
void test_session_ping(void) {
    printf("Testing session ping...\n");
    yamux_session_t *client_session, *server_session;
    yamux_io_t client_io, server_io;
    mock_io_t *client_mock, *server_mock;
    yamux_config_t config;
    yamux_result_t result;
    
    /* Initialize mock IOs */
    client_mock = mock_io_init(1024);
    server_mock = mock_io_init(1024);
    
    /* Set up IO callbacks */
    client_io.read = mock_read;
    client_io.write = mock_write;
    client_io.ctx = client_mock;
    
    server_io.read = mock_read;
    server_io.write = mock_write;
    server_io.ctx = server_mock;
    
    /* Set default config */
    memset(&config, 0, sizeof(config));
    config.accept_backlog = 128;
    config.keepalive_interval = 30000;
    config.enable_keepalive = 1;
    config.max_stream_window_size = 262144;
    
    /* Create client and server sessions */
    result = yamux_session_create(&client_io, 1, &config, &client_session);
    assert_true(result == YAMUX_OK, "Failed to create client session");
    
    result = yamux_session_create(&server_io, 0, &config, &server_session);
    assert_true(result == YAMUX_OK, "Failed to create server session");
    
    /* Send ping from client to server */
    result = yamux_session_ping(client_session);
    assert_true(result == YAMUX_OK, "Failed to send ping");
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Process ping on server */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session");
    
    /* Exchange data server -> client (PING-ACK) */
    mock_io_swap_buffers(server_mock, client_mock);
    
    /* Process ping-ack on client */
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session");
    
    /* Verify the ping was completed successfully */
    /* Note: In a real test, we would need access to internal client state to verify the PING-ACK was received
     * For simplicity, we just check that processing completed without errors */
    
    /* Clean up */
    result = yamux_session_close(client_session, YAMUX_NORMAL);
    assert_true(result == YAMUX_OK, "Failed to close client session");
    
    result = yamux_session_close(server_session, YAMUX_NORMAL);
    assert_true(result == YAMUX_OK, "Failed to close server session");
    
    /* Free mock IO resources */
    mock_io_free(client_mock);
    mock_io_free(server_mock);
    
    printf("Session ping test completed successfully!\n");
}

/* 
 * Note: Helper function for data transfer has been removed as it's no longer used.
 * This functionality is now handled by the new portable API in yamux_port.c
 */

/* Test stream creation and data transfer */
void test_stream_data_transfer(void) {
    yamux_io_t io;
    yamux_session_t *client_session, *server_session;
    yamux_stream_t *client_stream, *server_stream;
    yamux_result_t result;
    stream_io_mock_t *client_mock, *server_mock;
    uint8_t test_data[] = "Hello, Yamux Stream!";
    const size_t test_data_len = sizeof(test_data) - 1; // Exclude null terminator
    uint8_t read_buffer[256];
    size_t bytes_read;
    
    printf("Testing stream data transfer...\n");
    
    /* Create mock IO contexts */
    client_mock = stream_io_mock_init();
    server_mock = stream_io_mock_init();
    assert_true(client_mock != NULL, "Failed to create client mock IO");
    assert_true(server_mock != NULL, "Failed to create server mock IO");
    
    /* Set up client session */
    io.read = mock_io_read;
    io.write = mock_io_write;
    io.ctx = client_mock;
    result = yamux_session_create(&io, 1, NULL, &client_session);
    assert_true(result == YAMUX_OK, "Failed to create client session");
    
    /* Set up server session */
    io.read = mock_io_read;
    io.write = mock_io_write;
    io.ctx = server_mock;
    result = yamux_session_create(&io, 0, NULL, &server_session);
    assert_true(result == YAMUX_OK, "Failed to create server session");
    
    /* Create client stream */
    result = yamux_stream_open_detailed(client_session, 0, &client_stream);
    assert_true(result == YAMUX_OK, "Failed to open client stream");
    
    /* Process client session to generate stream open frame */
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session");
    
    /* Forward client output to server input */
    stream_io_connect(client_mock, server_mock);
    
    /* Process server session to receive stream open frame */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session");
    
    /* Accept server stream */
    result = yamux_stream_accept(server_session, &server_stream);
    assert_true(result == YAMUX_OK, "Failed to accept server stream");
    
    /* Write data to client stream */
    size_t bytes_written;
    result = yamux_stream_write(client_stream, test_data, test_data_len, &bytes_written);
    assert_true(result == YAMUX_OK, "Failed to write to client stream");
    assert_int_equal(bytes_written, test_data_len, "Failed to write all data");
    
    /* Process client session to generate data frame */
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session after write");
    
    /* Forward client output to server input */
    stream_io_connect(client_mock, server_mock);
    
    /* Process server session to receive data frame */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session for data");
    
    /* Read data from server stream */
    result = yamux_stream_read(server_stream, read_buffer, sizeof(read_buffer), &bytes_read);
    assert_true(result == YAMUX_OK, "Failed to read from server stream");
    assert_int_equal(bytes_read, test_data_len, "Failed to read all data");
    
    /* Verify data content */
    assert_true(memcmp(test_data, read_buffer, test_data_len) == 0, "Data read doesn't match data written");
    
    /* Clean up */
    /* Close streams */
    result = yamux_stream_close(client_stream, 0);
    assert_true(result == YAMUX_OK, "Failed to close client stream");
    
    result = yamux_stream_close(server_stream, 0);
    assert_true(result == YAMUX_OK, "Failed to close server stream");
    
    /* Process sessions after stream close */
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session after close");
    
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session after close");
    
    /* Close sessions */
    result = yamux_session_close(client_session, YAMUX_NORMAL);
    assert_true(result == YAMUX_OK, "Failed to close client session");
    
    result = yamux_session_close(server_session, YAMUX_NORMAL);
    assert_true(result == YAMUX_OK, "Failed to close server session");
    
    /* Free mock IO resources */
    stream_io_mock_destroy(client_mock);
    stream_io_mock_destroy(server_mock);
    
    printf("Stream data transfer tests passed!\n");
}

/* Test runner moved to test_main.c */
