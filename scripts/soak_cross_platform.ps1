$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$env:SOAK_LOG_DIR = if ($env:SOAK_LOG_DIR) { $env:SOAK_LOG_DIR } else { Join-Path $Root "soak_logs" }
New-Item -ItemType Directory -Force -Path $env:SOAK_LOG_DIR | Out-Null
& python3 scripts/soak_cross_platform.py @args
