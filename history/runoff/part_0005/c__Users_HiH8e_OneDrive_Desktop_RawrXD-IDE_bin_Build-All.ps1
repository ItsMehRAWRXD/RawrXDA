param(
    [string]$SourceDir = 'D:\RawrXD-production-lazy-init',
    [string]$BuildDir = 'D:\RawrXD-production-lazy-init\build',
    [string]$Configuration = 'Release',
    [int]$Parallel = [int]([Environment]::GetEnvironmentVariable('NUMBER_OF_PROCESSORS')),
    [string[]]$Targets = @(
        'RawrXD-AgenticIDE',
        'RawrXD-Agent',
        'gguf_hotpatch_tester',
        'test_chat_streaming',
        'benchmark_completions',
        'vulkan-shaders-gen'
    )
)

$ErrorActionPreference = 'Stop'
$log = 'D:\ide-build-log.txt'

Write-Host "[Build-All] Source: $SourceDir" -ForegroundColor Cyan
Write-Host "[Build-All] Build:  $BuildDir" -ForegroundColor Cyan
Write-Host "[Build-All] Config: $Configuration  Parallel: $Parallel" -ForegroundColor Cyan
Write-Host "[Build-All] Targets:`n  - " ($Targets -join "`n  - ") -ForegroundColor Yellow

if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

# Configure if needed
if (!(Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
    Write-Host "[Build-All] Configuring CMake..." -ForegroundColor Yellow
    cmake -S $SourceDir -B $BuildDir -DENABLE_MASM_INTEGRATION=OFF | Tee-Object -FilePath $log -Append
}

# Build all known targets; continue-on-error for optional ones
$overallExit = 0
foreach ($t in $Targets) {
    Write-Host "[Build-All] Building target: $t" -ForegroundColor Yellow
    $args = @('--build',"$BuildDir",'--config',$Configuration,'--target',$t,'--parallel',$Parallel)
    $p = Start-Process -FilePath cmake -ArgumentList $args -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError $log
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) {
        Write-Host "[Build-All] Target '$t' failed (code $($p.ExitCode)). Continuing..." -ForegroundColor Red
        if ($overallExit -eq 0) { $overallExit = $p.ExitCode }
    } else {
        Write-Host "[Build-All] Target '$t' built successfully." -ForegroundColor Green
    }
}

# Summarize outputs
$release = Join-Path $BuildDir 'Release'
if (Test-Path $release) {
    Write-Host "[Build-All] Built executables:" -ForegroundColor Cyan
    Get-ChildItem $release -Filter *.exe -ErrorAction SilentlyContinue | Select-Object Name,@{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}} | Format-Table -AutoSize
}

exit $overallExit
