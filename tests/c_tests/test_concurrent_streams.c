#include "../../src/yamux_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mock_io.h"

/* External assert function declaration */
void assert_true(int condition, const char *message);

/* Test concurrent streams */
void test_concurrent_streams(void) {
    printf("Testing concurrent streams...\n");
    yamux_session_t *client_session, *server_session;
    yamux_io_t client_io, server_io;
    mock_io_t *client_mock, *server_mock;
    yamux_config_t config;
    yamux_result_t result;
    yamux_stream_t *client_streams[10];
    yamux_stream_t *server_streams[10];
    uint8_t data[10][64];
    uint8_t read_buf[64];
    size_t bytes_read;
    int i, j;
    const int num_streams = 10;
    
    /* Initialize data for each stream */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < (int)sizeof(data[0]); j++) {
            data[i][j] = (i * 16 + j) & 0xFF;
        }
    }
    
    /* Initialize mock IOs */
    client_mock = mock_io_init(8192);
    server_mock = mock_io_init(8192);
    
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
    config.max_stream_window_size = 262144; /* Set an appropriate window size */
    
    /* Create client and server sessions */
    result = yamux_session_create(&client_io, 1, &config, &client_session);
    assert_true(result == YAMUX_OK, "Failed to create client session");
    
    result = yamux_session_create(&server_io, 0, &config, &server_session);
    assert_true(result == YAMUX_OK, "Failed to create server session");
    
    /* Open multiple streams from client */
    for (i = 0; i < num_streams; i++) {
        result = yamux_stream_open_detailed(client_session, 0, &client_streams[i]);
        assert_true(result == YAMUX_OK, "Failed to open client stream");
    }
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Process all SYN messages on server */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(server_session);
        assert_true(result == YAMUX_OK, "Failed to process server session");
        
        /* Accept the stream on server */
        result = yamux_stream_accept(server_session, &server_streams[i]);
        assert_true(result == YAMUX_OK, "Failed to accept server stream");
        
        /* Verify stream IDs match expected pattern */
        uint32_t client_id = yamux_stream_get_id(client_streams[i]);
        uint32_t server_id = yamux_stream_get_id(server_streams[i]);
        
        assert_true(client_id == server_id, "Stream IDs don't match");
        assert_true(client_id == (uint32_t)(i * 2 + 1), "Stream ID doesn't follow expected pattern");
    }
    
    /* Exchange data server -> client */
    mock_io_swap_buffers(server_mock, client_mock);
    
    /* Process all ACK messages on client */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(client_session);
        assert_true(result == YAMUX_OK, "Failed to process client session");
        
        /* Send explicit ACK back to server for each stream */
        yamux_header_t ack_header;
        uint8_t ack_frame[12];
        memset(&ack_header, 0, sizeof(ack_header));
        ack_header.version = YAMUX_PROTO_VERSION;
        ack_header.type = YAMUX_WINDOW_UPDATE;
        ack_header.flags = YAMUX_FLAG_ACK;
        ack_header.stream_id = client_streams[i]->id;
        ack_header.length = 0;
        yamux_encode_header(&ack_header, ack_frame);
        
        /* Append the ACK frame to client's write buffer */
        memcpy(client_mock->write_buf + client_mock->write_buf_used, ack_frame, sizeof(ack_frame));
        client_mock->write_buf_used += sizeof(ack_frame);
    }
    
    /* Exchange data client -> server to deliver ACKs */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Process all ACK messages on server */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(server_session);
        assert_true(result == YAMUX_OK, "Failed to process server session ACK");
    }
    
    /* All streams should be established now */
    for (i = 0; i < num_streams; i++) {
        yamux_stream_state_t state = yamux_stream_get_state(client_streams[i]);
        assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                    "Client stream not in ESTABLISHED state");
        
        state = yamux_stream_get_state(server_streams[i]);
        assert_true(state == YAMUX_STREAM_ESTABLISHED, 
                    "Server stream not in ESTABLISHED state");
    }
    
    /* Write data on all streams from client to server */
    for (i = 0; i < num_streams; i++) {
        size_t bytes_written;
        result = yamux_stream_write(client_streams[i], data[i], sizeof(data[i]), &bytes_written);
        assert_true(result == YAMUX_OK, "Failed to write to client stream");
    }
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Process all data messages on server */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(server_session);
        assert_true(result == YAMUX_OK, "Failed to process server session");
    }
    
    /* Read data on all streams on server */
    for (i = 0; i < num_streams; i++) {
        result = yamux_stream_read(server_streams[i], read_buf, sizeof(read_buf), &bytes_read);
        assert_true(result == YAMUX_OK, "Failed to read from server stream");
        assert_true(bytes_read == sizeof(data[i]), "Incorrect number of bytes read");
        assert_true(memcmp(data[i], read_buf, bytes_read) == 0, "Data mismatch");
    }
    
    /* Exchange data server -> client for window updates */
    mock_io_swap_buffers(server_mock, client_mock);
    
    /* Process all window update messages on client */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(client_session);
        assert_true(result == YAMUX_OK, "Failed to process client session");
    }
    
    /* Now close all streams from both sides */
    for (i = 0; i < num_streams; i++) {
        result = yamux_stream_close(client_streams[i], 0);
        assert_true(result == YAMUX_OK, "Failed to close client stream");
    }
    
    /* Exchange data client -> server */
    mock_io_swap_buffers(client_mock, server_mock);
    
    /* Process all FIN messages on server */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(server_session);
        assert_true(result == YAMUX_OK, "Failed to process server session");
    }
    
    /* Close all server-side streams */
    for (i = 0; i < num_streams; i++) {
        result = yamux_stream_close(server_streams[i], 0);
        assert_true(result == YAMUX_OK, "Failed to close server stream");
    }
    
    /* Exchange data server -> client */
    mock_io_swap_buffers(server_mock, client_mock);
    
    /* Process all FIN messages on client */
    for (i = 0; i < num_streams; i++) {
        result = yamux_session_process(client_session);
        assert_true(result == YAMUX_OK, "Failed to process client session");
    }
    
    /* All streams should be closed now */
    for (i = 0; i < num_streams; i++) {
        yamux_stream_state_t state = yamux_stream_get_state(client_streams[i]);
        assert_true(state == YAMUX_STREAM_CLOSED, 
                    "Client stream not in CLOSED state");
        
        state = yamux_stream_get_state(server_streams[i]);
        assert_true(state == YAMUX_STREAM_CLOSED, 
                    "Server stream not in CLOSED state");
    }
    
    /* Clean up */
    result = yamux_session_close(client_session, 0);
    assert_true(result == YAMUX_OK, "Failed to close client session");
    
    result = yamux_session_close(server_session, 0);
    assert_true(result == YAMUX_OK, "Failed to close server session");
    
    mock_io_free(client_mock);
    mock_io_free(server_mock);
}
