/**
 * @file test_frame.c
 * @brief Test for yamux frame encoding and decoding
 */

#include "test_common.h"

/* Test frame encoding and decoding */
void test_frame_encoding(void) {
    yamux_header_t header_in, header_out;
    uint8_t buffer[12];  /* 8-byte header + 4-byte optional data */
    yamux_result_t result;
    
    printf("Testing frame encoding and decoding...\n");
    
    /* Test DATA frame */
    printf("  Testing DATA frame...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = YAMUX_PROTO_VERSION;
    header_in.type = YAMUX_DATA;
    header_in.flags = 0;
    header_in.stream_id = 1;
    header_in.length = 1024;
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_OK);
    
    assert(header_out.version == header_in.version);
    assert(header_out.type == header_in.type);
    assert(header_out.flags == header_in.flags);
    assert(header_out.stream_id == header_in.stream_id);
    assert(header_out.length == header_in.length);
    
    /* Test WINDOW_UPDATE frame with SYN flag */
    printf("  Testing WINDOW_UPDATE frame with SYN flag...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = YAMUX_PROTO_VERSION;
    header_in.type = YAMUX_WINDOW_UPDATE;
    header_in.flags = YAMUX_FLAG_SYN;
    header_in.stream_id = 2;
    header_in.length = 4;
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_OK);
    
    assert(header_out.version == header_in.version);
    assert(header_out.type == header_in.type);
    assert(header_out.flags == header_in.flags);
    assert(header_out.stream_id == header_in.stream_id);
    assert(header_out.length == header_in.length);
    
    /* Test PING frame */
    printf("  Testing PING frame...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = YAMUX_PROTO_VERSION;
    header_in.type = YAMUX_PING;
    header_in.flags = YAMUX_FLAG_ACK;
    header_in.stream_id = 0;
    header_in.length = 0;
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_OK);
    
    assert(header_out.version == header_in.version);
    assert(header_out.type == header_in.type);
    assert(header_out.flags == header_in.flags);
    assert(header_out.stream_id == header_in.stream_id);
    assert(header_out.length == header_in.length);
    
    /* Test GO_AWAY frame */
    printf("  Testing GO_AWAY frame...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = YAMUX_PROTO_VERSION;
    header_in.type = YAMUX_GO_AWAY;
    header_in.flags = 0;
    header_in.stream_id = 0;
    header_in.length = 4;
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_OK);
    
    assert(header_out.version == header_in.version);
    assert(header_out.type == header_in.type);
    assert(header_out.flags == header_in.flags);
    assert(header_out.stream_id == header_in.stream_id);
    assert(header_out.length == header_in.length);
    
    /* Test invalid version */
    printf("  Testing invalid version...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = 0xFF;  /* Invalid version */
    header_in.type = YAMUX_DATA;
    header_in.flags = 0;
    header_in.stream_id = 1;
    header_in.length = 0;
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_ERR_PROTOCOL);
    
    /* Test big values */
    printf("  Testing big values...\n");
    memset(&header_in, 0, sizeof(header_in));
    header_in.version = YAMUX_PROTO_VERSION;
    header_in.type = YAMUX_DATA;
    header_in.flags = 0xFFFF;  /* All flags set */
    header_in.stream_id = 0xFFFFFFFF;  /* Max stream ID */
    header_in.length = 0xFFFFFFFF;  /* Max length */
    
    result = yamux_encode_header(&header_in, buffer);
    assert(result == YAMUX_OK);
    
    result = yamux_decode_header(buffer, 12, &header_out);
    assert(result == YAMUX_OK);
    
    assert(header_out.version == header_in.version);
    assert(header_out.type == header_in.type);
    assert(header_out.flags == header_in.flags);
    assert(header_out.stream_id == header_in.stream_id);
    assert(header_out.length == header_in.length);
    
    printf("Frame encoding and decoding tests passed!\n");
}

/* Test runner moved to test_main.c */
