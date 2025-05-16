#ifndef CGO_HELPERS_H
#define CGO_HELPERS_H

#include "../../../include/yamux.h" 
#include <stddef.h>      
#include <stdint.h>      

typedef struct {
    int read_fd;
    int write_fd;
} cgo_pipe_ctx_t;

int cgo_read_callback(void *io_ctx, uint8_t *buf, size_t len);
int cgo_write_callback(void *user_ctx, const uint8_t *buf, size_t len);
yamux_session_t* init_c_session_public(void* user_io_ctx, int is_client);
int process_c_messages_public(yamux_session_t* session_handle);
yamux_stream_t* open_c_stream_public(yamux_session_t* session_handle);
yamux_stream_t* accept_c_stream_public(yamux_session_t* session_handle);
int write_c_stream_public(yamux_stream_t* stream_handle, const char *buf, size_t len);
int read_c_stream_public(yamux_stream_t* stream_handle, char *buf, size_t len);
int close_c_stream_public(yamux_stream_t* stream_handle, int reset_reason);
void destroy_c_session_public(yamux_session_t* session_handle);

// Declaration for the dummy C function
void dummy_c_function();

#endif // CGO_HELPERS_H
