● Based on the code analysis, I can document the .ini file format used by the Rire
  DLL. Although the Config class implementation (from the missing
  Share/Simple/Simple.h library) isn't available in the codebase, I can derive the
  complete specification from its usage patterns across the codebase.

  RirePE .ini File Documentation

  File Name

  - 32-bit: RirePE.ini
  - 64-bit: RirePE64.ini

  Defined at /home/amit/Downloads/reverse-engineering/RirePE/RirePE/RirePE.h:17

  Syntax

  Standard Windows INI file format:
  [SectionName]
  Key=Value

  Sections and Keys

  [RirePE] or [RirePE64] Section

  Main executable configuration

  | Key         | Type    | Values             | Default | Usage
            |
  |-------------|---------|--------------------|---------|-------------------------
  ----------|
  | DEBUG_MODE  | Integer | 0 or 1             | 0 (off) | Enables debug logging
  mode        |
  | THMS88_MODE | Integer | 0 or 1             | 0 (off) | Enables THMS88
  compatibility mode |
  | HeaderSize  | Integer | Positive integer   | 2       | Packet header size in
  bytes       |
  | IgnoreSend  | String  | Packet filter list | (empty) | Currently commented out
  in code   |
  | IgnoreRecv  | String  | Packet filter list | (empty) | Currently commented out
  in code   |

  Location: /home/amit/Downloads/reverse-engineering/RirePE/RirePE/Config.cpp:8-41

  ---
  [Packet] or [Packet64] Section

  DLL hook configuration

  | Key               | Type    | Values | Default | Description
                    |
  |-------------------|---------|--------|---------|-------------------------------
  ------------------|
  | DEBUG_MODE        | Integer | 0 or 1 | 0       | Debug mode for packet hooks
                    |
  | USE_THREAD        | Integer | 0 or 1 | 0       | Hook from separate thread
                    |
  | HIGH_VERSION_MODE | Integer | 0 or 1 | 0       | High version mode (CInPacket)
  - TODO            |
  | USE_ADDR          | Integer | 0 or 1 | 0       | Use hardcoded addresses
  instead of AOB scanning |

  When USE_ADDR=1, the following addresses are required (hex format):

  Send-related addresses:
  - SendPacket - Main send packet function
  - SendPacket2 - Alternative send packet function
  - COutPacket - COutPacket constructor
  - COutPacket2 - Alternative COutPacket constructor
  - COutPacket3 - Third alternative COutPacket constructor
  - Encode1 - Encode 1-byte value
  - Encode2 - Encode 2-byte value
  - Encode4 - Encode 4-byte value
  - Encode8 - Encode 8-byte value
  - EncodeStr - Encode string
  - EncodeBuffer - Encode buffer

  Receive-related addresses:
  - ProcessPacket - Packet processing function
  - Decode1 - Decode 1-byte value
  - Decode2 - Decode 2-byte value
  - Decode4 - Decode 4-byte value
  - Decode8 - Decode 8-byte value
  - DecodeStr - Decode string
  - DecodeBuffer - Decode buffer

  Address Format:
  - 32-bit: %08X (e.g., 00401000)
  - 64-bit: %llX (e.g., 140001000)

  Location:
  /home/amit/Downloads/reverse-engineering/RirePE/Packet/DllMain.cpp:26-111

  ---
  [Packet_TENVI] Section

  TENVI (TalesWeaver) specific DLL configuration

  | Key        | Type    | Values              | Default | Description          |
  |------------|---------|---------------------|---------|----------------------|
  | REGION     | String  | JP, KR, KRX, HK, CN | JP      | Game region selector |
  | DEBUG_MODE | Integer | 0 or 1              | 0       | Debug mode           |

  Location:
  /home/amit/Downloads/reverse-engineering/RirePE/Packet_TENVI/DllMain.cpp:151-166

  Parsing Behavior

  Value Reading:
  - String values are read as-is via Config::Read(section, key, output_string)
  - Integer values are converted using _wtoi() after reading as string
  - Boolean checks: Any non-zero integer is treated as true
  - Missing keys return empty string and are treated as false/0

  Value Writing:
  - Values are written via Config::Update(section, key, value_string)
  - Integers are converted to strings before writing
  - Addresses are formatted as hex strings

  Address Parsing (Packet/DllMain.cpp:7-23):
  // 64-bit: swscanf_s(wAddr.c_str(), L"%llX", &uAddr);
  // 32-bit: swscanf_s(wAddr.c_str(), L"%08X", &uAddr);

  Example Configuration

  [RirePE]
  DEBUG_MODE=1
  HeaderSize=4
  THMS88_MODE=0

  [Packet]
  DEBUG_MODE=1
  USE_ADDR=1
  SendPacket=00401000
  COutPacket=00402000
  Encode1=00403000
  Encode2=00403100
  Encode4=00403200
  ProcessPacket=00500000
  Decode1=00501000
  Decode2=00501100
  Decode4=00501200

  [Packet_TENVI]
  REGION=JP
  DEBUG_MODE=0

  Implementation Notes

  1. The Config class is from the missing Share/Simple/Simple.h library (excluded
  via .gitignore:353)
  2. Supports optional HINSTANCE parameter for DLL-relative path resolution
  3. Wrapper around Windows GetPrivateProfileString/WritePrivateProfileString APIs
  (inferred from usage pattern)
  4. Auto-save functionality exists but is commented out in most places

● Answer: ONE .ini file

  File Name:

  - 32-bit: RirePE.ini
  - 64-bit: RirePE64.ini

  Key Points:

  1. Defined at
  /home/amit/Downloads/reverse-enginee
  ring/RirePE/RirePE/RirePE.h:17:
  #define INI_FILE_NAME EXE_NAME".ini"
  2. Both DLL and EXE use the same 
  file:
    - Packet.dll reads from [Packet]
  or [Packet64] section
    - RirePE.exe reads from [RirePE]
  or [RirePE64] section
    - Packet_TENVI.dll (if used) reads
   from [Packet_TENVI] section
  3. File Location: According to
  Readme.md:4, both files must be in
  the same folder, so they share the
  same .ini file.
  4. Config Constructor Behavior:
    - DLL: Config conf(INI_FILE_NAME, 
  hinstDLL); - searches relative to
  DLL location
    - EXE: Config conf(INI_FILE_NAME);
   - searches relative to EXE location
    - Since they're in the same
  folder, they find the same file

  The file is NOT named Packet.dll.ini
   - it's always named after the
  executable (RirePE.ini), regardless
  of which component is reading it.



● Yes, this behavior follows the logic perfectly.

  Look at line 192 in
  /home/amit/Downloads/reverse-engineering/RirePE/Packet/DllMain.cpp:

  BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
      if (fdwReason == DLL_PROCESS_ATTACH) {
          DisableThreadLibraryCalls(hinstDLL);
          LoadPacketConfig(hinstDLL);
          PacketHook(gHookSettings);
          //SavePacketConfig();    // ← COMMENTED OUT!
      }
      return TRUE;
  }

  The SavePacketConfig() call is commented out!

  So even though:
  - The SavePacketConfig() function exists (lines 81-111)
  - It has all the logic to save found addresses to the .ini file
  - The conditions are met (32-bit, USE_ADDR=0, USE_THREAD=0)

  It's never actually called because line 192 has it commented out.

  This means the auto-save feature is currently disabled in the code. The AOB
  scanner finds the addresses and uses them during runtime, but never writes them
  back to RirePE.ini.

  If you wanted to enable auto-save, you'd need to uncomment that line and
  recompile the DLL.


● Based on the code in
  /home/amit/Downloads/reverse-engineering/RirePE/Packet/DllMain.cpp:

  It's either/or, not both:

  When USE_ADDR=0 (AOB mode):

  - Addresses from config are IGNORED (never read from the .ini file)
  - ONLY uses AOB (Array of Bytes) scanning to find addresses

  When USE_ADDR=1 (Manual mode):

  - AOB scanning is NOT performed
  - ONLY uses addresses from the SendPacket=, COutPacket=, etc. config entries

  Evidence from code:

  Line 48-49: Addresses are only read inside the if (USE_ADDR) block:
  if (conf.Read(DLL_NAME, L"USE_ADDR", wUseAddr) && _wtoi(wUseAddr.c_str())) {
      hs.use_addr = true;
      // Send
      hs.addr_SendPacket = ConftoAddress(conf, L"SendPacket");

  Lines 167-173: Different hook functions are called:
  if (hs.use_addr) {
      PacketHook_Conf(hs);     // Uses config addresses
      return true;
  }
  // aob scan mode (default)
  PacketHook_Thread(hs);        // Uses AOB scanning

  So with your current config (USE_ADDR=1), the SendPacket=004AC120 address WILL be
   used, and AOB scanning will be skipped entirely.


---

● Summary: RirePE SendPacket Button Analysis

  Key Findings

  1. The SendPacket Button Does NOT Directly Use SendPacket= Address

  When USE_ADDR=1 with SendPacket=004AC120:
  - The SendPacket= address is used for packet logging/interception ✓
  - The SendPacket button uses EnterSendPacket (found via AOB scan) ✓
  - EnterSendPacket is a wrapper that internally calls SendPacket

  2. USE_ADDR=1 is NOT Pure Manual Mode

  USE_ADDR=1 is a hybrid mode:
  - Main functions (SendPacket, ProcessPacket, COutPacket, Encode*, Decode*): Uses
  config addresses
  - Wrapper functions (EnterSendPacket, SendPacket_EH, CClientSocket): Still uses 
  AOB scanning

  3. Why Your Scenario Occurs (Logging Works, Button Doesn't)

  Flow when USE_ADDR=1 with SendPacket=004AC120:

  1. SendPacket=004AC120 (config) → _SendPacket hooked ✓
  2. Packet interception works → Logging in GUI ✓
  3. EnterSendPacket AOB scan runs (triggered by _SendPacket != NULL)
  4. IF AOB pattern matches → _EnterSendPacket hooked ✓ → Button works ✓
     IF AOB pattern fails → _EnterSendPacket = NULL ✗ → Button crashes/fails ✗

  4. EnterSendPacket AOB Scan Mechanism

  - Scans for byte patterns: FF 74 24 04 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C3
  - Validates each match by checking if the CALL instruction targets
  SendPacket=004AC120
  - Only accepts matches that actually call the configured SendPacket address

  5. Version Coverage Issue

  AOB_EnterSendPacket[] only has patterns for:
  - v164.0 to v186.1
  - v188.0+

  No v62.1 pattern exists → AOB scan fails for v62.1 → Button doesn't work

  6. What COutPacket Does

  - COutPacket: Constructor that initializes outgoing packet structures
  - Used for both logging (tracking packet construction) and sending (creating
  packets)
  - SendPacket must be hooked for COutPacket to be hooked (due to conditional logic
   at line 587)

  7. Critical Dependency Chain

  SendPacket= (config) → Required for:
    ├─ Packet logging ✓
    ├─ Triggering EnterSendPacket AOB scan
    └─ Enabling COutPacket hooks (line 587 condition)

  EnterSendPacket (AOB scan) → Required for:
    └─ SendPacket button functionality

  Without SendPacket=: Everything fails (no logging, no hooks, no button)
  Without EnterSendPacket AOB match: Logging works, but button fails

  Conclusion

  For v62.1, the SendPacket button cannot work with current code because there's no
   EnterSendPacket AOB pattern. You would need to either:
  1. Add a v62.1 pattern to AOB_EnterSendPacket[] in AobList.h
  2. Modify the code to support manual EnterSendPacket= configuration (currently
  not implemented)

● Summary: RirePE SendPacket Button Analysis

  Key Findings

  1. The SendPacket Button Does NOT Directly Use SendPacket= Address

  When USE_ADDR=1 with SendPacket=004AC120:
  - The SendPacket= address is used for packet logging/interception ✓
  - The SendPacket button uses EnterSendPacket (found via AOB scan) ✓
  - EnterSendPacket is a wrapper that internally calls SendPacket

  2. USE_ADDR=1 is NOT Pure Manual Mode

  USE_ADDR=1 is a hybrid mode:
  - Main functions (SendPacket, ProcessPacket, COutPacket, Encode*, Decode*): Uses
  config addresses
  - Wrapper functions (EnterSendPacket, SendPacket_EH, CClientSocket): Still uses 
  AOB scanning

  3. Why Your Scenario Occurs (Logging Works, Button Doesn't)

  Flow when USE_ADDR=1 with SendPacket=004AC120:

  1. SendPacket=004AC120 (config) → _SendPacket hooked ✓
  2. Packet interception works → Logging in GUI ✓
  3. EnterSendPacket AOB scan runs (triggered by _SendPacket != NULL)
  4. IF AOB pattern matches → _EnterSendPacket hooked ✓ → Button works ✓
     IF AOB pattern fails → _EnterSendPacket = NULL ✗ → Button crashes/fails ✗

  4. EnterSendPacket AOB Scan Mechanism

  - Scans for byte patterns: FF 74 24 04 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C3
  - Validates each match by checking if the CALL instruction targets
  SendPacket=004AC120
  - Only accepts matches that actually call the configured SendPacket address

  5. Version Coverage Issue

  AOB_EnterSendPacket[] only has patterns for:
  - v164.0 to v186.1
  - v188.0+

  No v62.1 pattern exists → AOB scan fails for v62.1 → Button doesn't work

  6. What COutPacket Does

  - COutPacket: Constructor that initializes outgoing packet structures
  - Used for both logging (tracking packet construction) and sending (creating
  packets)
  - SendPacket must be hooked for COutPacket to be hooked (due to conditional logic
   at line 587)

  7. Critical Dependency Chain

  SendPacket= (config) → Required for:
    ├─ Packet logging ✓
    ├─ Triggering EnterSendPacket AOB scan
    └─ Enabling COutPacket hooks (line 587 condition)

  EnterSendPacket (AOB scan) → Required for:
    └─ SendPacket button functionality

  Without SendPacket=: Everything fails (no logging, no hooks, no button)
  Without EnterSendPacket AOB match: Logging works, but button fails

  Conclusion

  For v62.1, the SendPacket button cannot work with current code because there's no
   EnterSendPacket AOB pattern. You would need to either:
  1. Add a v62.1 pattern to AOB_EnterSendPacket[] in AobList.h
  2. Modify the code to support manual EnterSendPacket= configuration (currently
  not implemented)


