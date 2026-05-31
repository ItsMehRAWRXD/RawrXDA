param(
    [string]$BuildDir = "d:/rawrxd/build",
    [string]$Target = "RawrXD-Win32IDE",
    [int]$Jobs = 8,
    [int]$LockTimeoutSec = 900,
    [switch]$CleanStaleObjLocks
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BuildDir)) {
    Write-Host "ERROR: BuildDir not found: $BuildDir" -ForegroundColor Red
    exit 1
}

$lockDir = Join-Path $BuildDir ".single_writer_build.lock"
$lockMeta = Join-Path $lockDir "owner.txt"
$lockStart = Get-Date
$acquired = $false

while (((Get-Date) - $lockStart).TotalSeconds -lt $LockTimeoutSec) {
    try {
        New-Item -ItemType Directory -Path $lockDir -ErrorAction Stop | Out-Null
        $owner = "pid=$PID time=$((Get-Date).ToString('o')) host=$env:COMPUTERNAME"
        Set-Content -Path $lockMeta -Value $owner -Encoding UTF8
        $acquired = $true
        break
    }
    catch {
        if (Test-Path -LiteralPath $lockMeta) {
            $currentOwner = Get-Content -Path $lockMeta -ErrorAction SilentlyContinue
            Write-Host "Build lock held by: $currentOwner" -ForegroundColor Yellow
        }
        Start-Sleep -Seconds 2
    }
}

if (-not $acquired) {
    Write-Host "ERROR: Timed out waiting for build lock after $LockTimeoutSec seconds." -ForegroundColor Red
    exit 2
}

try {
    Write-Host "Acquired build lock: $lockDir" -ForegroundColor Green

    if ($CleanStaleObjLocks) {
        Write-Host "Cleaning stale lock-prone object files (best effort)..." -ForegroundColor DarkGray
        $headlessObj = Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/src/win32app/HeadlessIDE.cpp.obj"
        if (Test-Path -LiteralPath $headlessObj) {
            try { Remove-Item -LiteralPath $headlessObj -Force -ErrorAction Stop } catch {}
        }
    }

    Push-Location $BuildDir
    try {
        & ninja -j $Jobs $Target
        $code = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }

    if ($code -ne 0) {
        Write-Host "Build failed with exit code $code" -ForegroundColor Red
        exit $code
    }

    Write-Host "Build succeeded for target $Target" -ForegroundColor Green
}
finally {
    if (Test-Path -LiteralPath $lockDir) {
        Remove-Item -LiteralPath $lockDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
