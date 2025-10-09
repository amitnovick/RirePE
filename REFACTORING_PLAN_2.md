# Refactoring Plan: Remove RirePE.exe GUI Project

## Executive Summary

This plan outlines the steps to remove the RirePE.exe GUI project and convert Packet.dll into a **headless logging library** that communicates exclusively via TCP. Users will monitor packets using remote TCP clients (Python scripts, web dashboards, etc.) instead of a local Windows GUI.

## Motivation

### Why Remove the GUI?

1. **Simplified Architecture**: Eliminates Windows GUI dependencies, named pipes, and complex IPC
2. **Cross-Platform Monitoring**: TCP allows monitoring from any OS (Linux, macOS, Windows)
3. **Modern Workflow**: Python/web-based monitoring is more flexible than Win32 GUI
4. **Reduced Maintenance**: One less executable to build, test, and maintain
5. **Already Working**: TCP infrastructure is mature and battle-tested

### What Users Lose

- Local Windows GUI with packet list and format viewer
- Visual packet blocking checkbox
- RirePE.exe executable

### What Users Gain

- Monitor from anywhere (remote machines, WSL, Docker containers)
- Script-based packet analysis (Python, Node.js, etc.)
- Web dashboard possibilities
- Simpler deployment (just inject Packet.dll)
- Lower overhead (no GUI thread)

## Current Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       MapleStory.exe                            │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                     Packet.dll                            │ │
│  │                                                           │ │
│  │  - Hooks game functions                                   │ │
│  │  - Queues packets                                         │ │
│  │  - Launches RirePE.exe via ShellExecuteW()               │ │
│  │  - Connects to RirePE.exe via named pipe                 │ │
│  │  - (Optional) Starts TCP server for remote monitoring    │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                          │
                          │ Named Pipe (primary)
                          │ TCP (optional)
                          ↓
┌─────────────────────────────────────────────────────────────────┐
│                      RirePE.exe (GUI)                           │
│                                                                 │
│  - Receives packets via pipe server                            │
│  - Displays in ListView (MainGUI)                              │
│  - Format viewer (FormatGUI)                                   │
│  - Packet blocking UI                                          │
│  - Packet sending UI                                           │
└─────────────────────────────────────────────────────────────────┘
```

## Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       MapleStory.exe                            │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                     Packet.dll                            │ │
│  │                                                           │ │
│  │  - Hooks game functions                                   │ │
│  │  - Queues packets                                         │ │
│  │  - Starts TCP server (mandatory)                          │ │
│  │  - Sends packets to all connected TCP clients            │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                          │
                          │ TCP (port 8275, configurable)
                          ↓
        ┌─────────────────┴────────────────────┐
        │                 │                    │
        ↓                 ↓                    ↓
┌─────────────┐  ┌─────────────┐  ┌─────────────────┐
│  Python     │  │  Web        │  │  Custom         │
│  Script     │  │  Dashboard  │  │  Tools          │
└─────────────┘  └─────────────┘  └─────────────────┘

Examples:
- packet_monitor.py (already exists)
- tcp_inject_example.py (already exists)
- Custom web UI (future)
- Packet analysis scripts (future)
```

## Benefits of New Architecture

### Performance
- No GUI overhead (no window messages, no redraws)
- No pipe connection delays
- Smaller memory footprint

### Flexibility
- Monitor from WSL/Linux VM without Windows RDP
- Multiple clients can connect simultaneously
- Easy to integrate with existing analysis tools
- Network monitoring (tcpdump-style workflows)

### Development
- Simpler codebase (no Win32 GUI code)
- Easier testing (no GUI automation needed)
- Better CI/CD (no GUI artifacts)

## Refactoring Strategy

**Key Principle**: Each phase must be independently testable. After completing any phase, the DLL must build and inject successfully.

### Phase 1: Analysis and Documentation

**Goal**: Document all code that will be removed or modified.

#### Step 1.1: Identify RirePE.exe launch code
```
Packet/DllMain.cpp:156-199 (RunRirePE function)
  - Line 166: ShellExecuteW() call
  - Lines 169-183: Pipe connection retry loop
  - Lines 186-192: TCP server startup
```

#### Step 1.2: Identify pipe-related code
```
RirePE/RirePE.h:
  - PE_LOGGER_PIPE_NAME
  - PE_SENDER_PIPE_NAME

Packet/DllMain.cpp:
  - GetPipeNameLogger() (142-147)
  - GetPipeNameSender() (149-154)
  - StartPipeClient() calls (178)

Packet/PacketQueue.cpp:
  - RestartPipeClient() call (258)

Packet/PacketLogging.cpp:
  - Pipe client functions
```

#### Step 1.3: Identify RirePE project references
```
RirePE.sln:
  - Lines 6-7: RirePE project entry
  - Lines 33: Nested in gui folder
  - Lines 20-23: Build configurations

.github/workflows/build-windows.yml:
  - Lines 43-59: Commented-out RirePE build steps
```

**Verification**: Create REFACTORING_PLAN_2.md (this document)

**Commit**: `git commit -m "Phase 1: Document RirePE.exe removal plan"`

---

### Phase 2: Create PacketDefs.h and Refactor Initialization

**Goal**: Extract definitions from RirePE.h into Packet.dll, then remove GUI launch and pipe connection code.

#### Step 2.1: Create Packet/PacketDefs.h
Create new header file with necessary definitions extracted from RirePE/RirePE.h:

File: `Packet/PacketDefs.h` (NEW FILE)
```cpp
#pragma once
#include <Windows.h>

// Configuration
#define DLL_NAME L"Packet"
#define INI_FILE_NAME L"RirePE.ini"

// TCP Configuration (replaces pipe names)
#define DEFAULT_TCP_PORT 8275

// Removed pipe names - no longer using named pipes:
// #define PE_LOGGER_PIPE_NAME L"PacketLogger"
// #define PE_SENDER_PIPE_NAME L"PacketSender"

#pragma pack(push, 1)

// Message header types
enum MessageHeader {
	SENDPACKET,        // stop encoding
	RECVPACKET,        // start decoding
	// encode
	ENCODE_BEGIN,
	ENCODEHEADER,
	ENCODE1,
	ENCODE2,
	ENCODE4,
	ENCODE8,
	ENCODESTR,
	ENCODEBUFFER,
	TV_ENCODEHEADER,
	TV_ENCODESTRW1,
	TV_ENCODESTRW2,
	TV_ENCODEFLOAT,
	ENCODE_END,
	// decode
	DECODE_BEGIN,
	DECODEHEADER,
	DECODE1,
	DECODE2,
	DECODE4,
	DECODE8,
	DECODESTR,
	DECODEBUFFER,
	TV_DECODEHEADER,
	TV_DECODESTRW1,
	TV_DECODESTRW2,
	TV_DECODEFLOAT,
	DECODE_END,        // not a tag
	// unknown
	UNKNOWNDATA,       // not decoded by function
	NOTUSED,           // recv not used
	WHEREFROM,         // not encoded by function
	UNKNOWN,
};

enum FormatUpdate {
	FORMAT_NO_UPDATE,
	FORMAT_UPDATE,
};

// Packet editor message structure
typedef struct {
	MessageHeader header;
	DWORD id;
#ifdef _WIN64
	ULONG_PTR addr;
#else
	ULONGLONG addr;
#endif
	union {
		// SEND or RECV
		struct {
			DWORD length;     // packet size
			BYTE packet[1];   // packet data
		} Binary;
		// Encode or Decode
		struct {
			DWORD pos;        // encoded/decoded position
			DWORD size;       // size
			FormatUpdate update;
			BYTE data[1];     // packet buffer (may change before read)
		} Extra;
		// Encode or Decode completion
		DWORD status;         // status
	};
} PacketEditorMessage;

#pragma pack(pop)
```

**Why this step first?**
- Packet.dll currently depends on `#include "../RirePE/RirePE.h"`
- We cannot remove RirePE directory until Packet.dll has its own definitions
- Creating PacketDefs.h first ensures Packet.dll remains buildable throughout refactoring

#### Step 2.2: Update Packet.dll includes
Replace RirePE.h includes with PacketDefs.h:

File: `Packet/DllMain.cpp`
```cpp
// Before:
#include"../Share/Simple/Simple.h"
#include"../Share/Simple/DebugLog.h"
#include"../Packet/PacketHook.h"
#include"../Packet/PacketLogging.h"
#include"../Packet/PacketQueue.h"
#include"../RirePE/RirePE.h"  // ← Remove this

// After:
#include"../Share/Simple/Simple.h"
#include"../Share/Simple/DebugLog.h"
#include"../Packet/PacketHook.h"
#include"../Packet/PacketLogging.h"
#include"../Packet/PacketQueue.h"
#include"PacketDefs.h"  // ← Add this
```

Search for all files that include RirePE.h:
```bash
grep -rn '#include.*RirePE\.h' Packet/
```

Update each file to include `PacketDefs.h` instead.

Likely files:
- `Packet/DllMain.cpp`
- `Packet/PacketLogging.cpp`
- `Packet/PacketQueue.cpp`
- Any other Packet/*.cpp files

#### Step 2.3: Add PacketDefs.h to Packet.vcxproj
File: `Packet/Packet.vcxproj`

Add to `<ItemGroup>` with ClInclude entries:
```xml
<ClInclude Include="PacketDefs.h" />
```

#### Step 2.4: Test build (critical checkpoint)
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

**Verification**:
- Packet.dll builds successfully
- No longer depends on RirePE/RirePE.h
- All PacketEditorMessage usages work correctly

**Important**: This step MUST pass before proceeding. If build fails, Packet.dll is missing definitions that need to be added to PacketDefs.h.

**Commit**: `git commit -m "Phase 2.1-2.4: Extract RirePE.h definitions into Packet/PacketDefs.h"`

---

#### Step 2.5: Refactor RunRirePE() function
File: `Packet/DllMain.cpp:156-199`

```cpp
// Before (simplified):
bool RunRirePE(HookSettings &hs) {
    // Launch RirePE.exe
    ShellExecuteW(NULL, NULL, (wDir + L"\\RirePE.exe").c_str(), ...);

    // Connect to pipe
    for (int i = 0; i < 10; i++) {
        pipe_connected = StartPipeClient();
    }

    // Optionally start TCP
    if (g_UseTCP) {
        StartTCPClient();
    }

    RunPacketSender();
    return true;
}

// After:
bool RunPacketLogger(HookSettings &hs) {
    DEBUGLOG(L"[INIT] Starting headless packet logger (TCP-only mode)");

    // Always start TCP server (no longer optional)
    DEBUGLOG(L"[INIT] Starting TCP server...");
    if (!StartTCPClient()) {  // Function name kept for compatibility
        DEBUGLOG(L"[INIT] WARNING: TCP server failed to start");
        // Continue anyway - packets will queue until client connects
    } else {
        extern int g_TCPPort;
        DEBUGLOG(L"[INIT] TCP server listening on port " + std::to_wstring(g_TCPPort));
    }

    DEBUGLOG(L"[INIT] Starting packet sender...");
    RunPacketSender();

    DEBUGLOG(L"[INIT] Headless logger initialized successfully");
    DEBUGLOG(L"[INIT] Connect using: python packet_monitor.py");
    return true;
}
```

#### Step 2.6: Update PipeStartup() function
File: `Packet/DllMain.cpp:201-214`

```cpp
// Before:
bool PipeStartup(HookSettings &hs) {
    target_pid = GetCurrentProcessId();
    if (!InitializePacketQueue()) {
        return false;
    }
    HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)RunRirePE, &hs, NULL, NULL);
    // ...
}

// After:
bool PacketLoggerStartup(HookSettings &hs) {
    target_pid = GetCurrentProcessId();
    if (!InitializePacketQueue()) {
        return false;
    }
    HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)RunPacketLogger, &hs, NULL, NULL);
    if (hThread) {
        CloseHandle(hThread);
    }
    return true;
}
```

#### Step 2.7: Update PacketHook() function
File: `Packet/DllMain.cpp:216-235`

```cpp
// Before:
bool PacketHook(HookSettings &hs) {
    PipeStartup(hs);
    // ...
}

// After:
bool PacketHook(HookSettings &hs) {
    PacketLoggerStartup(hs);
    // rest unchanged
    // ...
}
```

#### Step 2.8: Remove pipe helper functions
File: `Packet/DllMain.cpp:142-154`

```cpp
// Remove or comment out:
std::wstring GetPipeNameLogger() {
    if (target_pid) {
        return PE_LOGGER_PIPE_NAME + std::to_wstring(target_pid);
    }
    return PE_LOGGER_PIPE_NAME;
}

std::wstring GetPipeNameSender() {
    if (target_pid) {
        return PE_SENDER_PIPE_NAME + std::to_wstring(target_pid);
    }
    return PE_SENDER_PIPE_NAME;
}
```

#### Step 2.9: Update PacketQueue.cpp
File: `Packet/PacketQueue.cpp:250-260`

```cpp
// Before:
if (failure_count == 50) {
    extern bool g_UseTCP;
    // Always try to restart pipe first since that's for RirePE.exe
    RestartPipeClient();
    failure_count = 0;
}

// After:
if (failure_count == 50) {
    // No pipe to restart - TCP clients can reconnect on their own
    // Just log the failure
    DEBUGLOG(L"[QUEUE] WARNING: 50 consecutive send failures - check TCP connection");
    failure_count = 0;
}
```

#### Step 2.10: Test build
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

**Verification**:
- Packet.dll builds without errors
- No references to RirePE.exe launch
- No pipe connection attempts
- TCP server starts automatically

**Commit**: `git commit -m "Phase 2.5-2.10: Convert Packet.dll to headless TCP-only mode"`

---

**Phase 2 Summary - Build Independence Achieved:**

After Phase 2 completes:
- ✅ Packet.dll no longer depends on RirePE/RirePE.h (uses PacketDefs.h instead)
- ✅ Packet.dll builds successfully
- ✅ RirePE.exe still builds (still uses RirePE/RirePE.h)
- ✅ Both projects are independently buildable
- ✅ Ready to delete RirePE directory in Phase 6

**Why this matters**: Phases 3-5 can safely modify configuration and solution files without breaking builds, because Packet.dll is already self-contained.

---

### Phase 3: Update Configuration System

**Goal**: Remove USE_TCP flag (TCP is now mandatory) and document new behavior.

#### Step 3.1: Update RirePE.ini.example
File: `RirePE.ini.example`

```ini
# Before:
[Packet]
# ...
# USE_TCP=0

# After:
[Packet]
# ============================================================================
# NETWORK SETTINGS
# ============================================================================

# TCP_PORT specifies the port for remote monitoring
# Default: 8275
# Connect using: python packet_monitor.py localhost 8275
TCP_PORT=8275

# TCP_HOST specifies the bind address (usually localhost)
# Default: localhost (127.0.0.1)
# Use 0.0.0.0 to allow connections from other machines (SECURITY RISK!)
TCP_HOST=localhost

# Note: Packet.dll now operates in TCP-only mode
# The RirePE.exe GUI has been removed
# Use Python scripts for packet monitoring:
#   - packet_monitor.py: View packet logs
#   - tcp_inject_example.py: Send/receive packets programmatically
```

#### Step 3.2: Update LoadPacketConfig() function
File: `Packet/DllMain.cpp:28-106`

```cpp
// Before:
bool LoadPacketConfig(HINSTANCE hinstDLL) {
    // ...
    extern bool g_UseTCP;
    std::wstring wUseTCP;
    if (conf.Read(DLL_NAME, L"USE_TCP", wUseTCP) && _wtoi(wUseTCP.c_str())) {
        g_UseTCP = true;
        // Read TCP host/port
    }
    // ...
}

// After:
bool LoadPacketConfig(HINSTANCE hinstDLL) {
    HookSettings &hs = gHookSettings;
    hs.hinstDLL = hinstDLL;
    Config conf(INI_FILE_NAME, hs.hinstDLL);

    // debug mode
    std::wstring wDebugMode;
    if (conf.Read(DLL_NAME, L"DEBUG_MODE", wDebugMode) && _wtoi(wDebugMode.c_str())) {
        hs.debug_mode = true;
    }

    // hook from thread
    std::wstring wUseThread;
    if (conf.Read(DLL_NAME, L"USE_THREAD", wUseThread) && _wtoi(wUseThread.c_str())) {
        hs.use_thread = true;
    }

    // enable packet blocking (default: false for performance)
    std::wstring wEnableBlocking;
    if (conf.Read(DLL_NAME, L"ENABLE_BLOCKING", wEnableBlocking) && _wtoi(wEnableBlocking.c_str())) {
        hs.enable_blocking = true;
        g_EnableBlocking = true;
    }

    // TCP configuration (now mandatory)
    extern std::string g_TCPHost;
    extern int g_TCPPort;

    // Read TCP host (default: localhost)
    std::wstring wTCPHost;
    if (conf.Read(DLL_NAME, L"TCP_HOST", wTCPHost)) {
        g_TCPHost = std::string(wTCPHost.begin(), wTCPHost.end());
    } else {
        g_TCPHost = "127.0.0.1";  // Default
    }

    // Read TCP port (default: 8275)
    std::wstring wTCPPort;
    if (conf.Read(DLL_NAME, L"TCP_PORT", wTCPPort)) {
        g_TCPPort = _wtoi(wTCPPort.c_str());
    } else {
        g_TCPPort = 8275;  // Default
    }

    // high version mode (CInPacket), TODO
    std::wstring wHighVersionMode;
    if (conf.Read(DLL_NAME, L"HIGH_VERSION_MODE", wHighVersionMode) && _wtoi(wHighVersionMode.c_str())) {
        hs.high_version_mode = true;
    }

    // hook without using aob scan
    std::wstring wUseAddr;
    if (conf.Read(DLL_NAME, L"USE_ADDR", wUseAddr) && _wtoi(wUseAddr.c_str())) {
        hs.use_addr = true;
        // ... (rest of USE_ADDR logic unchanged)
    }

    return true;
}
```

#### Step 3.3: Update DllMain() logging
File: `Packet/DllMain.cpp:248-266`

```cpp
// Before:
DEBUGLOG(L"[INIT] Config loaded - USE_TCP=" + std::to_wstring(g_UseTCP) + L", Port=" + std::to_wstring(g_TCPPort));

// After:
extern std::string g_TCPHost;
std::wstring wTCPHost(g_TCPHost.begin(), g_TCPHost.end());
DEBUGLOG(L"[INIT] Config loaded - TCP Host=" + wTCPHost + L", Port=" + std::to_wstring(g_TCPPort));
```

#### Step 3.4: Remove g_UseTCP global variable
File: `Packet/PacketLogging.cpp` (or wherever declared)

```cpp
// Before:
bool g_UseTCP = false;

// After:
// Removed - TCP is now always enabled
```

Search for all references to `g_UseTCP` and remove or update them:
```bash
grep -rn "g_UseTCP" Packet/
```

#### Step 3.5: Test build
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

**Verification**:
- No USE_TCP conditional logic
- TCP server always starts
- Configuration file updated

**Commit**: `git commit -m "Phase 3: Make TCP mandatory, remove USE_TCP flag"`

---

### Phase 4: Remove Pipe Client Code

**Goal**: Delete all named pipe client implementation from Packet.dll.

#### Step 4.1: Identify pipe client functions
Search for pipe client code:
```bash
grep -rn "StartPipeClient\|RestartPipeClient\|StopPipeClient\|PIPE" Packet/
```

Likely in `PacketLogging.cpp` or similar file.

#### Step 4.2: Remove or stub out pipe functions
```cpp
// Before:
bool StartPipeClient() {
    // ... pipe connection code
}

bool RestartPipeClient() {
    // ... pipe reconnection code
}

// After:
// Removed - using TCP only
```

#### Step 4.3: Update SendPacketData() to use TCP only
If `SendPacketData()` has pipe fallback logic:

```cpp
// Before:
bool SendPacketData(BYTE* data, size_t size) {
    if (pipe_connected) {
        return SendViaPipe(data, size);
    } else if (tcp_connected) {
        return SendViaTCP(data, size);
    }
    return false;
}

// After:
bool SendPacketData(BYTE* data, size_t size) {
    // TCP-only implementation
    return SendViaTCP(data, size);
}
```

#### Step 4.4: Update RecvPacketData() similarly
```cpp
// Before:
bool RecvPacketData(std::vector<BYTE>& vData) {
    if (pipe_connected) {
        return RecvViaPipe(vData);
    } else if (tcp_connected) {
        return RecvViaTCP(vData);
    }
    return false;
}

// After:
bool RecvPacketData(std::vector<BYTE>& vData) {
    // TCP-only implementation
    return RecvViaTCP(vData);
}
```

#### Step 4.5: Remove pipe client global variables
```cpp
// Remove variables like:
PipeClient* g_PipeClient = NULL;
bool g_PipeConnected = false;
// etc.
```

#### Step 4.6: Clean up includes
Remove pipe-related includes:
```cpp
// Remove:
#include "../Share/Simple/SimplePipe.h"  // If only used for client
```

#### Step 4.7: Test build
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

**Verification**:
- No pipe client code remains
- DLL uses TCP exclusively
- No compilation errors

**Commit**: `git commit -m "Phase 4: Remove named pipe client code"`

---

### Phase 5: Remove RirePE Project from Solution

**Goal**: Remove RirePE.exe project references from solution file.

#### Step 5.1: Update RirePE.sln
File: `RirePE.sln`

Remove RirePE project entry:
```diff
-Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "RirePE", "RirePE\RirePE.vcxproj", "{EC90ABE6-8A35-4275-ABA0-2D810554A506}"
-EndProject
```

Remove from GlobalSection(ProjectConfigurationPlatforms):
```diff
-{EC90ABE6-8A35-4275-ABA0-2D810554A506}.Debug|x86.ActiveCfg = Debug|Win32
-{EC90ABE6-8A35-4275-ABA0-2D810554A506}.Debug|x86.Build.0 = Debug|Win32
-{EC90ABE6-8A35-4275-ABA0-2D810554A506}.Release|x86.ActiveCfg = Release|Win32
-{EC90ABE6-8A35-4275-ABA0-2D810554A506}.Release|x86.Build.0 = Release|Win32
```

Remove from GlobalSection(NestedProjects):
```diff
-{EC90ABE6-8A35-4275-ABA0-2D810554A506} = {AA5C4694-EB93-4592-89E2-E9C70C4B609B}
```

Remove the gui solution folder if empty:
```diff
-Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "gui", "gui", "{AA5C4694-EB93-4592-89E2-E9C70C4B609B}"
-EndProject
```

#### Step 5.2: Test solution load
```bash
# Open solution in Visual Studio or build via msbuild
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```

**Verification**:
- Solution loads without errors
- Only Packet project remains
- Packet.dll builds successfully

**Commit**: `git commit -m "Phase 5: Remove RirePE project from solution"`

---

### Phase 6: Delete RirePE Project Directory

**Goal**: Physically remove RirePE source files.

#### Step 6.1: Backup before deletion
```bash
# Create backup branch if needed
git checkout -b backup-before-rirepe-removal
git checkout main
```

#### Step 6.2: Remove RirePE directory
```bash
rm -rf RirePE/
```

Files removed:
- Config.cpp/h
- ControlList.h
- FilterGUI.cpp/h
- FormatGUI.cpp/h
- MainGUI.cpp/h
- PacketLogger.cpp/h
- PacketScript.cpp/h
- PacketSender.cpp/h
- RirePE.h
- RirePE.vcxproj
- RirePE.vcxproj.filters
- WinMain.cpp

#### Step 6.3: Verify no RirePE.h references remain

The include cleanup was already done in Phase 2 (Step 2.2). Verify:

```bash
# Should return no results
grep -rn '#include.*RirePE\.h' Packet/
```

If any references remain, they indicate files that were missed in Phase 2. Update them to use `PacketDefs.h` instead.

**Note**: PacketDefs.h was created in Phase 2.1, so Packet.dll is already independent of RirePE/RirePE.h.

#### Step 6.4: Test build
```bash
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```

**Verification**:
- Packet.dll builds successfully
- No missing header errors
- All necessary constants defined

**Commit**: `git commit -m "Phase 6: Delete RirePE project directory"`

---

### Phase 7: Update CI/CD Pipeline

**Goal**: Remove RirePE.exe build steps from GitHub Actions.

#### Step 7.1: Update build-windows.yml
File: `.github/workflows/build-windows.yml`

Remove commented-out RirePE build steps:
```diff
-#    - name: Build RirePE (32-bit) and Packet.dll (32-bit)
-#      run: msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=${{ env.SDK_VERSION }}
-
-#    - name: Upload RirePE.exe artifact
-#      uses: actions/upload-artifact@v4
-#      with:
-#        name: RirePE-32bit
-#        path: |
-#          **/Release/RirePE.exe
-#          **/Win32/Release/RirePE.exe
-#        if-no-files-found: error
```

Update workflow name and description:
```diff
-name: Build Windows 32-bit Binaries
+name: Build Packet.dll (32-bit)
```

Update list outputs step (optional):
```diff
     - name: List build outputs
       shell: powershell
       run: |
         Write-Host "Build outputs:"
-        Get-ChildItem -Recurse -Include *.exe,*.dll | Where-Object { $_.Directory -like "*Release*" }
+        Get-ChildItem -Recurse -Include *.dll | Where-Object { $_.Directory -like "*Release*" }
```

#### Step 7.2: Test CI locally (optional)
```bash
# Install act (GitHub Actions local runner)
# https://github.com/nektos/act
act push
```

#### Step 7.3: Commit and test on GitHub
```bash
git add .github/workflows/build-windows.yml
git commit -m "Phase 7: Update CI/CD to remove RirePE.exe build"
git push
```

**Verification**:
- GitHub Actions workflow runs successfully
- Only Packet.dll artifact is uploaded
- No errors about missing RirePE.exe

---

### Phase 8: Update Documentation

**Goal**: Rewrite documentation to reflect headless TCP-only architecture.

#### Step 8.1: Update README.md
File: `Readme.md`

```markdown
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

## Note

Around BB updates, the client started protecting the SendPacket function. You may need to bypass anti-debugging checks to hook the function successfully.
```

#### Step 8.2: Update ARCHITECTURE.md
File: `ARCHITECTURE.md`

Update the component architecture diagram (lines 184-224):

```diff
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
-│  │                      │                                    │ │
+│  │                      │  (TCP Server)                      │ │
 │  └──────────────────────┼────────────────────────────────────┘ │
 │                         │                                      │
 └─────────────────────────┼──────────────────────────────────────┘
-                          │ Named Pipe
-                          │ (\\.\pipe\PacketLogger)
+                          │ TCP Socket
+                          │ (localhost:8275)
 ┌─────────────────────────┼──────────────────────────────────────┐
-│                         │                                      │
-│  ┌──────────────────────▼────────────────────────────────────┐ │
-│  │                   PipeServer                             │ │
-│  │                (SimplePipe.h)                            │ │
-│  └──────────────────────┬────────────────────────────────────┘ │
-│                         │                                      │
-│  ┌──────────────────────▼────────────────────────────────────┐ │
-│  │               PacketLogger.cpp                           │ │
-│  │         (Receive, log, send response)                    │ │
-│  └──────────────────────┬────────────────────────────────────┘ │
-│                         │                                      │
-│  ┌──────────────────────▼────────────────────────────────────┐ │
-│  │                   MainGUI                                │ │
-│  │         (Display packets, block checkbox)                │ │
-│  └──────────────────────────────────────────────────────────┘ │
-│                                                                │
-│                      RirePE.exe                                │
+│              Python/JavaScript/Custom Clients                 │
+│                                                                │
+│  ┌──────────────────┐  ┌──────────────┐  ┌─────────────────┐ │
+│  │ packet_monitor.py│  │ tcp_inject   │  │ Web Dashboard   │ │
+│  │ (View packets)   │  │ (Inject)     │  │ (Future)        │ │
+│  └──────────────────┘  └──────────────┘  └─────────────────┘ │
 └────────────────────────────────────────────────────────────────┘
 ```
```

Add section explaining the new architecture:

```markdown
## Headless Architecture

### Why No GUI?

RirePE now operates as a **headless packet logger**. The Windows GUI (RirePE.exe) has been removed in favor of TCP-based monitoring tools.

**Advantages:**
- **Cross-platform monitoring**: View packets from Linux, macOS, WSL, Docker containers
- **Remote monitoring**: Monitor from different machines over network
- **Scriptable**: Automate packet analysis with Python/Node.js
- **Multiple clients**: Several tools can connect simultaneously
- **Lower overhead**: No GUI thread consuming resources
- **Simpler deployment**: Just inject one DLL

**Trade-offs:**
- No visual packet list (use terminal-based tools)
- No built-in format viewer (scripts provide similar functionality)
- Requires basic command-line knowledge

### Monitoring Tools

#### packet_monitor.py
Real-time packet viewer with:
- Opcode display (with symbolic names)
- Packet size
- Hex dump of packet data
- Timestamp logging

#### tcp_inject_example.py
Demonstrates programmatic packet injection:
- Send crafted packets to game
- Receive and parse packets
- Filter by opcode
- Custom packet analysis

#### Custom Tools
Use the TCP API (see `TCP_API_DOCUMENTATION.md`) to build:
- Web dashboards
- Packet replay tools
- ML-based packet analysis
- Network traffic recorders
```

#### Step 8.3: Create MIGRATION_GUIDE.md
File: `MIGRATION_GUIDE.md`

```markdown
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

### Task: Filter Packets by Opcode

**Old:**
- Use filter textbox in RirePE.exe GUI

**New:**
```python
# custom_monitor.py
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8275))

FILTER_OPCODE = 0x0015

while True:
    data = sock.recv(4096)
    header, opcode, addr, size = struct.unpack('<IIII', data[:16])

    if opcode == FILTER_OPCODE:
        packet_data = data[16:16+size]
        print(f"Found packet: {opcode:04X} - {packet_data.hex()}")
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

header = 0x1000  # SENDPACKET
addr = 0
size = len(packet_data)

message = struct.pack('<IIII', header, opcode, addr, size) + packet_data
sock.send(message)

# Wait for response
response = sock.recv(1)
if response[0] == 1:
    print("Packet blocked")
else:
    print("Packet sent")
```

### Task: Block Specific Packets

**Old:**
- Select packet in ListView
- Check "Block" checkbox

**New:**
1. Set `ENABLE_BLOCKING=1` in RirePE.ini
2. Write packet blocking script:

```python
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8275))

BLOCK_OPCODE = 0x0099

while True:
    data = sock.recv(4096)
    header, opcode, addr, size = struct.unpack('<IIII', data[:16])

    # Respond with block=1 for specific opcode
    if header == 0x1000:  # SENDPACKET
        if opcode == BLOCK_OPCODE:
            sock.send(bytes([1]))  # Block
            print(f"Blocked packet {opcode:04X}")
        else:
            sock.send(bytes([0]))  # Allow
```

### Task: View Packet Format

**Old:**
- Double-click packet in ListView
- Format viewer window opens showing field breakdown

**New:**
```python
# Parse packet format in script
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8275))

def parse_packet(data):
    offset = 0
    fields = []

    while offset < len(data):
        if offset + 1 <= len(data):
            byte_val = data[offset]
            fields.append(f"BYTE: {byte_val}")
            offset += 1
        if offset + 2 <= len(data):
            short_val = struct.unpack('<H', data[offset:offset+2])[0]
            fields.append(f"SHORT: {short_val}")
            offset += 2
        # Add more field types as needed

    return fields

while True:
    data = sock.recv(4096)
    header, opcode, addr, size = struct.unpack('<IIII', data[:16])
    packet_data = data[16:16+size]

    print(f"Packet {opcode:04X}:")
    fields = parse_packet(packet_data)
    for field in fields:
        print(f"  {field}")
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

### "I want the GUI back"

If you strongly prefer the GUI, you can:
1. Check out the last commit before removal: `git checkout <commit-hash>`
2. Build old version with RirePE.exe
3. Use the old binaries

**Or** consider building a web-based GUI:
- Connect to TCP API from browser (WebSocket proxy)
- Display packets in HTML table
- Provide visual filtering/blocking controls

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
```

#### Step 8.4: Update other documentation files

**Files to update:**
- `QUICK_REFERENCE.md`: Remove RirePE.exe references
- `TCP_USAGE.md`: Update to reflect TCP-only mode
- `Ini-README.md`: Remove USE_TCP, update TCP configuration section

#### Step 8.5: Test documentation
```bash
# Render markdown to verify formatting
python -m markdown README.md > /tmp/readme.html
```

**Verification**:
- All documentation is consistent
- No references to RirePE.exe remain
- Migration guide is clear and comprehensive

**Commit**: `git commit -m "Phase 8: Update documentation for headless architecture"`

---

### Phase 9: Final Testing and Release

**Goal**: Comprehensive testing of headless architecture.

#### Step 9.1: Clean build test
```bash
# Clean workspace
git clean -fdx

# Rebuild from scratch
msbuild Share/Simple/Simple.sln /p:Configuration=Release /p:Platform=x86
msbuild Share/Hook/Hook.sln /p:Configuration=Release /p:Platform=x86
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```

#### Step 9.2: Functional testing

**Test checklist:**
- [ ] Packet.dll injects successfully
- [ ] TCP server starts on configured port
- [ ] packet_monitor.py connects and displays packets
- [ ] Packets are logged correctly (opcode, size, data)
- [ ] tcp_inject_example.py can send packets
- [ ] Multiple clients can connect simultaneously
- [ ] Packet blocking works (ENABLE_BLOCKING=1)
- [ ] No crashes or hangs
- [ ] Debug log is useful

#### Step 9.3: Performance testing
```bash
# Run game for 10 minutes
# Monitor:
- CPU usage
- Memory usage
- Packet queue depth (check debug log)
- TCP connection stability
```

#### Step 9.4: Update CHANGELOG
File: `CHANGELOG.md` (create if doesn't exist)

```markdown
# Changelog

## [2.0.0] - 2025-10-10

### Breaking Changes
- **Removed RirePE.exe GUI application**
- **Removed named pipe communication**
- **TCP monitoring is now mandatory** (no longer optional)

### Added
- Headless packet logging architecture
- Python monitoring scripts (packet_monitor.py)
- TCP-only communication mode
- Support for multiple simultaneous monitoring clients
- Remote monitoring capability
- Migration guide (MIGRATION_GUIDE.md)

### Changed
- Packet.dll now operates independently (no GUI launch)
- TCP server starts automatically on DLL injection
- Configuration: removed USE_TCP flag, TCP is always enabled
- Documentation rewritten for headless architecture

### Removed
- RirePE.exe Windows GUI executable
- Named pipe client/server code
- Pipe-related configuration options
- GUI solution folder from RirePE.sln

### Migration
See MIGRATION_GUIDE.md for detailed migration instructions.

## [1.0.0] - Previous version

- Original RirePE.exe GUI implementation
- Named pipe communication
- Optional TCP support
```

#### Step 9.5: Create release tag
```bash
git tag -a v2.0.0 -m "Version 2.0.0: Headless packet logger"
git push origin v2.0.0
```

**Verification**:
- All tests pass
- Documentation is complete
- Clean git history
- Release tagged

**Commit**: `git commit -m "Phase 9: Final testing and release preparation"`

---

## Rollback Plan

If critical issues are discovered after deployment:

### Option 1: Quick revert
```bash
# Revert to commit before refactoring started
git revert <commit-hash-range>
git push origin main
```

### Option 2: Branch switch
```bash
# Create legacy branch
git checkout -b legacy-with-gui <commit-before-removal>
git push origin legacy-with-gui

# Users can checkout legacy branch if needed
```

### Option 3: Dual release
- Maintain two branches: `main` (headless) and `legacy` (GUI)
- Provide both DLL versions for users to choose

---

## Estimated Timeline

- **Phase 1** (Analysis): 30 minutes
- **Phase 2** (DLL initialization): 1 hour
- **Phase 3** (Configuration): 45 minutes
- **Phase 4** (Remove pipe code): 1 hour
- **Phase 5** (Solution file): 15 minutes
- **Phase 6** (Delete directory): 45 minutes
- **Phase 7** (CI/CD): 30 minutes
- **Phase 8** (Documentation): 2 hours
- **Phase 9** (Testing): 1.5 hours

**Total: ~8 hours**

Spread over 2-3 sessions with testing breaks.

---

## Success Criteria

- [ ] RirePE.exe project fully removed from codebase
- [ ] Packet.dll builds successfully
- [ ] Packet.dll operates in headless TCP-only mode
- [ ] TCP server starts automatically on injection
- [ ] packet_monitor.py works correctly
- [ ] tcp_inject_example.py works correctly
- [ ] No named pipe code remains
- [ ] No USE_TCP configuration flag
- [ ] All documentation updated and accurate
- [ ] CI/CD pipeline builds successfully
- [ ] Clean git history with logical commits
- [ ] Migration guide is comprehensive
- [ ] No regressions in core functionality

---

## Alternative Approaches Considered

### 1. Keep RirePE.exe as optional
**Pros**: Backward compatibility
**Cons**: Maintenance burden, complex codebase, pipe code remains

### 2. Convert RirePE.exe to TCP client GUI
**Pros**: Preserve GUI, modernize architecture
**Cons**: Still need to maintain GUI code, added complexity

### 3. Build web-based GUI
**Pros**: Cross-platform, modern UI, remote access
**Cons**: Large scope, requires web server, overkill for simple logging

**Decision**: Full removal is cleanest. Users who need GUI can build custom tools using TCP API.

---

## Post-Refactoring Improvements

Future enhancements after removal:

### Short-term
1. Improve packet_monitor.py UI (colors, filters, search)
2. Add packet replay tool (replay captured packets)
3. Create packet diff tool (compare capture files)

### Medium-term
1. Web-based dashboard (optional GUI replacement)
2. Packet capture to PCAP format (Wireshark integration)
3. Symbolic opcode names in logs (maintained opcode database)

### Long-term
1. Machine learning packet analysis
2. Automated exploit detection
3. Network traffic visualization

---

## Notes

### Why This Approach?

1. **Clean break**: Complete removal avoids half-measures and technical debt
2. **Modern workflow**: TCP/scripting is more flexible than Win32 GUI
3. **Maintainability**: Less code = less bugs = easier maintenance
4. **User empowerment**: Python scripts are easier to modify than C++ GUI

### Risks

1. **User backlash**: Some users may prefer GUI
   - **Mitigation**: Provide clear migration guide, responsive support

2. **Feature gaps**: Some GUI features may be hard to replicate
   - **Mitigation**: Identify critical features, provide script equivalents

3. **Learning curve**: Command-line tools require more technical knowledge
   - **Mitigation**: Comprehensive documentation, example scripts

### Communication Plan

When releasing:
1. **Announcement**: Explain rationale, benefits, migration path
2. **Documentation**: Ensure all guides are polished
3. **Support**: Be responsive to user issues during transition
4. **Examples**: Provide many example scripts for common tasks

---

## Appendix A: Key Files Changed

### Created
- `REFACTORING_PLAN_2.md` (this file)
- `Packet/PacketDefs.h` (NEW - Phase 2.1)
- `MIGRATION_GUIDE.md` (Phase 8)
- `CHANGELOG.md` (Phase 9)

### Modified
- `Packet/DllMain.cpp` (Phase 2: includes, GUI launch, pipe code; Phase 3: config)
- `Packet/PacketQueue.cpp` (Phase 2: remove pipe restart)
- `Packet/PacketLogging.cpp` (Phase 3-4: TCP-only, remove pipe client)
- `Packet/Packet.vcxproj` (Phase 2: add PacketDefs.h)
- `RirePE.sln` (Phase 5: remove RirePE project)
- `.github/workflows/build-windows.yml` (Phase 7: remove RirePE build)
- `README.md` (Phase 8: rewrite for headless)
- `ARCHITECTURE.md` (Phase 8: update diagrams)
- `RirePE.ini.example` (Phase 3: remove USE_TCP, add TCP_PORT/TCP_HOST)

### Deleted
- `RirePE/` (entire directory, 17 files) - Phase 6

---

## Appendix B: Build Verification Checklist

After each phase, verify the build is successful:

### Phase 1 (Documentation)
```bash
# No build required - documentation only
```

### Phase 2 (Create PacketDefs.h + Refactor)
```bash
# Critical: Both projects should build
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild RirePE/RirePE.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```
**Expected**: All builds succeed. Packet.dll uses PacketDefs.h. RirePE.exe still uses RirePE.h.

### Phase 3 (Configuration)
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```
**Expected**: All builds succeed. Configuration changes don't affect compilation.

### Phase 4 (Remove Pipe Code)
```bash
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```
**Expected**: Packet.dll builds successfully with TCP-only code.

### Phase 5 (Solution File)
```bash
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```
**Expected**: Solution loads and builds only Packet project (RirePE removed from solution).

### Phase 6 (Delete Directory)
```bash
# Verify RirePE.h references are gone first
grep -rn '#include.*RirePE\.h' Packet/  # Should return nothing

# Then test build
msbuild RirePE.sln /p:Configuration=Release /p:Platform=x86
```
**Expected**: Packet.dll builds successfully even after RirePE directory is deleted.

### Phase 7 (CI/CD)
```bash
# Test locally or via GitHub Actions
git push  # Triggers CI workflow
```
**Expected**: GitHub Actions workflow completes successfully.

### Phase 8-9 (Documentation + Testing)
```bash
# Full clean build
git clean -fdx
msbuild Share/Simple/Simple.sln /p:Configuration=Release /p:Platform=x86
msbuild Share/Hook/Hook.sln /p:Configuration=Release /p:Platform=x86
msbuild Packet/Packet.vcxproj /p:Configuration=Release /p:Platform=Win32
```
**Expected**: Complete rebuild from scratch succeeds.

---

## Appendix C: Rollback Points

Each phase creates a safe rollback point:

| Phase | Rollback Command | State After Rollback |
|-------|------------------|----------------------|
| 1 | `git reset --hard HEAD~1` | Clean start |
| 2 | `git reset --hard HEAD~2` | Before PacketDefs.h creation |
| 3 | `git reset --hard HEAD~3` | Before config changes |
| 4 | `git reset --hard HEAD~4` | Before pipe removal |
| 5 | `git reset --hard HEAD~5` | Before solution changes |
| 6 | `git reset --hard HEAD~6` | Before directory deletion |
| 7 | `git reset --hard HEAD~7` | Before CI/CD changes |
| 8 | `git reset --hard HEAD~8` | Before doc updates |
| 9 | `git reset --hard HEAD~9` | Before final testing |

**Emergency full rollback:**
```bash
git checkout <commit-before-phase-1>
```

---

**End of Refactoring Plan**
