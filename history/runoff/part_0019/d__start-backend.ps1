param(
  [int]$Port = 8080
)

$ErrorActionPreference = 'Stop'
$logDir = 'D:\logs'
$logFile = Join-Path $logDir 'mycopilot-backend.log'

if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

$env:PORT = "$Port"

$node = 'node'
$script = 'D:\MyCoPilot-Complete-Portable\backend-server.js'
$stdout = Join-Path $logDir 'mycopilot-backend.out.log'
$stderr = Join-Path $logDir 'mycopilot-backend.err.log'

"=== Starting backend at $(Get-Date) on port $env:PORT ===" | Tee-Object -FilePath $logFile -Append | Out-Null
Start-Process -FilePath $node -ArgumentList $script -RedirectStandardOutput $stdout -RedirectStandardError $stderr -NoNewWindow
Start-Sleep -Milliseconds 800
Get-Content $stdout -Tail 20 | ForEach-Object { "[STDOUT] $_" } | Add-Content $logFile
Get-Content $stderr -Tail 20 | ForEach-Object { "[STDERR] $_" } | Add-Content $logFile
Start-Sleep -Seconds 2
try {
  $status = (Invoke-WebRequest -UseBasicParsing "http://localhost:$env:PORT/health" -TimeoutSec 3).StatusCode
  Write-Host "Health: $status"; "Health: $status" | Add-Content $logFile
} catch {
  Write-Host 'Health check failed'; 'Health check failed' | Add-Content $logFile
}
