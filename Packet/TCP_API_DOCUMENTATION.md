# TCP Server API Documentation

## Overview

The Packet DLL provides a **bidirectional TCP server API** that enables real-time communication between the injected DLL and remote TCP clients (e.g., Python scripts). The server broadcasts intercepted game packets to clients and can receive responses for packet filtering in blocking mode.

**Server Location**: Runs within the injected DLL (Packet.dll) in the game process
**Protocol**: TCP with custom framing (bidirectional)
**Default Port**: 9999 (configurable via `config.ini`)
**Connection Model**: Single client at a time (new connections replace existing ones)
**Communication**: Bidirectional - DLL ⇄ TCP Client

---

## Configuration

Enable TCP mode in `config.ini`:

```ini
[Packet]
USE_TCP=1
TCP_HOST=127.0.0.1  ; (Ignored for server, used by client implementations)
TCP_PORT=9999       ; Port to listen on
ENABLE_BLOCKING=0   ; Set to 1 for blocking mode (waits for client response)
```

---

## Connection Flow

1. **Server Startup**: TCP server starts automatically when `USE_TCP=1` in config
2. **Client Connection**: Server accepts one client connection at a time
3. **Bidirectional Communication**:
   - **DLL → Client**: Server broadcasts intercepted packets using `TCPServerThread::Send()`
   - **Client → DLL**: Client sends responses using same framed protocol via `TCPServerThread::Recv()`
4. **Blocking Mode Response**: When `ENABLE_BLOCKING=1`, client **must** send back 1-byte response (0x00=allow, 0x01=block) for each `SENDPACKET` and `RECVPACKET` message
5. **Disconnection**: Server continues running and accepts new connections

---

## Message Protocol

### Message Frame Structure

**All TCP messages in BOTH directions** use the same framed protocol defined in `SimpleTCP.h`:

```c
#pragma pack(push, 1)
typedef struct {
    DWORD magic;    // 0xA11CE (constant magic value)
    DWORD length;   // Length of data in bytes
    BYTE data[1];   // Variable-length data payload
} TCPMessage;
#pragma pack(pop)
```

**Frame Layout:**
```
+----------------+----------------+------------------+
| Magic (4B)     | Length (4B)    | Data (Variable)  |
| 0xA11CE        | N bytes        | N bytes payload  |
+----------------+----------------+------------------+
```

**Bidirectional Protocol:**
- **DLL → Client**: Uses `TCPMessage` framing with `PacketEditorMessage` payload
- **Client → DLL**: Uses same `TCPMessage` framing (currently for block/allow responses)
- Both directions validated with magic number `0xA11CE`
- Protocol is symmetric - same Send/Recv implementation on both sides

### Message Payload Structure

The `data` field contains a `PacketEditorMessage` structure:

```c
#pragma pack(push, 1)
typedef struct {
    MessageHeader header;  // Message type (enum)
    DWORD id;              // Packet ID (auto-incremented)
    ULONG_PTR addr;        // Return address in game code (64-bit on x64)

    union {
        // For SENDPACKET or RECVPACKET
        struct {
            DWORD length;      // Packet size in bytes
            BYTE packet[1];    // Raw packet data
        } Binary;

        // For encode/decode format messages
        struct {
            DWORD pos;         // Position in packet buffer
            DWORD size;        // Size of encoded/decoded data
            FormatUpdate update; // FORMAT_UPDATE or FORMAT_NO_UPDATE
            BYTE data[1];      // Optional data content
        } Extra;

        // For status messages
        DWORD status;
    };
} PacketEditorMessage;
#pragma pack(pop)
```

---

## Message Types

### MessageHeader Enum

```c
enum MessageHeader {
    // Main packet types
    SENDPACKET,        // Outgoing packet (client→server)
    RECVPACKET,        // Incoming packet (server→client)

    // Outgoing packet encoding format
    ENCODE_BEGIN,
    ENCODEHEADER,
    ENCODE1,           // Encode 1-byte value
    ENCODE2,           // Encode 2-byte value
    ENCODE4,           // Encode 4-byte value
    ENCODE8,           // Encode 8-byte value
    ENCODESTR,         // Encode string
    ENCODEBUFFER,      // Encode buffer
    TV_ENCODEHEADER,
    TV_ENCODESTRW1,
    TV_ENCODESTRW2,
    TV_ENCODEFLOAT,
    ENCODE_END,

    // Incoming packet decoding format
    DECODE_BEGIN,
    DECODEHEADER,
    DECODE1,           // Decode 1-byte value
    DECODE2,           // Decode 2-byte value
    DECODE4,           // Decode 4-byte value
    DECODE8,           // Decode 8-byte value
    DECODESTR,         // Decode string
    DECODEBUFFER,      // Decode buffer
    TV_DECODEHEADER,
    TV_DECODESTRW1,
    TV_DECODESTRW2,
    TV_DECODEFLOAT,
    DECODE_END,

    // Other types
    UNKNOWNDATA,       // Data not decoded by function
    NOTUSED,           // Received but not used
    WHEREFROM,         // Not encoded by function
    UNKNOWN,
};
```

### Packet ID System

- **Outgoing packets** (`SENDPACKET`): Use even IDs starting from 2
- **Incoming packets** (`RECVPACKET`): Use odd IDs starting from 1
- IDs auto-increment by 2 for each packet type

---

## Client Implementation Guide

### 1. Connecting to the Server

```python
import socket
import struct

# Connect to the TCP server
HOST = '127.0.0.1'
PORT = 9999
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))
```

### 2. Receiving Messages

```python
def recv_message(sock):
    """Receive a framed TCP message"""
    # Read frame header (8 bytes)
    header = sock.recv(8)
    if len(header) < 8:
        return None

    magic, length = struct.unpack('<II', header)

    # Verify magic value
    if magic != 0xA11CE:
        raise ValueError(f"Invalid magic: 0x{magic:X}")

    # Read payload
    data = b''
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            return None
        data += chunk

    return data

def parse_packet_message(data):
    """Parse PacketEditorMessage from data"""
    if len(data) < 16:  # Minimum size
        return None

    # Parse header (architecture-dependent)
    # For 64-bit: header(4) + id(4) + addr(8) = 16 bytes
    # For 32-bit: header(4) + id(4) + addr(8) = 16 bytes (addr is ULONGLONG)
    header_type = struct.unpack('<I', data[0:4])[0]
    packet_id = struct.unpack('<I', data[4:8])[0]
    addr = struct.unpack('<Q', data[8:16])[0]

    result = {
        'header': header_type,
        'id': packet_id,
        'addr': addr
    }

    # Parse payload based on message type
    if header_type in [0, 1]:  # SENDPACKET or RECVPACKET
        length = struct.unpack('<I', data[16:20])[0]
        packet_data = data[20:20+length]
        result['binary'] = {
            'length': length,
            'packet': packet_data
        }
    else:  # Format messages (encode/decode)
        pos = struct.unpack('<I', data[16:20])[0]
        size = struct.unpack('<I', data[20:24])[0]
        update = struct.unpack('<I', data[24:28])[0]
        extra_data = data[28:28+size] if update == 1 else None
        result['extra'] = {
            'pos': pos,
            'size': size,
            'update': update,
            'data': extra_data
        }

    return result
```

### 3. Sending Messages to DLL (Bidirectional Communication)

The TCP protocol supports bidirectional communication. The client can send messages back to the DLL using the same `TCPMessage` framing:

```python
def send_message(sock, data):
    """
    Send a framed message to the DLL server

    Args:
        sock: Socket connection
        data: Raw bytes to send (will be wrapped in TCPMessage frame)
    """
    magic = 0xA11CE
    length = len(data)

    # Pack: magic (4 bytes) + length (4 bytes) + data
    frame = struct.pack('<II', magic, length) + data
    sock.sendall(frame)
```

**Current Use Cases:**

#### a) Blocking Mode Response

When `ENABLE_BLOCKING=1`, the server waits for a 1-byte response after sending `SENDPACKET` or `RECVPACKET` messages:

```python
def send_response(sock, block_packet):
    """
    Send response to indicate whether packet should be blocked

    Args:
        sock: Socket connection
        block_packet: True to block packet, False to allow it
    """
    response = b'\x01' if block_packet else b'\x00'
    sock.send(response)  # Raw 1-byte response (no framing needed)
```

**Important Notes:**
- Response is **required** only for `SENDPACKET` (header=0) and `RECVPACKET` (header=1) messages
- Format messages (encode/decode) do **not** require responses
- If `ENABLE_BLOCKING=0` (default), no response is needed at all
- Server will hang if response is not sent in blocking mode
- Block/allow responses are sent as **raw bytes** (not framed) for compatibility

#### b) Future Extensions

The bidirectional protocol enables future enhancements such as:
- **Command injection**: Send custom packets to inject into game
- **DLL control**: Start/stop packet capture, change filters
- **Real-time modification**: Send modified packet data back to DLL

These features would require DLL-side implementation to process incoming framed messages in the `TCPCommunicate()` function (PacketTCP.cpp:25).

### 4. Complete Client Example

```python
import socket
import struct

def main():
    # Connect
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 9999))
    print("Connected to packet server")

    try:
        while True:
            # Receive message
            data = recv_message(sock)
            if not data:
                print("Connection closed")
                break

            # Parse message
            msg = parse_packet_message(data)

            # Handle different message types
            if msg['header'] == 0:  # SENDPACKET
                print(f"[SEND #{msg['id']}] Packet: {msg['binary']['packet'].hex()}")
                # Send response if blocking enabled
                # send_response(sock, False)  # Don't block

            elif msg['header'] == 1:  # RECVPACKET
                print(f"[RECV #{msg['id']}] Packet: {msg['binary']['packet'].hex()}")
                # Send response if blocking enabled
                # send_response(sock, False)  # Don't block

            else:  # Format messages
                print(f"[Format] Type={msg['header']}, Pos={msg['extra']['pos']}, Size={msg['extra']['size']}")

    finally:
        sock.close()

if __name__ == '__main__':
    main()
```

---

## API Reference

### Server Functions

#### `bool StartTCPClient()`
**Location**: `PacketTCP.cpp:59`

Starts the TCP server (note: function name is historical, it actually starts a server).

**Returns**: `true` if server started successfully, `false` otherwise

**Behavior**:
- Initializes critical section for thread safety
- Creates TCP server on configured port
- Runs in background thread accepting connections
- Only one client connection active at a time

---

#### `bool RestartTCPClient()`
**Location**: `PacketTCP.cpp:83`

Restarts the TCP server by closing existing connections and starting fresh.

**Returns**: `true` if restart successful, `false` otherwise

---

#### `bool SendPacketDataTCP(BYTE *bData, ULONG_PTR uLength)`
**Location**: `PacketTCP.cpp:96`

Sends packet data to connected TCP client.

**Parameters**:
- `bData`: Pointer to packet data buffer
- `uLength`: Length of data in bytes

**Returns**:
- `true` if client connected and send successful
- `true` if no client connected (not an error, just skips send)
- `false` if send failed (client disconnected)

**Behavior**:
- Thread-safe (uses critical section)
- Non-blocking (doesn't wait for response)
- Automatically detects client disconnection
- Logs send status for debugging

---

#### `bool RecvPacketDataTCP(std::vector<BYTE> &vData)`
**Location**: `PacketTCP.cpp:131`

Receives response data from TCP client (used in blocking mode).

**Parameters**:
- `vData`: Output vector to receive data

**Returns**: `true` if data received, `false` if no client or receive failed

**Behavior**:
- Thread-safe (uses critical section)
- Blocks until data received or timeout

---

### TCPServerThread Class

#### `bool Send(BYTE *bData, ULONG_PTR uLength)`
**Location**: `SimpleTCP.h:38`

Sends framed message to client.

**Framing**: Automatically wraps data in `TCPMessage` frame with magic and length

**Returns**: `true` on success, `false` on failure

---

#### `bool Recv(std::vector<BYTE> &vData)`
**Location**: `SimpleTCP.h:39`

Receives framed message from client.

**Framing**: Automatically parses `TCPMessage` frame and extracts data

**Returns**: `true` on success, `false` on failure or disconnection

**Protocol**: Uses same bidirectional framing as Send - validates magic `0xA11CE`

---

## Performance Characteristics

### Non-Blocking Mode (Default: `ENABLE_BLOCKING=0`)

- **Latency**: Minimal (~1ms per packet)
- **Throughput**: High (handles bursts well)
- **Queue**: Asynchronous packet queue with worker thread
- **Batch Processing**: Processes up to 16 packets per iteration
- **Client Response**: Not required
- **Use Case**: Real-time monitoring, logging, analysis

### Blocking Mode (`ENABLE_BLOCKING=1`)

- **Latency**: Higher (waits for client response on each packet)
- **Throughput**: Lower (limited by round-trip time)
- **Client Response**: Required (1 byte: 0x00=allow, 0x01=block)
- **Use Case**: Packet filtering, injection, modification

### Buffer Pool

- **Pool Size**: 64 buffers
- **Buffer Size**: 8192 bytes each
- **Allocation**: O(1) pool allocation with fallback to heap
- **Thread Safety**: Critical section protected

---

## Error Handling

### Connection Errors

- **No Client Connected**: Not an error - packets are queued until client connects
- **Client Disconnected**: Detected automatically, server continues running
- **Send Failure**: Logged for debugging, doesn't crash server

### Recovery

- **Auto-Recovery**: Server continues accepting new connections after disconnect
- **Restart Function**: `RestartTCPClient()` available for manual restart
- **Failure Threshold**: After 50 consecutive packet send failures, attempts restart

---

## Thread Safety

All TCP operations are thread-safe with bidirectional communication support:

- **Critical Section**: `tcp_client_cs` protects client pointer access
- **Atomic Operations**: Client pointer read/write done atomically
- **Lock Minimization**: Sends/receives occur outside critical section to avoid blocking
- **Bidirectional Safety**: Both `Send()` and `Recv()` operations are protected by the same critical section

---

## Protocol Examples

### Example 1: SENDPACKET Message

```
Frame:
  Magic:  CE 11 0A 00        (0xA11CE)
  Length: 20 00 00 00        (32 bytes)

Payload:
  Header: 00 00 00 00        (SENDPACKET = 0)
  ID:     02 00 00 00        (Packet #2)
  Addr:   A0 B1 C2 D3 E4 F5 06 07  (Return address)
  Length: 08 00 00 00        (8-byte packet)
  Packet: 1A 00 05 01 02 03 04 05  (Raw packet data)
```

### Example 2: ENCODE4 Format Message

```
Frame:
  Magic:  CE 11 0A 00        (0xA11CE)
  Length: 1C 00 00 00        (28 bytes)

Payload:
  Header: 06 00 00 00        (ENCODE4 = 6)
  ID:     02 00 00 00        (Packet #2)
  Addr:   A0 B1 C2 D3 E4 F5 06 07  (Return address)
  Pos:    04 00 00 00        (Position = 4)
  Size:   04 00 00 00        (Size = 4 bytes)
  Update: 01 00 00 00        (FORMAT_UPDATE = 1)
  Data:   64 00 00 00        (Value = 100)
```

### Example 3: RECVPACKET Message

```
Frame:
  Magic:  CE 11 0A 00        (0xA11CE)
  Length: 26 00 00 00        (38 bytes)

Payload:
  Header: 01 00 00 00        (RECVPACKET = 1)
  ID:     01 00 00 00        (Packet #1)
  Addr:   50 60 70 80 90 A0 B0 C0  (Return address)
  Length: 0E 00 00 00        (14-byte packet)
  Packet: 2B 00 01 00 48 65 6C 6C 6F 20 57 6F 72 6C 64  (Raw packet)
```

### Example 4: Client → DLL Message (Bidirectional)

The client can send framed messages back to the DLL using the same protocol:

```
Frame (Client → DLL):
  Magic:  CE 11 0A 00        (0xA11CE)
  Length: 08 00 00 00        (8 bytes payload)

Payload:
  Custom command data (8 bytes)
  Data:   01 02 03 04 05 06 07 08
```

**Note:** The DLL currently does not process incoming framed messages except for raw 1-byte block/allow responses in blocking mode. To implement command processing, modify `TCPCommunicate()` in PacketTCP.cpp to call `client.Recv()` and handle the received data.

---

## Security Considerations

1. **Local Only**: Server binds to configured port (typically 127.0.0.1)
2. **No Authentication**: No authentication mechanism implemented
3. **Single Client**: Only one client can connect at a time
4. **Data Exposure**: All intercepted packets are sent to client
5. **Packet Blocking**: In blocking mode, client can block/modify game packets

**Recommendation**: Use only on localhost or trusted networks

---

## Debugging

### Debug Logging

Enable debug logging in `config.ini`:

```ini
[Packet]
DEBUG_MODE=1
```

Debug logs are written to `debug.log` in the DLL directory.

### Log Messages

- `[TCP] Client connected to TCP server` - Client connection established
- `[TCP] Client disconnected from TCP server` - Client disconnected
- `[TCP] TCP client is now connected - broadcasting packets` - First packet sent
- `[TCP PACKET #N] Sent to TCP client: ✓/✗` - Packet send status

---

## Source Code Reference

| File | Description |
|------|-------------|
| `PacketTCP.cpp` | TCP server implementation and send/receive functions |
| `SimpleTCP.h` | TCP protocol definitions and classes |
| `RirePE.h` | Message structures (`PacketEditorMessage`, `MessageHeader`) |
| `PacketQueue.cpp` | Asynchronous packet queue system |
| `PacketLogging.cpp` | Packet capture and formatting logic |
| `DllMain.cpp` | Initialization and configuration |

---

## FAQ

**Q: Can multiple clients connect simultaneously?**
A: No, only one client at a time. New connections replace existing ones.

**Q: What happens if no client is connected?**
A: Packets are still captured and queued, but TCP sends are skipped (not an error).

**Q: Does the server send format information (encode/decode messages)?**
A: Yes, all message types are sent including format information for packet structure analysis.

**Q: Can I modify or block packets?**
A: Yes, enable `ENABLE_BLOCKING=1` and send 0x01 response to block packets.

**Q: What's the difference between pipe and TCP mode?**
A: Pipe connects to local RirePE.exe GUI, TCP allows remote monitoring. Both can run simultaneously.

**Q: Is the TCP protocol bidirectional?**
A: Yes! The protocol supports bidirectional communication using the same `TCPMessage` framing in both directions. The DLL currently implements DLL→Client broadcasts and Client→DLL block/allow responses. Additional features (like command injection) would require extending the `TCPCommunicate()` function.

**Q: Can I send custom commands from the Python client to the DLL?**
A: The protocol supports it (via `TCPServerThread::Recv()`), but the DLL currently doesn't process incoming framed commands. You can extend `TCPCommunicate()` in PacketTCP.cpp to handle custom command messages.

**Q: What happens on server restart?**
A: Existing connections are closed, server rebinds to port and accepts new connections.

---

## Version History

- Initial implementation with TCP server support
- Added non-blocking mode for performance
- Added buffer pool for zero-copy optimization
- Added asynchronous packet queue system
- Added dual broadcast (pipe + TCP) support

----

## Architecture Clarification

### Bidirectional Protocol Architecture

The TCP server uses a **bidirectional protocol** with symmetric framing in both directions:

```
MapleStory Process (with injected Packet.dll)
    ↓
[Hooked Send/Recv Functions] ← Intercepts game packets
    ↓
[PacketQueue] ← Queues packets asynchronously
    ↓
[TCP Server] ⇄ [Remote TCP Client]  ← Bidirectional communication
    ↓                ↑
  Send          Recv (both use TCPMessage framing)
```

### Data Flow

**DLL → Client (Broadcasting):**
1. Packet.dll hooks game's network functions (SendPacket/RecvPacket)
2. When game sends/receives packets, hooks capture them
3. Packets are queued in PacketQueue with format metadata
4. TCP server broadcasts packets to connected client via `TCPServerThread::Send()`

**Client → DLL (Responses/Commands):**
1. Client sends framed messages using `TCPMessage` protocol
2. DLL receives via `TCPServerThread::Recv()`
3. Currently used for: block/allow responses in blocking mode (raw 1-byte)
4. Future use: Custom commands, packet injection (requires DLL extension)

### Current Capabilities

**Currently Implemented:**
- ✅ **Monitoring**: Real-time packet capture and analysis (DLL → Client)
- ✅ **Logging**: Recording all game network traffic (DLL → Client)
- ✅ **Filtering**: Blocking mode - client decides which packets to block (Client → DLL)
- ✅ **Format Analysis**: Understanding packet structure via encode/decode messages (DLL → Client)
- ✅ **Bidirectional Protocol**: Both directions use same `TCPMessage` framing
- ✅ **Thread-Safe I/O**: Both Send and Recv protected by critical sections

**Protocol-Ready But Not Implemented:**
- ⚠️ **Packet Injection**: Client sending custom packets to inject into game
- ⚠️ **DLL Control**: Client sending commands to control packet capture
- ⚠️ **Real-time Modification**: Client sending modified packet data back

### Implementation Notes

The protocol is **fully bidirectional** at the transport level:
- Same `TCPMessage` framing (`magic + length + data`) used in both directions
- `TCPServerThread::Send()` and `TCPServerThread::Recv()` are symmetric
- Both validated with magic number `0xA11CE`

To add new features (like packet injection), you would:
1. Define new message types (similar to `PacketEditorMessage`)
2. Modify `TCPCommunicate()` in PacketTCP.cpp to call `client.Recv()` in a loop
3. Parse and process incoming framed messages
4. Execute appropriate actions based on message type

The architecture is designed to support full bidirectional control - it just needs the DLL-side message processing implementation.

