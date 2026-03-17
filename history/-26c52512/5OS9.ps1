Param(
  [string]$ConfigPath = "config.example.json",
  [switch]$SetEnv
)

# Read beacon path from config
if (!(Test-Path $ConfigPath)) {
  Write-Host "Config not found: $ConfigPath" -ForegroundColor Red
  exit 2
}

try {
  $cfg = Get-Content $ConfigPath -Raw | ConvertFrom-Json
} catch {
  Write-Host "Failed to parse config: $ConfigPath" -ForegroundColor Red
  exit 3
}

$beaconPath = $cfg.featureBeaconPath
if (-not $beaconPath) { $beaconPath = "./feature_beacon.json" }

if (!(Test-Path $beaconPath)) {
  Write-Host "Beacon not found: $beaconPath" -ForegroundColor Red
  exit 4
}

try {
  $beacon = Get-Content $beaconPath -Raw | ConvertFrom-Json
} catch {
  Write-Host "Failed to parse beacon: $beaconPath" -ForegroundColor Red
  exit 5
}

$feat = $beacon.features.gemini3_flash_preview
if (-not $feat) {
  Write-Host "Feature gemini3_flash_preview not present in beacon" -ForegroundColor Yellow
  exit 6
}

if ($feat.enabled) {
  Write-Host "Gemini 3 Flash (Preview) is ENABLED for all clients" -ForegroundColor Green
  Write-Host "Default Model: $($feat.defaultModel)" -ForegroundColor Cyan
  Write-Host "Expires: $($feat.expires) | Channel: $($feat.beacon.channel)" -ForegroundColor Gray

  if ($SetEnv) {
    $env:ENABLE_GEMINI_3_FLASH_PREVIEW = "true"
    $env:DEFAULT_MODEL = $feat.defaultModel
    Write-Host "Environment variables set for current session." -ForegroundColor Green
  }

  exit 0
} else {
  Write-Host "Gemini 3 Flash (Preview) is DISABLED" -ForegroundColor Yellow
  exit 1
}
