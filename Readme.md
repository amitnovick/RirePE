# RirePE - Headless Packet Logger

A MapleStory packet analysis tool that logs and monitors network packets via TCP.

## Architecture

- **Packet.dll**: Injected DLL that hooks game functions and logs packets
- **TCP Server**: Embedded in Packet.dll, listens on port 8275 (configurable)
- **Monitoring Tools**: Python scripts for remote packet viewing and analysis

### What Changed?

**RirePE.exe GUI has been removed.** The project now operates in headless mode with TCP-based monitoring.

**Benefits:**
- Monitor from any OS (Linux, macOS, Windows)
- Multiple clients can connect simultaneously
- Scriptable packet analysis (Python, Node.js, etc.)
- Simpler deployment (just inject Packet.dll)
- Lower overhead (no GUI thread)

## How to Use

### Setup

1. Put `Packet.dll` and `RirePE.ini` in the same folder as MapleStory.exe
2. Configure `RirePE.ini`:
   ```ini
   [Packet]
   TCP_PORT=8275
   TCP_HOST=localhost
   ```
3. Inject `Packet.dll` into the MapleStory process
4. Connect using a monitoring tool

### Monitoring Packets

#### Using packet_monitor.py
```bash
python packet_monitor.py localhost 8275
```

This displays packets in real-time with opcode, size, and hex dump.

#### Using tcp_inject_example.py
```python
python tcp_inject_example.py
```

Demonstrates how to:
- Connect to the packet logger
- Send packets to the game
- Receive packets from the game
- Parse packet data

### Creating Custom Monitoring Tools

See `Packet/TCP_API_DOCUMENTATION.md` for the TCP protocol specification.

Example TCP client:
```python
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8275))

while True:
    # Receive packet message
    data = sock.recv(4096)
    if not data:
        break

    # Parse header (see TCP_API_DOCUMENTATION.md)
    header, opcode, addr, size = struct.unpack('<IIII', data[:16])
    packet_data = data[16:16+size]

    print(f"Opcode: 0x{opcode:04X}, Size: {size}, Data: {packet_data.hex()}")
```

## Configuration

`RirePE.ini` settings:

```ini
[Packet]
# TCP port for remote monitoring (default: 8275)
TCP_PORT=8275

# TCP host/bind address (default: localhost)
# WARNING: Use 0.0.0.0 only in trusted networks!
TCP_HOST=localhost

# Enable packet blocking feature (slower, default: 0)
ENABLE_BLOCKING=0

# Enable debug logging (default: 0)
DEBUG_MODE=0

# Hook from separate thread (recommended, default: 1)
USE_THREAD=1
```

See `Ini-README.md` for detailed configuration documentation.

## How This Tool Works

1. **Packet.dll** hooks MapleStory's network functions
2. Packets are queued asynchronously (zero game thread blocking)
3. **TCP server** broadcasts packets to all connected clients
4. **Monitoring tools** receive and display/analyze packets

See `ARCHITECTURE.md` for detailed technical documentation.

## Building from Source

### Requirements
- Visual Studio 2019 or later
- Windows SDK 10
- MSBuild

### Build Steps
```bash
# Build dependencies
msbuild Share/Simple/Simple.sln /p:Configuration=Release /p:Platform=x86
msbuild Share/Hook/Hook.sln /p:Configuration=Release /p:Platform=x86

# Build Packet.dll
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

Output: `Packet/Release/Packet.dll`

## Migration Guide (from RirePE.exe)

**Old workflow:**
1. Inject Packet.dll
2. RirePE.exe launches automatically
3. View packets in Windows GUI

**New workflow:**
1. Inject Packet.dll
2. Run `python packet_monitor.py localhost 8275`
3. View packets in terminal

**Lost features:**
- Windows GUI with packet list
- Visual format viewer
- Packet blocking checkbox UI

**Replacement workflows:**
- Use `packet_monitor.py` for basic viewing
- Write custom Python scripts for advanced analysis
- Use ENABLE_BLOCKING=1 config for packet blocking

See `MIGRATION_GUIDE.md` for detailed migration instructions.

## Note

Around BB updates, the client started protecting the SendPacket function. You may need to bypass anti-debugging checks to hook the function successfully.
