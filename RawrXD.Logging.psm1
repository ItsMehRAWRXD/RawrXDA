# RawrXD.Logging.psm1
# Production-ready, dependency-free structured logging for RawrXD

function Get-RawrXDLogPath {
    $overridePath = $env:RAWRXD_LOG_PATH
    if ($overridePath) {
        try {
            $overrideDir = Split-Path -Parent $overridePath
            if ($overrideDir -and -not (Test-Path $overrideDir)) {
                New-Item -ItemType Directory -Path $overrideDir -Force | Out-Null
            }
        } catch {
            # Fall back to default path resolution.
        }
        if (Test-Path (Split-Path -Parent $overridePath)) {
            return $overridePath
        }
    }

    $envRoot = $env:LAZY_INIT_IDE_ROOT
    $root = if ($envRoot -and (Test-Path $envRoot)) { $envRoot } else { $PSScriptRoot }

    $logDir = Join-Path $root "logs"
    if (-not (Test-Path $logDir)) {
        New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    }

    return (Join-Path $logDir "RawrXD.log")
}

function Write-StructuredLog {
    param(
        [string]$Level,
        [string]$Message,
        [string]$Function = $null,
        [hashtable]$Data = $null
    )
    $timestamp = Get-Date -Format o
    $logEntry = [ordered]@{
        Timestamp = $timestamp
        Level     = $Level
        Function  = $Function
        Message   = $Message
        Data      = $Data
    }
    $json = $logEntry | ConvertTo-Json -Compress
    $logPath = Get-RawrXDLogPath
    Add-Content -Path $logPath -Value $json -ErrorAction SilentlyContinue
    if ($Level -eq 'ERROR' -or $Level -eq 'FATAL') {
        Write-Host "[RawrXD][${Level}] $Message" -ForegroundColor Red
    } elseif ($Level -eq 'WARN') {
        Write-Host "[RawrXD][${Level}] $Message" -ForegroundColor Yellow
    } elseif ($Level -eq 'INFO') {
        Write-Host "[RawrXD][${Level}] $Message" -ForegroundColor Green
    } else {
        Write-Host "[RawrXD][${Level}] $Message"
    }
}

function Measure-FunctionLatency {
    param(
        [ScriptBlock]$Script,
        [string]$FunctionName
    )
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $result = & $Script
        $sw.Stop()
        Write-StructuredLog -Level 'INFO' -Message "${FunctionName} completed in $($sw.ElapsedMilliseconds) ms" -Function $FunctionName -Data @{ LatencyMs = $sw.ElapsedMilliseconds }
        return $result
    } catch {
        $sw.Stop()
        Write-StructuredLog -Level 'ERROR' -Message $_.Exception.Message -Function $FunctionName -Data @{ LatencyMs = $sw.ElapsedMilliseconds }
        throw
    }
}

Export-ModuleMember -Function Write-StructuredLog, Measure-FunctionLatency, Get-RawrXDLogPath
