# RawrXD MASM Editor - Build & Compilation Guide

## Prerequisites

### Required Tools

#### 1. MASM64 Assembler

```powershell
# Location (typical Windows SDK installation)
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MASM\bin\ml64.exe

# Verify installation
ml64.exe /?
```

#### 2. Linker (link.exe)

```powershell
# Usually installed with Visual Studio
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe

# Verify
link.exe /?
```

#### 3. Windows SDK Libraries

```powershell
# Required libraries (should be in SDK)
kernel32.lib    # System functions
user32.lib      # Window API
gdi32.lib       # Graphics Device Interface
comctl32.lib    # Common controls (optional)
```

Typically located at:
```
C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x64\
```

### Optional: Ollama for ML Completion

```bash
# Download from https://ollama.ai
# Or install via Homebrew/choco:
choco install ollama

# Then pull the model:
ollama pull codellama:7b

# Start Ollama service (runs on localhost:11434):
ollama serve
```

---

## Directory Structure

```
d:\rawrxd\
├── RawrXD_MASM_SyntaxHighlighter.asm        (Main editor, 900 LOC)
├── RawrXD_MASM_Editor_Editing.asm           (Editing ops, 300 LOC)
├── RawrXD_MASM_Editor_MLCompletion.asm      (ML integration, 400 LOC)
├── Build_MASM_Editor.ps1                    (Build script)
├── RawrXD_MASM_Editor_INTEGRATION.md        (Integration guide)
├── RawrXD_MASM_Editor_API_QUICKREF.md       (API reference)
└── RawrXD_MASM_Editor_BUILD.md              (This file)
```

---

## Step-by-Step Build Process

### Option 1: Manual Command Line

#### 1. Assemble Main Module

```cmd
cd d:\rawrxd

ml64.exe RawrXD_MASM_SyntaxHighlighter.asm /c

REM Output: RawrXD_MASM_SyntaxHighlighter.obj (~50KB)
```

**Output should show:**
```
Microsoft (R) Macro Assembler (x64) Version 14.3x.xxxx
Copyright (C) Microsoft Corporation.  All rights reserved.

Assembling: RawrXD_MASM_SyntaxHighlighter.asm

RawrXD_MASM_SyntaxHighlighter.asm(1): Macro Call Count=0, Loop Count=0
```

#### 2. Assemble Editing Module

```cmd
ml64.exe RawrXD_MASM_Editor_Editing.asm /c

REM Output: RawrXD_MASM_Editor_Editing.obj (~20KB)
```

#### 3. Assemble ML Completion Module

```cmd
ml64.exe RawrXD_MASM_Editor_MLCompletion.asm /c

REM Output: RawrXD_MASM_Editor_MLCompletion.obj (~25KB)
```

#### 4. Link All Modules

```cmd
link.exe RawrXD_MASM_SyntaxHighlighter.obj ^
         RawrXD_MASM_Editor_Editing.obj ^
         RawrXD_MASM_Editor_MLCompletion.obj ^
         kernel32.lib user32.lib gdi32.lib ^
         /subsystem:windows ^
         /entry:main ^
         /out:RawrXD_MASM_Editor.exe

REM Output: RawrXD_MASM_Editor.exe (~150-200KB)
```

**Expected linker output:**
```
Microsoft (R) Incremental Linker Version 14.3x.xxxx
Copyright (C) Microsoft Corporation.  All rights reserved.

   Creating library RawrXD_MASM_Editor.lib and object RawrXD_MASM_Editor.exp
```

---

### Option 2: PowerShell Build Script

#### Create Build Script

Save this as `Build_MASM_Editor.ps1`:

```powershell
param(
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Rebuild
)

# Configuration
$ML64     = "ml64.exe"
$LINK     = "link.exe"
$OUTDIR   = "D:\rawrxd"
$MODULES  = @(
    "RawrXD_MASM_SyntaxHighlighter.asm",
    "RawrXD_MASM_Editor_Editing.asm",
    "RawrXD_MASM_Editor_MLCompletion.asm"
)
$OUTFILE  = "RawrXD_MASM_Editor.exe"
$LIBS     = @("kernel32.lib", "user32.lib", "gdi32.lib")

# Colors for output
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Error   { Write-Host $args -ForegroundColor Red }
function Write-Info    { Write-Host $args -ForegroundColor Cyan }

# Main build function
function Build-MASM {
    Write-Info "================================"
    Write-Info "RawrXD MASM Editor Build"
    Write-Info "================================"
    Write-Info "Configuration: $Configuration"
    Write-Info "Output: $OUTDIR\$OUTFILE"
    Write-Info ""

    Push-Location $OUTDIR

    try {
        # Clean step
        if ($Clean -or $Rebuild) {
            Write-Info "[1/4] Cleaning..."
            Remove-Item -Path *.obj -Force -ErrorAction SilentlyContinue
            Remove-Item -Path *.exe -Force -ErrorAction SilentlyContinue
            Remove-Item -Path *.lib -Force -ErrorAction SilentlyContinue
            Remove-Item -Path *.exp -Force -ErrorAction SilentlyContinue
            Write-Success "  ✓ Clean complete"
        }

        # Assemble step
        Write-Info "[2/4] Assembling modules..."
        $objs = @()
        
        foreach ($module in $MODULES) {
            Write-Info "  Assembling: $module"
            
            $output = & $ML64 $module /c 2>&1
            
            if ($LASTEXITCODE -ne 0) {
                Write-Error "  ✗ Assembly failed for $module"
                Write-Error $output
                throw "Assembly error"
            }
            
            $obj = $module -replace '\.asm$', '.obj'
            $objs += $obj
            Write-Success "  ✓ $obj"
        }

        # Link step
        Write-Info "[3/4] Linking modules..."
        
        $linkCmd = @(
            $objs
            $LIBS
            "/subsystem:windows"
            "/entry:main"
            "/out:$OUTFILE"
            "/nologo"
        )
        
        $output = & $LINK $linkCmd 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error "  ✗ Linking failed"
            Write-Error $output
            throw "Link error"
        }
        
        Write-Success "  ✓ Linking complete"

        # Verification step
        Write-Info "[4/4] Verifying output..."
        
        if (Test-Path $OUTFILE) {
            $size = (Get-Item $OUTFILE).Length / 1KB
            Write-Success "  ✓ $OUTFILE created (${size}KB)"
        }
        else {
            throw "Output file not created"
        }

        Write-Success ""
        Write-Success "Build SUCCESS!"
        Write-Success "Output: $OUTDIR\$OUTFILE"
        Write-Info ""
        Write-Info "To run the editor:"
        Write-Info "  .\$OUTFILE"
        
        return $true
    }
    catch {
        Write-Error ""
        Write-Error "Build FAILED: $_"
        return $false
    }
    finally {
        Pop-Location
    }
}

# Execute build
$success = Build-MASM
exit if $success { 0 } else { 1 }
```

#### Run Build Script

```powershell
# Set execution policy if needed
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process

# Standard build
.\Build_MASM_Editor.ps1

# Clean rebuild
.\Build_MASM_Editor.ps1 -Rebuild

# Debug with output
.\Build_MASM_Editor.ps1 -Configuration Debug
```

---

### Option 3: Batch File Build

Save as `Build_MASM_Editor.bat`:

```batch
@echo off
setlocal enabledelayedexpansion

echo ================================
echo RawrXD MASM Editor Build
echo ================================

cd d:\rawrxd

if "%1"=="clean" (
    echo Cleaning...
    del *.obj *.exe *.lib *.exp 2>nul
    goto :end
)

echo [1/4] Assembling modules...
ml64.exe RawrXD_MASM_SyntaxHighlighter.asm /c
if errorlevel 1 goto :error

ml64.exe RawrXD_MASM_Editor_Editing.asm /c
if errorlevel 1 goto :error

ml64.exe RawrXD_MASM_Editor_MLCompletion.asm /c
if errorlevel 1 goto :error

echo [2/4] Linking modules...
link.exe RawrXD_MASM_SyntaxHighlighter.obj ^
         RawrXD_MASM_Editor_Editing.obj ^
         RawrXD_MASM_Editor_MLCompletion.obj ^
         kernel32.lib user32.lib gdi32.lib ^
         /subsystem:windows /entry:main ^
         /out:RawrXD_MASM_Editor.exe

if errorlevel 1 goto :error

echo [3/4] Verifying...
if exist RawrXD_MASM_Editor.exe (
    echo.
    echo ================================
    echo Build SUCCESS!
    echo Output: RawrXD_MASM_Editor.exe
    echo ================================
    echo.
    echo Run: RawrXD_MASM_Editor.exe
    goto :end
)

:error
echo.
echo ================================
echo Build FAILED!
echo ================================
exit /b 1

:end
exit /b 0
```

Run with:
```cmd
Build_MASM_Editor.bat
```

---

## Debugging Assembly

### Common Errors & Fixes

#### Error: "ml64.exe not found"

```powershell
# Find ml64.exe
Get-Item "C:\Program Files*\*\VC\Tools\MASM\bin\ml64.exe" -Recurse

# Add to PATH
$env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MASM\bin"
ml64.exe /?
```

#### Error: "kernel32.lib not found"

```powershell
# Find Windows SDK libs
Get-Item "C:\Program Files*\Windows Kits\*\Lib\*\um\x64\kernel32.lib" -Recurse

# Add to LINK environment
$env:LIB += ";C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64"
```

#### Error: "Unmatched block nesting"

- Check for unbalanced PROC/ENDP
- Check for unbalanced braces in macros
- Verify line continuation characters (^)

#### Error: "Invalid instruction operand"

- Verify register sizes match (rax vs eax)
- Check memory access syntax: `[rax + 8]` not `[rax+8]`
- Verify 64-bit registers (not 32-bit in x64 context)

#### Error: "Symbol already defined"

- Check for duplicate procedure names
- Verify include guards in macro files
- Check for duplicate data labels

### Assembly Verification

```asm
; Verify instruction syntax
mov rax, rbx        ; ✓ Valid (64-bit)
mov eax, ebx        ; ⚠ Valid but sign-extends to rax
mov al, bl          ; ✓ Valid (8-bit)

; Verify memory operations
mov rax, [rdi]      ; ✓ Valid (64-byte access)
mov rax, [rdi + 8]  ; ✓ Valid (offset)
mov rax, [rdi*2]    ; ⚠ Invalid scale (only 1,2,4,8)
mov rax, [rdi*4]    ; ✓ Valid
mov rax, [rax + rdi*4 + 16]  ; ✓ Valid (complex addressing)

; Verify procedure frames
Proc:
    .pushreg rbx            ; ✓ Register preservation
    .allocstack 32          ; ✓ Local stack space
    .endprolog              ; ✓ End function prologue
    ; Function body...
    ret
Proc endp
```

---

## Testing the Build

### Run Executable

```cmd
d:\rawrxd\RawrXD_MASM_Editor.exe
```

Window should appear with:
- Dark gray background
- Blue title bar "RawrXD MASM Editor"
- Cursor blinking in editor area
- Line numbers on left

### Test Features

#### 1. Text Editing
```
1. Type some text
2. Press arrows to move cursor
3. Press Backspace to delete
4. Press Enter for new line
```

#### 2. Syntax Highlighting
```
1. Type: mov rax, rbx
2. Properties should highlight:
   - "mov" in orange (instruction)
   - "rax" in blue (register)
   - "rbx" in blue (register)
   - "," in white (operator)
```

#### 3. Code Completion (if Ollama running)
```
1. Type: m
2. Press Ctrl+Space
3. Popup should appear with suggestions:
   - "mov rax, rbx"
   - "mul rax, rdx"
   - "mov rsi, rdi"
4. Use arrows to select, Enter to insert
```

#### 4. Error Detection
```
1. Type: moov rax, rbx    (typo)
2. Should highlight red (error)
3. Hover/press Ctrl+. for fix suggestion
4. Accept "mov rax, rbx" (corrected)
```

---

## Release Build

### Optimization Flags

```cmd
: Optimize for speed (smaller executable)
link.exe ... /opt:ref /opt:icf

: Optimize for size
link.exe ... /opt:ref

: Debug symbols (for debugging)
link.exe ... /debug
```

### Strip Debug Info

```cmd
: Production build (no debug symbols)
ml64.exe RawrXD_MASM_SyntaxHighlighter.asm /c /Zi:none
link.exe ... /release
```

### Code Signing (Optional)

```powershell
: Sign executable with certificate
signtool.exe sign /f certificate.pfx /p password /d "RawrXD MASM Editor" RawrXD_MASM_Editor.exe
```

---

## Continuous Integration / Automated Testing

### GitHub Actions Example

```yaml
name: Build MASM Editor

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup dev environment
      uses: ilammy/msvc-dev-cmd@v1
    
    - name: Build MASM modules
      run: |
        cd d:\rawrxd
        ml64.exe RawrXD_MASM_SyntaxHighlighter.asm /c
        ml64.exe RawrXD_MASM_Editor_Editing.asm /c
        ml64.exe RawrXD_MASM_Editor_MLCompletion.asm /c
    
    - name: Link executable
      run: |
        link.exe RawrXD_MASM_SyntaxHighlighter.obj ^
                 RawrXD_MASM_Editor_Editing.obj ^
                 RawrXD_MASM_Editor_MLCompletion.obj ^
                 kernel32.lib user32.lib gdi32.lib ^
                 /subsystem:windows /entry:main
    
    - name: Archive binary
      uses: actions/upload-artifact@v2
      with:
        name: RawrXD_MASM_Editor
        path: d:\rawrxd\RawrXD_MASM_Editor.exe
```

---

## Performance Profiling

### Measure Build Time

```powershell
$elapsed = Measure-Command {
    & .\Build_MASM_Editor.ps1
}

Write-Host "Total build time: $($elapsed.TotalSeconds) seconds"
```

**Expected times:**
- Assembly (each module): 0.5-2 seconds
- Linking: 1-3 seconds
- Total: 3-10 seconds

### Executable Size Analysis

```powershell
$size = (Get-Item "RawrXD_MASM_Editor.exe").Length
Write-Host "Executable size: $($size / 1KB)KB"

: Typical:
: - Base executable: 120KB
: - With debug symbols: 200KB
: - Stripped: 100KB
```

---

## Troubleshooting Checklist

- [ ] ml64.exe in PATH
- [ ] link.exe in PATH  
- [ ] kernel32.lib accessible
- [ ] user32.lib accessible
- [ ] gdi32.lib accessible
- [ ] All .asm files in d:\rawrxd\
- [ ] No syntax errors in .asm files
- [ ] Correct subsystem (/subsystem:windows)
- [ ] Correct entry point (/entry:main)
- [ ] All object files generated
- [ ] Final .exe produced

---

## Build Configuration Summary

```
Configuration:      Release
Target Platform:    Windows x64
Assembler:          MASM64 (ml64.exe)
Linker:             Microsoft Link (link.exe)
Subsystem:          Windows (GUI)
Entry Point:        main
Output File:        RawrXD_MASM_Editor.exe
Expected Size:      120-200KB
Dependencies:       kernel32.lib, user32.lib, gdi32.lib
```

---

**Status: ✅ Ready to Build**

Execute: `.\Build_MASM_Editor.ps1` or `Build_MASM_Editor.bat`

Expected output: `RawrXD_MASM_Editor.exe` (~150KB)

