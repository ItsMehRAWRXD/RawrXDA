# Build-Extended-Harness.ps1
# ============================================================================
# Build script for Extended Pure MASM Harness with Real Components
# ============================================================================
# Includes Phase 3 (Configuration) and Phase 4 (Error Handling)
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
$ColorPhase = "Magenta"

# Paths
$MasmDir = "masm"
$BuildDir = "build\extended_$Configuration"
$OutputExe = "$BuildDir\RawrXD-Extended-Harness.exe"

# MASM files in dependency order (now including real components)
$MasmModules = @(
    # Runtime + real components
    "runtime_support.asm",
    "tool_registry.asm",
    "config_manager.asm",
    "error_handler.asm",
    "resource_guards.asm",
    
    # Testing Infrastructure
    "ollama_client_masm.asm",
    "model_discovery_ui.asm",
    "performance_baseline.asm",
    "integration_test_framework.asm",
    "smoke_test_suite.asm",
    
    # Agentic Engine + Integration
    "zero_day_agentic_engine.asm",
    "zero_day_integration.asm",
    "stub_implementations.asm",

    # Extended Harness (with real components - v3 final)
    "masm_extended_harness_v3.asm"
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

function Write-Phase {
    param([string]$Message)
    Write-Host $Message -ForegroundColor $ColorPhase
}

# Display banner
Write-Phase "`n╔════════════════════════════════════════════════════════════════╗"
Write-Phase "║                                                                ║"
Write-Phase "║   EXTENDED PURE MASM HARNESS BUILD SYSTEM                     ║"
Write-Phase "║   Real Components: Config + Error Handling + Resource Guards  ║"
Write-Phase "║                                                                ║"
Write-Phase "╚════════════════════════════════════════════════════════════════╝`n"

Write-Info "Configuration: $Configuration"
Write-Info "Build Directory: $BuildDir"
Write-Info "Output: $OutputExe"
Write-Phase "Modules: $($MasmModules.Count)"
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
Write-Phase "🏗️  COMPILATION PHASE"
Write-Host ""

$ObjectFiles = @()
$CompileSuccess = $true
$TotalSize = 0

foreach ($AsmFile in $MasmModules) {
    $SourcePath = Join-Path $MasmDir $AsmFile
    $ObjName = [System.IO.Path]::GetFileNameWithoutExtension($AsmFile) + ".obj"
    $ObjPath = Join-Path $BuildDir $ObjName
    
    $ModuleIndex = $MasmModules.IndexOf($AsmFile) + 1
    Write-Info "[$ModuleIndex/$($MasmModules.Count)] Compiling $AsmFile..."
    
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
        $TotalSize += $Size
        Write-Success "Compiled $AsmFile -> $ObjName ($([Math]::Round($Size/1KB, 1)) KB)"
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

Write-Host ""
Write-Success "All modules compiled successfully"
Write-Info "Total object code: $([Math]::Round($TotalSize/1KB, 1)) KB"

# Link executable
Write-Host ""
Write-Phase "🔗  LINKING PHASE"
Write-Host ""

Write-Info "Linking extended executable..."

$LinkArgs = @(
    "/OUT:$OutputExe",
    "/SUBSYSTEM:CONSOLE",
    "/ENTRY:mainCRTStartup",
    "/MACHINE:X64",
    "/MAP:$BuildDir\RawrXD-Extended-Harness.map",
    "/MAPINFO:EXPORTS",
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
$LinkArgs += "advapi32.lib"
$LinkArgs += "ws2_32.lib"

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
        Write-Host "Note: These symbols are expected if you haven't yet implemented" -ForegroundColor Yellow
        Write-Host "the corresponding modules. They can be replaced with stubs." -ForegroundColor Yellow
    }
    
    exit 1
}

Write-Host ""
Write-Success "Linking successful!"

if (Test-Path $OutputExe) {
    $ExeSize = (Get-Item $OutputExe).Length
    $ExeSizeKB = [Math]::Round($ExeSize / 1KB, 2)
    
    Write-Host ""
    Write-Phase "╔════════════════════════════════════════════════════════════════╗"
    Write-Phase "║              BUILD SUCCESSFUL - EXTENDED HARNESS ✅            ║"
    Write-Phase "╚════════════════════════════════════════════════════════════════╝"
    Write-Host ""
    Write-Host "📦 Output: $OutputExe" -ForegroundColor Cyan
    Write-Host "📊 Size:   $ExeSizeKB KB (from $([Math]::Round($TotalSize/1KB, 1)) KB object code)" -ForegroundColor Cyan
    Write-Host "📈 Ratio:  $([Math]::Round(($ExeSize/$TotalSize)*100, 1))% linker optimization" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "🚀 To run the extended harness:" -ForegroundColor Yellow
    Write-Host "   $OutputExe" -ForegroundColor White
    Write-Host ""
    Write-Host "✨ Real Components Integrated:" -ForegroundColor Green
    Write-Host "   • Phase 3: Configuration Management System" -ForegroundColor White
    Write-Host "   • Phase 4: Centralized Error Handling" -ForegroundColor White
    Write-Host "   • Resource Guards: RAII-style cleanup" -ForegroundColor White
    Write-Host ""
} else {
    Write-Error-Custom "Executable not created!"
    exit 1
}

exit 0
