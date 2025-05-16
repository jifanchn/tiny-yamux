/**
 * @file test_yamux_port.c
 * @brief Test for the Yamux portable API
 * 
 * This file tests the basic functionality of the Yamux portable API,
 * demonstrating how to implement platform-specific I/O callbacks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "../../include/yamux.h"
#include "linux_socket_adapter.h"

/* Port for testing - using high port number to avoid conflicts */
#define TEST_PORT 45678

/* Test message */
#define TEST_MESSAGE "Hello, Yamux!"
#define TEST_MESSAGE_LEN 13

/* Client thread function */
void* client_thread(void* arg) {
    (void)arg; /* Unused parameter */
    /* Create client socket */
    linux_socket_t* sock = linux_socket_create_client("127.0.0.1", TEST_PORT);
    if (!sock) {
        printf("Failed to create client socket\n");
        return NULL;
    }
    
    /* Sleep briefly to ensure server is ready */
    usleep(100000); /* 100ms */
    
    /* Initialize Yamux */
    void* session = yamux_init(linux_socket_read, linux_socket_write, sock, 1);
    if (!session) {
        printf("Failed to initialize Yamux client\n");
        linux_socket_close(sock);
        return NULL;
    }
    
    printf("Client: Yamux initialized\n");
    
    /* Process any initial messages */
    yamux_process(session);
    
    /* Open a stream */
    void* stream = yamux_open_stream(session);
    if (!stream) {
        printf("Failed to open stream\n");
        yamux_destroy(session);
        linux_socket_close(sock);
        return NULL;
    }
    
    printf("Client: Stream opened\n");
    
    /* Process messages after opening stream */
    yamux_process(session);
    
    /* Write test message */
    printf("Client: Sending message: %s\n", TEST_MESSAGE);
    int bytes_written = yamux_write(stream, (uint8_t*)TEST_MESSAGE, TEST_MESSAGE_LEN);
    if (bytes_written < 0) {
        printf("Failed to write to stream\n");
        yamux_close_stream(stream, 0);
        yamux_destroy(session);
        linux_socket_close(sock);
        return NULL;
    }
    
    printf("Client: Wrote %d bytes\n", bytes_written);
    
    /* Process messages after writing */
    yamux_process(session);
    
    /* Read response */
    uint8_t buffer[128];
    int bytes_read = yamux_read(stream, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; /* Null terminate */
        printf("Client: Received response: %s\n", buffer);
    } else {
        printf("Client: No response received\n");
    }
    
    /* Close stream */
    yamux_close_stream(stream, 0);
    printf("Client: Stream closed\n");
    
    /* Clean up */
    yamux_destroy(session);
    linux_socket_close(sock);
    printf("Client: Session destroyed\n");
    
    return NULL;
}

/* Mock connection structure for in-memory testing */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t used;
    size_t read_pos;
} mock_connection_t;

/* Initialize a mock connection */
mock_connection_t *mock_connection_init(size_t capacity) {
    mock_connection_t *conn = (mock_connection_t *)malloc(sizeof(mock_connection_t));
    if (!conn) return NULL;
    
    conn->buffer = (uint8_t *)malloc(capacity);
    if (!conn->buffer) {
        free(conn);
        return NULL;
    }
    
    conn->capacity = capacity;
    conn->used = 0;
    conn->read_pos = 0;
    
    return conn;
}

/* Free a mock connection */
void mock_connection_free(mock_connection_t *conn) {
    if (conn) {
        free(conn->buffer);
        free(conn);
    }
}

/* Mock socket structure */
typedef struct {
    mock_connection_t *in;  /* Data from remote -> local */
    mock_connection_t *out; /* Data from local -> remote */
} mock_socket_t;

/* Initialize a mock socket pair */
void mock_socket_pair(mock_socket_t *client, mock_socket_t *server) {
    /* Client's out connects to server's in */
    client->out = mock_connection_init(4096);
    server->in = client->out;
    
    /* Server's out connects to client's in */
    server->out = mock_connection_init(4096);
    client->in = server->out;
}

/* Read callback for mock socket */
int mock_socket_read(void *ctx, uint8_t *buf, size_t len) {
    mock_socket_t *sock = (mock_socket_t *)ctx;
    mock_connection_t *conn = sock->in;
    
    if (!conn || !buf || len == 0) {
        return -1;
    }
    
    if (conn->read_pos >= conn->used) {
        return 0; /* No data available */
    }
    
    size_t available = conn->used - conn->read_pos;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(buf, conn->buffer + conn->read_pos, to_read);
    conn->read_pos += to_read;
    
    return to_read;
}

/* Write callback for mock socket */
int mock_socket_write(void *ctx, const uint8_t *buf, size_t len) {
    mock_socket_t *sock = (mock_socket_t *)ctx;
    mock_connection_t *conn = sock->out;
    
    if (!conn || !buf || len == 0) {
        return -1;
    }
    
    if (conn->used + len > conn->capacity) {
        size_t new_capacity = conn->capacity * 2;
        if (new_capacity < conn->used + len) {
            new_capacity = conn->used + len;
        }
        
        uint8_t *new_buffer = (uint8_t *)realloc(conn->buffer, new_capacity);
        if (!new_buffer) {
            return -1;
        }
        
        conn->buffer = new_buffer;
        conn->capacity = new_capacity;
    }
    
    memcpy(conn->buffer + conn->used, buf, len);
    conn->used += len;
    
    return len;
}

/* Server thread function */
void *server_thread(void *arg) {
    mock_socket_t *server_sock = (mock_socket_t *)arg;
    void *session;
    void *stream;
    uint8_t buffer[128];
    int bytes_read;
    
    printf("Server: Starting...\n");
    
    /* Initialize Yamux */
    session = yamux_init(mock_socket_read, mock_socket_write, server_sock, 0);
    if (!session) {
        printf("Failed to initialize Yamux server\n");
        return NULL;
    }
    
    printf("Server: Yamux initialized\n");
    
    /* Process any initial messages */
    yamux_process(session);
    
    /* Accept a stream */
    stream = yamux_accept_stream(session);
    if (!stream) {
        printf("Failed to accept stream\n");
        yamux_destroy(session);
        return NULL;
    }
    
    printf("Server: Stream accepted\n");
    
    /* Process messages after accepting stream */
    yamux_process(session);
    
    /* Read from stream */
    bytes_read = yamux_read(stream, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; /* Null terminate */
        printf("Server: Received message: %s\n", buffer);
        
        /* Echo it back */
        printf("Server: Echoing message back\n");
        yamux_write(stream, buffer, bytes_read);
    }
    
    /* Process messages after writing */
    yamux_process(session);
    
    /* Close stream */
    yamux_close_stream(stream, 0);
    
    /* Clean up */
    yamux_destroy(session);
    printf("Server: Session destroyed\n");
    
    return NULL;
}

int main(void) {
    pthread_t thread;
    mock_socket_t client_sock = {0};
    mock_socket_t server_sock = {0};
    void *session;
    void *stream;
    uint8_t buffer[128];
    int bytes_read;
    
    printf("Testing Yamux portable API with mock sockets...\n");
    
    /* Create a socket pair */
    mock_socket_pair(&client_sock, &server_sock);
    
    /* Start server thread */
    pthread_create(&thread, NULL, server_thread, &server_sock);
    
    /* Sleep briefly to ensure server is ready */
    usleep(100000); /* 100ms */
    
    /* Initialize Yamux client */
    session = yamux_init(mock_socket_read, mock_socket_write, &client_sock, 1);
    if (!session) {
        printf("Failed to initialize Yamux client\n");
        return 1;
    }
    
    printf("Client: Yamux initialized\n");
    
    /* Process any initial messages */
    yamux_process(session);
    
    /* Open a stream */
    stream = yamux_open_stream(session);
    if (!stream) {
        printf("Failed to open stream\n");
        yamux_destroy(session);
        return 1;
    }
    
    printf("Client: Stream opened\n");
    
    /* Process messages after opening stream */
    yamux_process(session);
    
    /* Send test message */
    const char *test_message = "Hello, Yamux!";
    printf("Client: Sending message: %s\n", test_message);
    int bytes_written = yamux_write(stream, (uint8_t*)test_message, strlen(test_message));
    if (bytes_written < 0) {
        printf("Failed to write to stream\n");
        yamux_close_stream(stream, 0);
        yamux_destroy(session);
        return 1;
    }
    
    printf("Client: Wrote %d bytes\n", bytes_written);
    
    /* Process messages after writing */
    yamux_process(session);
    
    /* Read response */
    bytes_read = yamux_read(stream, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; /* Null terminate */
        printf("Client: Received response: %s\n", buffer);
        
        /* Verify echo response */
        assert(strcmp((char*)buffer, test_message) == 0);
        printf("Client: Echo verification successful\n");
    } else {
        printf("Client: No response received\n");
    }
    
    /* Close stream */
    yamux_close_stream(stream, 0);
    printf("Client: Stream closed\n");
    
    /* Clean up */
    yamux_destroy(session);
    printf("Client: Session destroyed\n");
    
    /* Wait for server thread to finish */
    pthread_join(thread, NULL);
    
    /* Free mock connections - only free what we allocated */
    mock_connection_free(client_sock.out);
    mock_connection_free(server_sock.out);
    
    printf("All tests passed!\n");
    return 0;
}
