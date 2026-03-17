# Deterministic-RawrXD-Rebuild.ps1
# Auto-generated preflight check before build

param(
    [string]$DatasetDir = ".\rawrxd_dataset",
    [switch]$StrictMode
)

$manifest = Get-Content "$DatasetDir\complete_manifest.json" -Raw | ConvertFrom-Json
$random   = Get-Content "$DatasetDir\random.json" -Raw | ConvertFrom-Json

$entropyCount = 0
if ($random.UnmappedFunctions)   { $entropyCount += $random.UnmappedFunctions.Count }
if ($random.OrphanSymbols)       { $entropyCount += $random.OrphanSymbols.Count }
if ($random.MysteryStructs)      { $entropyCount += $random.MysteryStructs.Count }

Write-Host "=== RawrXD Pre-Build Validation ===" -ForegroundColor Cyan
Write-Host "Files: $($manifest.Meta.TotalFiles)"
Write-Host "Lines: $($manifest.Meta.TotalLines)"
Write-Host "Entropy Score: $entropyCount"

if ($entropyCount -gt 0) {
    Write-Warning "RANDOM BUCKET NOT EMPTY ($entropyCount items)."
    if ($StrictMode) {
        Write-Error "StrictMode: Refusing non-deterministic build."
        exit 1
    }
}

# Validate critical messages
$cmds = Get-Content "$DatasetDir\commands.json" -Raw | ConvertFrom-Json
foreach ($kb in $cmds.KeyBindings) {
    if ($kb.Status -eq "MISSING") {
        Write-Warning "Critical message handler MISSING: $($kb.Message)"
    }
}

# Validate subsystems
$subs = Get-Content "$DatasetDir\subsystems.json" -Raw | ConvertFrom-Json
if ($subs.Rendering.Status -eq "Absent") {
    Write-Error "CRITICAL: No rendering subsystem. IDE will show white screen."
}

Write-Host "`nPreflight complete." -ForegroundColor Green
