# Architecture: Before vs After

## Before Optimizations (SLOW)

```
┌─────────────────────────────────────────────────────────────────┐
│ MapleStory Game Thread                                          │
│                                                                 │
│  SendPacket() called                                            │
│       ↓                                                         │
│  Hook intercepts                                                │
│       ↓                                                         │
│  new BYTE[] ← Memory allocation                                 │
│       ↓                                                         │
│  Copy packet data                                               │
│       ↓                                                         │
│  pc->Send() ← Synchronous pipe write                            │
│       ↓                                                         │
│  FlushFileBuffers() ← Expensive system call                     │
│       ↓                                                         │
│  pc->Recv() ← BLOCKING WAIT FOR RESPONSE (~1-2ms)              │
│       ↓                                                         │
│  wstring compare "Block" ← String comparison                    │
│       ↓                                                         │
│  delete[] ← Memory deallocation                                 │
│       ↓                                                         │
│  Return to game ← FINALLY! (~2-5ms total per packet)           │
│                                                                 │
│  With 100 packets/sec: 200-500ms of blocking!                  │
│  With 1000 packets/sec: 2-5 SECONDS of blocking!               │
└─────────────────────────────────────────────────────────────────┘

                            ↓
                 RirePE.exe receives
                            ↓
                 Checks block status
                            ↓
                 Sends L"Block" or L"OK"
                            ↓
                 (Game thread waiting...)
```

**Problems:**
- Game thread blocks on EVERY packet
- Memory allocated/freed on EVERY hook call
- Expensive FlushFileBuffers on every pipe operation
- String comparisons for simple true/false
- Linear vector search for packet tracking

---

## After Optimizations (FAST - ENABLE_BLOCKING=0)

```
┌─────────────────────────────────────────────────────────────────┐
│ MapleStory Game Thread                                          │
│                                                                 │
│  SendPacket() called                                            │
│       ↓                                                         │
│  Hook intercepts                                                │
│       ↓                                                         │
│  Allocate from pool ← O(1), reused buffer                       │
│       ↓                                                         │
│  Copy packet data                                               │
│       ↓                                                         │
│  Queue to async worker ← Just add to queue                      │
│       ↓                                                         │
│  Return to game ← IMMEDIATE! (~0.05ms total)                   │
│                                                                 │
│  With 100 packets/sec: ~5ms overhead                           │
│  With 1000 packets/sec: ~50ms overhead                         │
│  With 10000 packets/sec: ~500ms overhead                       │
└─────────────────────────────────────────────────────────────────┘

    ║ (Async - no waiting)
    ║
    ↓

┌─────────────────────────────────────────────────────────────────┐
│ Background Worker Thread                                        │
│                                                                 │
│  While (running):                                               │
│    ↓                                                            │
│    Dequeue up to 16 packets (batch)                            │
│    ↓                                                            │
│    For each packet:                                             │
│      ↓                                                          │
│      pc->Send() ← Async pipe write (no flush)                   │
│      ↓                                                          │
│      (Skip recv if ENABLE_BLOCKING=0)                           │
│      ↓                                                          │
│      Free buffer back to pool                                   │
│    ↓                                                            │
│    Continue (game thread not affected)                          │
└─────────────────────────────────────────────────────────────────┘

                            ↓
                 RirePE.exe receives
                            ↓
                 Logs packet
                            ↓
                 Sends response (1 byte)
                            ↓
                 (Nobody waiting, no problem!)
```

**Improvements:**
- Game thread returns immediately (~0.05ms vs ~2ms)
- Memory pool eliminates allocation overhead
- No FlushFileBuffers
- Binary protocol (1 byte vs wstring)
- Hash map for O(1) packet tracking
- Batch processing for efficiency

---

## After Optimizations (ENABLE_BLOCKING=1)

```
┌─────────────────────────────────────────────────────────────────┐
│ MapleStory Game Thread                                          │
│                                                                 │
│  SendPacket() called                                            │
│       ↓                                                         │
│  Hook intercepts                                                │
│       ↓                                                         │
│  Allocate from pool ← O(1), reused                              │
│       ↓                                                         │
│  Copy packet data                                               │
│       ↓                                                         │
│  Queue with event handle ← Add to queue + event                 │
│       ↓                                                         │
│  WaitForSingleObject() ← BLOCKS here (~0.5-2ms)                │
│       ↓                                                         │
│  Check bBlock result                                            │
│       ↓                                                         │
│  Return to game ← (~1-3ms total)                               │
│                                                                 │
│  Better than before but still blocking per packet              │
└─────────────────────────────────────────────────────────────────┘

    ║ (Waiting...)
    ║
    ↓

┌─────────────────────────────────────────────────────────────────┐
│ Background Worker Thread                                        │
│                                                                 │
│  Dequeue packet                                                 │
│    ↓                                                            │
│  pc->Send() ← Async pipe write                                  │
│    ↓                                                            │
│  pc->Recv() ← Wait for response (1 byte)                        │
│    ↓                                                            │
│  Set bBlock result                                              │
│    ↓                                                            │
│  SetEvent() ← Signal game thread                                │
│    ↓                                                            │
│  Free buffer                                                    │
└─────────────────────────────────────────────────────────────────┘

                            ↓
                 RirePE.exe receives
                            ↓
                 Checks block status
                            ↓
                 Sends 1 or 0 (binary)
                            ↓
                 Worker thread continues
```

**Still improved from before:**
- Memory pool (no new/delete per packet)
- Binary protocol (1 byte)
- Hash map tracking
- No FlushFileBuffers

**But still slow because:**
- Game thread blocks waiting for response
- Can't batch process (each packet waits)

---

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       MapleStory.exe                            │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                     Packet.dll                            │ │
│  │                                                           │ │
│  │  ┌─────────────┐    ┌──────────────┐   ┌─────────────┐  │ │
│  │  │   Hooks     │───>│ PacketQueue  │──>│ BufferPool  │  │ │
│  │  │  (Game Fns) │    │   (Async)    │   │  (Reuse)    │  │ │
│  │  └─────────────┘    └──────────────┘   └─────────────┘  │ │
│  │         │                    │                            │ │
│  │         │                    │                            │ │
│  │         └──── PacketLogging ─┘                            │ │
│  │                      │                                    │ │
│  └──────────────────────┼────────────────────────────────────┘ │
│                         │                                      │
└─────────────────────────┼──────────────────────────────────────┘
                          │ Named Pipe
                          │ (\\.\pipe\PacketLogger)
┌─────────────────────────┼──────────────────────────────────────┐
│                         │                                      │
│  ┌──────────────────────▼────────────────────────────────────┐ │
│  │                   PipeServer                             │ │
│  │                (SimplePipe.h)                            │ │
│  └──────────────────────┬────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼────────────────────────────────────┐ │
│  │               PacketLogger.cpp                           │ │
│  │         (Receive, log, send response)                    │ │
│  └──────────────────────┬────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼────────────────────────────────────┐ │
│  │                   MainGUI                                │ │
│  │         (Display packets, block checkbox)                │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│                      RirePE.exe                                │
└────────────────────────────────────────────────────────────────┘
```

---

## Data Flow - Sending a Packet

### Fast Mode (ENABLE_BLOCKING=0)

```
Game calls SendPacket()
  → Hook: SendPacket_Hook()
     → AddSendPacket(op, addr, bBlock)
        → AddExtraAll(op) [Process queued format info]
        → Allocate buffer from pool
        → Copy packet data
        → g_PacketQueue->QueuePacket() [Non-blocking]
           → Add to queue
           → Signal worker thread
        → Return immediately (bBlock = false)
     → Check bBlock (false)
     → Call _SendPacket() [Original function]
  → Packet sent to server
  → Game continues (total: ~0.05ms)

[Meanwhile in background...]

Worker thread:
  → Dequeue packet
  → Send to RirePE via pipe
  → Free buffer to pool
  → Continue

RirePE receives:
  → Log packet
  → Display in UI
  → Send response (ignored by DLL)
```

### Blocking Mode (ENABLE_BLOCKING=1)

```
Game calls SendPacket()
  → Hook: SendPacket_Hook()
     → AddSendPacket(op, addr, bBlock)
        → AddExtraAll(op)
        → Allocate buffer from pool
        → Copy packet data
        → Create event handle
        → g_PacketQueue->QueuePacketBlocking()
           → Add to queue with event
           → Signal worker thread
           → WaitForSingleObject() [BLOCKS HERE]
              [Game thread paused...]

[Worker thread:]
  → Dequeue packet
  → Send to RirePE via pipe
  → Wait for response
  → Receive response (1 byte)
  → Set bBlock result
  → SetEvent() [Wake game thread]
  → Free buffer

           → [Game thread wakes]
           → Return with bBlock result
     → Check bBlock
     → If blocked: return early
     → Else: Call _SendPacket()
  → Packet sent to server (unless blocked)
  → Game continues (total: ~1-3ms)
```

---

## Memory Management

### Before
```
Hook called
  → new BYTE[packet_size]
  → Use memory
  → delete[]

With 1000 packets/sec:
  → 1000 allocations/sec
  → 1000 deallocations/sec
  → Memory fragmentation
  → Slow performance
```

### After
```
Startup:
  → Pre-allocate 64 buffers (8KB each)
  → Total: 512KB

Hook called:
  → Find free buffer in pool O(1)
  → Use buffer
  → Return buffer to pool O(1)

With 1000 packets/sec:
  → 0 allocations (unless >64 concurrent)
  → 0 deallocations
  → No fragmentation
  → Fast performance
```

---

## Configuration Impact

| Setting           | Game Thread | Throughput | Block Feature | Use Case                    |
|-------------------|-------------|------------|---------------|-----------------------------|
| ENABLE_BLOCKING=0 | ~0.05ms     | Very High  | Disabled      | Logging/viewing (99% users) |
| ENABLE_BLOCKING=1 | ~1-3ms      | Medium     | Enabled       | Packet filtering (power users) |

---

## Performance Comparison

### Scenario: 1000 packets over 10 seconds (100 pkt/s)

**Before optimizations:**
- Total blocking time: 2-5 seconds
- Allocations: 1000+
- User experience: Severe lag

**After with ENABLE_BLOCKING=0:**
- Total blocking time: ~50ms
- Allocations: 0 (pool)
- User experience: No perceptible lag

**After with ENABLE_BLOCKING=1:**
- Total blocking time: 1-3 seconds
- Allocations: 0 (pool)
- User experience: Some lag (acceptable for blocking use case)

---

## Key Takeaways

1. **Async is critical** - Don't block game thread unless necessary
2. **Memory pools matter** - Eliminate allocation overhead
3. **Batching helps** - Process multiple items at once
4. **Configuration is key** - Let users choose speed vs features
5. **Profile first** - We found the real bottleneck after measuring
