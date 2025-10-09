# Phase 9 Testing Checklist

This document contains the comprehensive testing checklist for the headless packet logger (Phase 9).

## Build Testing

### Clean Build Test
```bash
# Clean workspace
git clean -fdx

# Rebuild dependencies
msbuild Share/Simple/Simple.sln /p:Configuration=Release /p:Platform=x86
msbuild Share/Hook/Hook.sln /p:Configuration=Release /p:Platform=x86

# Rebuild Packet.dll
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

**Expected Results:**
- [ ] All projects build without errors
- [ ] No warnings related to missing RirePE.h
- [ ] Packet.dll binary is created in `Packet/Release/` or `Release/`

## Functional Testing

### Basic Functionality
- [ ] Packet.dll injects successfully into target process
- [ ] No immediate crashes after injection
- [ ] TCP server starts on configured port (default: 8275)
- [ ] Debug log shows initialization messages

### TCP Server Testing
- [ ] TCP server binds to configured port
- [ ] Server accepts client connections
- [ ] Multiple clients can connect simultaneously
- [ ] Server doesn't crash when client disconnects
- [ ] Server handles reconnection gracefully

### Packet Monitoring
- [ ] packet_monitor.py connects successfully
- [ ] Packets are displayed in real-time
- [ ] Packet data includes correct opcode
- [ ] Packet data includes correct size
- [ ] Packet data shows correct hex dump
- [ ] Both SEND and RECV packets are captured
- [ ] Packet timestamps are reasonable

### Packet Injection
- [ ] tcp_inject_example.py connects successfully
- [ ] Can send packets to the game
- [ ] Game receives injected packets
- [ ] Server responds with success/block status
- [ ] No crashes when injecting malformed packets

### Configuration Testing
- [ ] TCP_PORT configuration is respected
- [ ] TCP_HOST configuration is respected
- [ ] ENABLE_BLOCKING configuration works
- [ ] DEBUG_MODE enables detailed logging
- [ ] Configuration changes take effect after restart

### Packet Blocking
Set `ENABLE_BLOCKING=1` in RirePE.ini, then test:
- [ ] Blocking script can connect
- [ ] Blocking script receives packet notification
- [ ] Responding with 0x01 blocks packet
- [ ] Responding with 0x00 allows packet
- [ ] Game behavior reflects blocked/allowed packets

### Multiple Client Testing
- [ ] Run packet_monitor.py in one terminal
- [ ] Run tcp_inject_example.py in another terminal
- [ ] Both clients receive packets
- [ ] Both clients can operate independently
- [ ] No packet loss between clients
- [ ] Disconnecting one client doesn't affect others

### Error Handling
- [ ] Connecting to wrong port shows clear error
- [ ] Connection refused handled gracefully
- [ ] Client disconnect doesn't crash server
- [ ] Server handles partial message reads
- [ ] Invalid packet data doesn't crash server

### Performance Testing
Run the game for 10+ minutes with monitoring enabled:
- [ ] CPU usage is reasonable (< 5% idle, < 20% in combat)
- [ ] Memory usage is stable (no leaks)
- [ ] Packet queue doesn't grow unbounded
- [ ] No stuttering or frame drops
- [ ] TCP connection remains stable
- [ ] Debug log doesn't grow excessively

### Stress Testing
- [ ] Rapid connect/disconnect cycles don't crash server
- [ ] High packet rate (combat/boss fights) handled smoothly
- [ ] Queue handles burst traffic without loss
- [ ] Multiple simultaneous clients under load

## Integration Testing

### Python Script Testing
- [ ] packet_monitor.py runs without Python errors
- [ ] tcp_inject_example.py runs without Python errors
- [ ] Scripts handle keyboard interrupt (Ctrl+C) cleanly
- [ ] Scripts show helpful error messages
- [ ] Scripts reconnect after server restart

### Documentation Testing
- [ ] README.md instructions work as written
- [ ] MIGRATION_GUIDE.md steps are accurate
- [ ] Configuration examples in RirePE.ini.example are valid
- [ ] Code examples in documentation compile/run

## Regression Testing

### Core Functionality (shouldn't change)
- [ ] Packet hooking still works
- [ ] Send/Recv detection is accurate
- [ ] Packet opcodes are correct
- [ ] Packet data is not corrupted
- [ ] Game stability is maintained

### No GUI Launch
- [ ] Packet.dll does NOT launch RirePE.exe
- [ ] No attempt to connect to named pipe
- [ ] No GUI window appears
- [ ] No ShellExecuteW() calls

### No Pipe Code
- [ ] No named pipe client code is active
- [ ] No pipe-related errors in debug log
- [ ] g_UseTCP variable no longer exists
- [ ] All communication is via TCP

## Security Testing

### Network Security
- [ ] Default config binds to localhost only
- [ ] Binding to 0.0.0.0 requires explicit config
- [ ] No unauthorized connections accepted
- [ ] No buffer overflows with malformed packets

### Code Security
- [ ] No obvious injection vulnerabilities
- [ ] No use-after-free bugs
- [ ] No race conditions in packet queue
- [ ] Thread safety maintained

## Debug Log Verification

Check `Share/Simple/debug_output.txt` for:
- [ ] Initialization messages are clear
- [ ] TCP server start is logged
- [ ] Port number is displayed
- [ ] Connection events are logged
- [ ] Error messages are descriptive
- [ ] No unexpected warnings or errors

Expected log lines:
```
[INIT] Starting headless packet logger (TCP-only mode)
[INIT] Starting TCP server...
[INIT] TCP server listening on port 8275
[INIT] Starting packet sender...
[INIT] Headless logger initialized successfully
[INIT] Connect using: python packet_monitor.py
```

## CI/CD Testing

### GitHub Actions
- [ ] Workflow runs on push
- [ ] All build steps succeed
- [ ] Packet.dll artifact is uploaded
- [ ] No RirePE.exe artifact (removed)
- [ ] No errors or warnings

## Documentation Review

### Completeness
- [ ] README.md is up to date
- [ ] ARCHITECTURE.md reflects headless design
- [ ] MIGRATION_GUIDE.md covers all scenarios
- [ ] CHANGELOG.md documents all changes
- [ ] RirePE.ini.example has correct settings
- [ ] Ini-README.md is updated

### Accuracy
- [ ] No references to RirePE.exe remain
- [ ] No references to named pipes remain
- [ ] No references to USE_TCP flag remain
- [ ] All code examples work
- [ ] All configuration examples are valid

### Clarity
- [ ] Migration guide is easy to follow
- [ ] Setup instructions are clear
- [ ] Troubleshooting section is helpful
- [ ] Examples are well-documented

## Final Verification

### File Presence
- [ ] Packet.dll exists in build output
- [ ] RirePE.exe does NOT exist in build output
- [ ] RirePE.ini.example exists
- [ ] packet_monitor.py exists
- [ ] tcp_inject_example.py exists

### File Absence
- [ ] RirePE/ directory does NOT exist
- [ ] No .exe artifacts from GUI build
- [ ] No pipe-related DLLs

### Git History
- [ ] All commits have clear messages
- [ ] Commit messages follow format: "Phase X: Description"
- [ ] No large binary files in history
- [ ] No sensitive data committed

### Version Tag
- [ ] v2.0.0 tag exists
- [ ] Tag message describes changes
- [ ] Tag points to correct commit

## Known Issues to Document

List any known issues or limitations:
- [ ] (None identified yet)

## Rollback Readiness

Verify rollback capability:
- [ ] Can identify commit before Phase 1
- [ ] Can create legacy branch if needed
- [ ] Have backup of working GUI version

## User Acceptance Criteria

From a user perspective:
- [ ] Can inject Packet.dll easily
- [ ] Can monitor packets with Python script
- [ ] Can send packets with Python script
- [ ] Can understand configuration options
- [ ] Can troubleshoot common issues
- [ ] Migration guide answers common questions

## Post-Release Monitoring

After release, monitor:
- [ ] GitHub issues for bug reports
- [ ] User feedback on new architecture
- [ ] Performance reports
- [ ] Feature requests

## Sign-off

### Developer Sign-off
- [ ] All critical tests pass
- [ ] No known blocking issues
- [ ] Documentation is complete
- [ ] Code is clean and maintainable

### Testing Notes
```
Date: _____________
Tester: _____________
Platform: Windows __ (version)
Game Version: _____________

Notes:
_________________________________________________
_________________________________________________
_________________________________________________
```

---

## Test Results Summary

### Passed: __ / __
### Failed: __ / __
### Blocked: __ / __
### Not Tested: __ / __

### Critical Issues Found:
1. _________________________________________________
2. _________________________________________________

### Non-Critical Issues Found:
1. _________________________________________________
2. _________________________________________________

### Recommendations:
- [ ] Ready for release
- [ ] Needs fixes before release
- [ ] Needs more testing

---

**Note**: This checklist should be completed before creating the v2.0.0 release tag. Any critical failures should be addressed before release.
