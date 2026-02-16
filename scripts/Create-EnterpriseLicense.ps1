# Create-EnterpriseLicense.ps1 — Enterprise License Creator
# Generates or activates RawrXD Enterprise licenses for dev and production.
#
# Usage:
#   .\Create-EnterpriseLicense.ps1 -DevUnlock          # Show dev unlock instructions
#   .\Create-EnterpriseLicense.ps1 -ShowStatus         # Show current license status
#   .\Create-EnterpriseLicense.ps1 -CreateForMachine   # Create .rawrlic (requires RawrXD_KeyGen)
#   .\Create-EnterpriseLicense.ps1 -PythonIssue        # Use Python license_generator.py

param(
    [switch]$DevUnlock,
    [switch]$ShowStatus,
    [switch]$CreateForMachine,
    [switch]$PythonIssue,
    [string]$OutputPath = "license.rawrlic",
    [int]$Days = 365
)

$ErrorActionPreference = "Stop"

function Write-Step { param($Msg) Write-Host "`n[License] $Msg" -ForegroundColor Cyan }
function Write-Ok   { param($Msg) Write-Host "  OK: $Msg" -ForegroundColor Green }
function Write-Warn { param($Msg) Write-Host "  WARN: $Msg" -ForegroundColor Yellow }
function Write-Info { param($Msg) Write-Host "  $Msg" -ForegroundColor Gray }

# =============================================================================
# Dev Unlock (RAWRXD_ENTERPRISE_DEV=1)
# =============================================================================
if ($DevUnlock) {
    Write-Step "Enterprise Dev Unlock"
    Write-Host @"

  Set environment variable to unlock all Enterprise features (dev builds):

    `$env:RAWRXD_ENTERPRISE_DEV = "1"

  Then launch RawrXD IDE. All 8 features unlock:
    • DualEngine800B
    • AVX512Premium
    • DistributedSwarm
    • GPUQuant4Bit
    • EnterpriseSupport
    • UnlimitedContext
    • FlashAttention
    • MultiGPU

  Note: Only works when using C++ stubs (no MASM). For production, use
        RawrXD_KeyGen or license_generator.py to create .rawrlic files.

"@ -ForegroundColor Gray
    if (-not $env:RAWRXD_ENTERPRISE_DEV) {
        Write-Info "To enable now: `$env:RAWRXD_ENTERPRISE_DEV='1'; .\build_ide\bin\RawrXD-Win32IDE.exe"
    } else {
        Write-Ok "RAWRXD_ENTERPRISE_DEV is already set"
    }
    exit 0
}

# =============================================================================
# Show Status
# =============================================================================
if ($ShowStatus) {
    Write-Step "Enterprise License Status"
    $ideExe = Join-Path $PSScriptRoot "..\build_ide\bin\RawrXD-Win32IDE.exe"
    if (Test-Path $ideExe) {
        Write-Info "IDE found. Launch IDE and use Help > Enterprise License / Features... to see status."
    }
    Write-Host @"

  To check license status:
    1. Launch RawrXD IDE
    2. Help > Enterprise License / Features...
    3. Or: Agent > 800B Dual-Engine Status

  Enterprise features (see ENTERPRISE_FEATURES_MANIFEST.md):
    Bit 0: DualEngine800B
    Bit 1: AVX512Premium
    Bit 2: DistributedSwarm
    Bit 3: GPUQuant4Bit
    Bit 4: EnterpriseSupport
    Bit 5: UnlimitedContext
    Bit 6: FlashAttention
    Bit 7: MultiGPU

"@ -ForegroundColor Gray
    exit 0
}

# =============================================================================
# Create for Machine (RawrXD_KeyGen)
# =============================================================================
if ($CreateForMachine) {
    Write-Step "Create license via RawrXD_KeyGen"
    $keyGen = Join-Path $PSScriptRoot "..\build_ide\bin\RawrXD_KeyGen.exe"
    if (-not (Test-Path $keyGen)) {
        Write-Warn "RawrXD_KeyGen.exe not found. Build with: cmake --build build_ide --target RawrXD_KeyGen"
        Write-Info "Alternatively, use: .\Create-EnterpriseLicense.ps1 -PythonIssue"
        exit 1
    }
    & $keyGen --hwid
    & $keyGen --issue --type enterprise --features 0xFF --days $Days --output $OutputPath
    Write-Ok "License written to $OutputPath"
    Write-Info "Copy to project root as rawrxd.rawrlic or install via Help > Enterprise License"
    exit 0
}

# =============================================================================
# Python Issue (license_generator.py)
# =============================================================================
if ($PythonIssue) {
    Write-Step "Create license via Python license_generator.py"
    $pyScript = Join-Path $PSScriptRoot "..\src\tools\license_generator.py"
    if (-not (Test-Path $pyScript)) {
        Write-Warn "license_generator.py not found at $pyScript"
        exit 1
    }
    Write-Info "Run: python $pyScript hwid"
    Write-Info "Then: python $pyScript issue --hwid <hex64> --output $OutputPath"
    & python $pyScript hwid 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "Use the HWID above with: python $pyScript issue --hwid <hex> -o $OutputPath"
    } else {
        Write-Warn "pip install cryptography required"
    }
    exit 0
}

# =============================================================================
# Default: Show usage
# =============================================================================
Write-Host @"

Create-EnterpriseLicense.ps1 — Enterprise License Creator
=========================================================

  -DevUnlock        Show dev unlock instructions (RAWRXD_ENTERPRISE_DEV=1)
  -ShowStatus       Show how to check license status
  -CreateForMachine Create .rawrlic via RawrXD_KeyGen (requires build)
  -PythonIssue      Use Python license_generator.py

  -OutputPath <file>  Output path for .rawrlic (default: license.rawrlic)
  -Days <N>           License validity in days (default: 365)

Examples:
  .\Create-EnterpriseLicense.ps1 -DevUnlock
  .\Create-EnterpriseLicense.ps1 -CreateForMachine -OutputPath rawrxd.rawrlic

See: ENTERPRISE_FEATURES_MANIFEST.md for full feature list and wiring.

"@ -ForegroundColor Cyan
