/**
 * @file yamux_defs.h
 * @brief Common definitions for yamux implementation
 */

#ifndef YAMUX_DEFS_H
#define YAMUX_DEFS_H

#include "../include/yamux.h"

/* Protocol constants */
#define YAMUX_PROTO_VERSION 0

/* Frame types */
#define YAMUX_DATA          0x0
#define YAMUX_WINDOW_UPDATE 0x1
#define YAMUX_PING          0x2
#define YAMUX_GO_AWAY       0x3

/* Frame flags */
#define YAMUX_FLAG_SYN      0x1
#define YAMUX_FLAG_ACK      0x2
#define YAMUX_FLAG_FIN      0x4
#define YAMUX_FLAG_RST      0x8

/* Frame format */
#define YAMUX_HEADER_SIZE   12  /* 8 bytes for header + 4 bytes for length */

/* Flow control */
#define YAMUX_DEFAULT_WINDOW_SIZE (256 * 1024)  /* 256 KB */
#define YAMUX_WINDOW_UPDATE_THRESHOLD (YAMUX_DEFAULT_WINDOW_SIZE / 2)  /* 50% threshold for window update */

/* Initial buffer size */
#define YAMUX_INITIAL_BUFFER_SIZE 4096

/* Stream states are defined in yamux.h */

#define YAMUX_MAX_DATA_FRAME_SIZE 16384 /* 16KB, max payload for a single DATA frame */

#endif /* YAMUX_DEFS_H */
