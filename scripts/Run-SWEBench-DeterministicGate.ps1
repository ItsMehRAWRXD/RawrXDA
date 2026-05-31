param(
    [string]$BuildDir = "d:/rawrxd/build",
    [string]$ExePath,
    [string]$ConfigPath = "d:/rawrxd/reports/BENCHMARK_CONFIG.json",
    [string]$OutputJson = "d:/rawrxd/reports/swe_deterministic_candidate.json",
    [string]$OutputJsonl = "d:/rawrxd/reports/swe_deterministic_candidate.jsonl",
    [string]$Model,
    [string]$OllamaHost,
    [int]$Port,
    [int]$MaxTasks,
    [int]$MaxOutputTokens,
    [switch]$Run,
    [string]$CandidateJson,
    [double]$MinPassAt1 = -1,
    [double]$MinPatchCorrectness = -1,
    [double]$MaxRegressionPassAt1 = 0.02,
    [double]$MaxRegressionPatchCorrectness = 0.02
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-JsonFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "File not found: $Path"
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Resolve-ExecutablePath([string]$ExplicitExe, [string]$BuildRoot) {
    if ($ExplicitExe -and $ExplicitExe.Trim().Length -gt 0) {
        return $ExplicitExe
    }
    return (Join-Path $BuildRoot "bin/RawrXD-SWEBench.exe")
}

function Ensure-DirForFile([string]$FilePath) {
    $dir = Split-Path -Parent $FilePath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

function Get-MetricValue($obj, [string[]]$keys, [double]$fallback = 0.0) {
    foreach ($k in $keys) {
        $p = $obj.PSObject.Properties[$k]
        if ($null -ne $p -and $null -ne $p.Value) {
            return [double]$p.Value
        }
    }
    return $fallback
}

$config = Read-JsonFile $ConfigPath
$baselineSummary = $config.results_summary
$baselineMeta = $config.baseline
$repoRoot = Split-Path -Parent (Split-Path -Parent ([System.IO.Path]::GetFullPath($ConfigPath)))

if ($MinPassAt1 -lt 0) {
    $MinPassAt1 = [double]$baselineSummary.pass_at_1
}
if ($MinPatchCorrectness -lt 0) {
    $MinPatchCorrectness = [double]$baselineSummary.patch_correctness
}

if (-not $OllamaHost -or $OllamaHost.Trim().Length -eq 0) {
    $OllamaHost = [string]$baselineMeta.host
}
if ($Port -le 0) {
    $Port = [int]$baselineMeta.port
}
if ($MaxTasks -le 0) {
    $MaxTasks = [int]$baselineMeta.max_tasks
}
if ($MaxOutputTokens -le 0) {
    $MaxOutputTokens = [int]$baselineMeta.max_output_tokens
}
if (-not $Model -or $Model.Trim().Length -eq 0) {
    $Model = [string]$baselineMeta.model
}

if ($Run) {
    $exe = Resolve-ExecutablePath -ExplicitExe $ExePath -BuildRoot $BuildDir
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "SWE harness executable not found: $exe"
    }

    Ensure-DirForFile $OutputJson
    Ensure-DirForFile $OutputJsonl

    $args = @(
        "--real-agent",
        "--host", $OllamaHost,
        "--port", "$Port",
        "--model", $Model,
        "--max-tasks", "$MaxTasks",
        "--max-output-tokens", "$MaxOutputTokens",
        "--json", $OutputJson,
        "--jsonl", $OutputJsonl,
        "--deterministic",
        "--no-summary-json"
    )

    Write-Host "[gate] Running deterministic lane:" -ForegroundColor Cyan
    Write-Host "[gate] $exe $($args -join ' ')" -ForegroundColor DarkCyan

    & $exe @args
    if ($LASTEXITCODE -ne 0) {
        throw "SWE harness run failed with exit code $LASTEXITCODE"
    }

    $CandidateJson = $OutputJson
}

if (-not $CandidateJson -or $CandidateJson.Trim().Length -eq 0) {
    throw "Candidate JSON is required. Pass -Run or -CandidateJson <path>."
}

$candidate = Read-JsonFile $CandidateJson
$baselinePath = [string]$baselineMeta.report_file
if (-not [System.IO.Path]::IsPathRooted($baselinePath)) {
    $baselinePath = Join-Path $repoRoot $baselinePath
}
$baseline = Read-JsonFile $baselinePath

$cPass = Get-MetricValue $candidate @("pass_at_1", "pass@1", "overall_score")
$cPatch = Get-MetricValue $candidate @("patch_correctness")
$bPass = Get-MetricValue $baseline @("pass_at_1", "pass@1", "overall_score")
$bPatch = Get-MetricValue $baseline @("patch_correctness")

$passDelta = $cPass - $bPass
$patchDelta = $cPatch - $bPatch

Write-Host "[gate] Candidate pass@1:          $([math]::Round($cPass, 4))" -ForegroundColor Gray
Write-Host "[gate] Baseline pass@1:           $([math]::Round($bPass, 4))" -ForegroundColor Gray
Write-Host "[gate] Candidate patch_correctness: $([math]::Round($cPatch, 4))" -ForegroundColor Gray
Write-Host "[gate] Baseline patch_correctness:  $([math]::Round($bPatch, 4))" -ForegroundColor Gray
Write-Host "[gate] pass@1 delta:             $([math]::Round($passDelta, 4))" -ForegroundColor Gray
Write-Host "[gate] patch_correctness delta:  $([math]::Round($patchDelta, 4))" -ForegroundColor Gray

$failed = $false

if ($cPass -lt $MinPassAt1) {
    Write-Host "[gate] FAIL: pass@1 below minimum threshold ($MinPassAt1)." -ForegroundColor Red
    $failed = $true
}
if ($cPatch -lt $MinPatchCorrectness) {
    Write-Host "[gate] FAIL: patch_correctness below minimum threshold ($MinPatchCorrectness)." -ForegroundColor Red
    $failed = $true
}
if ($passDelta -lt -1.0 * $MaxRegressionPassAt1) {
    Write-Host "[gate] FAIL: pass@1 regression exceeds tolerance ($MaxRegressionPassAt1)." -ForegroundColor Red
    $failed = $true
}
if ($patchDelta -lt -1.0 * $MaxRegressionPatchCorrectness) {
    Write-Host "[gate] FAIL: patch_correctness regression exceeds tolerance ($MaxRegressionPatchCorrectness)." -ForegroundColor Red
    $failed = $true
}

if ($failed) {
    exit 1
}

Write-Host "[gate] PASS: deterministic SWE benchmark gate passed." -ForegroundColor Green
exit 0
