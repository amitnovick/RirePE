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

The refactoring must be done gradually to ensure the 32-bit builds continue to work. The key insight is:

- **Source code lives in 64-bit directories for Share projects**
- **Source code lives in 32-bit directories for main projects (RirePE, Packet)**

### Phase 1: Prepare Share Projects (Move Source Files)

**Goal:** Move source files from 64-bit directories to 32-bit directories or shared locations.

#### Step 1.1: Refactor Share/Simple
- Create or verify Simple/ directory has all source files (currently in Simple64/)
- Move all .cpp/.h files from Simple64/ to Simple/
- Update Simple/Simple.vcxproj to reference local files instead of ../Simple64/
- Update Simple64/Simple64.vcxproj to reference ../Simple/ files (temporarily)
- Test build for both Simple and Simple64

#### Step 1.2: Refactor Share/Hook/Hook
- Move SimpleHook.cpp/h from Hook64/ to Hook/
- Update Hook/Hook.vcxproj to reference local files
- Update Hook64/Hook64.vcxproj to reference ../Hook/ files (temporarily)
- Test build

#### Step 1.3: Refactor Share/Hook/Zycore
- Move all Zycore source files from Zycore64/Zycore/ to Zycore/
- Update Zycore/Zycore.vcxproj to reference local files
- Update Zycore64/Zycore64.vcxproj to reference ../Zycore/ files (temporarily)
- Test build

#### Step 1.4: Refactor Share/Hook/Zydis
- Move all Zydis source files from Zydis64/Zydis/ to Zydis/
- Update Zydis/Zydis.vcxproj to reference local files
- Update include directories to point to ../Zycore/ and ../Zydis/
- Update Zydis64/Zydis64.vcxproj to reference ../Zydis/ files (temporarily)
- Test build

**Verification:** Build Share/Hook/Hook.sln and Share/Simple/Simple.sln for Win32 configuration

### Phase 2: Update Include Directories in 32-bit Projects

**Goal:** Replace all references to 64-bit directories with 32-bit directories.

#### Step 2.1: Update Hook/Hook.vcxproj
- Change `<AdditionalIncludeDirectories>../Zydis64;../Zycore64</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>../Zydis;../Zycore</AdditionalIncludeDirectories>`

#### Step 2.2: Update Zycore/Zycore.vcxproj
- Change `<AdditionalIncludeDirectories>../Zycore64</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>./</AdditionalIncludeDirectories>` or remove

#### Step 2.3: Update Zydis/Zydis.vcxproj
- Change `<AdditionalIncludeDirectories>../Zycore64;../Zydis64;../Zydis64/Zydis</AdditionalIncludeDirectories>`
- To: `<AdditionalIncludeDirectories>../Zycore;./;./Zydis</AdditionalIncludeDirectories>`

**Verification:** Build 32-bit configurations successfully

### Phase 3: Clean Up Conditional 64-bit Code

**Goal:** Remove _WIN64 conditional code that references 64-bit executables/DLLs.

#### Step 3.1: Update RirePE/RirePE.h
- Remove or update lines 9 and 15 that define RirePE64 and Packet64 names
- Keep only 32-bit names

#### Step 3.2: Update Packet/DllMain.cpp
- Remove #ifdef _WIN64 blocks (lines 141-145)
- Keep only the 32-bit RirePE.exe launch code

#### Step 3.3: Clean up conditional compilation
- Search for `#ifdef _WIN64` blocks in Packet source files
- Review each block to determine if it's architecture-specific or project-specific
- Remove project-specific 64-bit code
- Keep architecture-specific code (pointer sizes, etc.)

**Verification:** Code review and grep for remaining 64-bit references

### Phase 4: Remove 64-bit Projects from Solutions

**Goal:** Remove all 64-bit project references from solution files.

#### Step 4.1: Update RirePE.sln
- Remove RirePE64 project (lines 16-17)
- Remove Packet64 project (lines 14-15)
- Remove Packet_TENVI project (lines 20-21)
- Remove corresponding GlobalSection entries
- Remove x64 platform configurations from 32-bit projects (keep only Win32)

#### Step 4.2: Update Share/Hook/Hook.sln
- Remove Hook64 project
- Remove Zycore64 project
- Remove Zydis64 project
- Remove corresponding GlobalSection entries
- Remove x64 platform configurations

#### Step 4.3: Update Share/Simple/Simple.sln
- Remove Simple64 project
- Remove corresponding GlobalSection entries
- Remove x64 platform configurations

**Verification:** Solutions load correctly in Visual Studio, all remaining projects build

### Phase 5: Delete 64-bit Project Directories

**Goal:** Physically remove 64-bit project directories.

#### Step 5.1: Remove main 64-bit projects
```bash
rm -rf RirePE64/
rm -rf Packet64/
rm -rf Packet_TENVI/
```

#### Step 5.2: Remove Share 64-bit projects
```bash
rm -rf Share/Hook/Hook64/
rm -rf Share/Hook/Zycore64/
rm -rf Share/Hook/Zydis64/
rm -rf Share/Simple/Simple64/
```

#### Step 5.3: Remove build event scripts (if 64-bit specific)
- Check and potentially remove:
  - Share/Hook/BuildEvent64.bat
  - Share/Simple/BuildEvent64.bat

**Verification:** Full solution builds, no broken file references

### Phase 6: Update Documentation and Configuration

**Goal:** Update all documentation to reflect 32-bit only architecture.

#### Step 6.1: Update README files
- Update ARCHITECTURE.md to reflect new structure
- Update build instructions
- Update any references to 64-bit versions

#### Step 6.2: Update .gitignore
- Remove 64-bit specific build output directories if present

#### Step 6.3: Update CI/CD (if applicable)
- Check .github/workflows/ for 64-bit build configurations
- Update to build only 32-bit versions

**Verification:** Documentation review, test build from clean checkout

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

- **Phase 1:** 2-3 hours (file moves, vcxproj updates, testing)
- **Phase 2:** 1 hour (include directory updates, testing)
- **Phase 3:** 1-2 hours (code cleanup, testing)
- **Phase 4:** 1 hour (solution file updates)
- **Phase 5:** 30 minutes (directory deletion)
- **Phase 6:** 1 hour (documentation updates)

**Total:** 6.5-8.5 hours

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
1. **Phase 1 first** because we need source files accessible to 32-bit projects
2. **Phase 2 second** to fix include paths before removing projects
3. **Phase 3 third** to clean conditional code before solution changes
4. **Phases 4-6** are cleanup that won't affect builds

### Alternative Approach
If the team prefers to keep source code in separate directories (not colocated with 32-bit projects), consider creating a `Share/Common/` directory structure instead of moving files into 32-bit project directories.
