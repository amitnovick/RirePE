# TCP Socket Support for RirePE

This document explains how to use the TCP socket functionality to communicate with the RirePE DLL from remote machines (e.g., a Linux Python client).

## Overview

The DLL now supports **dual communication modes**:

1. **Named Pipes** (always active) - Local RirePE.exe GUI for Windows monitoring
2. **TCP Sockets** (optional) - Simultaneous remote monitoring from any platform

**Key Feature**: Both modes can run **simultaneously**! You can have the local GUI running on Windows while also streaming packets to a remote Linux/Mac Python client.

## Configuration

To enable TCP mode, edit `RirePE.ini` in the DLL directory:

```ini
[Packet]
USE_TCP=1
TCP_HOST=127.0.0.1
TCP_PORT=9999
```

### Configuration Options

- `USE_TCP`: Set to `1` to enable TCP mode in addition to pipes (default: 0)
- `TCP_HOST`: Server IP address to connect to (default: 127.0.0.1)
- `TCP_PORT`: Server port number (default: 9999)

**Behavior:**
- `USE_TCP=0` (default): Only RirePE.exe GUI (named pipes only)
- `USE_TCP=1`: **Both** RirePE.exe GUI AND TCP streaming to remote clients

**Note**: The DLL always launches RirePE.exe for the local GUI. When TCP is enabled, packets are **broadcast** to both the local GUI and remote TCP clients simultaneously.

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

### Dual Mode (Local GUI + Remote Monitoring)

**Use Case**: Monitor packets on both Windows GUI and remote Linux machine simultaneously.

**Linux side** (192.168.1.50):
```bash
python3 packet_monitor.py --host 0.0.0.0 --port 9999 --log remote_packets.log
```

**Windows side** (DLL config):
```ini
[Packet]
USE_TCP=1
TCP_HOST=192.168.1.50
TCP_PORT=9999
```

**Result**:
- ✅ RirePE.exe GUI opens on Windows (local monitoring)
- ✅ Python client on Linux receives same packets (remote monitoring)
- ✅ Both see identical packet streams in real-time!

## How Dual Mode Works

### Packet Broadcasting

When `USE_TCP=1`, the DLL **broadcasts** each packet to multiple destinations:

```
Game Packet
    ↓
DLL Hook
    ↓
PacketQueue (async)
    ↓
    ├─→ Named Pipe → RirePE.exe (Windows GUI)
    └─→ TCP Socket → Python Client (Remote)
```

**Send Behavior:**
- Packets are sent to **both** pipe and TCP simultaneously
- If either fails, the other continues working
- Success is reported if **at least one** destination receives the packet

**Receive Behavior (for blocking mode):**
- Priority given to local GUI (named pipe) response
- Falls back to TCP response if pipe fails
- First response wins

### Benefits

1. **Redundancy**: If one monitoring system fails, the other keeps working
2. **Flexibility**: Local debugging with GUI + remote logging simultaneously
3. **Team collaboration**: Multiple people can monitor same game instance
4. **Automation**: Python client can auto-process packets while GUI shows them visually

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

The TCP socket code is already integrated into the project files:

**Files Added:**
- `Share/Simple/SimpleTCP.h` - TCP socket classes header
- `Share/Simple/SimpleTCP.cpp` - TCP socket implementation
- `Packet/PacketTCP.cpp` - TCP wrapper to avoid winsock header conflicts

**Project Configuration:**
- `Packet.vcxproj` has been updated to include both `SimpleTCP.cpp` and `PacketTCP.cpp`
- `ws2_32.lib` is automatically linked via `#pragma comment` in `SimpleTCP.h`

**Build Instructions:**
```bash
# Build with MSBuild (from project root)
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86

# Or open in Visual Studio and build normally
```

**Important Notes:**
- The TCP code uses `winsock2.h` which must be included before `windows.h`
- To avoid header conflicts, TCP functionality is isolated in `PacketTCP.cpp`
- No manual configuration needed - just build the solution

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

## Example Configurations

### Example 1: Local Only (Default)

**RirePE.ini:**
```ini
[Packet]
# USE_TCP not set or commented out
```

**Result:**
- Only RirePE.exe GUI shows packets
- No network traffic
- Original behavior

### Example 2: Local GUI + Remote Monitoring

**RirePE.ini:**
```ini
[Packet]
USE_TCP=1
TCP_HOST=192.168.1.50
TCP_PORT=9999
```

**On Linux (192.168.1.50):**
```bash
python3 packet_monitor.py --host 0.0.0.0 --port 9999 --log game_packets.log
```

**On Windows:**
1. Configure RirePE.ini as shown above
2. Inject the DLL into the game
3. **Both** RirePE.exe GUI and Python client receive packets!

### Example 3: Localhost Monitoring (Windows Only)

If you want to use the Python client on the same Windows machine:

**RirePE.ini:**
```ini
[Packet]
USE_TCP=1
TCP_HOST=127.0.0.1
TCP_PORT=9999
```

**On Windows:**
1. Run Python client: `python packet_monitor.py --host 127.0.0.1 --port 9999`
2. Inject the DLL
3. Both RirePE.exe GUI and Python console show packets

**Note**: Currently the DLL connects to **one** TCP server. For multiple remote clients, you would need to implement a TCP server mode in the DLL (future enhancement).

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
