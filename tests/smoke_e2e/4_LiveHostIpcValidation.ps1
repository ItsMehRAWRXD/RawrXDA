# =============================================================================
# Scenario 4: Live IDE <-> extension-host client IPC (native ping)
# =============================================================================

param(
    [string]$BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$RepoRoot = "D:\rawrxd",
    [int]$BootWaitMs = 800,
    [int]$IngestWaitMs = 600
)

$ErrorActionPreference = "Stop"

$logDir = Join-Path $PSScriptRoot "logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

$scenarioLog = Join-Path $logDir "scenario4_live_ipc.log"
$pingExe = Join-Path $PSScriptRoot "tools\RawrXDIpcPing.exe"
$pingSrc = Join-Path $PSScriptRoot "tools\RawrXDIpcPing.cpp"
$buildScript = Join-Path $PSScriptRoot "tools\Build-RawrXDIpcPing.ps1"

function Log {
    param([string]$Msg, [string]$Level = "INFO")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$ts] [$Level] $Msg"
    Write-Host $line -ForegroundColor Cyan
    Add-Content -Path $scenarioLog $line
}

. (Join-Path $PSScriptRoot "modules\SmokeDiagnostics.ps1")
. (Join-Path $PSScriptRoot "modules\IpcFramingHelper.ps1")

function Send-IpcPingPowerShellClient {
    $client = New-Object System.IO.Pipes.NamedPipeClientStream(
        ".", "RawrXDExtensionPipe",
        [System.IO.Pipes.PipeDirection]::InOut,
        [System.IO.Pipes.PipeOptions]::None)
    $client.Connect(5000)
    $frame = New-LegacyFrame -PayloadUtf8 "PING_LIVE_TRAFFIC_TEST" -MessageType 1
    $wire = Add-WirePrefix -PhysicalFrame $frame
    $client.Write($wire, 0, $wire.Length)
    $client.Flush()
    $client.Dispose()
}

Log "════════════════════════════════════════════════════════════════"
Log "SCENARIO 4: Live IDE <-> Host Cross-Process IPC Verification"
Log "════════════════════════════════════════════════════════════════"

if (-not (Test-Path $BinaryPath)) {
    Log "Binary not found: $BinaryPath" "ERROR"
    exit 1
}

$ninjaPing = "d:\rxdn_ninja\bin\RawrXDIpcPing.exe"
if (Test-Path $ninjaPing) {
    Copy-Item -Path $ninjaPing -Destination $pingExe -Force -ErrorAction SilentlyContinue
}

$pingNeedsBuild = (-not (Test-Path $pingExe))
if (-not $pingNeedsBuild -and (Test-Path $pingSrc)) {
    $pingNeedsBuild = (Get-Item $pingSrc).LastWriteTime -gt (Get-Item $pingExe).LastWriteTime
}
if ($pingNeedsBuild) {
    Log "Building RawrXDIpcPing.exe..."
    try {
        & $buildScript -RepoRoot $RepoRoot
    } catch {
        Log "Native ping build skipped: $($_.Exception.Message)" "WARN"
    }
}

$binDir = Split-Path -Parent $BinaryPath
$ideLogCandidates = @(
    (Join-Path $binDir "RawrXD_IDE.log"),
    (Join-Path $env:APPDATA "RawrXD\ide.log"),
    (Join-Path $logDir "scenario4_ide.log")
)

function Get-IdeLogTail {
    foreach ($path in $ideLogCandidates) {
        if (Test-Path $path) {
            return @{ Path = $path; Lines = @(Get-Content $path -Tail 120 -ErrorAction SilentlyContinue) }
        }
    }
    return $null
}

$preLog = Get-IdeLogTail
$preLineCount = if ($preLog) { $preLog.Lines.Count } else { 0 }

Log "Launching IDE (smoke env, no PS pipe mock — IDE owns the pipe server)"
$ideProc = Start-RawrIDESmokeProcess -BinaryPath $BinaryPath

if (-not $ideProc) {
    Log "Failed to start IDE" "ERROR"
    exit 1
}

Log "IDE PID=$($ideProc.Id); waiting for extension pipe server (up to 20s)..."

if (-not ([System.Management.Automation.PSTypeName]'RawrSmoke.NativePipe').Type) {
    Add-Type @"
using System.Runtime.InteropServices;
public static class RawrSmokeNativePipe {
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool WaitNamedPipe(string lpNamedPipeName, int nTimeOut);
}
"@
}

function Test-ExtensionPipeListed {
    param([int]$TimeoutMs = 250)
    return [RawrSmokeNativePipe]::WaitNamedPipe("\\.\pipe\RawrXDExtensionPipe", $TimeoutMs)
}

$pipeReady = $false
$pipeDeadline = [DateTime]::UtcNow.AddSeconds(20)
while ([DateTime]::UtcNow -lt $pipeDeadline -and -not $ideProc.HasExited) {
    if (Test-ExtensionPipeListed -TimeoutMs 300) {
        $pipeReady = $true
        break
    }
    Start-Sleep -Milliseconds 200
}

if (-not $pipeReady) {
    Log "Extension pipe not listed within 20s (CreateNamedPipe not reached)" "ERROR"
    Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

Log "Extension pipe instance is listed (server created; client not yet connected)"

if ($ideProc.HasExited) {
    Log "IDE exited before ping (ExitCode=$($ideProc.ExitCode))" "ERROR"
    Write-EarlyExitDiagnostics -BinaryPath $BinaryPath -ProcessId $ideProc.Id -OnLog { param($m, $l) Log $m $l }
    exit 1
}

$pingExit = 0
if (Test-Path $pingExe) {
    Log "Running native IPC ping client: $pingExe"
    & $pingExe
    $pingExit = $LASTEXITCODE
} else {
    Log "Native ping missing; using PowerShell NamedPipe client (production framing helpers)"
    try {
        Send-IpcPingPowerShellClient
        $pingExit = 0
    } catch {
        Log "PowerShell IPC ping failed: $($_.Exception.Message)" "ERROR"
        $pingExit = 106
    }
}

if ($pingExit -ne 0) {
    Log "IPC ping client failed with exit code $pingExit" "ERROR"
    Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

Log "Ping OK; waiting ${IngestWaitMs}ms for PollExtensionEngineLsp ingest..."
Start-Sleep -Milliseconds $IngestWaitMs

for ($i = 0; $i -lt 20; $i++) {
    $tail = Get-IdeLogTail
    if ($tail -and $tail.Lines) {
        $match = $tail.Lines | Select-String -Pattern "ExtensionEngine IPC:" | Select-Object -Last 1
        if ($match) {
            Log "Matched trace: $($match.Line)" "SUCCESS"
            Log "SCENARIO 4 RESULT: PASS" "SUCCESS"
            Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
            exit 0
        }
    }
    Start-Sleep -Milliseconds 100
}

Log "No 'ExtensionEngine IPC:' line found in IDE logs" "ERROR"
if ($tail) {
    Log "Checked log: $($tail.Path)" "WARN"
    Log "Last 8 lines:" "WARN"
    $tail.Lines | Select-Object -Last 8 | ForEach-Object { Log $_ "WARN" }
} else {
    Log "No IDE log candidates found under bin or %APPDATA%\RawrXD" "WARN"
}

Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
exit 1
