#include "cgo_helpers.h"
#include <stdio.h>  // For fprintf, stderr, fflush, perror
#include <stdlib.h> // For NULL, malloc, free
#include <string.h> // For memcpy, strerror
#include <unistd.h> // For read/write (assuming POSIX)
#include <errno.h>  // For errno
// Note: yamux.h (included via cgo_helpers.h) defines yamux_session_t, yamux_stream_t, etc.

// Callback for yamux to read data (called by C library, implemented by Go via CGO)
// This function is passed to yamux_init. It's called when yamux needs to read from the underlying transport.
// It should return the number of bytes read, or a YAMUX_ERR_* code on error.
int cgo_read_callback(void *user_ctx, uint8_t *buf, size_t len) {
    cgo_pipe_ctx_t* pipe_ctx = (cgo_pipe_ctx_t*)user_ctx;
    ssize_t bytes = read(pipe_ctx->read_fd, buf, len);
    if (bytes < 0) {
        int current_errno = errno; // Capture errno immediately
        if (current_errno == EAGAIN || current_errno == EWOULDBLOCK) {
            return YAMUX_ERR_WOULD_BLOCK;
        }
        fprintf(stderr, "cgo_read_callback read error: fd=%d, len=%zu, errno=%d (%s)\n", 
                pipe_ctx->read_fd, len, current_errno, strerror(current_errno));
        fflush(stderr);
        return YAMUX_ERR_IO; // Use defined error code
    }
    return (int)bytes;
}

// Callback for yamux to write data (called by C library, implemented by Go via CGO)
// This function is passed to yamux_init. It's called when yamux needs to write to the underlying transport.
// It should return the number of bytes written, or a YAMUX_ERR_* code on error.
int cgo_write_callback(void* user_ctx, const uint8_t* buf, size_t len) {
    cgo_pipe_ctx_t* pipe_ctx = (cgo_pipe_ctx_t*)user_ctx;
    ssize_t n = write(pipe_ctx->write_fd, buf, len);
    if (n < 0) {
        int current_errno = errno; // Capture errno immediately
        if (current_errno == EAGAIN || current_errno == EWOULDBLOCK) {
            return YAMUX_ERR_WOULD_BLOCK;
        }
        fprintf(stderr, "cgo_write_callback write error: fd=%d, len=%zu, errno=%d (%s)\n", 
                pipe_ctx->write_fd, len, current_errno, strerror(current_errno));
        fflush(stderr); 
        return YAMUX_ERR_IO; // Use defined error code
    }
    return (int)n; // Return number of bytes written
}

// Changed return type and argument types to yamux_session_t* and yamux_stream_t*
yamux_session_t* init_c_session_public(void* user_io_ctx, int is_client) {
    // The user_io_ctx from Go is already a pointer to the cgoPipeCtx struct.
    // yamux_init expects this context for its callbacks.
    return (yamux_session_t*)yamux_init(cgo_read_callback, cgo_write_callback, user_io_ctx, is_client);
}

int process_c_messages_public(yamux_session_t* session_handle) {
    return yamux_process(session_handle);
}

yamux_stream_t* open_c_stream_public(yamux_session_t* session_handle) {
    return (yamux_stream_t*)yamux_open_stream(session_handle);
}

yamux_stream_t* accept_c_stream_public(yamux_session_t* session_handle) {
    return (yamux_stream_t*)yamux_accept_stream(session_handle);
}

int write_c_stream_public(yamux_stream_t* stream_handle, const char *buf, size_t len) {
    // yamux_write expects uint8_t*. Cast from const char*.
    int result = yamux_write(stream_handle, (const uint8_t*)buf, len);

    if (result == YAMUX_OK) return (int)len; // Successful write of all bytes asked

    // The specific YAMUX_ERR_STREAM_WINDOW_FULL and YAMUX_ERR_SESSION_SEND_WINDOW_FULL
    // are not defined in the public yamux.h. The underlying yamux_write should return
    // YAMUX_ERR_WOULD_BLOCK or another defined error if the window is full.
    // If yamux_write returns YAMUX_ERR_WOULD_BLOCK, this function will pass it through.
    // If specific handling for window full was intended beyond what YAMUX_ERR_WOULD_BLOCK conveys,
    // those error codes would need to be properly defined and returned by yamux_write.
    // For now, we rely on the error codes defined in yamux.h.

    // Return the yamux error code directly for other failures.
    // If result is YAMUX_ERR_WOULD_BLOCK, it will be returned.
    return result;
}

int read_c_stream_public(yamux_stream_t* stream_handle, char *buf, size_t len) {
    // yamux_read expects uint8_t*. Cast to char* is fine if buf is char*.
    return yamux_read(stream_handle, (uint8_t*)buf, len);
}

int close_c_stream_public(yamux_stream_t* stream_handle, int reset_reason) {
    // Assuming reset_reason 0 maps to YAMUX_NORMAL for a clean close.
    // If reset_reason can be other yamux_error_t values, it can be cast directly.
    // For simplicity, let's assume 0 is normal, non-zero is some form of reset.
    // The original yamux_close_stream expects yamux_error_t for the reason.
    // We might need a mapping if Go only passes simple ints.
    // Let's stick to yamux_result_t for the return type for consistency with other calls.
    yamux_error_t err_code = (reset_reason == 0) ? YAMUX_NORMAL : (yamux_error_t)reset_reason;
    return yamux_close_stream(stream_handle, err_code);
}

void destroy_c_session_public(yamux_session_t* session_handle) {
    yamux_destroy(session_handle);
}
