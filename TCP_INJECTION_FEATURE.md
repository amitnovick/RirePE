# TCP Packet Injection Feature

## Overview

The TCP API now supports **full bidirectional packet injection**, allowing remote TCP clients (e.g., Python scripts) to inject custom packets into the game, just like the RirePE.exe GUI.

## What Was Implemented

### Code Changes

**File**: `Packet/PacketTCP.cpp`

Modified the `TCPCommunicate()` function to:
1. Actively listen for incoming `PacketEditorMessage` commands from TCP clients
2. Process SENDPACKET and RECVPACKET injection requests
3. Use the existing packet injection infrastructure (shared with GUI)

**Key Change**: Changed from passive connection monitoring to active command processing loop.

```cpp
// OLD: Just kept connection alive
while (true) {
    Sleep(1000);  // Did nothing
}

// NEW: Processes injection commands
while (true) {
    if (!client.Recv(data)) break;

    PacketEditorMessage* pcm = (PacketEditorMessage*)&data[0];
    if (pcm->header == SENDPACKET || pcm->header == RECVPACKET) {
        global_data = data;
        bToBeInject = true;  // Queue for injection
    }
}
```

### Architecture

The implementation reuses the **existing packet injection infrastructure** from the GUI:

```
TCP Client (Python)
    ↓
[Sends PacketEditorMessage via TCP]
    ↓
TCPCommunicate() in PacketTCP.cpp
    ↓
[Stores in global_data, sets bToBeInject=true]
    ↓
PacketInjector() timer callback (50ms)  ← SHARED WITH GUI
    ↓
SendPacket_Hook() or ProcessPacket_Hook()
    ↓
Game sends/receives injected packet
```

This is **identical** to how the GUI injection works, except:
- GUI uses **Sender Pipe** → `CommunicateThread()` (Packet/PacketSender.cpp:139)
- TCP uses **TCP Socket** → `TCPCommunicate()` (Packet/PacketTCP.cpp:29)

Both feed into the same `global_data` + `bToBeInject` + `PacketInjector()` pipeline.

## Usage

### Python Client Example

See `tcp_inject_example.py` for complete working example.

**Basic injection:**

```python
import socket
import struct

TCP_MESSAGE_MAGIC = 0xA11CE
SENDPACKET = 0
RECVPACKET = 1

def send_inject_packet(sock, header_type, packet_bytes):
    """Inject packet into game"""
    # Build PacketEditorMessage
    message = struct.pack('<IIQI',
        header_type,          # SENDPACKET or RECVPACKET
        0,                    # id (unused)
        0,                    # addr (unused)
        len(packet_bytes)     # packet length
    ) + packet_bytes

    # Wrap in TCPMessage frame
    frame = struct.pack('<II', TCP_MESSAGE_MAGIC, len(message)) + message
    sock.sendall(frame)

# Connect to DLL
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 9999))

# Inject SENDPACKET (client→server)
packet = struct.pack('<H', 0x1234) + b'\x01\x02\x03'  # Header + data
send_inject_packet(sock, SENDPACKET, packet)

# Inject RECVPACKET (server→client simulation)
packet = struct.pack('<H', 0x5678) + b'\xAA\xBB\xCC'
send_inject_packet(sock, RECVPACKET, packet)
```

### Workflow Example

**Scenario**: Respond to server packet with custom client packet

```python
# 1. Wait for RECVPACKET from server
while True:
    data = recv_message(sock)
    msg = parse_packet_message(data)

    if msg['header'] == RECVPACKET:
        packet_data = msg['binary']['packet']
        packet_header = struct.unpack('<H', packet_data[0:2])[0]

        # 2. Check if this is our trigger packet
        if packet_header == 0x1234:
            print("Trigger packet detected!")

            # 3. Inject response SENDPACKET
            response = struct.pack('<H', 0x5678) + b'\x00\x01\x02'
            send_inject_packet(sock, SENDPACKET, response)

            # 4. Disconnect
            sock.close()
            break
```

## Features

### What Works

✅ **SENDPACKET Injection** - Inject outgoing packets (client→server)
- Calls `SendPacket_Hook()` or `SendPacket_EH_Hook()` depending on header encryption
- Identical to GUI "Send" button

✅ **RECVPACKET Injection** - Inject incoming packets (server→client simulation)
- Calls `ProcessPacket_Hook()`
- Identical to GUI "Recv" button

✅ **Same as GUI** - Uses identical code path as pipe-based GUI injection
- No functional differences between TCP and Pipe injection
- Same reliability and compatibility

✅ **Bidirectional** - Can both receive broadcasts AND send injections
- Monitor packets in real-time (DLL → Client)
- Inject packets on demand (Client → DLL)

### Limitations

⚠️ **Single Injection Queue** - Only one injection can be pending at a time
- If `bToBeInject` is already true, new injection is dropped
- Timer callback polls at 50ms, so ~20 injections/second max

⚠️ **No Injection Confirmation** - Client doesn't receive acknowledgment
- Fire-and-forget model
- Cannot determine if injection succeeded

⚠️ **Requires Main Thread** - Injection happens via timer callback
- Needs game's main thread to be running
- Won't work if game is frozen/paused

## Comparison: TCP vs Pipe Injection

| Feature | Pipe (GUI) | TCP (Remote) |
|---------|-----------|--------------|
| **Location** | Local Windows only | Any network client |
| **Language** | C++ (GUI) | Any (Python, etc.) |
| **Injection Method** | Sender Pipe → `CommunicateThread()` | TCP Socket → `TCPCommunicate()` |
| **Shared Code** | `PacketInjector()` timer callback | Same |
| **Hooked Functions** | `SendPacket_Hook()`, `ProcessPacket_Hook()` | Same |
| **Reliability** | Identical | Identical |
| **Latency** | ~50ms (timer poll) | ~50ms (timer poll) + network |

## Testing

### Prerequisites

1. Build and inject Packet.dll with TCP enabled:
   ```ini
   [Packet]
   USE_TCP=1
   TCP_PORT=9999
   ```

2. Ensure `RunPacketSender()` is called in DllMain to start injection infrastructure

### Test Steps

1. **Run example script**:
   ```bash
   python3 tcp_inject_example.py
   ```

2. **Trigger game packet**:
   - Login to game or perform action that triggers target packet header
   - Script will detect RECVPACKET and inject response

3. **Verify injection**:
   - Check debug.log for `[TCP] Packet injection request`
   - Monitor RirePE.exe GUI to see injected packet appear in packet list
   - Observe game behavior (response packet should affect game state)

### Debug Logging

Enable debug mode to see injection flow:

```ini
[Packet]
DEBUG_MODE=1
```

Look for these log messages:
```
[TCP] Client connected to TCP server
[TCP] Received 28 bytes from client
[TCP] Packet injection request: SENDPACKET
[TCP] Packet queued for injection
PacketInjector: SENDPACKET requested
PacketInjector: Packet sent successfully!
```

## Implementation Notes

### Why This Approach Works

1. **Reuses Proven Code** - The GUI injection has been tested extensively
2. **Minimal Changes** - Only added TCP command processing loop
3. **Thread-Safe** - Uses same `global_data` + `bToBeInject` synchronization as pipe
4. **No New Dependencies** - No additional libraries or hooks needed

### Design Decisions

**Q: Why not process injections immediately in `TCPCommunicate()`?**

A: The game's hooked functions (`SendPacket`, `ProcessPacket`) must be called from the **game's main thread** to avoid race conditions and crashes. The timer callback ensures injection happens in the correct thread context.

**Q: Why share `global_data` with pipe injection?**

A: Simplicity and reliability. Rather than create parallel injection systems, we reuse the battle-tested pipe infrastructure. This means:
- Same bugs/features as GUI
- No surprising differences in behavior
- Less code to maintain

**Q: Can pipe and TCP inject simultaneously?**

A: No. Both use the same `global_data` + `bToBeInject` flags. If both try to inject at once, one will be dropped (whichever sets `bToBeInject` first wins). This is acceptable since:
- Most users won't use both simultaneously
- Collision is unlikely (50ms window)
- Adding separate queues would complicate thread safety

## Future Enhancements

Possible improvements (not yet implemented):

1. **Injection Queue** - Buffer multiple injections instead of dropping
2. **Injection Callback** - Send success/failure response to client
3. **Priority System** - Separate queues for TCP vs Pipe with priority
4. **Batch Injection** - Send multiple packets in one command
5. **Conditional Injection** - DLL-side scripting (if packet X, then inject Y)

## Files Modified/Created

### Modified
- `Packet/PacketTCP.cpp` - Added command processing loop
- `Packet/TCP_API_DOCUMENTATION.md` - Documented injection feature

### Created
- `tcp_inject_example.py` - Complete working example
- `TCP_INJECTION_FEATURE.md` - This document

## Conclusion

The TCP API now has **feature parity** with the GUI for packet injection. Remote clients can monitor packets AND inject custom packets, enabling automation, testing, and reverse engineering workflows that were previously only possible with the local GUI.

The implementation is **minimal, safe, and maintainable** by reusing the existing GUI infrastructure rather than creating a parallel system.
