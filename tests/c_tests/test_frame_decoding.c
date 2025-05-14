#include "../../src/yamux_internal.h"
#include "../../src/yamux_defs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* External assert function declaration */
void assert_true(int condition, const char *message);

/* Test dedicated to frame decoding with various edge cases */
void test_frame_decoding(void) {
    yamux_header_t header;
    uint8_t buffer[YAMUX_HEADER_SIZE];
    uint8_t invalid_buffer[7]; /* Too small to be valid - header is 8 bytes */
    yamux_result_t result;
    
    /* Test with NULL parameters */
    result = yamux_decode_header(NULL, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_ERR_INVALID, "NULL buffer should return INVALID error");
    
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, NULL);
    assert_true(result == YAMUX_ERR_INVALID, "NULL header should return INVALID error");
    
    /* Test with invalid buffer size */
    result = yamux_decode_header(invalid_buffer, 7, &header);
    assert_true(result == YAMUX_ERR_INVALID, "Buffer too small should return INVALID error");
    
    /* Test with invalid protocol version */
    memset(buffer, 0, YAMUX_HEADER_SIZE);
    buffer[0] = 0xFF; /* Invalid version */
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_ERR_PROTOCOL, "Invalid version should return PROTOCOL error");
    
    /* Test with invalid type */
    memset(buffer, 0, YAMUX_HEADER_SIZE);
    buffer[0] = YAMUX_PROTO_VERSION;
    buffer[1] = 0xFF; /* Invalid type */
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_ERR_PROTOCOL, "Invalid type should return PROTOCOL error");
    
    /* Test with valid DATA frame */
    memset(buffer, 0, YAMUX_HEADER_SIZE);
    buffer[0] = YAMUX_PROTO_VERSION;
    buffer[1] = YAMUX_DATA;
    buffer[2] = 0x00; /* Flags high byte */
    buffer[3] = 0x03; /* Flags low byte = SYN(1) | ACK(2) = 0x3 */
    buffer[4] = 0x00; /* Stream ID high byte */
    buffer[5] = 0x00;
    buffer[6] = 0x00;
    buffer[7] = 0x0A; /* Stream ID = 10 */
    buffer[8] = 0x00; /* Length high byte */
    buffer[9] = 0x00;
    buffer[10] = 0x04;
    buffer[11] = 0x00; /* Length = 1024 */
    
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_OK, "Valid DATA frame should decode correctly");
    assert_true(header.version == YAMUX_PROTO_VERSION, "Version should match");
    assert_true(header.type == YAMUX_DATA, "Type should match");
    assert_true(header.flags == (YAMUX_FLAG_SYN | YAMUX_FLAG_ACK), "Flags should match");
    assert_true(header.stream_id == 10, "Stream ID should match");
    assert_true(header.length == 1024, "Length should match");
    
    /* Test with all bytes set to maximum values */
    memset(buffer, 0xFF, YAMUX_HEADER_SIZE);
    buffer[0] = YAMUX_PROTO_VERSION; /* Valid version */
    buffer[1] = YAMUX_PING; /* Valid type */
    
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_OK, "Max values should decode correctly");
    assert_true(header.version == YAMUX_PROTO_VERSION, "Version should match");
    assert_true(header.type == YAMUX_PING, "Type should match");
    assert_true(header.flags == 0xFFFF, "Flags should match");
    assert_true(header.stream_id == 0xFFFFFFFF, "Stream ID should match");
    assert_true(header.length == 0xFFFFFFFF, "Length should match");
    
    /* Test byte order (endianness) */
    memset(buffer, 0, YAMUX_HEADER_SIZE);
    buffer[0] = YAMUX_PROTO_VERSION;
    buffer[1] = YAMUX_DATA;
    /* Flags: 0x1234 in big-endian */
    buffer[2] = 0x12;
    buffer[3] = 0x34;
    /* Stream ID: 0x12345678 in big-endian */
    buffer[4] = 0x12;
    buffer[5] = 0x34;
    buffer[6] = 0x56;
    buffer[7] = 0x78;
    /* Length: 0x87654321 in big-endian */
    buffer[8] = 0x87;
    buffer[9] = 0x65;
    buffer[10] = 0x43;
    buffer[11] = 0x21;
    
    result = yamux_decode_header(buffer, YAMUX_HEADER_SIZE, &header);
    assert_true(result == YAMUX_OK, "Big-endian should decode correctly");
    assert_true(header.flags == 0x1234, "Flags should match");
    assert_true(header.stream_id == 0x12345678, "Stream ID should match");
    assert_true(header.length == 0x87654321, "Length should match");
}
