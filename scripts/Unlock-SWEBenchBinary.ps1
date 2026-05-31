param(
    [string]$BuildDir = "",
    [string]$ExeName = "RawrXD-SWEBench.exe",
    [int]$WaitMs = 3000,
    [switch]$Force,
    [switch]$Quiet
)

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    foreach ($c in @(
            (Join-Path $repoRoot "build-ninja-ctx2"),
            (Join-Path $repoRoot "build-win32"),
            (Join-Path $repoRoot "build-ninja"))) {
        if (Test-Path -LiteralPath (Join-Path $c "CMakeCache.txt")) {
            $BuildDir = $c
            break
        }
    }
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build-ninja-ctx2"
}

$exePath = Join-Path $BuildDir ("bin/" + $ExeName)
$buildDirFull = [System.IO.Path]::GetFullPath($BuildDir)

if (-not $Quiet) {
    Write-Host "[unlock] target: $exePath" -ForegroundColor Cyan
}

# Find processes by exact process name first.
$procName = [System.IO.Path]::GetFileNameWithoutExtension($ExeName)
$procs = Get-Process -Name $procName -ErrorAction SilentlyContinue

if (-not $procs) {
    if (-not $Quiet) {
        Write-Host "[unlock] no running process named $procName" -ForegroundColor Green
    }
    exit 0
}

# Match only processes whose executable path is under the requested BuildDir.
$matched = @()
foreach ($p in $procs) {
    try {
        if ($p.Path -and [System.IO.Path]::GetFullPath($p.Path).StartsWith($buildDirFull, [System.StringComparison]::OrdinalIgnoreCase)) {
            $matched += $p
        }
    } catch {
        # Access denied for some process metadata; ignore.
    }
}

if (-not $matched) {
    if ($Force) {
        Write-Warning "[unlock] FORCE MODE ENABLED - broad termination by process name"
        $matched = $procs
    } else {
        Write-Warning "[unlock] no processes matched BuildDir; refusing broad termination"
        exit 2
    }
}

if (-not $Quiet) {
    $ids = ($matched | ForEach-Object { $_.Id }) -join ","
    Write-Host "[unlock] stopping PIDs: $ids" -ForegroundColor Yellow
}

if ($Force) {
    $matched | Stop-Process -Force -ErrorAction SilentlyContinue
} else {
    $matched | Stop-Process -ErrorAction SilentlyContinue
}

Start-Sleep -Milliseconds $WaitMs

# Verify scoped lock target is no longer running.
$remaining = @(Get-Process -Name $procName -ErrorAction SilentlyContinue | Where-Object {
    try {
        $_.Path -and [System.IO.Path]::GetFullPath($_.Path).StartsWith($buildDirFull, [System.StringComparison]::OrdinalIgnoreCase)
    } catch {
        $false
    }
})
if ($remaining) {
    if (-not $Quiet) {
        Write-Host "[unlock] warning: process still running; binary may remain locked" -ForegroundColor Red
    }
    exit 2
}

if (-not $Quiet) {
    Write-Host "[unlock] done" -ForegroundColor Green
}
exit 0
