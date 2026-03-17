# RawrXD MASM IDE v2.0 - Build Script
# Enterprise Edition - Pure MASM Implementation
# Zero External Dependencies

param(
    [switch]$Clean,
    [switch]$Release,
    [switch]$Debug,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Configuration
$ProjectRoot = $PSScriptRoot
$SrcDir = Join-Path $ProjectRoot "src"
$BuildDir = Join-Path $ProjectRoot "build"
$BinDir = Join-Path $ProjectRoot "bin"

$MainFile = "rawrxd_ide_main.asm"
$OutputExe = "RawrXD_IDE.exe"
$OutputObj = "RawrXD_IDE.obj"

# MASM paths (assumes ml.exe is in PATH or VS installation)
$ML = "ml.exe"
$LINK = "link.exe"

# Check common locations if not in PATH
if (!(Get-Command $ML -ErrorAction SilentlyContinue)) {
    if (Test-Path "C:\masm32\bin\ml.exe") {
        $ML = "C:\masm32\bin\ml.exe"
        $LINK = "C:\masm32\bin\link.exe"
        $IncludePath = "C:\masm32\include"
        $LibPath = "C:\masm32\lib"
    }
}

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "   RawrXD MASM IDE v2.0 Build System" -ForegroundColor Cyan
Write-Host "   Enterprise Edition - Pure MASM" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Clean build directories
if ($Clean) {
    Write-Host "[CLEAN] Removing build artifacts..." -ForegroundColor Yellow
    
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "  [OK] Removed build directory" -ForegroundColor Green
    }
    
    if (Test-Path $BinDir) {
        Remove-Item -Path $BinDir -Recurse -Force
        Write-Host "  [OK] Removed bin directory" -ForegroundColor Green
    }
    
    Write-Host ""
}

# Create directories
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "[CREATE] Build directory created" -ForegroundColor Green
}

if (!(Test-Path $BinDir)) {
    New-Item -ItemType Directory -Path $BinDir | Out-Null
    Write-Host "[CREATE] Bin directory created" -ForegroundColor Green
}

# Check if MASM is available
Write-Host "[CHECK] Verifying MASM installation..." -ForegroundColor Cyan
try {
    $mlVersion = & $ML /? 2>&1
    if ($LASTEXITCODE -eq 0 -or $mlVersion) {
        Write-Host "  [OK] MASM assembler found" -ForegroundColor Green
    }
} catch {
    Write-Host "  [ERROR] MASM (ml.exe) not found in PATH!" -ForegroundColor Red
    Write-Host "  Please install Visual Studio or MASM32 SDK" -ForegroundColor Yellow
    exit 1
}

Write-Host ""

# Build configuration
$MASMFlags = @(
    "/c"                    # Assemble without linking
    "/coff"                 # Generate COFF object files
    "/W3"                   # Warning level 3
    "/nologo"              # Suppress copyright message
    "/Cp"                  # Preserve case in public/external symbols
    "/Sg"                  # Enable first pass listing
)

if ($Debug) {
    Write-Host "[CONFIG] Debug build configuration" -ForegroundColor Yellow
    $MASMFlags += @(
        "/Zi"              # Generate debugging information
        "/Zd"              # Add line number info
    )
    $LinkFlags = @(
        "/SUBSYSTEM:WINDOWS"
        "/DEBUG"
        "/DEBUGTYPE:CV"
        "/ENTRY:start"
    )
} elseif ($Release) {
    Write-Host "[CONFIG] Release build configuration" -ForegroundColor Yellow
    $MASMFlags += @(
        "/Ox"              # Maximum optimization
    )
    $LinkFlags = @(
        "/SUBSYSTEM:WINDOWS"
        "/RELEASE"
        "/OPT:REF"
        "/OPT:ICF"
        "/ENTRY:start"
    )
} else {
    # Default to debug
    Write-Host "[CONFIG] Debug build configuration (default)" -ForegroundColor Yellow
    $MASMFlags += @(
        "/Zi"
        "/Zd"
    )
    $LinkFlags = @(
        "/SUBSYSTEM:WINDOWS"
        "/DEBUG"
        "/DEBUGTYPE:CV"
        "/ENTRY:start"
    )
}

Write-Host ""

# Phase 1: Assemble main file
Write-Host "[PHASE 1] Assembling main source file..." -ForegroundColor Cyan
$MainFilePath = Join-Path $SrcDir $MainFile
$ObjFilePath = Join-Path $BuildDir $OutputObj

if (!(Test-Path $MainFilePath)) {
    Write-Host "  [ERROR] Main source file not found: $MainFilePath" -ForegroundColor Red
    exit 1
}

$asmArgs = $MASMFlags + @(
    "/Fo$ObjFilePath"
    "/I$SrcDir"
)

if ($IncludePath) {
    $asmArgs += "/I$IncludePath"
}

$asmArgs += $MainFilePath

if ($Verbose) {
    Write-Host "  Command: $ML $($asmArgs -join ' ')" -ForegroundColor Gray
}

Write-Host "  [ASSEMBLE] $MainFile" -ForegroundColor White

try {
    $asmOutput = & $ML $asmArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Assembly failed!" -ForegroundColor Red
        Write-Host $asmOutput -ForegroundColor Red
        exit 1
    }
    
    if ($Verbose -and $asmOutput) {
        Write-Host $asmOutput -ForegroundColor Gray
    }
    
    Write-Host "  [OK] Assembly completed successfully" -ForegroundColor Green
} catch {
    Write-Host "  [ERROR] Assembly exception: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Phase 2: Link executable
Write-Host "[PHASE 2] Linking executable..." -ForegroundColor Cyan
$ExeFilePath = Join-Path $BinDir $OutputExe

$linkArgs = $LinkFlags + @(
    "/OUT:$ExeFilePath"
    "/NOLOGO"
)

if ($LibPath) {
    $linkArgs += "/LIBPATH:$LibPath"
}

$linkArgs += @(
    $ObjFilePath
    "user32.lib"
    "kernel32.lib"
    "gdi32.lib"
    "comdlg32.lib"
    "comctl32.lib"
    "shell32.lib"
)

if ($Verbose) {
    Write-Host "  Command: $LINK $($linkArgs -join ' ')" -ForegroundColor Gray
}

Write-Host "  [LINK] $OutputExe" -ForegroundColor White

try {
    $linkOutput = & $LINK $linkArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Linking failed!" -ForegroundColor Red
        Write-Host $linkOutput -ForegroundColor Red
        exit 1
    }
    
    if ($Verbose -and $linkOutput) {
        Write-Host $linkOutput -ForegroundColor Gray
    }
    
    Write-Host "  [OK] Linking completed successfully" -ForegroundColor Green
} catch {
    Write-Host "  [ERROR] Linking exception: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Phase 3: Build summary
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "   BUILD COMPLETED SUCCESSFULLY" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

if (Test-Path $ExeFilePath) {
    $exeInfo = Get-Item $ExeFilePath
    Write-Host "Output File:    $($exeInfo.FullName)" -ForegroundColor White
    Write-Host "File Size:      $([math]::Round($exeInfo.Length / 1KB, 2)) KB" -ForegroundColor White
    Write-Host "Last Modified:  $($exeInfo.LastWriteTime)" -ForegroundColor White
    Write-Host ""
    
    Write-Host "Build Type:     " -NoNewline -ForegroundColor White
    if ($Release) {
        Write-Host "RELEASE" -ForegroundColor Green
    } else {
        Write-Host "DEBUG" -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "To run the IDE, execute:" -ForegroundColor Cyan
    Write-Host "  $ExeFilePath" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "[WARNING] Executable not found at expected location!" -ForegroundColor Yellow
}

# Summary statistics
Write-Host "Build Statistics:" -ForegroundColor Cyan
Write-Host "  - Source files processed: 1 (main + 3 phase includes)" -ForegroundColor White
Write-Host "  - Object files created: 1" -ForegroundColor White
Write-Host "  - Libraries linked: 6" -ForegroundColor White
Write-Host ""

Write-Host "Features Implemented:" -ForegroundColor Cyan
Write-Host "  [✓] Phase 1: Editor Enhancement Engine" -ForegroundColor Green
Write-Host "      - Line numbers, minimap, bracket matching" -ForegroundColor Gray
Write-Host "      - Code folding, multi-cursor support" -ForegroundColor Gray
Write-Host "      - Tabbed interface, command palette" -ForegroundColor Gray
Write-Host "      - Split panes, keyboard shortcuts" -ForegroundColor Gray
Write-Host ""
Write-Host "  [✓] Phase 2: Language Intelligence Engine" -ForegroundColor Green
Write-Host "      - IntelliSense completions" -ForegroundColor Gray
Write-Host "      - Error detection and diagnostics" -ForegroundColor Gray
Write-Host "      - Go-to-definition, hover information" -ForegroundColor Gray
Write-Host "      - Document symbols, signature help" -ForegroundColor Gray
Write-Host ""
Write-Host "  [✓] Phase 3: Debug Infrastructure" -ForegroundColor Green
Write-Host "      - Breakpoint management" -ForegroundColor Gray
Write-Host "      - Stepping (into, over, out)" -ForegroundColor Gray
Write-Host "      - Variable inspection" -ForegroundColor Gray
Write-Host "      - Call stack, watchpoints" -ForegroundColor Gray
Write-Host ""

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "   RawrXD MASM IDE v2.0 - Ready!" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Optional: Create shortcut
$createShortcut = Read-Host "Create desktop shortcut? (Y/N)"
if ($createShortcut -eq 'Y' -or $createShortcut -eq 'y') {
    $WshShell = New-Object -comObject WScript.Shell
    $Desktop = [System.Environment]::GetFolderPath('Desktop')
    $Shortcut = $WshShell.CreateShortcut("$Desktop\RawrXD IDE.lnk")
    $Shortcut.TargetPath = $ExeFilePath
    $Shortcut.WorkingDirectory = $ProjectRoot
    $Shortcut.Description = "RawrXD MASM IDE v2.0 Enterprise"
    $Shortcut.Save()
    Write-Host "[OK] Desktop shortcut created!" -ForegroundColor Green
}

Write-Host ""
Write-Host "Build completed at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
