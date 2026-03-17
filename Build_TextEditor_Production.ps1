#!/usr/bin/env pwsh
# ============================================================================
# Build_TextEditor_Complete.ps1
# Unified build system for text editor with rendering and syntax highlighting
# ============================================================================

param(
    [string]$Configuration = "Release",
    [switch]$Rebuild = $false,
    [switch]$Run = $false
)

# Configuration
$MASM64_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MASM\bin\x64\ml64.exe"
$LINK_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe"
$LIB_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\lib\x64"

$BUILD_DIR = ".\build\text-editor-complete"
$OUTPUT_EXE = "$BUILD_DIR\RawrXD_TextEditor.exe"

# Source files - in dependency order
$SOURCE_FILES = @(
    "RawrXD_TextBuffer.asm",
    "RawrXD_CursorTracker.asm",
    "RawrXD_TextEditor_Renderer.asm",
    "RawrXD_TextEditor_SyntaxHighlighter.asm",
    "RawrXD_TextEditorGUI.asm",
    "RawrXD_TextEditor_DisplayIntegration.asm",
    "RawrXD_TextEditor_Integration.asm",
    "RawrXD_TextEditor_Main.asm"
)

# Colors
$GREEN = "`e[32m"
$RED = "`e[31m"
$YELLOW = "`e[33m"
$RESET = "`e[0m"

function Write-Status {
    param([string]$Message, [string]$Color = $GREEN)
    Write-Host "$Color[BUILD]$RESET $Message"
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "$RED[ERROR]$RESET $Message"
}

function Verify-Tools {
    Write-Status "Verifying build tools..."
    
    if (-not (Test-Path $MASM64_PATH)) {
        Write-Error-Custom "ml64.exe not found: $MASM64_PATH"
        exit 1
    }
    
    if (-not (Test-Path $LINK_PATH)) {
        Write-Error-Custom "link.exe not found: $LINK_PATH"
        exit 1
    }
    
    Write-Status "Build tools verified" $GREEN
}

function Create-BuildDirectory {
    Write-Status "Creating build directory: $BUILD_DIR"
    
    if ($Rebuild -and (Test-Path $BUILD_DIR)) {
        Write-Status "Cleaning previous build..."
        Remove-Item $BUILD_DIR -Recurse -Force
    }
    
    if (-not (Test-Path $BUILD_DIR)) {
        New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
    }
    
    Write-Status "Build directory ready"
}

function Compile-Assembly {
    param([string]$SourceFile)
    
    $ObjectFile = "$BUILD_DIR\$([System.IO.Path]::GetFileNameWithoutExtension($SourceFile)).obj"
    
    Write-Host -NoNewline "$YELLOW[COMPILING]$RESET $SourceFile ... "
    
    # Run ml64.exe
    & $MASM64_PATH /Fo $ObjectFile /W3 /nologo $SourceFile 2>&1 | ForEach-Object {
        if ($_ -like "*error*") {
            Write-Host "`n$RED$_$RESET"
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error-Custom "Compilation failed for $SourceFile (exit code: $LASTEXITCODE)"
        return $false
    }
    
    if (-not (Test-Path $ObjectFile)) {
        Write-Error-Custom "Object file not created: $ObjectFile"
        return $false
    }
    
    Write-Host "$GREEN✓$RESET"
    return $true
}

function Compile-All {
    Write-Status "Starting compilation phase..." $YELLOW
    
    $ObjFiles = @()
    $FailedFiles = @()
    
    foreach ($SourceFile in $SOURCE_FILES) {
        if (Test-Path $SourceFile) {
            if (Compile-Assembly $SourceFile) {
                $ObjFiles += "$BUILD_DIR\$([System.IO.Path]::GetFileNameWithoutExtension($SourceFile)).obj"
            } else {
                $FailedFiles += $SourceFile
            }
        } else {
            Write-Error-Custom "Source file not found: $SourceFile"
            $FailedFiles += $SourceFile
        }
    }
    
    if ($FailedFiles.Count -gt 0) {
        Write-Error-Custom "Failed to compile $($FailedFiles.Count) file(s):"
        $FailedFiles | ForEach-Object { Write-Host "  - $_" }
        return $null
    }
    
    Write-Status "All modules compiled successfully" $GREEN
    return $ObjFiles
}

function Link-Executable {
    param([string[]]$ObjectFiles)
    
    Write-Status "Linking executable..." $YELLOW
    
    # Build response file for linker
    $RSP_FILE = "$BUILD_DIR\link.rsp"
    
    $LinkResponse = @()
    foreach ($ObjFile in $ObjectFiles) {
        $LinkResponse += $ObjFile
    }
    
    # Add Windows API libraries
    $LinkResponse += "kernel32.lib"
    $LinkResponse += "user32.lib"
    $LinkResponse += "gdi32.lib"
    $LinkResponse += "comctl32.lib"
    
    # Output directive
    $LinkResponse += "/OUT:$OUTPUT_EXE"
    $LinkResponse += "/SUBSYSTEM:WINDOWS"
    $LinkResponse += "/MACHINE:X64"
    $LinkResponse += "/NOLOGO"
    $LinkResponse += "/LIBPATH:$LIB_PATH"
    
    # Write response file
    $LinkResponse | Out-File -Encoding ASCII $RSP_FILE
    
    # Run linker
    Write-Host -NoNewline "$YELLOW[LINKING]$RESET Building executable ... "
    
    & $LINK_PATH @($RSP_FILE) 2>&1 | ForEach-Object {
        if ($_ -like "*error*" -or $_ -like "*warning*") {
            Write-Host "`n$RED$_$RESET"
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error-Custom "Linking failed (exit code: $LASTEXITCODE)"
        return $false
    }
    
    if (-not (Test-Path $OUTPUT_EXE)) {
        Write-Error-Custom "Executable not created: $OUTPUT_EXE"
        return $false
    }
    
    Write-Host "$GREEN✓$RESET"
    Write-Status "Executable created: $OUTPUT_EXE" $GREEN
    
    # Show file size
    $FileSize = (Get-Item $OUTPUT_EXE).Length
    $FileSizeMB = [math]::Round($FileSize / 1MB, 2)
    Write-Status "Size: $FileSize bytes ($FileSizeMB MB)"
    
    return $true
}

function Run-Executable {
    Write-Status "Launching editor..." $YELLOW
    
    try {
        & $OUTPUT_EXE
    } catch {
        Write-Error-Custom "Failed to run executable: $_"
    }
}

function Show-Summary {
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════╗"
    Write-Host "║         TEXT EDITOR BUILD COMPLETE                         ║"
    Write-Host "╠════════════════════════════════════════════════════════════╣"
    Write-Host "║ Status:           $GREEN✓ SUCCESS$RESET"
    Write-Host "║ Output:           $OUTPUT_EXE"
    Write-Host "║ Configuration:    $Configuration"
    Write-Host "║ Modules:          $($SOURCE_FILES.Count) files"
    Write-Host "║                                                            ║"
    Write-Host "║ Features:                                                  ║"
    Write-Host "║  - Full text editing with insert/delete/replace          ║"
    Write-Host "║  - MASM syntax highlighting                              ║"
    Write-Host "║  - Line numbers and margins                              ║"
    Write-Host "║  - Cursor positioning and blinking                       ║"
    Write-Host "║  - 60+ FPS rendering with double buffering               ║"
    Write-Host "║  - Win32 native GUI with keyboard/mouse support          ║"
    Write-Host "╚════════════════════════════════════════════════════════════╝"
    Write-Host ""
}

# Main build process
Write-Host ""
Write-Status "═══════════════════════════════════════════════════════════" $YELLOW
Write-Status "  RawrXD Text Editor - Complete Build System" $YELLOW
Write-Status "═══════════════════════════════════════════════════════════" $YELLOW
Write-Host ""

# Execute build steps
Verify-Tools
Create-BuildDirectory
$ObjectFiles = Compile-All

if ($ObjectFiles) {
    if (Link-Executable $ObjectFiles) {
        Show-Summary
        
        if ($Run) {
            Write-Status "Executing with --Run flag..."
            Run-Executable
        }
        
        exit 0
    }
}

Write-Error-Custom "Build failed. Check compilation errors above."
exit 1
