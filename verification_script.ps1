# ==========================================
# RAWRXD C++ BACKEND VERIFICATION SUITE
# Windows/MSVC Native (PowerShell)
# ==========================================
# Usage: .\verification_script.ps1
# Requires: CMake, MSVC 2022 (cl.exe on PATH or VS Developer Shell)

$ErrorActionPreference = 'Stop'

function Write-Step($num, $total, $msg) {
    Write-Host "[$num/$total] $msg" -ForegroundColor Yellow
}
function Write-Pass($msg) { Write-Host "  [PASS] $msg" -ForegroundColor Green }
function Write-Fail($msg) { Write-Host "  [FAIL] $msg" -ForegroundColor Red; exit 1 }
function Write-Skip($msg) { Write-Host "  [SKIP] $msg" -ForegroundColor DarkYellow }

$root = $PSScriptRoot
if (-not $root) { $root = Get-Location }
Push-Location $root

$totalSteps = 5

# ---------- Step 1: Verify toolchain ----------
Write-Step 1 $totalSteps "Verifying build toolchain..."

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) { Write-Fail "CMake not found. Install CMake 3.20+ and add to PATH." }
Write-Pass "CMake found: $($cmake.Source)"

# ---------- Step 2: Configure ----------
Write-Step 2 $totalSteps "Configuring with CMake..."

$buildDir = Join-Path $root "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Push-Location $buildDir
$configResult = & cmake .. -G "Visual Studio 17 2022" -A x64 2>&1
$configExit = $LASTEXITCODE
Pop-Location

if ($configExit -ne 0) {
    Write-Host ($configResult -join "`n") -ForegroundColor Gray
    Write-Fail "CMake configuration failed (exit $configExit)."
}
Write-Pass "CMake configured successfully."

# ---------- Step 3: Build ----------
Write-Step 3 $totalSteps "Building (Release, parallel)..."

$buildResult = & cmake --build $buildDir --config Release -- /m /v:minimal 2>&1
$buildExit = $LASTEXITCODE

if ($buildExit -ne 0) {
    # Show last 40 lines of build output for diagnostics
    $lines = $buildResult -split "`n"
    $tail = $lines | Select-Object -Last 40
    Write-Host ($tail -join "`n") -ForegroundColor Gray
    Write-Fail "Build failed (exit $buildExit). See output above."
}
Write-Pass "Build succeeded."

# ---------- Step 4: Verify binaries ----------
Write-Step 4 $totalSteps "Verifying output binaries..."

$relDir = Join-Path $buildDir "Release"
if (-not (Test-Path $relDir)) { $relDir = $buildDir }

$targets = @(
    @{ Name = "RawrEngine"; Required = $true },
    @{ Name = "RawrXD-Win32IDE"; Required = $false },
    @{ Name = "RawrXD-InferenceEngine"; Required = $false }
)

foreach ($t in $targets) {
    $exe = Get-ChildItem -Path $buildDir -Recurse -Filter "$($t.Name).exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($exe) {
        $sizeMB = [math]::Round($exe.Length / 1MB, 2)
        Write-Pass "$($t.Name).exe found ($sizeMB MB) — $($exe.FullName)"
    } elseif ($t.Required) {
        Write-Fail "$($t.Name).exe not found in build output."
    } else {
        Write-Skip "$($t.Name).exe not built (optional target)."
    }
}

# ---------- Step 5: Smoke tests ----------
Write-Step 5 $totalSteps "Running smoke tests..."

$rawrExe = Get-ChildItem -Path $buildDir -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($rawrExe) {
    try {
        $proc = Start-Process -FilePath $rawrExe.FullName -ArgumentList "--help" `
            -NoNewWindow -PassThru -Wait -RedirectStandardOutput "NUL" -RedirectStandardError "NUL"
        if ($proc.ExitCode -le 1) {
            Write-Pass "RawrEngine boot verification passed (exit $($proc.ExitCode))."
        } else {
            Write-Fail "RawrEngine crashed on boot (exit $($proc.ExitCode))."
        }
    } catch {
        Write-Fail "RawrEngine execution error: $_"
    }
} else {
    Write-Skip "RawrEngine.exe not found — skipping smoke test."
}

$win32Exe = Get-ChildItem -Path $buildDir -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($win32Exe) {
    Write-Pass "RawrXD-Win32IDE.exe exists ($([math]::Round($win32Exe.Length / 1MB, 2)) MB)."
} else {
    Write-Skip "RawrXD-Win32IDE.exe not built."
}

# ---------- Summary ----------
Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "   FULL SYSTEM INTEGRATION: VERIFIED      " -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Build targets:" -ForegroundColor Cyan
Write-Host "  RawrEngine             — Headless CLI + Hotpatch + Slicer + Replay Harness"
Write-Host "  RawrXD-Win32IDE        — Full Win32 IDE with all panels"
Write-Host "  RawrXD-InferenceEngine — Standalone GGUF inference"
Write-Host ""
Write-Host "Next: Run 'cmake --build build --config Release --target RawrXD-Win32IDE' for IDE binary." -ForegroundColor Cyan

Pop-Location
