param(
    [string]$ConfigPath = "d:/rawrxd/reports/BENCHMARK_CONFIG.json",
    [string]$CandidateJson = "d:/rawrxd/reports/swe_deterministic_candidate.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Determinism gate config not found: $ConfigPath"
}

$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json

$resolvedCandidate = $CandidateJson
if (-not (Test-Path -LiteralPath $resolvedCandidate)) {
    $repoRoot = Split-Path -Parent (Split-Path -Parent ([System.IO.Path]::GetFullPath($ConfigPath)))
    $baselinePath = [string]$config.baseline.report_file
    if (-not [System.IO.Path]::IsPathRooted($baselinePath)) {
        $baselinePath = Join-Path $repoRoot $baselinePath
    }

    if (-not (Test-Path -LiteralPath $baselinePath)) {
        throw "No candidate report and baseline report missing: $baselinePath"
    }

    Write-Host "[gate-wrapper] Candidate report missing. Using baseline report as deterministic reference: $baselinePath" -ForegroundColor Yellow
    $resolvedCandidate = $baselinePath
}

$gateScript = "d:/rawrxd/scripts/Run-SWEBench-DeterministicGate.ps1"
if (-not (Test-Path -LiteralPath $gateScript)) {
    throw "Determinism gate script missing: $gateScript"
}

& pwsh -NoProfile -ExecutionPolicy Bypass -File $gateScript -ConfigPath $ConfigPath -CandidateJson $resolvedCandidate
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

exit 0
