# Gradual Refactoring Plan: Remove 64-bit Projects and Packet_TENVI

## Executive Summary

This plan outlines the steps to remove all 64-bit projects (RirePE64, Packet64, Hook64, Simple64, Zycore64, Zydis64) and the Packet_TENVI project from the codebase. The main challenge is that **32-bit projects currently reference source files located in 64-bit project directories**.

## Current Architecture Analysis

### Project Dependencies

#### Main Projects
1. **RirePE (32-bit GUI)** - Self-contained, references its own source files
2. **RirePE64 (64-bit GUI)** - References ../RirePE/ source files
3. **Packet (32-bit DLL)** - Self-contained, references its own source files
4. **Packet64 (64-bit DLL)** - References ../Packet/ source files
5. **Packet_TENVI** - Standalone project with own source files (for different game)

#### Share/Simple Projects
- **Simple64/** - Contains actual source code (Simple.cpp, SimpleGUI.cpp, etc.)
- **Simple/** - References all source files from ../Simple64/

#### Share/Hook Projects
- **Hook64/** - Contains SimpleHook.cpp/h source files
- **Hook/** - References ../Hook64/SimpleHook.cpp and includes ../Zydis64, ../Zycore64
- **Zycore64/** - Contains Zycore library source code
- **Zycore/** - References all source files from ../Zycore64/Zycore/
- **Zydis64/** - Contains Zydis library source code
- **Zydis/** - Includes ../Zycore64, ../Zydis64, ../Zydis64/Zydis

### Key Findings

**Critical Cross-References (32-bit → 64-bit):**

1. **Share/Hook/Hook/Hook.vcxproj (32-bit)**
   - Line 120: `<AdditionalIncludeDirectories>../Zydis64;../Zycore64</AdditionalIncludeDirectories>`
   - Lines 151-154: `<ClCompile Include="..\Hook64\SimpleHook.cpp" />`

2. **Share/Hook/Zycore/Zycore.vcxproj (32-bit)**
   - Lines 22-54: References ALL source files from ../Zycore64/
   - Line 172: `<AdditionalIncludeDirectories>../Zycore64</AdditionalIncludeDirectories>`

3. **Share/Hook/Zydis/Zydis.vcxproj (32-bit)**
   - Line 138: `<AdditionalIncludeDirectories>../Zycore64;../Zydis64;../Zydis64/Zydis</AdditionalIncludeDirectories>`

4. **Share/Simple/Simple/Simple.vcxproj (32-bit)**
   - Lines 22-44: References ALL source files from ../Simple64/

**Additional References:**

5. **RirePE/RirePE.h**
   - Lines 9, 15: Conditional defines for RirePE64.exe and Packet64.dll when _WIN64 is defined

6. **Packet/DllMain.cpp**
   - Lines 142-144: Launches RirePE.exe (32-bit) or RirePE64.exe (64-bit) based on _WIN64

## Refactoring Strategy

**Key Principle:** Each phase must be independently buildable. After completing any phase, the entire solution must build successfully without errors.

The refactoring uses an **atomic update per component** approach:
- Each phase completely migrates one component (move files + update all references)
- Both 32-bit and 64-bit projects remain buildable until we explicitly remove them
- We move files and update references in the same phase to maintain consistency

### Phase 1: Migrate Share/Simple Component

**Goal:** Move Simple source files and update all references atomically.

**Status Before:** Simple64/ has source, Simple/ references it
**Status After:** Simple/ has source, Simple64/ references it (reverse)

#### Step 1.1: Move source files
```bash
cd Share/Simple/
# Move all source files from Simple64/ to Simple/
mv Simple64/*.cpp Simple/
mv Simple64/*.h Simple/
```

#### Step 1.2: Update Simple/Simple.vcxproj
Replace all file references:
- Change: `<ClInclude Include="..\Simple64\Simple.h" />`
- To: `<ClInclude Include="Simple.h" />`
- Repeat for all .h and .cpp files (lines 22-44)

#### Step 1.3: Update Simple64/Simple64.vcxproj
Add references to ../Simple/ files:
- Change: `<ClInclude Include="Simple.h" />`
- To: `<ClInclude Include="..\Simple\Simple.h" />`
- Repeat for all files

#### Step 1.4: Test builds
```bash
# Build 32-bit
msbuild Simple/Simple.vcxproj /p:Configuration=Release /p:Platform=Win32
# Build 64-bit (should still work)
msbuild Simple64/Simple64.vcxproj /p:Configuration=Release /p:Platform=x64
```

**Verification:** Both Simple.sln Win32 and x64 configurations build successfully
**Commit:** `git commit -m "Phase 1: Migrate Share/Simple component"`

### Phase 2: Migrate Share/Hook/Hook Component

**Goal:** Move Hook source files and update all references atomically.

**Status Before:** Hook64/ has SimpleHook source, Hook/ references it
**Status After:** Hook/ has SimpleHook source, Hook64/ references it

#### Step 2.1: Move source files
```bash
cd Share/Hook/
mv Hook64/SimpleHook.cpp Hook/
mv Hook64/SimpleHook.h Hook/
```

#### Step 2.2: Update Hook/Hook.vcxproj
- Line 151: Change `<ClCompile Include="..\Hook64\SimpleHook.cpp" />`
  To: `<ClCompile Include="SimpleHook.cpp" />`
- Line 154: Change `<ClInclude Include="..\Hook64\SimpleHook.h" />`
  To: `<ClInclude Include="SimpleHook.h" />`

#### Step 2.3: Update Hook64/Hook64.vcxproj
- Line 156: Change `<ClCompile Include="SimpleHook.cpp" />`
  To: `<ClCompile Include="..\Hook\SimpleHook.cpp" />`
- Line 159: Change `<ClInclude Include="SimpleHook.h" />`
  To: `<ClInclude Include="..\Hook\SimpleHook.h" />`

#### Step 2.4: Test builds
```bash
msbuild Hook/Hook.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild Hook64/Hook64.vcxproj /p:Configuration=Release /p:Platform=x64
```

**Verification:** Both Hook configurations build successfully
**Commit:** `git commit -m "Phase 2: Migrate Share/Hook/Hook component"`

### Phase 3: Migrate Share/Hook/Zycore Component

**Goal:** Move Zycore source files and update all references atomically.

#### Step 3.1: Move source directory
```bash
cd Share/Hook/
# Move the Zycore source subdirectory
mv Zycore64/Zycore/* Zycore/
# Keep Zycore64/ZycoreExportConfig.h separate
mv Zycore64/ZycoreExportConfig.h Zycore/
```

#### Step 3.2: Update Zycore/Zycore.vcxproj
Replace all file references (lines 22-54):
- Change: `<ClInclude Include="..\Zycore64\ZycoreExportConfig.h" />`
- To: `<ClInclude Include="ZycoreExportConfig.h" />`
- Change: `<ClInclude Include="..\Zycore64\Zycore\Allocator.h" />`
- To: `<ClInclude Include="Zycore\Allocator.h" />`
- Repeat for all source files

Update include directory (line 172):
- Change: `<AdditionalIncludeDirectories>../Zycore64</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>./</AdditionalIncludeDirectories>`

#### Step 3.3: Update Zycore64/Zycore64.vcxproj
Add references pointing back to ../Zycore/:
- Change all file paths from local to `../Zycore/...`
- Update include directories similarly

#### Step 3.4: Test builds
```bash
msbuild Zycore/Zycore.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild Zycore64/Zycore64.vcxproj /p:Configuration=Release /p:Platform=x64
```

**Verification:** Both Zycore configurations build successfully
**Commit:** `git commit -m "Phase 3: Migrate Share/Hook/Zycore component"`

### Phase 4: Migrate Share/Hook/Zydis Component and Update Include Paths

**Goal:** Move Zydis source files and update ALL include directory references.

#### Step 4.1: Move source directory
```bash
cd Share/Hook/
mv Zydis64/Zydis/* Zydis/
mv Zydis64/ZydisExportConfig.h Zydis/
```

#### Step 4.2: Update Zydis/Zydis.vcxproj
- Update all file references from `Zydis64/...` to local paths
- Line 138: Change `<AdditionalIncludeDirectories>../Zycore64;../Zydis64;../Zydis64/Zydis</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>../Zycore;./;./Zydis</AdditionalIncludeDirectories>`

#### Step 4.3: Update Zydis64/Zydis64.vcxproj
- Update file references to point to ../Zydis/
- Update include directories

#### Step 4.4: Update Hook/Hook.vcxproj (Critical!)
- Line 120: Change `<AdditionalIncludeDirectories>../Zydis64;../Zycore64</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>../Zydis;../Zycore</AdditionalIncludeDirectories>`

#### Step 4.5: Test builds
```bash
# Test entire Hook solution
msbuild Hook.sln /p:Configuration=Release /p:Platform=Win32
msbuild Hook.sln /p:Configuration=Release /p:Platform=x64
```

**Verification:** All Hook solution projects build for both platforms
**Commit:** `git commit -m "Phase 4: Migrate Zydis component and update all include paths"`

### Phase 5: Clean Up Conditional 64-bit Code in Main Projects

**Goal:** Remove _WIN64 conditional code that references 64-bit executables/DLLs. This prepares for removing the 64-bit projects.

#### Step 5.1: Update RirePE/RirePE.h
Remove 64-bit exe/dll name definitions:
```cpp
// Before:
#ifndef _WIN64
#define EXE_NAME L"RirePE"
#else
#define EXE_NAME L"RirePE64"  // Remove this
#endif

#ifndef _WIN64
#define DLL_NAME L"Packet"
#else
#define DLL_NAME L"Packet64"  // Remove this
#endif

// After:
#define EXE_NAME L"RirePE"
#define DLL_NAME L"Packet"
```

#### Step 5.2: Update Packet/DllMain.cpp
Remove 64-bit launch code (lines 141-145):
```cpp
// Before:
#ifndef _WIN64
    ShellExecuteW(NULL, NULL, (wDir + L"\\RirePE.exe").c_str(), ...);
#else
    ShellExecuteW(NULL, NULL, (wDir + L"\\RirePE64.exe").c_str(), ...);  // Remove
#endif

// After:
    ShellExecuteW(NULL, NULL, (wDir + L"\\RirePE.exe").c_str(), ...);
```

**Note:** Keep architecture-specific `#ifdef _WIN64` blocks that handle pointer sizes, data types, etc. Only remove project-specific references.

#### Step 5.3: Review remaining _WIN64 usage
```bash
grep -n "#ifdef _WIN64" Packet/*.cpp Packet/*.h RirePE/*.cpp RirePE/*.h
```
Review each occurrence - keep architecture code, remove project references.

#### Step 5.4: Test builds
```bash
msbuild RirePE.sln /p:Configuration=Release /p:Platform=Win32
```

**Verification:** 32-bit builds succeed, no references to RirePE64/Packet64 executables
**Commit:** `git commit -m "Phase 5: Remove 64-bit project references from source code"`

### Phase 6: Remove 64-bit Projects from Solutions

**Goal:** Remove all 64-bit project entries from solution files. After this phase, only 32-bit projects remain.

#### Step 6.1: Update RirePE.sln
Remove projects:
- Lines 14-15: Remove Packet64 project
- Lines 16-17: Remove RirePE64 project
- Lines 20-21: Remove Packet_TENVI project

Remove from GlobalSection(ProjectConfigurationPlatforms):
- Remove all lines containing `{0390B5AE-2948-4BCD-BD23-57F44A72E04D}` (Packet64)
- Remove all lines containing `{62E49245-74FD-49C2-8E41-285F5FF2606D}` (RirePE64)
- Remove all lines containing `{92BCAA26-D741-4637-9923-5B5E4BE3D704}` (Packet_TENVI)

Remove from GlobalSection(NestedProjects):
- Remove corresponding lines

Remove x64 platform configurations from remaining projects:
- Keep only `Debug|x86` and `Release|x86` in GlobalSection(SolutionConfigurationPlatforms)
- Remove all `Debug|x64` and `Release|x64` lines from remaining projects

#### Step 6.2: Update Share/Hook/Hook.sln
Remove projects:
- Lines 8-9: Remove Hook64
- Lines 14-15: Remove Zycore64
- Lines 18-19: Remove Zydis64
- Remove corresponding GlobalSection entries
- Remove x64 platform configurations

#### Step 6.3: Update Share/Simple/Simple.sln
Remove project:
- Remove Simple64 project entry
- Remove corresponding GlobalSection entries
- Remove x64 platform configurations

#### Step 6.4: Test builds
```bash
# Open and build solutions
msbuild RirePE.sln /p:Configuration=Release /p:Platform=Win32
msbuild Share/Hook/Hook.sln /p:Configuration=Release /p:Platform=Win32
msbuild Share/Simple/Simple.sln /p:Configuration=Release /p:Platform=Win32
```

**Verification:** All solutions load in Visual Studio, only 32-bit projects visible, all build successfully
**Commit:** `git commit -m "Phase 6: Remove 64-bit projects from solution files"`

### Phase 7: Delete 64-bit Project Directories and Files

**Goal:** Physically remove 64-bit project directories and related files.

#### Step 7.1: Remove main 64-bit projects
```bash
rm -rf RirePE64/
rm -rf Packet64/
rm -rf Packet_TENVI/
```

#### Step 7.2: Remove Share 64-bit projects
```bash
rm -rf Share/Hook/Hook64/
rm -rf Share/Hook/Zycore64/
rm -rf Share/Hook/Zydis64/
rm -rf Share/Simple/Simple64/
```

#### Step 7.3: Remove 64-bit build scripts
```bash
rm Share/Hook/BuildEvent64.bat
rm Share/Simple/BuildEvent64.bat
```

#### Step 7.4: Test full clean build
```bash
# Clean all outputs
git clean -fdx

# Rebuild everything
msbuild RirePE.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild
```

**Verification:** Full solution builds from clean state, no broken file references
**Commit:** `git commit -m "Phase 7: Delete 64-bit project directories"`

### Phase 8: Update Documentation

**Goal:** Update all documentation to reflect 32-bit only architecture.

#### Step 8.1: Update ARCHITECTURE.md
- Remove references to 64-bit builds
- Update project structure diagrams
- Document new file locations

#### Step 8.2: Update README.md
- Update build instructions (remove 64-bit steps)
- Update system requirements

#### Step 8.3: Update .gitignore
- Remove 64-bit specific entries if present:
  - `/x64/`
  - `/Release64/`
  - etc.

#### Step 8.4: Check CI/CD workflows
```bash
cat .github/workflows/*.yml
```
- Remove or update 64-bit build steps

**Verification:** Documentation review, test build from clean checkout
**Commit:** `git commit -m "Phase 8: Update documentation for 32-bit only"`

## Risk Mitigation

### Backup Strategy
- Create a git branch before starting: `git checkout -b refactor-remove-64bit`
- Commit after each phase completes successfully
- Keep original codebase intact until full testing is complete

### Testing Checkpoints
After each phase:
1. Clean build (delete all build outputs)
2. Rebuild all 32-bit configurations
3. Test basic functionality (if test suite exists)
4. Commit changes with descriptive message

### Rollback Plan
If issues arise:
1. Identify which phase caused the problem
2. Use `git diff` to review changes
3. Revert specific commits: `git revert <commit-hash>`
4. Or reset to before phase: `git reset --hard <commit-before-phase>`

## Estimated Timeline

- **Phase 1 (Simple):** 30-45 minutes (move files, update 2 vcxproj, test)
- **Phase 2 (Hook):** 20-30 minutes (move files, update 2 vcxproj, test)
- **Phase 3 (Zycore):** 45-60 minutes (move directory, update many file refs, test)
- **Phase 4 (Zydis):** 45-60 minutes (move directory, update refs + includes, test)
- **Phase 5 (Code cleanup):** 30-45 minutes (remove conditional code, test)
- **Phase 6 (Solutions):** 45-60 minutes (edit 3 .sln files, test all)
- **Phase 7 (Delete):** 15-20 minutes (rm directories, clean build)
- **Phase 8 (Docs):** 30-45 minutes (update documentation)

**Total:** 4-5.5 hours

Each phase is independently testable and can be done in a separate session.

## Success Criteria

- [ ] All 32-bit projects build without errors
- [ ] No references to RirePE64, Packet64, or Packet_TENVI remain
- [ ] No references to Hook64, Simple64, Zycore64, or Zydis64 directories
- [ ] Solution files contain only 32-bit projects
- [ ] All source code uses local or 32-bit project references
- [ ] Documentation reflects 32-bit only architecture
- [ ] Clean git history with logical commits

## Notes

### Why This Order?
1. **Phases 1-4:** Migrate components atomically - each phase moves files and updates ALL references so both 32/64-bit builds work
2. **Phase 5:** Clean source code to prepare for removing 64-bit projects
3. **Phase 6:** Remove project references (safe because source is already migrated)
4. **Phases 7-8:** Physical cleanup and documentation

### Key Design Decision
We move source files from 64-bit to 32-bit directories (not to a shared location) because:
- 32-bit is the primary target going forward
- Simpler project structure (no extra Common/ directory)
- Files live with the project that uses them

### Alternative Approach
If you prefer a shared location, create `Share/Hook/Common/` and `Share/Simple/Common/` directories instead of moving to the 32-bit project folders. Both projects would then reference the Common location.
