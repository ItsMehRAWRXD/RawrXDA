# RawrXD Text Editor - Build & Deployment Guide

## Environment Setup

### Required Tools

1. **Microsoft Visual Studio 2019+ (or Build Tools)**
   - MASM x64 Assembler (ml64.exe)
   - Linker (link.exe)
   - C++ development tools (for kernel32.lib, user32.lib, gdi32.lib)

2. **Installation Verification**
   ```cmd
   where ml64.exe
   where link.exe
   ```
   Should return paths like:
   ```
   C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.3x.xxxxx\bin\Hostx64\x64\ml64.exe
   C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.3x.xxxxx\bin\Hostx64\x64\link.exe
   ```

3. **Library Paths**
   - kernel32.lib
   - user32.lib  
   - gdi32.lib
   
   Default location: `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.xxxxx.0\um\x64\`

## File Organization

Create project structure:
```
D:\rawrxd\
├── RawrXD_TextEditorGUI.asm
├── RawrXD_TextEditor_Main.asm
├── RawrXD_TextEditor_Completion.asm
├── build/
│   ├── RawrXD_TextEditorGUI.obj
│   ├── RawrXD_TextEditor_Main.obj
│   ├── RawrXD_TextEditor_Completion.obj
│   ├── RawrXDEditor.exe
│   └── RawrXDEditor.pdb
└── bin/
    └── RawrXDEditor.exe
```

## Build Process

### Step 1: Assembly (Create Object Files)

Open Visual Studio Command Prompt (x64 mode) and navigate to project directory:

```cmd
cd D:\rawrxd
```

Assemble each file:

```cmd
ml64.exe /c /Zi /W3 /Fo build\ /I "C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um" RawrXD_TextEditorGUI.asm
```

Options:
- `/c` = Assemble only (no linking)
- `/Zi` = Generate debugging information
- `/W3` = Warning level 3
- `/Fo` = Output directory
- `/I` = Include directories

Expected output:
```
 Assembling: RawrXD_TextEditorGUI.asm
RawrXD_TextEditorGUI.asm(1234) : warning A4052: identifier `EditorWindow_Create' already defined
RawrXDEditor_TextEditorGUI.obj
```

Repeat for other files:
```cmd
ml64.exe /c /Zi /W3 /Fo build\ /I "C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um" RawrXD_TextEditor_Main.asm
ml64.exe /c /Zi /W3 /Fo build\ /I "C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um" RawrXD_TextEditor_Completion.asm
```

Verify .obj files exist:
```cmd
dir build\*.obj
```

### Step 2: Linking (Create Executable)

Link all object files into single executable:

```cmd
link.exe /subsystem:windows /entry:main /debug /out:build\RawrXDEditor.exe ^
    build\RawrXD_TextEditorGUI.obj ^
    build\RawrXD_TextEditor_Main.obj ^
    build\RawrXD_TextEditor_Completion.obj ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64\kernel32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64\user32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64\gdi32.lib"
```

Options:
- `/subsystem:windows` = Create Windows GUI application (not console)
- `/entry:main` = Entry point is `main` procedure
- `/debug` = Generate debug symbols (.pdb)
- `/out:` = Output executable name

Expected output:
```
Microsoft (R) Incremental Linker Version 14.34.xxxxx
Copyright (C) Microsoft Corporation.  All rights reserved.

RawrXD_TextEditorGUI.obj : warning LNK4024: unresolved external symbol 'printf', discard
  ...
```

Warnings about unused symbols are OK. Fatal errors like "unresolved external symbol 'main'" are critical.

### Step 3: Verify Executable

```cmd
dir build\RawrXDEditor.exe
```

Should show file size >50KB (includes debug symbols).

## Build Script (Automated)

Create `build.bat` in `D:\rawrxd\`:

```batch
@echo off
setlocal enabledelayedexpansion

REM === Configuration ===
set ML64=ml64.exe
set LINK=link.exe
set OUTDIR=build
set BINDIR=bin

REM === Create directories ===
if not exist %OUTDIR% mkdir %OUTDIR%
if not exist %BINDIR% mkdir %BINDIR%

echo.
echo === Step 1: Assembling GUI Module ===
%ML64% /c /Zi /W3 /Fo %OUTDIR%\ RawrXD_TextEditorGUI.asm
if errorlevel 1 goto :error

echo.
echo === Step 2: Assembling Main Module ===
%ML64% /c /Zi /W3 /Fo %OUTDIR%\ RawrXD_TextEditor_Main.asm
if errorlevel 1 goto :error

echo.
echo === Step 3: Assembling Completion Module ===
%ML64% /c /Zi /W3 /Fo %OUTDIR%\ RawrXD_TextEditor_Completion.asm
if errorlevel 1 goto :error

echo.
echo === Step 4: Linking Executable ===
%LINK% /subsystem:windows /entry:main /debug /out:%OUTDIR%\RawrXDEditor.exe ^
    %OUTDIR%\RawrXD_TextEditorGUI.obj ^
    %OUTDIR%\RawrXD_TextEditor_Main.obj ^
    %OUTDIR%\RawrXD_TextEditor_Completion.obj ^
    kernel32.lib user32.lib gdi32.lib
if errorlevel 1 goto :error

echo.
echo === Step 5: Copying to bin ===
copy %OUTDIR%\RawrXDEditor.exe %BINDIR%\RawrXDEditor.exe
copy %OUTDIR%\RawrXDEditor.pdb %BINDIR%\RawrXDEditor.pdb

echo.
echo [SUCCESS] Build complete!
echo Executable: %BINDIR%\RawrXDEditor.exe
echo Debug symbols: %BINDIR%\RawrXDEditor.pdb
goto :end

:error
echo.
echo [FAILED] Build failed with errors
exit /b 1

:end
endlocal
```

Run build:
```cmd
build.bat
```

## Troubleshooting

### Error: "ml64.exe: Command not found"

**Solution**: Run from Visual Studio Developer Command Prompt
- Open Visual Studio
- Tools → Command Line → Developer Command Prompt
- Ensure x64 is selected (not x86)

### Error: "unresolved external symbol 'EditorWindow_Create'"

**Causes**:
1. Symbol defined in one .asm, used in another, but not declared with PROC
2. Spelling mismatch (EditorWindow_Create vs EditorWindow_create)
3. Not included in linking

**Solution**:
```asm
; In file1.asm:
EditorWindow_Create PROTO :PTR, :PTR    ; Forward declaration

; In file2.asm:
call EditorWindow_Create

; At link time:
link file1.obj file2.obj
```

### Error: "A1000: cannot open file 'windows.h'"

**Note**: MASM doesn't use C headers. Don't include `windows.h`.

All API definitions should be in .asm files as constants:

```asm
; Define in .asm, not via #include
WM_PAINT EQU 15
WM_KEYDOWN EQU 256
WM_CHAR EQU 258
```

### Warning: "A4024: too many errors; abandon assembly"

**Solution**: First error will be printed. Run again to fix current error, then repeat.

### Executable too large (>100MB)

**Check**:
```cmd
dumpbin /headers build\RawrXDEditor.exe | grep Size
```

If debug symbols are >50MB:

Remove `/debug` from link command for smaller release build:
```cmd
link.exe /subsystem:windows /entry:main /out:build\RawrXDEditor.exe ...
```

## Testing the Executable

### 1. Launch GUI

```cmd
build\RawrXDEditor.exe
```

Expected:
- Window appears with title "RawrXD Text Editor"
- Menu bar (File, Edit)
- Toolbar buttons (Open, Save, Cut, Copy, Paste)
- Status bar at bottom showing "Ready"
- Blinking cursor in text area

### 2. Test Keyboard

- Type text → appears on screen
- Arrow keys → cursor moves
- Home/End → cursor to line start/end
- Backspace → character deleted
- Delete → character after cursor deleted

### 3. Test Menu

- File > Open → File dialog appears
- Select any text file → content loads
- File > Save → Save dialog appears
- File > Exit → Application closes

### 4. Test Clipboard

- Select text (Shift+Arrow)
- Edit > Cut → Text removed from buffer, in clipboard
- Click elsewhere, paste (Ctrl+V) → Text appears
- Edit > Copy → Text copied (not removed)
- Edit > Paste → Text inserted at cursor

## Debugging with WinDbg

### 1. Start Debugger

```cmd
windbg.exe build\RawrXDEditor.exe
```

### 2. Set Breakpoint at Entry

```
bp main
g
```

### 3. Step Through Setup

```
t       ; Step one instruction
p       ; Step over procedure calls
g       ; Continue
```

### 4. Inspect Registers

```
rax rax   ; Show all registers
```

### 5. Inspect Memory

```
dd [rbx]  ; Dump 8 bytes as DWORDs at rbx
da [rsi] L20  ; Dump ASCII at rsi (20 bytes)
```

## Performance Profiling

### Using Windows Performance Toolkit

1. Install Windows Performance Toolkit (part of Windows SDK)

2. Record trace:
```cmd
wpr.exe -start GeneralProfile -recordtempto c:\temp\profiles
RawrXDEditor.exe
wpr.exe -stop profile.etl
```

3. Analyze:
```cmd
wpa.exe profile.etl
```

Focus on:
- CPU usage
- Memory allocations
- I/O operations

## Release vs Debug

### Debug Build
```cmd
link ... /debug ...
```
- Includes .pdb symbol file
- Larger file size (~10MB with symbols)
- Can debug with breakpoints
- Slower execution

### Release Build
```cmd
link ... /release ...
```
- No debug symbols
- Smaller file size (~500KB)
- Optimized code
- Recommended for distribution

## Distribution

### Create Installer

1. Copy executable and dependencies:
```cmd
xcopy build\RawrXDEditor.exe deployment\
xcopy build\RawrXDEditor.pdb deployment\ (optional for debugging)
```

2. Include runtime dependencies:
- vcruntime140.dll (Microsoft C Runtime)
- msvcp140.dll (Microsoft C++ Runtime)

3. Create installer script (.msi or .nsi for NSIS):
```nsis
InstallDir "$PROGRAMFILES\RawrXD\TextEditor"

File "RawrXDEditor.exe"
File "vcruntime140.dll"
File "msvcp140.dll"

CreateDirectory "$SMPROGRAMS\RawrXD"
CreateShortCut "$SMPROGRAMS\RawrXD\Text Editor.lnk" "$INSTDIR\RawrXDEditor.exe"
```

## Continuous Integration

### GitHub Actions Example

Create `.github/workflows/build.yml`:

```yaml
name: Build RawrXD Editor

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup MASM
      uses: microsoft/setup-msbuild@v1
    
    - name: Build Executable
      run: |
        ml64 /c /Zi RawrXD_TextEditorGUI.asm
        ml64 /c /Zi RawrXD_TextEditor_Main.asm
        ml64 /c /Zi RawrXD_TextEditor_Completion.asm
        link /subsystem:windows /entry:main /debug ^
          RawrXD_TextEditorGUI.obj ^
          RawrXD_TextEditor_Main.obj ^
          RawrXD_TextEditor_Completion.obj ^
          kernel32.lib user32.lib gdi32.lib ^
          /out:RawrXDEditor.exe
    
    - name: Upload Artifact
      uses: actions/upload-artifact@v2
      with:
        name: RawrXDEditor.exe
        path: RawrXDEditor.exe
```

## Verification Checklist

Before distributing:

- [ ] Executable runs without crashes
- [ ] All menu items functional
- [ ] File open/save works
- [ ] Text input/editing works
- [ ] Cut/copy/paste functional
- [ ] No memory leaks (Task Manager after 1 hour)
- [ ] File size reasonable (<10MB)
- [ ] Installation on clean system succeeds
- [ ] No admin privileges required (unless needed for file access)
- [ ] Help documentation included

## Support Files

Place these files alongside executable:

1. **README.txt**
   - Usage instructions
   - Keyboard shortcuts
   - System requirements

2. **CHANGES.txt**
   - Version history
   - Bug fixes
   - New features

3. **LICENSE.txt**
   - License information

## Version Updates

### Updating Source

1. Edit .asm files
2. Update version constant:
   ```asm
   EDITOR_VERSION EQU "2.0.1"
   ```
3. Rebuild:
   ```cmd
   build.bat
   ```

### Rollback

1. Git revert:
   ```cmd
   git revert HEAD
   ```
2. Rebuild:
   ```cmd
   build.bat
   ```

## Deployment to Production

### Pre-Deployment Checklist

- [ ] All tests pass
- [ ] No compiler warnings
- [ ] No linker warnings
- [ ] Release build tested
- [ ] Executable integrity verified
- [ ] Documentation updated
- [ ] Version number incremented
- [ ] Git repository tagged
- [ ] Backup of previous version taken

### Deployment Steps

1. Archive current production version
2. Copy new executable to deployment location
3. Notify users of availability
4. Monitor for crash reports
5. Keep previous version available for rollback

### Monitoring

Watch for:
- Crash reports
- Performance issues
- File I/O errors
- Memory usage spikes
- UI responsiveness problems

## Emergency Hotfixes

If critical bug found:

1. Identify issue
2. Make minimal fix to .asm
3. Rebuild
4. Test thoroughly
5. Deploy as hotfix version (e.g., 2.0.1-hotfix1)
6. Push full fix in next major release
