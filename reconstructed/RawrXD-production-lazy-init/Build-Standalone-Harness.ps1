# Build-Standalone-Harness.ps1
# ============================================================================
# Build script for Pure MASM Standalone Harness
# ============================================================================
# Creates a fully self-contained executable with zero C++ dependencies
# ============================================================================

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    
    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Colors
$ColorInfo = "Cyan"
$ColorSuccess = "Green"
$ColorError = "Red"
$ColorWarning = "Yellow"

# Paths
$MasmDir = "masm"
$BuildDir = "build\standalone_$Configuration"
$OutputExe = "$BuildDir\RawrXD-Standalone-Harness.exe"

# MASM files in dependency order
$MasmModules = @(
    "config_manager.asm",
    "ollama_client_masm.asm",
    "model_discovery_ui.asm",
    "performance_baseline.asm",
    "integration_test_framework.asm",
    "smoke_test_suite.asm",
    "masm_standalone_harness.asm"
)

# Compiler/Linker paths
$ML64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

# SDK paths
$SDKLib = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

function Write-Info {
    param([string]$Message)
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $ColorInfo
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor $ColorSuccess
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor $ColorError
}

function Write-Warning-Custom {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor $ColorWarning
}

# Display banner
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                                                                ║" -ForegroundColor Magenta
Write-Host "║   PURE MASM STANDALONE HARNESS BUILD SYSTEM                   ║" -ForegroundColor Magenta
Write-Host "║   Zero C++ Dependencies • 100% Assembly • Self-Contained      ║" -ForegroundColor Magenta
Write-Host "║                                                                ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

Write-Info "Configuration: $Configuration"
Write-Info "Build Directory: $BuildDir"
Write-Info "Output: $OutputExe"
Write-Host ""

# Verify tools exist
Write-Info "Verifying build tools..."

if (-not (Test-Path $ML64)) {
    Write-Error-Custom "ml64.exe not found at: $ML64"
    exit 1
}

if (-not (Test-Path $LINK)) {
    Write-Error-Custom "link.exe not found at: $LINK"
    exit 1
}

Write-Success "Build tools verified"

# Create build directory
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Info "Cleaning build directory..."
    Remove-Item -Path $BuildDir -Recurse -Force
}

if (-not (Test-Path $BuildDir)) {
    Write-Info "Creating build directory..."
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Compile MASM modules
Write-Host ""
Write-Info "Compiling MASM modules..."
Write-Host ""

$ObjectFiles = @()
$CompileSuccess = $true

foreach ($AsmFile in $MasmModules) {
    $SourcePath = Join-Path $MasmDir $AsmFile
    $ObjName = [System.IO.Path]::GetFileNameWithoutExtension($AsmFile) + ".obj"
    $ObjPath = Join-Path $BuildDir $ObjName
    
    Write-Info "[$($MasmModules.IndexOf($AsmFile) + 1)/$($MasmModules.Count)] Compiling $AsmFile..."
    
    if (-not (Test-Path $SourcePath)) {
        Write-Error-Custom "Source file not found: $SourcePath"
        $CompileSuccess = $false
        continue
    }
    
    # Assemble
    $MasmArgs = @(
        "/c",                    # Compile only
        "/Fo$ObjPath",           # Output file
        "/W3",                   # Warning level 3
        "/nologo",               # No banner
        "/Zi"                    # Debug info
    )
    
    if ($Configuration -eq "Debug") {
        $MasmArgs += "/D_DEBUG"
    }
    
    $MasmArgs += $SourcePath
    
    if ($Verbose) {
        Write-Host "  Command: $ML64 $($MasmArgs -join ' ')" -ForegroundColor DarkGray
    }
    
    $Output = & $ML64 $MasmArgs 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error-Custom "Compilation failed: $AsmFile"
        Write-Host $Output -ForegroundColor Red
        $CompileSuccess = $false
        continue
    }
    
    if (Test-Path $ObjPath) {
        $Size = (Get-Item $ObjPath).Length
        Write-Success "Compiled $AsmFile -> $ObjName ($Size bytes)"
        $ObjectFiles += $ObjPath
    } else {
        Write-Error-Custom "Object file not created: $ObjPath"
        $CompileSuccess = $false
    }
}

if (-not $CompileSuccess) {
    Write-Host ""
    Write-Error-Custom "Compilation failed. Build aborted."
    exit 1
}

# Link executable
Write-Host ""
Write-Info "Linking standalone executable..."

$LinkArgs = @(
    "/OUT:$OutputExe",
    "/SUBSYSTEM:CONSOLE",
    "/ENTRY:mainCRTStartup",
    "/MACHINE:X64",
    "/NOLOGO"
)

if ($Configuration -eq "Debug") {
    $LinkArgs += "/DEBUG"
}

# Add libraries
$LinkArgs += "/LIBPATH:$SDKLib"
$LinkArgs += "kernel32.lib"
$LinkArgs += "user32.lib"
$LinkArgs += "winhttp.lib"

# Add object files
$LinkArgs += $ObjectFiles

if ($Verbose) {
    Write-Host "  Command: $LINK $($LinkArgs -join ' ')" -ForegroundColor DarkGray
}

$LinkOutput = & $LINK $LinkArgs 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Error-Custom "Linking failed!"
    Write-Host ""
    Write-Host "Link output:" -ForegroundColor Yellow
    Write-Host $LinkOutput -ForegroundColor Red
    Write-Host ""
    
    # Analyze unresolved externals
    $UnresolvedSymbols = $LinkOutput | Select-String "unresolved external symbol" | ForEach-Object {
        if ($_ -match "symbol (\S+)") {
            $matches[1]
        }
    } | Sort-Object -Unique
    
    if ($UnresolvedSymbols.Count -gt 0) {
        Write-Host "Unresolved external symbols ($($UnresolvedSymbols.Count)):" -ForegroundColor Yellow
        $UnresolvedSymbols | ForEach-Object {
            Write-Host "  • $_" -ForegroundColor Red
        }
        Write-Host ""
        Write-Warning-Custom "These symbols need mock implementations or library linking"
    }
    
    exit 1
}

Write-Host ""
Write-Success "Linking successful!"

if (Test-Path $OutputExe) {
    $ExeSize = (Get-Item $OutputExe).Length
    $ExeSizeKB = [math]::Round($ExeSize / 1KB, 2)
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║                    BUILD SUCCESSFUL! ✅                        ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output: $OutputExe" -ForegroundColor Cyan
    Write-Host "Size:   $ExeSizeKB KB" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "To run the harness:" -ForegroundColor Yellow
    Write-Host "  $OutputExe" -ForegroundColor White
    Write-Host ""
} else {
    Write-Error-Custom "Executable not created!"
    exit 1
}

exit 0
