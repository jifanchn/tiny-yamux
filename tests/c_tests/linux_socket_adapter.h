/**
 * @file linux_socket_adapter.h
 * @brief Linux socket adapter for Yamux testing
 * 
 * This file provides a socket-based transport layer for testing Yamux on Linux.
 * It serves as an example of how to implement the platform-specific I/O callbacks
 * required by Yamux.
 * 
 * NOTE: This is for testing purposes only and should be replaced with platform-specific
 * implementations in actual production environments.
 */

#ifndef LINUX_SOCKET_ADAPTER_H
#define LINUX_SOCKET_ADAPTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

/**
 * Socket context structure
 */
typedef struct {
    int fd;                     /* Socket file descriptor */
    struct sockaddr_in addr;    /* Socket address */
    int is_server;              /* Is this a server socket? */
    int connected;              /* Is the socket connected? */
} linux_socket_t;

/**
 * Create a server socket
 * 
 * @param port Port to listen on
 * @return Socket context or NULL on error
 */
linux_socket_t* linux_socket_create_server(int port) {
    linux_socket_t* sock = malloc(sizeof(linux_socket_t));
    if (!sock) {
        return NULL;
    }
    
    memset(sock, 0, sizeof(linux_socket_t));
    sock->is_server = 1;
    
    /* Create socket */
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0) {
        free(sock);
        return NULL;
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sock->fd);
        free(sock);
        return NULL;
    }
    
    /* Bind to port */
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = INADDR_ANY;
    sock->addr.sin_port = htons(port);
    
    if (bind(sock->fd, (struct sockaddr*)&sock->addr, sizeof(sock->addr)) < 0) {
        close(sock->fd);
        free(sock);
        return NULL;
    }
    
    /* Listen for connections */
    if (listen(sock->fd, 5) < 0) {
        close(sock->fd);
        free(sock);
        return NULL;
    }
    
    return sock;
}

/**
 * Create a client socket
 * 
 * @param host Hostname or IP address to connect to
 * @param port Port to connect to
 * @return Socket context or NULL on error
 */
linux_socket_t* linux_socket_create_client(const char* host, int port) {
    linux_socket_t* sock = malloc(sizeof(linux_socket_t));
    if (!sock) {
        return NULL;
    }
    
    memset(sock, 0, sizeof(linux_socket_t));
    sock->is_server = 0;
    
    /* Create socket */
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0) {
        free(sock);
        return NULL;
    }
    
    /* Prepare address */
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &sock->addr.sin_addr) <= 0) {
        close(sock->fd);
        free(sock);
        return NULL;
    }
    
    /* Connect to server */
    if (connect(sock->fd, (struct sockaddr*)&sock->addr, sizeof(sock->addr)) < 0) {
        close(sock->fd);
        free(sock);
        return NULL;
    }
    
    sock->connected = 1;
    return sock;
}

/**
 * Accept a connection on a server socket
 * 
 * @param server Server socket context
 * @return New client socket context or NULL on error
 */
linux_socket_t* linux_socket_accept(linux_socket_t* server) {
    if (!server || !server->is_server) {
        return NULL;
    }
    
    linux_socket_t* client = malloc(sizeof(linux_socket_t));
    if (!client) {
        return NULL;
    }
    
    memset(client, 0, sizeof(linux_socket_t));
    client->is_server = 0;
    
    /* Accept connection */
    socklen_t addrlen = sizeof(client->addr);
    client->fd = accept(server->fd, (struct sockaddr*)&client->addr, &addrlen);
    if (client->fd < 0) {
        free(client);
        return NULL;
    }
    
    client->connected = 1;
    return client;
}

/**
 * Read data from a socket
 * 
 * @param ctx Socket context
 * @param buf Buffer to read into
 * @param len Maximum number of bytes to read
 * @return Number of bytes read, 0 on EOF, or -1 on error
 */
int linux_socket_read(void* ctx, uint8_t* buf, size_t len) {
    linux_socket_t* sock = (linux_socket_t*)ctx;
    if (!sock || !sock->connected) {
        return -1;
    }
    
    int bytes_read = recv(sock->fd, buf, len, 0);
    if (bytes_read < 0) {
        /* Would block is not an error for non-blocking sockets */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    
    return bytes_read;
}

/**
 * Write data to a socket
 * 
 * @param ctx Socket context
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written or -1 on error
 */
int linux_socket_write(void* ctx, const uint8_t* buf, size_t len) {
    linux_socket_t* sock = (linux_socket_t*)ctx;
    if (!sock || !sock->connected) {
        return -1;
    }
    
    int bytes_written = send(sock->fd, buf, len, 0);
    if (bytes_written < 0) {
        /* Would block is not an error for non-blocking sockets */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    
    return bytes_written;
}

/**
 * Close and free a socket
 * 
 * @param sock Socket context
 */
void linux_socket_close(linux_socket_t* sock) {
    if (sock) {
        if (sock->fd >= 0) {
            close(sock->fd);
        }
        free(sock);
    }
}

#endif /* LINUX_SOCKET_ADAPTER_H */
