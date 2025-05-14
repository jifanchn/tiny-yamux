/**
 * @file yamux_config.h
 * @brief Configuration options for tiny-yamux
 * 
 * This file contains configurable parameters that can be adjusted
 * to customize the behavior and resource usage of the tiny-yamux library.
 * Users can modify these values to suit their specific platform and
 * resource constraints.
 */

#ifndef YAMUX_CONFIG_H
#define YAMUX_CONFIG_H

/* Protocol version */
#define YAMUX_VERSION 0

/**
 * Memory allocation configuration
 * Uncomment to use static memory allocation instead of dynamic allocation
 */
/* #define YAMUX_STATIC_MEMORY */

#ifdef YAMUX_STATIC_MEMORY
/**
 * Define your own memory allocation functions
 * These are only used if YAMUX_STATIC_MEMORY is defined
 */
void *yamux_alloc(size_t size);
void yamux_free(void *ptr);
#endif

/**
 * Buffer size configuration
 */
/* Initial receive buffer size for streams */
#define YAMUX_INITIAL_BUFFER_SIZE 4096

/* Default window size for flow control */
#define YAMUX_DEFAULT_WINDOW_SIZE (256 * 1024)

/**
 * Session configuration defaults
 */
/* Default accept backlog size */
#define YAMUX_DEFAULT_ACCEPT_BACKLOG 256

/* Default keepalive enabled/disabled */
#define YAMUX_DEFAULT_KEEPALIVE_ENABLE 1

/* Default connection write timeout in milliseconds */
#define YAMUX_DEFAULT_CONN_WRITE_TIMEOUT 30000

/* Default keepalive interval in milliseconds */
#define YAMUX_DEFAULT_KEEPALIVE_INTERVAL 60000

/**
 * Maximum stream configuration
 */
/* Maximum number of concurrent streams per session */
#define YAMUX_MAX_STREAMS 1024

/* Maximum stream ID value */
#define YAMUX_MAX_STREAM_ID 0x7FFFFFFF

/**
 * Debug configuration
 */
/* Uncomment to enable debug logging */
/* #define YAMUX_DEBUG */

#ifdef YAMUX_DEBUG
/**
 * Define your own debug logging function
 */
void yamux_debug_log(const char *format, ...);
#endif

#endif /* YAMUX_CONFIG_H */
