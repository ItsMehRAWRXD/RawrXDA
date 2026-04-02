# Run MoE grouped-GEMM microbench: single large shape (default) or full --sweep grid.
# Requires: cmake --build <dir> --target moe_grouped_gemm_bench
param(
    [switch] $Sweep,
    [string] $BuildDir = "",
    [string] $OutCsv = "",
    [string] $SummaryTxt = "",
    [int] $InDim = 4096,
    [int] $OutDim = 4096,
    [int] $K = 4,
    [int] $Repeat = 8,
    [int] $Iters = 25,
    [int] $Warmup = 5
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $repo "build-ninja"
}
$exe = Join-Path $BuildDir "tests/moe_grouped_gemm_bench.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Missing $exe — build with: cmake --build `"$BuildDir`" --target moe_grouped_gemm_bench"
}

if ($Sweep) {
    if (-not $OutCsv) { $OutCsv = Join-Path $BuildDir "moe_grouped_gemm_sweep.csv" }
    Write-Host "Full sweep -> $OutCsv (300 cases: 5×5 dims × K∈{2,4,8,16} × repeat∈{1,8,32}; heavy RAM/time at 4096²)"
    & $exe --sweep $OutCsv
} else {
    if (-not $OutCsv) { $OutCsv = Join-Path $BuildDir "moe_grouped_gemm_single.csv" }
    Write-Host "Single case in=$InDim out=$OutDim K=$K rep=$Repeat it=$Iters -> $OutCsv"
    & $exe $OutCsv $InDim $OutDim $K $Repeat $Iters $Warmup
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Sweep -and (Test-Path -LiteralPath $OutCsv)) {
    $rows = Import-Csv -LiteralPath $OutCsv
    $n = $rows.Count
    $looped = @($rows | Where-Object { $_.policy_timed -eq "looped_faster" }).Count
    $grouped = @($rows | Where-Object { $_.policy_timed -eq "grouped_faster" }).Count
    $other = $n - $looped - $grouped
    Write-Host "Sweep summary: rows=$n policy_timed: looped_faster=$looped grouped_faster=$grouped other=$other"
    $gf = @($rows | Where-Object { $_.policy_timed -eq "grouped_faster" } | Sort-Object { [int64]$_.inDim }, { [int64]$_.outDim }, { [int]$_.numExperts })
    if ($gf.Count -gt 0) {
        $g0 = $gf[0]
        Write-Host "  First grouped_faster (in×out×K sort): in=$($g0.inDim) out=$($g0.outDim) K=$($g0.numExperts) rep=$($g0.repeat)"
    } else {
        Write-Host "  No grouped_faster rows in this sweep (looped wins all timed cases)."
    }
    $lines = @(
        "moe_grouped_gemm_bench sweep summary",
        "csv: $OutCsv",
        "rows: $n",
        "policy_timed looped_faster: $looped",
        "policy_timed grouped_faster: $grouped",
        "policy_timed other: $other"
    )
    if ($gf.Count -gt 0) {
        $g0 = $gf[0]
        $lines += "first grouped_faster: in=$($g0.inDim) out=$($g0.outDim) K=$($g0.numExperts) rep=$($g0.repeat)"
    }
    if (-not $SummaryTxt) { $SummaryTxt = [System.IO.Path]::ChangeExtension($OutCsv, ".summary.txt") }
    $lines | Set-Content -LiteralPath $SummaryTxt -Encoding utf8
    Write-Host "Wrote $SummaryTxt"
}

Write-Host "Done."
# Post-process: .\PostProcess-MoeGroupedGemmSweep.ps1 -InputCsv <sweep.csv> → decision_surface + grouped_thresholds.csv
