param(
    [string]$SourceDir = 'D:\RawrXD-production-lazy-init',
    [string]$BuildDir = 'D:\RawrXD-production-lazy-init\build',
    [string]$Configuration = 'Release',
    [string]$Target = 'RawrXD-AgenticIDE',
    [int]$Parallel = [int]([Environment]::GetEnvironmentVariable('NUMBER_OF_PROCESSORS'))
)

$ErrorActionPreference = 'Stop'
$log = 'D:\ide-build-log.txt'

Write-Host "[Build-IDE] Source: $SourceDir" -ForegroundColor Cyan
Write-Host "[Build-IDE] Build:  $BuildDir" -ForegroundColor Cyan
Write-Host "[Build-IDE] Config: $Configuration  Target: $Target  Parallel: $Parallel" -ForegroundColor Cyan

if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

# Configure if needed (idempotent)
if (!(Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
    Write-Host "[Build-IDE] Configuring CMake..." -ForegroundColor Yellow
    cmake -S $SourceDir -B $BuildDir -DENABLE_MASM_INTEGRATION=OFF | Tee-Object -FilePath $log -Append
}

# Build
Write-Host "[Build-IDE] Building $Target ($Configuration)..." -ForegroundColor Yellow
$buildCmd = "cmake --build `"$BuildDir`" --config $Configuration --target $Target --parallel $Parallel"
Write-Host "[Build-IDE] cmd: $buildCmd" -ForegroundColor DarkGray

$start = Get-Date
$proc = Start-Process -FilePath cmake -ArgumentList "--build","$BuildDir","--config",$Configuration,"--target",$Target,"--parallel",$Parallel -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError $log
$proc.WaitForExit()
$end = Get-Date

$dur = [int]($end - $start).TotalSeconds
Write-Host "[Build-IDE] Build finished with code $($proc.ExitCode) in ${dur}s." -ForegroundColor Cyan

if ($proc.ExitCode -ne 0) {
    Write-Host "[Build-IDE] Build failed. See log: $log" -ForegroundColor Red
    exit $proc.ExitCode
}

# Quick existence check
$exe = Join-Path $BuildDir "Release\$Target.exe"
if (Test-Path $exe) {
    Write-Host "[Build-IDE] Output found: $exe  Size: $([Math]::Round((Get-Item $exe).Length/1MB,2)) MB" -ForegroundColor Green
} else {
    Write-Host "[Build-IDE] Output not found yet (link pending or out-of-tree target)." -ForegroundColor Yellow
}
