# Migration Guide: RirePE.exe GUI → Headless TCP Mode

This guide helps users transition from the old RirePE.exe GUI to the new headless TCP-based architecture.

## What Changed?

### Removed
- **RirePE.exe** Windows GUI application
- Named pipe communication
- Visual packet list window
- Built-in format viewer
- Packet blocking checkbox UI

### Added
- **Headless operation** (no GUI, TCP-only)
- Python monitoring scripts
- Remote monitoring capability
- Multiple simultaneous clients

## Migration Steps

### Step 1: Update Files

**Old deployment:**
```
MapleStory/
  ├── MapleStory.exe
  ├── Packet.dll
  ├── RirePE.exe       ← Remove this
  └── RirePE.ini
```

**New deployment:**
```
MapleStory/
  ├── MapleStory.exe
  ├── Packet.dll       ← Updated version
  └── RirePE.ini       ← Update configuration
```

### Step 2: Update Configuration

**Old RirePE.ini:**
```ini
[Packet]
USE_TCP=0              ← Old: TCP was optional
```

**New RirePE.ini:**
```ini
[Packet]
TCP_PORT=8275          ← New: TCP is mandatory
TCP_HOST=localhost
```

### Step 3: Change Monitoring Workflow

#### Old Workflow
1. Inject Packet.dll into MapleStory
2. RirePE.exe launches automatically
3. View packets in Windows GUI
4. Check "Block" checkbox to block packets

#### New Workflow
1. Inject Packet.dll into MapleStory
2. Run monitoring tool: `python packet_monitor.py localhost 8275`
3. View packets in terminal
4. Set `ENABLE_BLOCKING=1` in config to block packets programmatically

## Feature Comparison

| Feature | Old (RirePE.exe) | New (Headless) |
|---------|------------------|----------------|
| Packet viewing | Windows ListView | Terminal/script |
| Format viewer | Visual window | Script-based parsing |
| Packet blocking | Checkbox UI | Config file + API |
| Packet sending | Button + input box | Python script |
| Remote monitoring | No | Yes (TCP) |
| Multiple viewers | No | Yes (multiple TCP clients) |
| Cross-platform | Windows only | Any OS with TCP |

## Common Tasks

### Task: View Packets

**Old:**
- Look at RirePE.exe ListView window

**New:**
```bash
python packet_monitor.py localhost 8275
```

Output:
```
[2025-10-10 12:34:56] SEND 0x0015 (12 bytes)
  00 01 02 03 04 05 06 07 08 09 0A 0B
[2025-10-10 12:34:56] RECV 0x0027 (48 bytes)
  48 65 6C 6C 6F 20 57 6F 72 6C 64 ...
```

### Task: Send Packet to Game

**Old:**
- Click "Send" button in RirePE.exe
- Enter packet data in textbox

**New:**
```bash
python tcp_inject_example.py
```

Or programmatically:
```python
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8275))

# Send packet with opcode 0x15
opcode = 0x15
packet_data = bytes([0x01, 0x02, 0x03, 0x04])

header = 0  # SENDPACKET
addr = 0
size = len(packet_data)

message = struct.pack('<IIII', header, opcode, addr, size) + packet_data
sock.send(message)
```

## Troubleshooting

### "I can't see any packets"

**Check:**
1. Is Packet.dll injected? (check with Process Explorer)
2. Is TCP server running? (check debug log: `Share/Simple/debug_output.txt`)
3. Is monitoring script connected? (should show "Connected" message)
4. Is MapleStory sending packets? (try logging in or moving character)

**Debug:**
```bash
# Enable debug mode
# RirePE.ini:
DEBUG_MODE=1

# Check log file
cat Share/Simple/debug_output.txt
```

### "RirePE.exe is missing"

This is expected. RirePE.exe has been removed. Use `packet_monitor.py` instead.

### "Connection refused on port 8275"

**Possible causes:**
1. Packet.dll not injected yet
2. TCP server failed to start (check debug log)
3. Port already in use (change TCP_PORT in config)

**Solution:**
```bash
# Check if port is in use
netstat -an | grep 8275

# Try different port
# RirePE.ini:
TCP_PORT=9999
```

## Advantages of New Architecture

### 1. Remote Monitoring
Monitor packets from any machine on your network:
```bash
python packet_monitor.py 192.168.1.100 8275
```

### 2. Multiple Viewers
Run multiple monitoring scripts simultaneously:
```bash
# Terminal 1: General packet viewer
python packet_monitor.py localhost 8275

# Terminal 2: Opcode-specific filter
python custom_filter.py localhost 8275

# Terminal 3: Packet injection tool
python tcp_inject_example.py localhost 8275
```

### 3. Automation
Automate packet analysis:
```bash
# Log all packets to file
python packet_monitor.py localhost 8275 > packets.log

# Analyze with grep/awk
cat packets.log | grep "SEND 0x0015"
```

### 4. Integration
Integrate with existing tools:
- Parse packets with Wireshark dissectors
- Store in database for analysis
- Build web dashboards
- Create Discord bots that report events

## Need Help?

See documentation:
- `README.md`: Overview and quick start
- `ARCHITECTURE.md`: Technical details
- `Packet/TCP_API_DOCUMENTATION.md`: TCP protocol specification
- `tcp_inject_example.py`: Example code
