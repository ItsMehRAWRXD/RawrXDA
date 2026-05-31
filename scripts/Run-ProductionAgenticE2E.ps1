#Requires -Version 5.1
<#
.SYNOPSIS
  Production agentic end-to-end: build core targets, run CTest agentic slice, turnkey parity, optional chat parity script.

.DESCRIPTION
  Uses RAWRXD_BUILD_DIR when set (default: <repo>/build-win32). Builds Win32IDE, RawrEngine, agentic tests, runs:
    - ctest: test_tool_registry, test_agentic_file_operations, test_ide_model_autonomy, win32ide_agentic_smoke
    - scripts/Run-TurnkeyIdeSmoke.ps1 (full copilot path when RawrEngine.exe exists)
    - RawrXD-Win32IDE --headless --no-server (exit 0 after headless init; no HTTP listen loop)
    - Ship/smoke_agentic_chat_parity.ps1 when -IncludeChatParity

.PARAMETER SkipBuild
  Do not invoke cmake --build.

.PARAMETER IncludeChatParity
  Also run Ship/smoke_agentic_chat_parity.ps1 (duplicate overlap with turnkey; use for release gates).

.PARAMETER BuildDir
  Override CMake build directory (else env RAWRXD_BUILD_DIR or build-win32).
#>
param(
    [switch]$SkipBuild,
    [switch]$IncludeChatParity,
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    if ($env:RAWRXD_BUILD_DIR -and $env:RAWRXD_BUILD_DIR.Trim().Length -gt 0) {
        $BuildDir = $env:RAWRXD_BUILD_DIR
    }
    else {
        $BuildDir = Join-Path $repoRoot "build-win32"
    }
}
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "No CMakeCache.txt in $BuildDir — configure this build directory first."
}

function Resolve-FirstExistingPath {
    param([string[]]$Candidates)
    foreach ($c in $Candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    return $null
}

if (-not $SkipBuild) {
    Write-Host "[e2e] Building targets in $BuildDir ..." -ForegroundColor Cyan
    cmake --build $BuildDir --parallel 8 `
        --target RawrXD-Win32IDE `
        --target RawrEngine `
        --target test_tool_registry `
        --target test_agentic_file_operations `
        --target test_ide_model_autonomy `
        --target agentic_orchestrator_smoke_test
    if ($LASTEXITCODE -ne 0) { throw "cmake --build failed with exit $LASTEXITCODE" }
}

$ide = Resolve-FirstExistingPath @(
    (Join-Path $BuildDir "bin\Release\RawrXD-Win32IDE.exe"),
    (Join-Path $BuildDir "bin\Debug\RawrXD-Win32IDE.exe"),
    (Join-Path $BuildDir "bin\RawrXD-Win32IDE.exe")
)
$engine = Resolve-FirstExistingPath @(
    (Join-Path $BuildDir "bin\Release\RawrEngine.exe"),
    (Join-Path $BuildDir "bin\Debug\RawrEngine.exe"),
    (Join-Path $BuildDir "bin\RawrEngine.exe")
)
if (-not $ide) {
    throw "RawrXD-Win32IDE.exe not found under $BuildDir\bin (Release/Debug or flat) after build."
}

Write-Host "[e2e] CTest agentic slice ..." -ForegroundColor Cyan
Push-Location $BuildDir
try {
    $ctestRe = "win32ide_agentic_smoke|test_tool_registry|test_agentic_file_operations|test_ide_model_autonomy"
    if ((Test-Path -LiteralPath $engine)) {
        $ctestRe += "|rawrengine_copilot_smoke"
    }
    ctest -R $ctestRe --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "ctest failed with exit $LASTEXITCODE" }
}
finally {
    Pop-Location
}

Write-Host "[e2e] Turnkey IDE smoke (IdeExe + optional RawrEngine) ..." -ForegroundColor Cyan
$turnkeyArgs = @("-NoProfile", "-File", (Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"), "-IdeExe", $ide, "-BuildDir", $BuildDir)
if ((Test-Path -LiteralPath $engine)) { $turnkeyArgs += @("-EngineExe", $engine) }
& pwsh @turnkeyArgs
if ($LASTEXITCODE -ne 0) { throw "Run-TurnkeyIdeSmoke failed exit $LASTEXITCODE" }

Write-Host "[e2e] Win32IDE --headless --no-server (expect exit 0 after init) ..." -ForegroundColor Cyan
$p = Start-Process -FilePath $ide -ArgumentList @("--headless", "--no-server") -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) {
    throw "headless --no-server exit $($p.ExitCode)"
}

$orch = Resolve-FirstExistingPath @(
    (Join-Path $BuildDir "bin\Release\agentic_orchestrator_smoke_test.exe"),
    (Join-Path $BuildDir "bin\Debug\agentic_orchestrator_smoke_test.exe"),
    (Join-Path $BuildDir "bin\agentic_orchestrator_smoke_test.exe")
)
if (-not $orch) {
    Write-Host "[e2e] Building agentic_orchestrator_smoke_test (binary missing) ..." -ForegroundColor Yellow
    cmake --build $BuildDir --target agentic_orchestrator_smoke_test --parallel 8
    if ($LASTEXITCODE -ne 0) { throw "cmake --build agentic_orchestrator_smoke_test failed exit $LASTEXITCODE" }
    $orch = Resolve-FirstExistingPath @(
        (Join-Path $BuildDir "bin\Release\agentic_orchestrator_smoke_test.exe"),
        (Join-Path $BuildDir "bin\Debug\agentic_orchestrator_smoke_test.exe"),
        (Join-Path $BuildDir "bin\agentic_orchestrator_smoke_test.exe")
    )
}
if (-not $orch) {
    throw "agentic_orchestrator_smoke_test.exe not found under $BuildDir\bin after build."
}
& $orch 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) { throw "agentic_orchestrator_smoke_test failed exit $LASTEXITCODE" }

if ($IncludeChatParity) {
    Write-Host "[e2e] Ship/smoke_agentic_chat_parity.ps1 ..." -ForegroundColor Cyan
    & pwsh -NoProfile -File (Join-Path $repoRoot "Ship\smoke_agentic_chat_parity.ps1")
    if ($LASTEXITCODE -ne 0) { throw "smoke_agentic_chat_parity failed exit $LASTEXITCODE" }
}

Write-Host "[e2e] Production agentic E2E completed OK." -ForegroundColor Green
exit 0
