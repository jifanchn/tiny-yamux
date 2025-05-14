# Yamux Protocol Specification

This document describes the protocol implemented by the tiny-yamux library.

## Overview

Yamux (Yet another Multiplexer) is a connection-oriented, streaming multiplexing protocol that allows for
multiple logical streams to be multiplexed over a single connection. The protocol is designed to be simple,
lightweight, and easy to implement.

## Protocol Layers

Yamux operates as a session layer protocol on top of a reliable transport protocol (e.g., TCP, UART, etc.).

```
+---------------+
| Application   |
+---------------+
| Yamux Session |
+---------------+
| Yamux Stream  |
+---------------+
| Transport     |
+---------------+
```

## Framing

Yamux frames are binary encoded and have the following structure:

```
+----------------------+
| Version (8 bits)     |
+----------------------+
| Type (8 bits)        |
+----------------------+
| Flags (16 bits)      |
+----------------------+
| Stream ID (32 bits)  |
+----------------------+
| Length (32 bits)     |
+----------------------+
| Data (variable)      |
+----------------------+
```

### Version

The version field is 8 bits and indicates the protocol version. The current version is 0.

### Type

The type field is 8 bits and indicates the frame type:

| Type | Name      | Description                                            |
|------|-----------|--------------------------------------------------------|
| 0x0  | DATA      | Data frame containing application data                 |
| 0x1  | WINDOW    | Window update frame to manage flow control              |
| 0x2  | PING      | Ping frame for keep-alives and round-trip measurements |
| 0x3  | GO_AWAY   | Frame to initiate session shutdown                     |

### Flags

The flags field is 16 bits and contains frame-specific flags:

| Flag    | Value | Description                                           |
|---------|-------|-------------------------------------------------------|
| SYN     | 0x01  | Start a new stream (synchronize sequence numbers)     |
| ACK     | 0x02  | Acknowledge a stream or ping                          |
| FIN     | 0x04  | Close a stream gracefully                             |
| RST     | 0x08  | Reset/terminate a stream abnormally                   |

### Stream ID

The stream ID field is 32 bits and identifies the logical stream within the session. Stream IDs are unique within a session.

The most significant bit indicates the initiator of the stream:
* 0: Initiated by the client
* 1: Initiated by the server

The rest of the bits form an auto-incrementing value starting from 1.

### Length

The length field is 32 bits and indicates the length of the data portion in bytes.

### Data

The data field is variable length and contains the payload of the frame.

## Stream States

Streams can be in the following states:

1. **Idle**: The stream has been allocated but not yet used
2. **SynSent**: The stream has been initialized, and a SYN frame has been sent
3. **SynReceived**: A SYN frame has been received, but no ACK has been sent/received
4. **Established**: The stream is established and data can be sent/received
5. **FinSent**: A FIN frame has been sent, indicating no more data will be sent
6. **FinReceived**: A FIN frame has been received, indicating no more data will be received
7. **Closed**: The stream is fully closed and resources are ready for release

## Flow Control

Yamux uses a credit-based flow control mechanism similar to HTTP/2. Each stream maintains a send window and a receive window:

1. When a stream is created, both the sender and receiver initialize their windows to the default size (256KB)
2. As data is sent, the sender's window decreases by the number of bytes sent
3. As data is received and processed by the receiver, the receiver sends WINDOW frames to increase the sender's window
4. If the sender's window reaches zero, it must stop sending data until it receives a WINDOW frame

### Window Update Frames

Window Update frames (type 0x1) are critical for maintaining flow control. The data portion contains a 32-bit unsigned integer representing the additional number of bytes being added to the sender's window.

Window Update frames can be used in two contexts:

1. **Stream-level flow control**: When associated with a specific stream ID, the window update applies only to that stream
2. **Session-level flow control**: When sent with stream ID 0, the window update applies to the entire session

### Window Size Selection

Window size configuration is critical for performance. Several factors affect optimal window size:

1. **Bandwidth-Delay Product (BDP)**: For high-latency links, larger windows prevent throughput bottlenecks
2. **Memory Constraints**: On embedded systems, smaller windows may be necessary to conserve memory
3. **Application Pattern**: Streaming applications benefit from larger windows, while request-response patterns work well with smaller windows

### Auto-tuning

Advanced implementations may implement window auto-tuning, where the window size is adjusted based on observed network conditions and memory pressure.

## Session Lifecycle

### Session Establishment

A Yamux session is established over an existing transport connection. The process is as follows:

1. The underlying transport connection is established (e.g., TCP connection)
2. Both sides initialize their Yamux session with appropriate configuration:
   - One side must be configured as a client (is_client = 1)
   - The other side must be configured as a server (is_client = 0)
3. No explicit handshake is required at the Yamux level; the protocol begins immediately

### Stream Creation

Either side can create new streams by sending frames with the SYN flag. The process differs slightly for clients and servers:

- **Client stream creation**:
  1. Client selects a new stream ID (even number starting from 2)
  2. Client sends a frame with SYN flag and the selected stream ID
  3. Server acknowledges with a frame having ACK flag and the same stream ID

- **Server stream creation**:
  1. Server selects a new stream ID (odd number starting from 1)
  2. Server sends a frame with SYN flag and the selected stream ID
  3. Client acknowledges with a frame having ACK flag and the same stream ID

### Data Exchange

Once streams are established, data can be exchanged in both directions:

1. DATA frames carry application payload on specific streams
2. Flow control is maintained using WINDOW update frames
3. Out-of-band communication uses PING frames for keep-alives and metrics

### Stream Termination

Streams can be terminated in two ways:

- **Graceful termination (FIN)**:
  1. Sender indicates it will not send any more data by sending a frame with the FIN flag
  2. The stream remains half-closed, allowing the other side to continue sending data
  3. When both sides have sent FIN flags, the stream is fully closed

- **Abrupt termination (RST)**:
  1. Either side can immediately terminate a stream by sending a frame with the RST flag
  2. No further data should be sent or received on the stream
  3. All resources associated with the stream should be freed

### Session Termination

Either side can terminate the session using a GO_AWAY frame:

1. Sender transmits a GO_AWAY frame with appropriate error code
2. No new streams should be created after sending or receiving GO_AWAY
3. Existing streams can continue until they are naturally closed
4. When all streams are closed, the underlying transport can be closed

## Frame Handling

### DATA Frame

DATA frames (type 0x0) carry application data. They must be associated with a valid stream ID. The payload is passed directly to the application layer with minimal protocol overhead.

**Requirements:**
- Must have a non-zero stream ID
- Stream must be in the Established state
- Cannot exceed the peer's receive window

**Processing:**
1. Validate the stream ID and state
2. If combined with SYN flag, transition to Established state
3. If combined with FIN flag, mark remote side as closed
4. Update flow control windows
5. Deliver data to application if present

### WINDOW Frame

WINDOW frames (type 0x1) are used for flow control. The data portion contains a 32-bit value indicating the increase in window size.

**Requirements:**
- Data portion must be exactly 4 bytes
- Value must be a valid unsigned 32-bit integer

**Processing:**
1. If stream ID is 0, update session-level window
2. Otherwise, update stream-level window for the specified stream
3. Resume sending data if previously blocked

### PING Frame

PING frames (type 0x2) are used for keepalives and measuring round-trip time. The data portion is optional and if present, must be reflected in the PING response.

**Request vs Response:**
* To send a PING request, send a PING frame without the ACK flag
* To respond to a PING, send a PING frame with the ACK flag and the same data as the request

**Best Practices:**
- Use periodic PINGs to detect connection failures (every 30s recommended)
- Include timestamps in PING data to measure round-trip time
- Implement timeouts for PING responses (2-5s recommended)

### GO_AWAY Frame

GO_AWAY frames (type 0x3) indicate that the sender will not create any new streams and will close the connection after all existing streams are processed.

**Data Format:**
The data portion contains a 32-bit error code:

| Code | Name               | Description                                   |
|------|-------------------|-----------------------------------------------|
| 0x0  | NORMAL            | Normal termination                            |
| 0x1  | PROTOCOL_ERROR    | Protocol error                                |
| 0x2  | INTERNAL_ERROR    | Implementation error                          |

**Processing:**
1. Enter a shutdown state where no new streams are created
2. Continue processing existing streams until they close
3. If error code indicates an abnormal condition, log appropriate messages
4. Once all streams are closed, close the underlying transport

## Error Handling

When a protocol error occurs, the session should be terminated with a GO_AWAY frame with an appropriate error code.

For stream-specific errors, the stream can be reset using a frame with the RST flag.

## Implementation Considerations

### Fast Path Processing

To optimize performance, implementations should prioritize processing DATA frames, as they will be the most common. In tiny-yamux, we've implemented a direct handler for DATA frames that bypasses much of the protocol parsing overhead.

### Buffer Management

Implementations should use buffer pools to minimize memory allocations during data processing. In resource-constrained environments, using static buffers is recommended to avoid fragmentation.

### Keep-Alive Mechanism

Sessions can use PING frames as a keep-alive mechanism. The recommended interval is 30 seconds. PING frames can be used to:
1. Detect connection failures
2. Measure round-trip time
3. Keep NAT mappings alive in network environments

PING frames can contain arbitrary data which must be echoed in the response, allowing for enhanced keep-alive protocols if needed.

### Backpressure

Implementations should provide mechanisms to apply backpressure when buffer memory is exhausted. This is critical for resource-constrained systems to avoid memory exhaustion.

### I/O Abstraction

The I/O layer should be abstracted to allow for different transport mechanisms. tiny-yamux implements this through callback functions for reading and writing data:

```c
typedef struct {
    int (*read)(void *ctx, uint8_t *buf, size_t len);
    int (*write)(void *ctx, const uint8_t *buf, size_t len);
    void *ctx;
} yamux_io_t;
```

This abstraction allows the protocol to work over any transport layer that can provide these functions, such as:
- TCP sockets
- UART/Serial interfaces
- Custom transports (e.g., encrypted tunnels)

### Error Handling and Recovery

Robust error handling is essential, especially for embedded systems where connections may be unreliable. Implementations should:

1. Have clear timeout policies for operations
2. Implement exponential backoff for reconnection attempts
3. Use appropriate error codes to communicate issues
4. Gracefully degrade under resource constraints

### Resource Limits

Critical resource limits to consider in implementation:

1. **Maximum Streams**: Limit the number of concurrent streams (recommended: 100-1000 depending on available memory)
2. **Buffer Sizes**: Configure buffer sizes based on expected data patterns (recommended: 4-16KB for most applications)
3. **Window Size**: Tune flow control window size based on latency and bandwidth (default: 256KB)
4. **Ping Timeout**: Set appropriate timeout for PING responses (recommended: 2-5 seconds)

## References

- The original Go implementation: [github.com/hashicorp/yamux](https://github.com/hashicorp/yamux)
- The fork that powers Frp: [github.com/fatedier/yamux](https://github.com/fatedier/yamux)
