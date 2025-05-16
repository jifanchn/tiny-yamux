#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mock_io.h"

/* External assert function declaration */
void assert_true(int condition, const char *message);

/* Stream state strings for debugging */
static MAYBE_UNUSED const char *stream_state_str(yamux_stream_state_t state) {
    switch (state) {
        case YAMUX_STREAM_IDLE: return "IDLE";
        case YAMUX_STREAM_SYN_SENT: return "SYN_SENT";
        case YAMUX_STREAM_SYN_RECV: return "SYN_RECV";
        case YAMUX_STREAM_ESTABLISHED: return "ESTABLISHED";
        case YAMUX_STREAM_FIN_SENT: return "FIN_SENT";
        case YAMUX_STREAM_FIN_RECV: return "FIN_RECV";
        case YAMUX_STREAM_CLOSED: return "CLOSED";
        default: return "UNKNOWN";
    }
}

/* Test stream lifecycle - transition between all states */
void test_stream_lifecycle(void) {
    printf("Testing stream lifecycle...\n");
    yamux_session_t *client_session, *server_session;
    yamux_stream_t *client_stream, *server_stream;
    yamux_io_t client_io, server_io;
    mock_io_t *client_mock, *server_mock;
    yamux_config_t config;
    yamux_result_t result;
    yamux_stream_state_t state;
    uint8_t data[] = "test data";
    uint8_t read_buf[64];
    size_t bytes_read;
    
    /* Initialize mock IOs */
    client_mock = mock_io_init(4096);
    server_mock = mock_io_init(4096);
    
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
    config.max_stream_window_size = 262144; /* Set a proper window size */
    
    /* Create client and server sessions */
    result = yamux_session_create(&client_io, 1, &config, &client_session);
    assert_true(result == YAMUX_OK, "Failed to create client session");
    
    result = yamux_session_create(&server_io, 0, &config, &server_session);
    assert_true(result == YAMUX_OK, "Failed to create server session");
    
    /* TEST 1: Idle -> SYN_SENT (client opens stream) */

    result = yamux_stream_open_detailed(client_session, 0, &client_stream);
    assert_true(result == YAMUX_OK, "Failed to open client stream");
    assert_true(client_stream != NULL, "Client stream is NULL after open");
    
    state = yamux_stream_get_state(client_stream);
    assert_true(state == YAMUX_STREAM_SYN_SENT, 
                "Stream should be in SYN_SENT state");
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* TEST 2: SYN_RECV (server processes SYN) */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session");
    
    /* Accept the stream on server */
    result = yamux_stream_accept(server_session, &server_stream);
    assert_true(result == YAMUX_OK, "Failed to accept server stream");
    
    state = yamux_stream_get_state(server_stream);
    assert_true(state == YAMUX_STREAM_SYN_RECV, 
                "Server stream should be in SYN_RECV state");
    
    /* Exchange data server -> client */
    mock_io_swap_buffers(server_mock, client_mock);
    
    /* TEST 3: ESTABLISHED (both sides process SYN+ACK) */
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session");
    
    state = yamux_stream_get_state(client_stream);
    assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                "Client stream should be in ESTABLISHED state");
    
    /* First send WINDOW_UPDATE to provide client with send window */
    yamux_header_t window_header;
    uint8_t window_frame[16];
    memset(&window_header, 0, sizeof(window_header));
    window_header.version = YAMUX_PROTO_VERSION;
    window_header.type = YAMUX_WINDOW_UPDATE;
    window_header.flags = 0;
    window_header.stream_id = client_stream->id;
    window_header.length = 4;
    yamux_encode_header(&window_header, window_frame);
    uint32_t window_size = htonl(262144); /* Set an appropriate window size */
    memcpy(window_frame + YAMUX_HEADER_SIZE, &window_size, 4);
    client_mock->write_buf_used = 0; /* Clear any existing data */
    memcpy(client_mock->write_buf, window_frame, sizeof(window_frame));
    client_mock->write_buf_used = sizeof(window_frame);
    
    /* Exchange data client -> server for window update */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Server processes window update */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session after window update");
    
    /* Then send explicit ACK to server so it can move to ESTABLISHED state */
    yamux_header_t ack_header;
    uint8_t ack_frame[12];
    memset(&ack_header, 0, sizeof(ack_header));
    ack_header.version = YAMUX_PROTO_VERSION;
    ack_header.type = YAMUX_WINDOW_UPDATE;
    ack_header.flags = YAMUX_FLAG_ACK;
    ack_header.stream_id = client_stream->id;
    ack_header.length = 0;
    yamux_encode_header(&ack_header, ack_frame);
    client_mock->write_buf_used = 0; /* Clear any existing data */
    memcpy(client_mock->write_buf, ack_frame, sizeof(ack_frame));
    client_mock->write_buf_used = sizeof(ack_frame);
    
    /* Exchange data client -> server again for ACK */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Server processes explicit ACK */
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session after ACK");
    
    state = yamux_stream_get_state(server_stream);
    assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                "Server stream should be in ESTABLISHED state after client processes ACK");
    
    /* TEST 4: Write data - should remain ESTABLISHED */
    size_t bytes_written;
    result = yamux_stream_write(client_stream, data, sizeof(data), &bytes_written);
    assert_true(result == YAMUX_OK, "Failed to write to client stream");
    
    state = yamux_stream_get_state(client_stream);
    assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                "Client stream should remain in ESTABLISHED state after write");
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session");
    
    /* Read data on server */
    result = yamux_stream_read(server_stream, read_buf, sizeof(read_buf), &bytes_read);
    assert_true(result == YAMUX_OK, "Failed to read from server stream");
    assert_true(bytes_read == sizeof(data), "Incorrect number of bytes read");
    assert_true(memcmp(data, read_buf, bytes_read) == 0, "Data mismatch");
    
    state = yamux_stream_get_state(server_stream);
    assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                "Server stream should remain in ESTABLISHED state after read");
    
    /* Exchange data server -> client */
    mock_io_swap_buffers(server_mock, client_mock);
    
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session");
    
    /* TEST 5: FIN_SENT (client closes stream) */
    result = yamux_stream_close(client_stream, 0);
    assert_true(result == YAMUX_OK, "Failed to close client stream");
    
    state = yamux_stream_get_state(client_stream);
    assert_true(state == YAMUX_STREAM_FIN_SENT, 
                "Client stream should be in FIN_SENT state");
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    result = yamux_session_process(server_session);
    assert_true(result == YAMUX_OK, "Failed to process server session");
    
    /* TEST 6: FIN_RECV (server receives FIN) */
    state = yamux_stream_get_state(server_stream);
    assert_true(state == YAMUX_STREAM_FIN_RECV, 
                "Server stream should be in FIN_RECV state");
    
    /* Try to read more data - should return EOF */
    result = yamux_stream_read(server_stream, read_buf, sizeof(read_buf), &bytes_read);
    assert_true(result == YAMUX_OK && bytes_read == 0, 
                "Read after FIN should return EOF (0 bytes)");
    
    /* TEST 7: Server closes its side */
    result = yamux_stream_close(server_stream, 0);
    assert_true(result == YAMUX_OK, "Failed to close server stream");
    
    state = yamux_stream_get_state(server_stream);
    assert_true(state == YAMUX_STREAM_CLOSED, 
                "Server stream should be in CLOSED state");
    
    /* Exchange data server -> client */
    mock_io_swap_buffers(server_mock, client_mock);
    
    result = yamux_session_process(client_session);
    assert_true(result == YAMUX_OK, "Failed to process client session");
    
    /* TEST 8: CLOSED (client receives FIN) */
    state = yamux_stream_get_state(client_stream);
    assert_true(state == YAMUX_STREAM_CLOSED, 
                "Client stream should be in CLOSED state");
    
    /* TEST 9: Clean up */
    result = yamux_session_close(client_session, 0);
    assert_true(result == YAMUX_OK, "Failed to close client session");
    
    result = yamux_session_close(server_session, 0);
    assert_true(result == YAMUX_OK, "Failed to close server session");
    
    /* Free resources */
    mock_io_free(client_mock);
    mock_io_free(server_mock);
}
