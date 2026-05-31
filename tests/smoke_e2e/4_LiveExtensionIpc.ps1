# =============================================================================
# Scenario 4: Live IDE <-> extension host cross-process IPC (production framing)
# =============================================================================

param(
    [string]$BinaryPath = "",
    [string]$HostPath = "",
    [string]$PingPath = "",
    [string]$RepoRoot = "",
    [int]$IngestWaitMs = 800,
    [int]$PipeWaitSeconds = 20,
    [switch]$FullSmoke,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$logDir = Join-Path $PSScriptRoot "logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

$scenarioLog = Join-Path $logDir "scenario4_live_ipc.log"
$buildPingScript = Join-Path $PSScriptRoot "tools\Build-RawrXDIpcPing.ps1"

function Log {
    param([string]$Msg, [string]$Level = "INFO")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$ts] [$Level] $Msg"
    Write-Host $line -ForegroundColor Cyan
    Add-Content -Path $scenarioLog $line
}

. (Join-Path $PSScriptRoot "modules\SmokeDiagnostics.ps1")
. (Join-Path $PSScriptRoot "modules\IpcFramingHelper.ps1")

Log "════════════════════════════════════════════════════════════════"
Log "SCENARIO 4: Live IDE <-> Extension Host Cross-Process IPC"
Log "════════════════════════════════════════════════════════════════"

$SmokeRoot = $PSScriptRoot
if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $SmokeRoot "..\..")).Path
}
if (-not $BinaryPath) {
    foreach ($candidate in @(
            (Join-Path $RepoRoot "build-win32\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "build-ninja-final\bin\RawrXD-Win32IDE.exe"),
            "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe"
        )) {
        if (Test-Path $candidate) { $BinaryPath = $candidate; break }
    }
}

if (-not (Test-Path $BinaryPath)) {
    Log "IDE binary missing: $BinaryPath" "ERROR"
    exit 1
}

$smokeVerbose = $Verbose -or ($env:RAWRXD_SMOKE_VERBOSE -eq '1')
$companions = Resolve-SmokeCompanionBinaries -BinaryPath $BinaryPath -RepoRoot $RepoRoot
if (-not $HostPath) { $HostPath = $companions.HostPath }
if (-not $PingPath) { $PingPath = $companions.PingPath }
$binDir = $companions.BinDir
if ($smokeVerbose) {
    Log "Resolved IDE binary: $BinaryPath"
    Log "Resolved repo root: $($companions.RepoRoot)"
    Log "Resolved extension host: $HostPath (exists=$(Test-Path $HostPath))"
    Log "Resolved IPC ping tool: $PingPath (exists=$(Test-Path $PingPath))"
}
$ideLogCandidates = @(
    (Join-Path $binDir "RawrXD_IDE.log"),
    (Join-Path $env:APPDATA "RawrXD\ide.log")
)

function Get-IdeLogTail {
    foreach ($path in $ideLogCandidates) {
        if (Test-Path $path) {
            return @{ Path = $path; Lines = @(Get-Content $path -Tail 150 -ErrorAction SilentlyContinue) }
        }
    }
    return $null
}

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

Get-Process -Name "RawrXD-Win32IDE","RawrXD-ExtensionHost" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

Log "Launching IDE (smoke env; IDE owns pipe server via ExtensionEngine.asm)"
$ideProc = Start-RawrIDESmokeProcess -BinaryPath $BinaryPath
if (-not $ideProc) {
    Log "Failed to start IDE" "ERROR"
    exit 1
}

Log "IDE PID=$($ideProc.Id); waiting for extension pipe (up to ${PipeWaitSeconds}s)..."

$pipeReady = $false
$pipeDeadline = [DateTime]::UtcNow.AddSeconds($PipeWaitSeconds)
while ([DateTime]::UtcNow -lt $pipeDeadline -and -not $ideProc.HasExited) {
    if (Test-ExtensionPipeListed -TimeoutMs 300) {
        $pipeReady = $true
        break
    }
    Start-Sleep -Milliseconds 200
}

if (-not $pipeReady) {
    Log "Extension pipe not listed in time" "ERROR"
    Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

Log "Extension pipe instance listed"

if ($ideProc.HasExited) {
    Log "IDE exited before client ping (code=$($ideProc.ExitCode))" "ERROR"
    exit 1
}

$clientExit = 0
$clientName = ""

if (Test-Path $HostPath) {
    $hostArgs = if ($FullSmoke -or $env:RAWRXD_SMOKE_IPC_FULL -eq "1") {
        "--connect-smoke", "--full-smoke"
    } else {
        "--connect-smoke"
    }
    Log "Running companion host: $HostPath $($hostArgs -join ' ')"
    $clientName = "RawrXD-ExtensionHost"
    & $HostPath @hostArgs
    $clientExit = $LASTEXITCODE
} elseif (Test-Path $PingPath) {
    Log "Extension host missing; using RawrXDIpcPing: $PingPath"
    $clientName = "RawrXDIpcPing"
    & $PingPath
    $clientExit = $LASTEXITCODE
} else {
    if (Test-Path $buildPingScript) {
        Log "Building RawrXDIpcPing into $binDir ..."
        & $buildPingScript -RepoRoot $RepoRoot -OutDir (Join-Path $SmokeRoot "tools")
        $builtPing = Join-Path $SmokeRoot "tools\RawrXDIpcPing.exe"
        if (Test-Path $builtPing) {
            Copy-Item $builtPing (Join-Path $binDir "RawrXDIpcPing.exe") -Force -ErrorAction SilentlyContinue
            $PingPath = Join-Path $binDir "RawrXDIpcPing.exe"
        }
    }
    if (Test-Path $PingPath) {
        $clientName = "RawrXDIpcPing"
        & $PingPath
        $clientExit = $LASTEXITCODE
    } else {
        Log "No IPC client binary (ExtensionHost or RawrXDIpcPing)" "ERROR"
        Log "  Expected host: $HostPath" "ERROR"
        Log "  Expected ping: $PingPath" "ERROR"
        Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
        exit 1
    }
}

if ($clientExit -ne 0) {
    $exitHint = switch ($clientExit) {
        101 { "pipe not ready (WaitNamedPipe timeout)" }
        102 { "CreateFile on pipe failed" }
        103 { "segmenter Begin failed" }
        104 { "empty wire blob" }
        105 { "WriteFile failed" }
        2  { "client binary missing or bad launch environment" }
        default { "see client tool logs" }
    }
    Log "$clientName failed with exit code $clientExit ($exitHint)" "ERROR"
    Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

Log "$clientName OK; waiting ${IngestWaitMs}ms for MessageReceiver ingest..."
Start-Sleep -Milliseconds $IngestWaitMs

$needLegacy = $true
$needSegment = $false
if ($FullSmoke -or $env:RAWRXD_SMOKE_IPC_FULL -eq "1") {
    $needLegacy = $true
    $needSegment = $true
}

for ($i = 0; $i -lt 40; $i++) {
    $tail = Get-IdeLogTail
    if ($tail -and $tail.Lines) {
        $legacyOk = -not $needLegacy
        $segmentOk = -not $needSegment
        if ($needLegacy) {
            $legacyOk = $null -ne ($tail.Lines | Select-String -Pattern "ExtensionEngine IPC: type=257" | Select-Object -First 1)
        }
        if ($needSegment) {
            $segmentOk = $null -ne ($tail.Lines | Select-String -Pattern "ExtensionEngine IPC: type=258" | Select-Object -First 1)
        }
        if (-not $needLegacy -and -not $needSegment) {
            $any = $tail.Lines | Select-String -Pattern "ExtensionEngine IPC:" | Select-Object -Last 1
            if ($any) { $legacyOk = $true; $segmentOk = $true }
        }
        if ($legacyOk -and $segmentOk) {
            Log "IPC log verification OK (legacy=$needLegacy segment=$needSegment)" "SUCCESS"
            Log "SCENARIO 4 RESULT: PASS" "SUCCESS"
            Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
            exit 0
        }
    }
    Start-Sleep -Milliseconds 150
}

Log "No ExtensionEngine IPC log line found" "ERROR"
if ($tail) {
    Log "Checked: $($tail.Path)" "WARN"
    $tail.Lines | Select-Object -Last 8 | ForEach-Object { Log $_ "WARN" }
}

Stop-Process -Id $ideProc.Id -Force -ErrorAction SilentlyContinue
exit 1
