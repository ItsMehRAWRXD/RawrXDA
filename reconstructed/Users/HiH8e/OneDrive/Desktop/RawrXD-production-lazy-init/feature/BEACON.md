# Feature Beacon (Beaconism) – Global Feature Rollout

This repository uses a central JSON beacon (`feature_beacon.json`) to coordinate feature rollouts across all clients without changing application code. Clients read the beacon at startup and during runtime to adjust behaviors.

## Key Concept
- Single source of truth for feature flags and model selections
- Safe defaults + explicit overrides
- Works offline (local file) and can be swapped with remote URL later

## Current Beacon
- File: `feature_beacon.json`
- Feature: `gemini3_flash_preview`
- Status: Enabled for `all-clients`
- Default model: `gemini-3-flash-preview`
- Fallback model: `llama-3.1-8b-instruct`
- Expiry: `2026-03-31`

## Client Consumption Pattern
1. Read path from config `featureBeaconPath` (defaults to `./feature_beacon.json`).
2. Parse `features.gemini3_flash_preview.enabled` and `defaultModel`.
3. If enabled, set:
   - ENV `ENABLE_GEMINI_3_FLASH_PREVIEW=true`
   - ENV `DEFAULT_MODEL=gemini-3-flash-preview`
4. Respect `safety.failClosed` and rate limits.

## Safety & Overrides
- `safety.failClosed=false` → clients may continue with `fallbackModel` if preview fails.
- Enforced rate limit: `maxRequestRatePerClient=60` req/min.
- Local override:
  - Set `ENV ENABLE_GEMINI_3_FLASH_PREVIEW=false` to disable on a machine.

## Example PowerShell Consumption
```powershell
$beaconPath = (Get-Content "config.example.json" | ConvertFrom-Json).featureBeaconPath
$beacon     = Get-Content $beaconPath | ConvertFrom-Json
$feature    = $beacon.features.gemini3_flash_preview

if ($feature.enabled) {
  $env:ENABLE_GEMINI_3_FLASH_PREVIEW = "true"
  $env:DEFAULT_MODEL = $feature.defaultModel
  Write-Host "Gemini 3 Flash preview enabled for all clients" -ForegroundColor Green
} else {
  Write-Host "Gemini 3 Flash preview disabled" -ForegroundColor Yellow
}
```

## Rollout Channels
- `channel: preview` → Feature is pre-release; telemetry recommended.
- Promote by updating `feature_beacon.json`.

## CI Validation
Use `scripts/beacon_check.ps1` to assert beacon settings in CI.
