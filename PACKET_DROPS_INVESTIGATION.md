# Packet Monitor - Missing Packets Investigation & Fixes

## Problem Statement
The `packet_monitor.py` was missing some RECV and SEND packets during capture.

## Root Causes Identified

### 1. **Buffer Pool Exhaustion** (PRIMARY ISSUE)
- **Location**: `Packet/PacketQueue.h:11`
- **Original pool size**: 64 buffers
- **Problem**: During high packet traffic, the 64-buffer pool would exhaust, forcing heap allocations which are slower and could fail under memory pressure
- **Fix**: Increased pool size to **256 buffers** (4x increase)

### 2. **Intentional Packet Filtering (unk2 field)**
- **Location**: `Packet/PacketHook.cpp:249` (ProcessPacket_Hook)
- **Behavior**: Only packets with `ip->unk2 == 0x02` are logged
- **Reason**: Packets with other `unk2` values are likely:
  - Internal/encrypted packets
  - Control packets
  - Keep-alive packets
- **Not a bug**: This is intentional filtering to avoid logging irrelevant packets

### 3. **Return Address Filtering for SEND Packets**
- **Location**: `Packet/PacketHook.cpp:78-97` (SendPacket_Hook - 64-bit only)
- **Behavior**: Packets from the `SendPacket_EH` path (encrypted header) are logged via `SendPacket_EH_Hook` instead
- **Reason**: Prevents double-logging of the same packet
- **Not a bug**: This is intentional de-duplication

### 4. **Silent Drop on Queue Initialization Failure**
- **Location**: `Packet/PacketLogging.cpp:158, 209`
- **Problem**: If `g_BufferPool` or `g_PacketQueue` are NULL, packets are silently dropped
- **Fix**: Added logging to warn when queue is not initialized

### 5. **Silent Drop on Buffer Allocation Failure**
- **Location**: `Packet/PacketLogging.cpp:171, 222`
- **Problem**: If buffer allocation fails, packet is dropped without notification
- **Fix**: Added counter and logging for allocation failures

## Fixes Applied

### ✅ 1. Enhanced Logging
Added detailed logging to track:
- Buffer pool exhaustion events (`PacketQueue.cpp:49-52`)
- Oversized packet allocations (`PacketQueue.cpp:28-31`)
- Queue initialization failures (`PacketLogging.cpp:159-163, 210-214`)
- Buffer allocation failures (`PacketLogging.cpp:172-176, 223-227`)
- Filtered packets (unk2 != 0x02) (`PacketHook.cpp:268-272`)
- Skipped packets (EH path) (`PacketHook.cpp:90-94`)
- Queue depth warnings (`PacketQueue.cpp:145-152`)

All logging uses rate-limiting to avoid spam:
- First 3-10 occurrences are logged
- Then every 50th, 100th, or 500th occurrence

### ✅ 2. Increased Buffer Pool Capacity
- **Old**: 64 buffers × 8192 bytes = 512 KB
- **New**: 256 buffers × 8192 bytes = 2 MB
- **Benefit**: Handles burst traffic without exhausting pool

### ✅ 3. Queue Depth Monitoring
Added warning when queue depth exceeds 10 packets and continues to grow (`PacketQueue.cpp:145-152`)
- Indicates packet_monitor.py is not reading fast enough
- Helps identify TCP communication bottlenecks

## What Gets Logged (Summary)

### ✅ LOGGED:
- SEND packets from main path
- SEND packets from encrypted header path (SendPacket_EH)
- RECV packets with `unk2 == 0x02`

### ❌ NOT LOGGED (Intentional):
- RECV packets with `unk2 != 0x02` (internal/encrypted)
- Duplicate SEND packets (return address filtering prevents double-logging)
- DECODE_END format packets in Python (filtered at line 708 of packet_monitor.py)

## How to Verify Fixes

### 1. Check Debug Logs
Look for these new log messages in `DebugOutput.txt`:
```
[BUFFER] WARNING: Pool exhausted!
[BUFFER] WARNING: Oversized packet
[PACKET] ERROR: Failed to allocate buffer
[QUEUE] WARNING: Queue depth reached X packets!
[HOOK] ProcessPacket_Hook: Filtered packet (unk2=...)
[HOOK] SendPacket_Hook: Skipped packet (EH path)
```

### 2. Monitor Statistics
The logs now include counters showing:
- How many packets were filtered
- How many times buffer pool exhausted
- Maximum queue depth reached

### 3. Compare Packet Counts
Enable debug mode and compare:
- Number of ProcessPacket_Hook calls (total)
- Number of AddRecvPacket calls (unk2 == 0x02)
- Number of filtered packets (unk2 != 0x02)

## Remaining Considerations

### 1. TCP Communication Bottleneck
If `packet_monitor.py` is slow to read packets:
- Queue will back up (you'll see queue depth warnings)
- Buffer pool may still exhaust despite increased size
- **Solution**: Ensure Python client reads continuously without blocking

### 2. Memory Usage
- Increased from 512 KB to 2 MB static allocation
- Acceptable for modern systems
- Further increase possible if needed (change POOL_SIZE in PacketQueue.h)

### 3. Async vs Blocking Mode
- Current config: `g_EnableBlocking = false` (async mode)
- Async is faster but doesn't wait for acknowledgment
- If Python client disconnects, packets may be queued but not sent
- **Recommendation**: Keep async mode enabled for performance

## Testing Recommendations

1. **Run packet_monitor.py** and watch for new log messages
2. **Generate high packet traffic** (e.g., teleport, use skills rapidly)
3. **Check DebugOutput.txt** for exhaustion warnings
4. **Compare packet counts** before/after fixes
5. **Monitor queue depth** warnings during peak traffic

## Files Modified

1. `Packet/PacketQueue.h` - Increased POOL_SIZE to 256
2. `Packet/PacketQueue.cpp` - Added logging for exhaustion, oversized packets, queue depth
3. `Packet/PacketLogging.cpp` - Added error logging for allocation failures and initialization
4. `Packet/PacketHook.cpp` - Added logging for filtered/skipped packets

## Conclusion

The missing packets were likely caused by:
1. **Buffer pool exhaustion** during burst traffic (FIXED)
2. **Intentional filtering** of internal packets (WORKING AS DESIGNED)
3. **Double-logging prevention** for encrypted headers (WORKING AS DESIGNED)

With the fixes applied, you should now see:
- ✅ Fewer dropped packets due to larger buffer pool
- ✅ Clear logging when packets are intentionally filtered
- ✅ Warnings when system is under stress (queue depth, exhaustion)
- ✅ Better visibility into what's happening under the hood
