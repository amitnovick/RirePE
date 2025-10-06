# Debugging SendPacket Button Issue

## Summary
The SendPacket button is not working because the `EnterSendPacket` function cannot be found via AOB (Array of Bytes) scanning. This is the critical function needed for the SendPacket button to work.

## Root Cause Analysis

### How SendPacket Button Works (32-bit)
1. **User clicks SendPacket button** in RirePE.exe
2. **Packet is sent via named pipe** to Packet.dll
3. **PacketInjector** receives the packet (PacketSender.cpp:9)
4. **EnterSendPacket_Hook** is called (PacketSender.cpp:59)
5. **_EnterSendPacket** (the actual game function) is called to inject the packet

### The Problem
- `_EnterSendPacket` is found via AOB pattern matching (PacketHook.cpp:476-483)
- Two patterns are tried:
  - Pattern 0: v92 (AobList.h:110) - Has hardcoded address `A0 EF C2 00`
  - Pattern 1: v188 (AobList.h:114) - Generic pattern
- If BOTH patterns fail, `_EnterSendPacket` stays NULL
- When NULL, the SendPacket button will crash or fail silently

### Why It Fails for v92
The v92 pattern in AobList.h:110 contains a hardcoded address:
```
L"8B 44 24 04 8B 0D A0 EF C2 00 50 E8 ?? ?? ?? ?? C3"
                      ^^^^^^^^^^^
                      This address is specific to ONE v92 build!
```

Your KaizenMS v92 client has a different memory layout, so this exact pattern won't match.

## Diagnostic Logging

I've added comprehensive file-based logging to help diagnose the issue:

### Log File Location
```
<GameFolder>/RirePE_Debug.log
```
For you: `/home/amit/Downloads/KaizenMS/Kaizen v92/RirePE_Debug.log`

### What Gets Logged

**During DLL initialization:**
- Hook configuration start
- SendPacket address from config
- EnterSendPacket AOB scan attempts
- Success/failure of finding EnterSendPacket
- All found addresses

**When SendPacket button is clicked:**
- Packet injection request
- Packet length and header
- Whether _EnterSendPacket is NULL or valid
- Function call trace

## How to Diagnose

### Step 1: Rebuild the DLL
On Windows (since this is a Visual Studio project):
```bash
# Open RirePE.sln in Visual Studio
# Build Configuration: Release, Platform: Win32
# Build > Build Solution (or press F7)
```

The new DLL with logging will be in: `RirePE/Release/Packet.dll`

### Step 2: Replace the DLL
Copy the new `Packet.dll` to your executable folder:
```
/home/amit/Downloads/reverse-engineering/executable-RirePE/Amit/Packet.dll
```

### Step 3: Test and Collect Logs
1. Clear any existing log: Delete `RirePE_Debug.log` if it exists
2. Inject the DLL into MapleStory
3. Wait for RirePE.exe to load
4. Try clicking the SendPacket button
5. Check `RirePE_Debug.log` in the game folder

### Step 4: Analyze the Log

**Look for this line:**
```
PacketHook_Conf: ERROR - EnterSendPacket NOT FOUND! SendPacket button will not work!
```

**If you see it**, the AOB patterns don't match your client.

**Also check:**
```
ScannerEnterSendPacket: Expected SendPacket: 0x004AC120
ScannerEnterSendPacket: Found function: 0xXXXXXXXX
ScannerEnterSendPacket: Function mismatch, returning false
```

This shows the scanner found a pattern, but the function it calls doesn't match your configured SendPacket address.

## Solutions

### Solution 1: Find the Correct AOB Pattern (Manual - Windows Only)

You'll need to use a tool like Cheat Engine or x64dbg to:

1. **Find EnterSendPacket in your client:**
   - Look for a function that:
     - Takes 1 parameter (OutPacket pointer)
     - Calls SendPacket (0x004AC120)
     - Has the signature: `push [esp+4]; mov ecx, [XXXXXXXX]; call YYYYYYYY; ret`

2. **Extract the byte pattern:**
   - Use Cheat Engine's "Find what accesses this address" on SendPacket
   - Look for a small wrapper function
   - Copy the exact bytes

3. **Add it to AobList.h:**
   ```cpp
   std::wstring AOB_EnterSendPacket[] = {
       // v92 KaizenMS - YOUR PATTERN HERE
       L"YOUR PATTERN WITH ?? FOR WILDCARDS",
       // v92 original (won't work for you)
       L"8B 44 24 04 8B 0D A0 EF C2 00 50 E8 ?? ?? ?? ?? C3",
       // v164.0 to v186.1
       L"FF 74 24 04 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C3",
       // v188.0
       L"8B 44 24 04 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? C3",
   };
   ```

### Solution 2: Add Manual Configuration Support (Easier)

Modify the code to support manual `EnterSendPacket=` configuration in RirePE.ini.

I can help implement this if you prefer this approach.

### Solution 3: Use a Generic v92 Pattern

Try removing the hardcoded address from the v92 pattern:

**Edit AobList.h:110:**
```cpp
// OLD (specific address):
L"8B 44 24 04 8B 0D A0 EF C2 00 50 E8 ?? ?? ?? ?? C3",

// NEW (generic wildcards):
L"8B 44 24 04 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? C3",
```

This makes it match ANY v92 client, not just one specific build.

## Files Modified

1. **Share/Simple/DebugLog.h** (NEW)
   - File-based logging utility

2. **Packet/PacketHook.cpp**
   - Added logging to hook initialization
   - Added logging to EnterSendPacket scanners
   - Shows exactly what addresses are being searched and found

3. **Packet/PacketSender.cpp**
   - Added logging when SendPacket button is clicked
   - Shows whether _EnterSendPacket is NULL
   - Traces the packet injection flow

## Next Steps

1. **Build the modified DLL** (requires Windows + Visual Studio)
2. **Test with logging** to confirm the diagnosis
3. **Choose a solution** based on the log output:
   - If patterns are finding matches but wrong function → Try Solution 3 (generic pattern)
   - If no patterns match at all → Need Solution 1 (find correct pattern) or Solution 2 (manual config)

## Questions?

Check the log file first. It will tell you exactly what's happening:
- Is SendPacket being hooked?
- Is EnterSendPacket being found?
- What addresses are being discovered?
- Is the button actually being clicked?

The log file will answer all these questions!
