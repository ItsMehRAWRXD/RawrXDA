# ===========================================================================
# BUILD_RELEASE.PS1 - Complete Build Script for RawrXD MASM IDE
# Compiles all modules and links into RawrXD.exe
# ===========================================================================

param(
    [switch]$Clean = $false,
    [switch]$Verbose = $false,
    [switch]$DebugBuild = $false
)

$ErrorActionPreference = "Continue"
$ProgressPreference = "SilentlyContinue"

# Paths
$ProjectRoot = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide"
$SrcDir = Join-Path $ProjectRoot "src"
$BuildDir = Join-Path $ProjectRoot "build"

$MasmPath = "C:\masm32\bin\ml.exe"
$LinkPath = "C:\masm32\bin\link.exe"

# Build configuration
$AsmFlags = "/c /coff /Cp"
if ($DebugBuild) {
    $AsmFlags += " /Zi /Zd"
}

$LinkFlags = "/SUBSYSTEM:WINDOWS /ENTRY:WinMain /RELEASE /OPT:REF /OPT:ICF /LIBPATH:`"C:\masm32\lib`""
if ($DebugBuild) {
    $LinkFlags += " /DEBUG"
}

# Core libraries
$SystemLibs = @(
    "kernel32.lib"
    "user32.lib"
    "gdi32.lib"
    "comctl32.lib"
    "comdlg32.lib"
    "shell32.lib"
    "ole32.lib"
    "oleaut32.lib"
    "advapi32.lib"
)

# Source files in link order (dependencies first)
$SourceFiles = @(
    "fileDialogs_init.asm"
    "ideSettings.asm"
    "inferenceBackend_init.asm"
    "uiGguf_stubs.asm"
    "paneSystem.asm"
    "editor_impl.asm"
    "fileDialog_impl.asm"
    "findReplace_dialogs.asm"
    "dialogs.asm"
    "find_replace.asm"
    "gguf_unified_loader.asm"
    "gguf_disk_streaming.asm"
    "piram_hooks.asm"
    "reverse_quant.asm"
    "agent_system_core.asm"
    "model_hotpatch_engine.asm"
    "agentic_ide_full_control.asm"
    "ide_master_stubs.asm"
    "main_complete.asm"
)

# ===========================================================================
# Functions
# ===========================================================================

function Write-Status {
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $Color
}

function Write-Error-Status {
    param([string]$Message)
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] ERROR: $Message" -ForegroundColor Red
}

function Write-Success {
    param([string]$Message)
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] ✓ $Message" -ForegroundColor Green
}

function Test-Prerequisites {
    Write-Status "Checking prerequisites..."
    
    if (-not (Test-Path $MasmPath)) {
        Write-Error-Status "MASM assembler not found at $MasmPath"
        return $false
    }
    
    if (-not (Test-Path $LinkPath)) {
        Write-Error-Status "Linker not found at $LinkPath"
        return $false
    }
    
    Write-Success "Prerequisites OK"
    return $true
}

function Initialize-BuildEnvironment {
    Write-Status "Initializing build environment..."
    
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    
    if ($Clean) {
        Remove-Item "$BuildDir\*.obj" -ErrorAction SilentlyContinue
        Remove-Item "$BuildDir\*.exe" -ErrorAction SilentlyContinue
    }
    
    Write-Success "Build environment ready"
}

function Compile-Module {
    param([string]$Module)
    
    $SourcePath = Join-Path $SrcDir $Module
    $OutputObj = Join-Path $BuildDir $Module.Replace(".asm", ".obj")
    
    if (-not (Test-Path $SourcePath)) {
        Write-Host "  [$count/$($SourceFiles.Count)] SKIP: $Module (not found)"
        return $null
    }
    
    Write-Host "  [$count/$($SourceFiles.Count)] Compiling: $Module"
    
    $output = & $MasmPath /c /coff /Cp /I"$SrcDir" /Fo"$OutputObj" "$SourcePath" 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    → $($Module.Replace('.asm', '.obj'))"
        return $OutputObj
    } else {
        Write-Error-Status "$Module compilation failed"
        $output | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
        return $null
    }
}

# ===========================================================================
# Main Build Execution
# ===========================================================================

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗"
Write-Host "║               RawrXD MASM IDE - Build System                      ║"
Write-Host "║                                                                   ║"
Write-Host "║  Complete Assembly Build Pipeline                                 ║"
Write-Host "╚═══════════════════════════════════════════════════════════════════╝"
Write-Host ""

if (-not (Test-Prerequisites)) {
    exit 1
}

Initialize-BuildEnvironment

Write-Status "Compiling source files..."
Write-Host ""

$ObjectFiles = @()
$count = 1
foreach ($module in $SourceFiles) {
    $obj = Compile-Module $module
    if ($obj) {
        $ObjectFiles += $obj
    }
    $count++
}

Write-Host ""
Write-Status "Linking executable..."

$exePath = Join-Path $BuildDir "RawrXD.exe"
$linkArgs = @(
    "/OUT:$exePath",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:WinMain",
    "/RELEASE",
    "/OPT:REF",
    "/OPT:ICF",
    "/LIBPATH:C:\masm32\lib"
)
$linkArgs += $ObjectFiles
$linkArgs += $SystemLibs

$output = & $LinkPath $linkArgs 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Error-Status "Linking failed"
    $output | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

Write-Success "Build complete: $exePath"
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host "  BUILD SUMMARY"
Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host "  Source Files: $($SourceFiles.Count)"
Write-Host "  Compiled: $($ObjectFiles.Count)"
Write-Host "  Link Status: SUCCESS ✓"
Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host ""
