# Changelog

All notable changes to the RirePE project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2025-10-10

### Breaking Changes

**Major architectural refactoring**: RirePE has been converted from a GUI-based application to a headless packet logger with TCP-only communication.

- **Removed RirePE.exe GUI application** - The Windows GUI has been completely removed
- **Removed named pipe communication** - All pipe client/server code has been removed
- **TCP monitoring is now mandatory** - TCP is no longer optional; it's the only communication method
- **Configuration changes** - `USE_TCP` flag removed; TCP settings are now always active

### Added

- **Headless packet logging architecture** - Packet.dll now operates independently without launching a GUI
- **Packet/PacketDefs.h** - New header file containing packet definitions (extracted from RirePE.h)
- **TESTING_CHECKLIST.md** - Comprehensive testing checklist for Phase 9
- **MIGRATION_GUIDE.md** - Detailed guide for users migrating from GUI to headless mode
- **CHANGELOG.md** - This changelog file
- Python monitoring scripts are now the primary monitoring tools:
  - `packet_monitor.py` - Real-time packet viewer
  - `tcp_inject_example.py` - Packet injection demonstration
- Support for multiple simultaneous monitoring clients
- Remote monitoring capability (monitor from any machine)
- Cross-platform monitoring (monitor from Linux, macOS, Windows)

### Changed

- **Packet.dll initialization** - No longer launches RirePE.exe, starts TCP server immediately
- **TCP server behavior** - Always starts on DLL injection (previously optional)
- **Configuration system** - Simplified to focus on TCP settings:
  - Added `TCP_PORT` configuration (default: 8275)
  - Added `TCP_HOST` configuration (default: localhost)
  - Removed `USE_TCP` flag
- **Documentation completely rewritten** for headless architecture:
  - Updated README.md with new workflow
  - Updated ARCHITECTURE.md with new component diagram
  - Updated RirePE.ini.example with TCP-only settings
  - Updated Ini-README.md to remove pipe references
- **Function names updated** for clarity:
  - `RunRirePE()` → `RunPacketLogger()`
  - `PipeStartup()` → `PacketLoggerStartup()`
- **Build system simplified**:
  - RirePE.sln now only contains Packet project
  - Removed gui solution folder

### Removed

- **RirePE.exe** - Windows GUI executable (entire RirePE/ directory deleted)
- **RirePE/** source directory and all GUI-related files:
  - Config.cpp/h
  - ControlList.h
  - FilterGUI.cpp/h
  - FormatGUI.cpp/h
  - MainGUI.cpp/h
  - PacketLogger.cpp/h
  - PacketScript.cpp/h
  - PacketSender.cpp/h
  - RirePE.h (replaced by PacketDefs.h in Packet/)
  - RirePE.vcxproj
  - RirePE.vcxproj.filters
  - WinMain.cpp
- **Named pipe communication** - All pipe client/server code removed:
  - Removed `StartPipeClient()` function
  - Removed `RestartPipeClient()` function
  - Removed `GetPipeNameLogger()` function
  - Removed `GetPipeNameSender()` function
  - Removed pipe-related global variables
  - Removed pipe reconnection logic from PacketQueue
- **Configuration options**:
  - Removed `USE_TCP` flag
  - Removed `PE_LOGGER_PIPE_NAME` constant
  - Removed `PE_SENDER_PIPE_NAME` constant
- **GUI build artifacts** from CI/CD:
  - Removed RirePE.exe build steps from GitHub Actions
  - Removed RirePE.exe artifact upload

### Fixed

- Eliminated race conditions between pipe and TCP communication
- Removed unnecessary complexity from initialization flow
- Simplified error handling (no pipe fallback logic)

### Performance

- **Reduced overhead** - No GUI thread consuming resources
- **Reduced memory footprint** - No Win32 GUI objects in memory
- **Faster startup** - No GUI window creation or pipe connection delays
- **More reliable** - Direct TCP communication without pipe intermediary

### Security

- TCP server binds to localhost by default (explicit configuration required for remote access)
- No pipe security concerns (pipes completely removed)
- Simplified attack surface (one less executable)

### Migration

See [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) for detailed migration instructions from v1.x to v2.0.0.

**Quick migration summary:**
1. Remove RirePE.exe from deployment folder
2. Update RirePE.ini to include `TCP_PORT` and `TCP_HOST` settings
3. Replace GUI monitoring with `python packet_monitor.py localhost 8275`
4. Update packet injection to use `tcp_inject_example.py`

### Development

- **Simpler codebase** - Removed ~3000 lines of GUI code
- **Easier testing** - No GUI automation required
- **Better CI/CD** - Simplified build pipeline
- **Cleaner architecture** - Single-purpose DLL with clear interface

---

## [1.x] - Previous Versions

### Architecture (v1.x)

- **RirePE.exe** - Windows GUI application for packet monitoring
- **Packet.dll** - Injected DLL that hooked game functions
- **Named pipe communication** - Primary communication method between DLL and GUI
- **Optional TCP support** - Could enable TCP monitoring via USE_TCP flag
- **Integrated GUI** - ListView for packet display, format viewer, blocking checkbox

### Components (v1.x)

- RirePE.exe with MainGUI, FormatGUI, FilterGUI windows
- Named pipe server in RirePE.exe
- Named pipe client in Packet.dll
- Optional TCP server functionality
- Win32 GUI controls and message handling

---

## Version History Summary

| Version | Release Date | Major Changes |
|---------|--------------|---------------|
| 2.0.0   | 2025-10-10   | Headless architecture, TCP-only, removed GUI |
| 1.x     | (Previous)   | GUI-based, named pipe communication |

---

## Upgrade Path

### From 1.x to 2.0.0

**What you need to do:**
1. Read [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)
2. Update configuration file (RirePE.ini)
3. Learn Python monitoring scripts
4. Remove RirePE.exe from deployment

**What you gain:**
- Remote monitoring from any OS
- Multiple simultaneous viewers
- Scriptable packet analysis
- Lower overhead and simpler deployment

**What you lose:**
- Visual Windows GUI
- Built-in format viewer window
- Packet blocking checkbox UI

**Alternatives for lost features:**
- Use `packet_monitor.py` for viewing packets
- Write custom Python scripts for format parsing
- Use `ENABLE_BLOCKING=1` config + blocking script for packet blocking

---

## Future Roadmap

### Planned Features (Post-2.0.0)

- **Enhanced packet_monitor.py** - Colors, filters, search functionality
- **Packet replay tool** - Replay captured packet sequences
- **Packet diff tool** - Compare packet captures
- **Web dashboard** - Optional browser-based GUI
- **PCAP export** - Export to Wireshark format
- **Opcode database** - Symbolic opcode names in logs
- **Packet recording** - Save/load packet captures

### Under Consideration

- Machine learning packet analysis
- Automated exploit detection
- Network traffic visualization
- Packet fuzzing capabilities

---

## Support

For issues, questions, or feedback:
- GitHub Issues: [Report a bug or request a feature]
- Documentation: README.md, ARCHITECTURE.md, MIGRATION_GUIDE.md
- Examples: packet_monitor.py, tcp_inject_example.py

---

## Acknowledgments

Thanks to all users who provided feedback during the refactoring process.

---

**Note**: This changelog is maintained going forward. All future changes will be documented here.
