param(
  [int]$HttpPort = 8080,
  [int]$WsPort = 8081,
  [int]$BackendPort = 11434,
  [string]$BackendHost = '127.0.0.1',
  [string]$WebRoot = (Resolve-Path "$PSScriptRoot").Path,
  [string]$Url = '',
  [string]$BridgeExe = (Join-Path $PSScriptRoot 'build_ninja\bin\RawrXD-Standalone-WebBridge.exe'),
  [string]$BrowserExe = (Join-Path $PSScriptRoot 'build_ninja\bin\RawrXD-InHouseBrowser.exe')
)

$ErrorActionPreference = 'Stop'

function Wait-HttpReady([string]$ProbeUrl, [int]$Seconds = 8) {
  $deadline = (Get-Date).AddSeconds($Seconds)
  do {
    try {
      $r = Invoke-WebRequest -UseBasicParsing -TimeoutSec 1 -Uri $ProbeUrl
      if ($r.StatusCode -ge 200 -and $r.StatusCode -lt 500) { return $true }
    } catch {}
    Start-Sleep -Milliseconds 150
  } while ((Get-Date) -lt $deadline)
  return $false
}

if (-not $Url) { $Url = "http://127.0.0.1:$HttpPort/gui/ide_chatbot.html" }

$useNativeBridge = (Test-Path -LiteralPath $BridgeExe)
$bridge = $null

try {
  if ($useNativeBridge) {
    $bridgeArgs = @("$HttpPort", "$WsPort", "$BackendPort", "$BackendHost", "$WebRoot")
    $bridge = Start-Process -FilePath $BridgeExe -ArgumentList $bridgeArgs -PassThru -WindowStyle Hidden
    [void](Wait-HttpReady -ProbeUrl "http://127.0.0.1:$HttpPort/api/status")

    if (Test-Path -LiteralPath $BrowserExe) {
      $browser = Start-Process -FilePath $BrowserExe -ArgumentList @($Url) -PassThru
      Wait-Process -Id $browser.Id
    } else {
      Start-Process $Url | Out-Null
      Write-Host "Opened $Url"
      Write-Host "Press Enter to stop the bridge..."
      Read-Host | Out-Null
    }
  } else {
    $serverJs = Join-Path $PSScriptRoot 'server.js'
    if (-not (Test-Path -LiteralPath $serverJs)) {
      throw "Neither native bridge nor Node server was found."
    }
    if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
      throw "node.exe not found in PATH. Install Node.js or build RawrXD-Standalone-WebBridge.exe."
    }

    Write-Host "Native bridge not found. Using Node fallback server.js on port $HttpPort..."
    $bridge = Start-Process -FilePath node -WorkingDirectory $PSScriptRoot -ArgumentList @($serverJs) -PassThru -WindowStyle Hidden
    [void](Wait-HttpReady -ProbeUrl "http://127.0.0.1:$HttpPort/status")

    Start-Process $Url | Out-Null
    Write-Host "Opened $Url"
    Write-Host "Press Enter to stop server.js..."
    Read-Host | Out-Null
  }
}
finally {
  if ($bridge -and -not $bridge.HasExited) {
    Stop-Process -Id $bridge.Id -Force
  }
}
