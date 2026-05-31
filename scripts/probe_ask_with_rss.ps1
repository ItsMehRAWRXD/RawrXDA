param(
    [string]$ExePath = "d:\rawrxd\build\bin\RawrXD-Win32IDE.exe",
    [string]$TitanHostPath = "d:\rawrxd\build\bin\RawrXD-TitanHost.exe",
    [string]$ModelPath = "d:\ministral3.gguf",
    [string]$BaseUrl = "http://127.0.0.1:11435",
    [int]$Port = 11435,
    [int]$HealthWaitMs = 9000,
    [int]$AskTimeoutSec = 12
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $ExePath)) {
    Write-Error "ExePath not found: $ExePath"
    exit 2
}
if (-not (Test-Path $TitanHostPath)) {
    Write-Error "TitanHostPath not found: $TitanHostPath"
    exit 2
}

$env:RAWRXD_TITAN_HOST_PATH = $TitanHostPath
$env:RAWRXD_NATIVE_MODEL_PATH = $ModelPath

$stdout = "d:\rawrxd\tmp_probe_ask_stdout.log"
$stderr = "d:\rawrxd\tmp_probe_ask_stderr.log"
$result = "d:\rawrxd\tmp_probe_ask_results.txt"

Remove-Item $stdout, $stderr, $result -Force -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $ExePath -ArgumentList "--headless", "--port", "$Port" -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr

try {
    $ready = $false
    $attempts = [Math]::Max(1, [int]($HealthWaitMs / 300))
    for ($i = 0; $i -lt $attempts; $i++) {
        try {
            Invoke-WebRequest -Uri "$BaseUrl/health" -Method Get -UseBasicParsing -TimeoutSec 2 | Out-Null
            $ready = $true
            break
        } catch {
            Start-Sleep -Milliseconds 300
        }
    }

    Add-Content $result "pid=$($proc.Id)"
    Add-Content $result "health_ready=$ready"

    if (-not $ready) {
        Add-Content $result "fatal=server_not_ready"
        Get-Content $result
        exit 1
    }

    $rssBefore = (Get-Process -Id $proc.Id).WorkingSet64
    Add-Content $result "rss_before=$rssBefore"

    $loadBody = @{ path = $ModelPath } | ConvertTo-Json
    try {
        $loadResp = Invoke-WebRequest -Uri "$BaseUrl/api/model/load" -Method Post -Body $loadBody -ContentType "application/json" -UseBasicParsing -TimeoutSec 12
        Add-Content $result "model_load_code=$([int]$loadResp.StatusCode)"
    } catch {
        Add-Content $result "model_load_err=$($_.Exception.Message)"
    }

    $askBody = @{ prompt = "hello" } | ConvertTo-Json
    try {
        $askResp = Invoke-WebRequest -Uri "$BaseUrl/ask" -Method Post -Body $askBody -ContentType "application/json" -UseBasicParsing -TimeoutSec $AskTimeoutSec
        Add-Content $result "ask_code=$([int]$askResp.StatusCode)"
        Add-Content $result "ask_len=$($askResp.Content.Length)"
    } catch {
        Add-Content $result "ask_err=$($_.Exception.Message)"
    }

    $aliveAfter = [bool](Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)
    Add-Content $result "server_alive_after=$aliveAfter"

    if ($aliveAfter) {
        $rssAfter = (Get-Process -Id $proc.Id).WorkingSet64
    } else {
        $rssAfter = 0
    }
    Add-Content $result "rss_after=$rssAfter"
    Add-Content $result "rss_delta=$($rssAfter - $rssBefore)"

    $hostProc = Get-Process RawrXD-TitanHost -ErrorAction SilentlyContinue | Select-Object -First 1 Id, Path
    if ($hostProc) {
        Add-Content $result "titan_pid=$($hostProc.Id)"
        Add-Content $result "titan_path=$($hostProc.Path)"
    } else {
        Add-Content $result "titan_pid=NONE"
    }

    Get-Content $result
    "--- stderr tail ---"
    Get-Content $stderr -Tail 40
}
finally {
    if (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue) {
        Stop-Process -Id $proc.Id -Force
    }
}
