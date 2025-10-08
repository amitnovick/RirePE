# TCP Socket Support for RirePE

This document explains how to use the TCP socket functionality to communicate with the RirePE DLL from remote machines (e.g., a Linux Python client).

## Overview

The DLL now supports **two communication modes**:

1. **Named Pipes** (default) - Original Windows-only IPC
2. **TCP Sockets** (new) - Cross-platform network communication

## Configuration

To enable TCP mode, edit `RirePE.ini` in the DLL directory:

```ini
[Packet]
USE_TCP=1
TCP_HOST=127.0.0.1
TCP_PORT=9999
```

### Configuration Options

- `USE_TCP`: Set to `1` to enable TCP mode, `0` for named pipes (default: 0)
- `TCP_HOST`: Server IP address to connect to (default: 127.0.0.1)
- `TCP_PORT`: Server port number (default: 9999)

**Note**: When `USE_TCP=1`, the DLL will NOT launch RirePE.exe. It will connect to an external TCP server instead.

## Python CLI Client Usage

### Installation

No additional dependencies required - uses Python 3 standard library only.

### Monitoring Packets

To monitor packets in real-time:

```bash
python3 packet_monitor.py --host 127.0.0.1 --port 9999
```

With logging to file:

```bash
python3 packet_monitor.py --host 127.0.0.1 --port 9999 --log packets.log
```

Remote monitoring (from Linux to Windows):

```bash
python3 packet_monitor.py --host 192.168.1.100 --port 9999
```

### Sending Packets

Send a packet to the game (outbound):

```bash
python3 packet_monitor.py --host 127.0.0.1 --port 9999 --send "0A 00 01 02 03"
```

Send a packet as if received from server (inbound):

```bash
python3 packet_monitor.py --host 127.0.0.1 --port 9999 --send "FF 00 AA BB" --send-recv
```

### Python Client Features

- Real-time packet monitoring
- Hex dump display
- Packet injection (send/recv)
- File logging
- Automatic reconnection attempts
- Message parsing and formatting

## Network Setup

### Same Machine (Windows)

1. Configure DLL with `TCP_HOST=127.0.0.1`
2. Run a TCP server on port 9999 (or run the Python client in server mode)
3. Inject DLL into game

### Remote Machine (Linux → Windows)

1. **On Windows**: Configure DLL with the Linux machine's IP
   ```ini
   TCP_HOST=192.168.1.50
   TCP_PORT=9999
   ```

2. **On Linux**: Run the Python client as a server
   ```bash
   python3 packet_monitor.py --host 0.0.0.0 --port 9999
   ```

3. Configure firewall to allow port 9999

### Remote Machine (Windows → Linux)

This is the typical use case - run a Python server on Linux to collect packets from Windows DLL.

**Linux side** (create a simple TCP server):

```python
import socket
from packet_monitor import PacketMonitor

# Listen for connections
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind(('0.0.0.0', 9999))
server.listen(1)
print("[+] Waiting for DLL connection...")

client_sock, addr = server.accept()
print(f"[+] Connected from {addr}")

# Use PacketMonitor to handle the connection
monitor = PacketMonitor()
monitor.sock = client_sock
monitor.run()
```

**Windows side** (DLL config):

```ini
USE_TCP=1
TCP_HOST=192.168.1.50
TCP_PORT=9999
```

## Protocol Details

### Message Format

All messages use this wire format:

```
| Magic (4 bytes) | Length (4 bytes) | Data (variable) |
|    0xA11CE      |   data length    |   payload       |
```

### PacketEditorMessage Structure

The payload is a `PacketEditorMessage`:

```c
struct PacketEditorMessage {
    uint32_t header;        // Message type (SENDPACKET, RECVPACKET, etc.)
    uint32_t id;            // Packet ID
    uint64_t addr;          // Return address
    union {
        struct {
            uint32_t length;  // Packet size
            uint8_t packet[]; // Packet data
        } Binary;
        // ... other formats
    };
};
```

### Response Format

For `SENDPACKET` and `RECVPACKET` messages, the server should respond with:

- `0x00` = Allow packet
- `0x01` = Block packet

## Building the DLL with TCP Support

The TCP socket code is already integrated. You need to:

1. Add `SimpleTCP.cpp` to your Visual Studio project
2. Link against `ws2_32.lib` (already in SimpleTCP.h via `#pragma comment`)
3. Rebuild the DLL

## Troubleshooting

### Connection Refused

- Ensure the TCP server is running before injecting the DLL
- Check firewall settings
- Verify the IP/port in configuration

### No Packets Appearing

- Check that `USE_TCP=1` is set in RirePE.ini
- Verify the DLL successfully connected (check debug output)
- Ensure the game is actually sending packets

### Performance Issues

- TCP has slightly more overhead than named pipes
- For local monitoring, consider using named pipes instead
- Reduce logging verbosity if needed

## Example: Complete Setup

1. **Edit RirePE.ini**:
   ```ini
   [Packet]
   USE_TCP=1
   TCP_HOST=192.168.1.50
   TCP_PORT=9999
   ```

2. **On Linux (192.168.1.50)**:
   ```bash
   python3 packet_monitor.py --host 0.0.0.0 --port 9999 --log game_packets.log
   ```

3. **On Windows**: Inject the DLL into the game

4. **Watch packets flow** on the Linux terminal!

## Files Added/Modified

### New Files
- `Share/Simple/SimpleTCP.h` - TCP socket classes
- `Share/Simple/SimpleTCP.cpp` - TCP implementation
- `packet_monitor.py` - Python CLI client

### Modified Files
- `Packet/PacketLogging.h` - Added TCP support
- `Packet/PacketLogging.cpp` - TCP client integration
- `Packet/PacketQueue.cpp` - Use abstract send/recv
- `Packet/DllMain.cpp` - TCP configuration loading
