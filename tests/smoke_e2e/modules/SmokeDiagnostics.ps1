# Crash-log correlation helpers for live IDE smoke scenarios

function Start-RawrIDESmokeProcess {
    param(
        [string]$BinaryPath,
        [hashtable]$ExtraEnvironment = @{}
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $BinaryPath
    $psi.UseShellExecute = $false
    $psi.WorkingDirectory = Split-Path -Parent $BinaryPath

    $psi.EnvironmentVariables['RAWRXD_SMOKE_DEFERRED_INIT'] = '1'
    $psi.EnvironmentVariables['RAWRXD_DISABLE_SESSION_RESTORE'] = '1'
    $psi.EnvironmentVariables['RAWRXD_SMOKE_SKIP_GPU'] = '1'
    $psi.EnvironmentVariables['RAWRXD_SMOKE_ENABLE_COPILOT_CHAT'] = '1'
    if ($env:RAWRXD_SMOKE_GDI_TOLERANCE) {
        $psi.EnvironmentVariables['RAWRXD_SMOKE_GDI_TOLERANCE'] = [string]$env:RAWRXD_SMOKE_GDI_TOLERANCE
    }
    foreach ($kv in $ExtraEnvironment.GetEnumerator()) {
        $psi.EnvironmentVariables[$kv.Key] = [string]$kv.Value
    }

    return [System.Diagnostics.Process]::Start($psi)
}

function Resolve-SmokeCompanionBinaries {
    param(
        [string]$BinaryPath,
        [string]$RepoRoot = ""
    )

    $binDir = Split-Path -Parent $BinaryPath
    if (-not $RepoRoot) {
        if ($env:GITHUB_WORKSPACE -and (Test-Path $env:GITHUB_WORKSPACE)) {
            $RepoRoot = $env:GITHUB_WORKSPACE
        } else {
            $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
        }
    }

    $hostExe = Join-Path $binDir "RawrXD-ExtensionHost.exe"
    $pingExe = Join-Path $binDir "RawrXDIpcPing.exe"

    $hostCandidates = @(
        $hostExe,
        (Join-Path $RepoRoot "build-win32\bin\RawrXD-ExtensionHost.exe"),
        (Join-Path $RepoRoot "build-ninja-final\bin\RawrXD-ExtensionHost.exe"),
        "d:\rxdn_ninja\bin\RawrXD-ExtensionHost.exe"
    )
    foreach ($c in $hostCandidates) {
        if ($c -and (Test-Path $c)) { $hostExe = $c; break }
    }

    $pingCandidates = @(
        $pingExe,
        (Join-Path (Split-Path $PSScriptRoot -Parent) "tools\RawrXDIpcPing.exe"),
        (Join-Path $RepoRoot "build-win32\bin\RawrXDIpcPing.exe"),
        "d:\rxdn_ninja\bin\RawrXDIpcPing.exe"
    )
    foreach ($c in $pingCandidates) {
        if ($c -and (Test-Path $c)) { $pingExe = $c; break }
    }

    return @{
        BinDir   = $binDir
        HostPath = $hostExe
        PingPath = $pingExe
        RepoRoot = $RepoRoot
    }
}

function Get-RawrXDCrashLogCandidates {
    param([string]$BinaryPath = 'd:\rxdn_ninja\bin\RawrXD-Win32IDE.exe')
    $dir = Split-Path -Parent $BinaryPath
    @(
        (Join-Path $dir 'rawrxd_crash.log'),
        'D:\rawrxd\rawrxd_crash.log',
        'D:\rxdn_ninja\bin\rawrxd_crash.log'
    ) | Select-Object -Unique
}

function Write-EarlyExitDiagnostics {
    param(
        [string]$BinaryPath,
        [int]$ProcessId = 0,
        [scriptblock]$OnLog
    )

    $lines = @()
    $lines += "Early exit diagnostics: PID=$ProcessId"

    foreach ($path in (Get-RawrXDCrashLogCandidates -BinaryPath $BinaryPath)) {
        if (Test-Path $path) {
            $all = Get-Content $path -ErrorAction SilentlyContinue
            $tail = @()
            for ($i = $all.Count - 1; $i -ge 0 -and $tail.Count -lt 40; $i--) {
                if ($all[$i] -match '^=== CRASH (\d{4}-\d{2}-\d{2})') {
                    $crashDate = $Matches[1]
                    $today = (Get-Date).ToString('yyyy-MM-dd')
                    if ($crashDate -eq $today) {
                        $tail += $all[$i]
                        for ($j = $i + 1; $j -lt $all.Count; $j++) {
                            $tail += $all[$j]
                            if ($all[$j] -eq '=== END CRASH ===') { break }
                        }
                        break
                    }
                }
            }
            if ($tail.Count -eq 0) {
                $tail = $all | Select-Object -Last 12
            }
            $lines += "Crash log tail ($path):"
            $lines += $tail
            break
        }
    }

    if ($ProcessId -gt 0) {
        $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
        if ($proc) {
            $lines += "Process still running (unexpected if early-exit path fired)."
        } else {
            $lines += "Process handle is gone (terminated)."
        }
    }

    foreach ($line in $lines) {
        if ($OnLog) {
            & $OnLog $line 'WARN'
        } else {
            Write-Warning $line
        }
    }
}
