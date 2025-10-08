# Verification Checklist

Use this checklist to verify the performance fix is working correctly.

## Pre-Build Checks

- [ ] All new files are present:
  - `Packet/PacketQueue.h`
  - `Packet/PacketQueue.cpp`
  - `PERFORMANCE_FIX.md`
  - `CRITICAL_FIX_README.md`
  - `PERFORMANCE_OPTIMIZATIONS.md`
  - `CHANGES_SUMMARY.md`
  - `VERIFICATION_CHECKLIST.md` (this file)
  - `RirePE.ini.example`

- [ ] Project files updated:
  - `Packet/Packet.vcxproj` includes PacketQueue.cpp and PacketQueue.h
  - `Packet64/Packet64.vcxproj` includes PacketQueue.cpp and PacketQueue.h

- [ ] Modified files have changes:
  - `Packet/DllMain.cpp` - InitializePacketQueue, ShutdownPacketQueue, ENABLE_BLOCKING config
  - `Packet/PacketLogging.cpp` - g_EnableBlocking, async queue usage
  - `Packet/PacketHook.h` - enable_blocking field
  - `Share/Simple/SimplePipeClient.cpp` - FlushFileBuffers removed
  - `RirePE/PacketLogger.cpp` - Binary response (BYTE instead of wstring)

## Build Checks

- [ ] Solution builds without errors
- [ ] No warnings about PacketQueue files
- [ ] Both Packet.dll and RirePE.exe compile successfully
- [ ] (If x64) Both Packet64.dll and RirePE64.exe compile successfully

## Configuration Checks

- [ ] Created `RirePE.ini` (or `RirePE64.ini`) in DLL directory
- [ ] File contains `[Packet]` section
- [ ] File contains `ENABLE_BLOCKING=0` line
- [ ] File is in same directory as the DLL

Example minimal config:
```ini
[Packet]
ENABLE_BLOCKING=0
```

## Runtime Checks - Fast Mode (ENABLE_BLOCKING=0)

- [ ] DLL injects successfully into MapleStory
- [ ] RirePE.exe launches and shows "Connected"
- [ ] Client runs at near-native speed
- [ ] Packets appear in RirePE.exe logger
- [ ] Packet headers are correct
- [ ] Format information shows encode/decode operations
- [ ] No crashes during packet logging
- [ ] No memory leaks (Task Manager stable over 5+ minutes)

**Performance Test:**
- [ ] Login sequence is smooth
- [ ] Map changes are fast
- [ ] Inventory operations don't lag
- [ ] Combat feels responsive
- [ ] Can handle 100+ packets/second without slowdown

## Runtime Checks - Blocking Mode (ENABLE_BLOCKING=1)

Optional - only test if you need blocking functionality:

- [ ] Set `ENABLE_BLOCKING=1` in INI
- [ ] Restart client + DLL injection
- [ ] RirePE.exe shows "Connected"
- [ ] Client is slower (expected with blocking)
- [ ] "Block" checkbox in RirePE.exe UI works
- [ ] Blocking a packet prevents it from sending
- [ ] Unblocking allows packet to send again

**Note:** Some slowdown is expected with ENABLE_BLOCKING=1. This is normal.

## Stress Test (Optional)

Test with high packet volume:

- [ ] Stand in crowded map (many players/NPCs)
- [ ] Perform rapid actions (spam attack, open/close inventory)
- [ ] Client remains responsive
- [ ] No queue overflow errors
- [ ] Memory usage stays stable
- [ ] No crashes after 10+ minutes

## Troubleshooting If Checks Fail

### Build fails
→ Check that all files are in correct directories
→ Verify project files include PacketQueue.cpp/h
→ Clean solution and rebuild

### Client still slow with ENABLE_BLOCKING=0
→ Verify INI file is in correct location
→ Check INI file is named correctly (RirePE.ini or RirePE64.ini)
→ Enable DEBUG_MODE=1 to see if config is loading
→ Verify both DLL and EXE are newly compiled

### Packets don't appear in logger
→ Check RirePE.exe is running
→ Check pipe connection
→ Enable DEBUG_MODE=1 for detailed output
→ Verify no firewall blocking named pipes

### Memory leaks
→ Check Task Manager > Details > Packet.exe
→ Memory should stabilize after initial spike
→ If continuously growing, increase buffer pool size or report bug

### Crashes
→ Enable DEBUG_MODE=1
→ Check debug output for errors
→ Verify all code changes applied correctly
→ Consider rollback if persistent

## Success Criteria

✅ Client runs at near-native speed with ENABLE_BLOCKING=0
✅ All packets are logged correctly
✅ Format information is complete
✅ No memory leaks
✅ No crashes during normal gameplay
✅ Can handle high packet volume without slowdown

## If Everything Passes

🎉 **Congratulations!** The performance fix is working correctly.

You can now:
- Use RirePE for packet analysis without performance impact
- Share the fix with others experiencing the same issue
- Document any edge cases you find

## If Something Fails

1. Check the specific section above for troubleshooting steps
2. Read `CRITICAL_FIX_README.md` for detailed troubleshooting
3. Try setting ENABLE_BLOCKING=1 to see if it's a config issue
4. Consider partial rollback (keep optimizations, enable blocking)
5. Full rollback if persistent issues

## Reporting Issues

If you find bugs, document:
- Which checks failed
- Error messages (if any)
- Configuration used
- Client version
- Build configuration (Debug/Release, x86/x64)
- Steps to reproduce

This helps diagnose issues faster.
