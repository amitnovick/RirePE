# Bug Fix: Pipe Synchronization Issue

## Problem
After implementing async packet logging with `ENABLE_BLOCKING=0`, only the first packet was logged and then logging stopped.

## Root Cause

When `ENABLE_BLOCKING=0`:
1. Game thread queues packet and returns immediately (correct)
2. Worker thread sends packet to RirePE.exe via pipe
3. RirePE.exe processes packet and sends response (1 byte)
4. **Worker thread didn't read the response** (bug!)
5. Response stayed in pipe buffer
6. Next packet sent → pipe buffer full → deadlock

## The Issue

```cpp
// BEFORE (BROKEN):
if (qp.needs_response) {
    // Only read response if game thread is waiting
    pc->Recv(vData);
}
// Problem: RirePE.exe ALWAYS sends response for SEND/RECV packets
// If we don't read it, pipe buffer fills up!
```

## The Fix

```cpp
// AFTER (FIXED):
// Check if this packet TYPE expects a response
bool expects_response = false;
if (qp.size >= sizeof(PacketEditorMessage)) {
    PacketEditorMessage* pem = (PacketEditorMessage*)qp.data;
    expects_response = (pem->header == SENDPACKET || pem->header == RECVPACKET);
}

// Always read response to keep pipe in sync
if (expects_response) {
    pc->Recv(vData);
    // Only store result if game thread is waiting
    if (qp.needs_response) {
        qp.block_result = (response == 1);
    }
}
```

## Explanation

**Two different concepts:**
1. **`qp.needs_response`:** Does the *game thread* need to wait for the response?
   - `false` with `ENABLE_BLOCKING=0` (async mode)
   - `true` with `ENABLE_BLOCKING=1` (blocking mode)

2. **`expects_response`:** Does *RirePE.exe* send a response for this packet type?
   - `true` for SENDPACKET and RECVPACKET (always get responses)
   - `false` for ENCODE/DECODE format info (no response)

**The fix:**
- Worker thread ALWAYS reads responses when the packet type expects one
- This keeps the pipe synchronized
- Game thread only blocks when `needs_response` is true
- Result stored only when game thread is waiting

## Files Changed
- `Packet/PacketQueue.cpp` - Fixed ProcessQueue() to always consume responses

## Testing

### Before Fix
```
Send packet 1 → Logs ✓
Send packet 2 → Hangs ✗ (pipe buffer full)
Send packet 3 → Hangs ✗
```

### After Fix
```
Send packet 1 → Logs ✓
Send packet 2 → Logs ✓
Send packet 3 → Logs ✓
... all packets log correctly
```

## Lesson Learned

When using asynchronous communication over pipes:
- Always consume responses from the server
- Don't confuse "game thread needs to wait" with "server sends response"
- Keep pipe protocol in sync regardless of blocking mode

This is a common mistake in async I/O implementations!
