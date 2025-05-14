/**
 * @file simple_demo.c
 * @brief Simple example demonstrating tiny-yamux usage
 * 
 * This example creates two endpoints (client and server) using
 * pipes for communication and demonstrates basic stream operations.
 */

#include "../include/yamux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* IO context structure */
typedef struct {
    int read_fd;    /* File descriptor for reading */
    int write_fd;   /* File descriptor for writing */
} io_context_t;

/* Read callback function for yamux */
int demo_read(void *ctx, uint8_t *buf, size_t len) {
    io_context_t *io_ctx = (io_context_t *)ctx;
    ssize_t result = read(io_ctx->read_fd, buf, len);
    return (result < 0) ? -1 : (int)result;
}

/* Write callback function for yamux */
int demo_write(void *ctx, const uint8_t *buf, size_t len) {
    io_context_t *io_ctx = (io_context_t *)ctx;
    ssize_t result = write(io_ctx->write_fd, buf, len);
    return (result < 0) ? -1 : (int)result;
}

/* Server thread function */
void *server_thread(void *arg) {
    io_context_t *io_ctx = (io_context_t *)arg;
    void *session;
    void *stream;
    char buffer[1024];
    int result;
    
    printf("Server: Initializing yamux session\n");
    
    /* Initialize yamux session (server mode) */
    session = yamux_init(demo_read, demo_write, io_ctx, 0);
    if (!session) {
        printf("Server: Failed to initialize yamux session\n");
        return NULL;
    }
    
    printf("Server: Waiting for client connection\n");
    
    /* Process messages until we get a stream */
    stream = NULL;
    while (!stream) {
        /* Process incoming messages */
        result = yamux_process(session);
        if (result < 0) {
            printf("Server: Error processing messages\n");
            yamux_destroy(session);
            return NULL;
        }
        
        /* Try to accept a stream */
        stream = yamux_accept_stream(session);
        
        /* Short delay to avoid busy waiting */
        usleep(10000);  /* 10ms */
    }
    
    printf("Server: Accepted stream with ID %u\n", yamux_get_stream_id(stream));
    
    /* Read message from client */
    result = yamux_read(stream, (uint8_t *)buffer, sizeof(buffer) - 1);
    if (result > 0) {
        buffer[result] = '\0';
        printf("Server: Received message: %s\n", buffer);
    }
    
    /* Send response to client */
    const char *response = "Hello from server!";
    result = yamux_write(stream, (const uint8_t *)response, strlen(response));
    printf("Server: Sent response (%d bytes)\n", result);
    
    /* Close stream */
    printf("Server: Closing stream\n");
    yamux_close_stream(stream, 0);
    
    /* Clean up */
    printf("Server: Cleaning up\n");
    yamux_destroy(session);
    
    return NULL;
}

int main(void) {
    int client_to_server[2];  /* Pipe for client-to-server communication */
    int server_to_client[2];  /* Pipe for server-to-client communication */
    io_context_t client_ctx, server_ctx;
    void *client_session;
    void *client_stream;
    char buffer[1024];
    int result;
    pthread_t server_tid;
    
    /* Create pipes for communication */
    if (pipe(client_to_server) < 0 || pipe(server_to_client) < 0) {
        perror("Failed to create pipes");
        return 1;
    }
    
    /* Set up IO contexts */
    client_ctx.read_fd = server_to_client[0];  /* Client reads from server */
    client_ctx.write_fd = client_to_server[1]; /* Client writes to server */
    server_ctx.read_fd = client_to_server[0];  /* Server reads from client */
    server_ctx.write_fd = server_to_client[1]; /* Server writes to client */
    
    /* Start server thread */
    pthread_create(&server_tid, NULL, server_thread, &server_ctx);
    
    /* Initialize client yamux session */
    printf("Client: Initializing yamux session\n");
    client_session = yamux_init(demo_read, demo_write, &client_ctx, 1);
    if (!client_session) {
        printf("Client: Failed to initialize yamux session\n");
        return 1;
    }
    
    /* Open stream to server */
    printf("Client: Opening stream to server\n");
    client_stream = yamux_open_stream(client_session);
    if (!client_stream) {
        printf("Client: Failed to open stream\n");
        yamux_destroy(client_session);
        return 1;
    }
    
    printf("Client: Opened stream with ID %u\n", yamux_get_stream_id(client_stream));
    
    /* Send message to server */
    const char *message = "Hello from client!";
    result = yamux_write(client_stream, (const uint8_t *)message, strlen(message));
    printf("Client: Sent message (%d bytes)\n", result);
    
    /* Process incoming messages to handle the server's response */
    while (1) {
        /* Process messages */
        result = yamux_process(client_session);
        if (result < 0) {
            printf("Client: Error processing messages\n");
            break;
        }
        
        /* Try to read response */
        result = yamux_read(client_stream, (uint8_t *)buffer, sizeof(buffer) - 1);
        if (result > 0) {
            buffer[result] = '\0';
            printf("Client: Received response: %s\n", buffer);
            break;
        }
        
        /* Short delay to avoid busy waiting */
        usleep(10000);  /* 10ms */
    }
    
    /* Close stream */
    printf("Client: Closing stream\n");
    yamux_close_stream(client_stream, 0);
    
    /* Clean up */
    printf("Client: Cleaning up\n");
    yamux_destroy(client_session);
    
    /* Wait for server thread to exit */
    pthread_join(server_tid, NULL);
    
    /* Close pipes */
    close(client_to_server[0]);
    close(client_to_server[1]);
    close(server_to_client[0]);
    close(server_to_client[1]);
    
    printf("Demo completed successfully\n");
    
    return 0;
}
