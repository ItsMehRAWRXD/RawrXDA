# run_real_model_tests.ps1
# Smoke-test harness for 1B/7B/70B GGUF models (download + basic validation placeholders)
# Usage: pwsh -File scripts/run_real_model_tests.ps1 -Config .\config\real_models.json
param(
    [string]$Config = "config/real_models.json",
    [switch]$DownloadOnly
)

if (!(Test-Path $Config)) { throw "Config not found: $Config" }
$models = Get-Content $Config | ConvertFrom-Json

$logDir = "logs/real-model-tests"
if (!(Test-Path $logDir)) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }

foreach ($m in $models) {
    $name = $m.name
    $url  = $m.url
    $dest = $m.dest
    Write-Host "[INFO] Model: $name -> $dest" -ForegroundColor Cyan

    if (!(Test-Path (Split-Path $dest -Parent))) { New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null }

    if (!(Test-Path $dest)) {
        Write-Host "[INFO] Downloading $url" -ForegroundColor Yellow
        $code = (New-Object Net.WebClient)
        try {
            $code.DownloadFile($url, $dest)
        } catch {
            Write-Warning "Download failed for $name : $_"
            continue
        }
    } else {
        Write-Host "[INFO] Cached file found: $dest" -ForegroundColor Green
    }

    $size = (Get-Item $dest).Length
    Write-Host "[INFO] Size = $size bytes" -ForegroundColor DarkGray

    if ($DownloadOnly) { continue }

    # Placeholder: invoke loader once binary is available
    # Example: & .\bin\gguf_loader.exe --model $dest --test basic --log $logDir/$name.log
    Write-Host "[WARN] Loader execution not wired in this script. Hook your binary/exe here." -ForegroundColor Magenta
}
