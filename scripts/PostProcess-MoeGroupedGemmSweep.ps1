# Build decision_surface.csv (all rows + work_product) and grouped_thresholds.csv (aggregates for grouped_faster).
# Usage: .\PostProcess-MoeGroupedGemmSweep.ps1 -InputCsv build-ninja\moe_grouped_gemm_sweep.csv
param(
    [Parameter(Mandatory = $true)]
    [string] $InputCsv,
    [string] $OutDecisionSurface = "",
    [string] $OutGroupedThresholds = ""
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $InputCsv)) {
    Write-Error "Input CSV not found: $InputCsv"
}

$dir = Split-Path -Parent $InputCsv
if (-not $OutDecisionSurface) {
    $OutDecisionSurface = Join-Path $dir "moe_grouped_gemm_decision_surface.csv"
}
if (-not $OutGroupedThresholds) {
    $OutGroupedThresholds = Join-Path $dir "grouped_thresholds.csv"
}

$rows = Import-Csv -LiteralPath $InputCsv
$enriched = foreach ($r in $rows) {
    $inD = [int64]$r.inDim
    $outD = [int64]$r.outDim
    $k = [int]$r.numExperts
    $work = $inD * $outD * $k
    [pscustomobject]@{
        inDim                = $r.inDim
        outDim               = $r.outDim
        numExperts           = $r.numExperts
        repeat               = $r.repeat
        iters                = $r.iters
        warmup               = $r.warmup
        avg_ms_looped        = $r.avg_ms_looped
        avg_ms_pack          = $r.avg_ms_pack
        avg_ms_gemm          = $r.avg_ms_gemm
        avg_ms_grouped_total = $r.avg_ms_grouped_total
        avg_ms_cached_per_token = $r.avg_ms_cached_per_token
        ratio_loop_over_group = $r.ratio_loop_over_group
        policy_timed         = $r.policy_timed
        policy_heuristic     = $r.policy_heuristic
        work_product         = $work
        timed_grouped_faster = ($r.policy_timed -eq "grouped_faster")
    }
}

$enriched | Export-Csv -LiteralPath $OutDecisionSurface -NoTypeInformation -Encoding utf8
Write-Host "Wrote decision surface: $OutDecisionSurface ($($enriched.Count) rows)"

$gf = @($enriched | Where-Object { $_.timed_grouped_faster })

$agg = foreach ($g in ($enriched | Group-Object { "{0}|{1}" -f $_.numExperts, $_.repeat })) {
    $keyParts = $g.Name -split '\|'
    $kExp = [int]$keyParts[0]
    $rep = [int]$keyParts[1]
    $inGroup = @($g.Group)
    $gF = @($inGroup | Where-Object { $_.timed_grouped_faster })
    $lF = @($inGroup | Where-Object { -not $_.timed_grouped_faster })
    $minWorkGf = if ($gF.Count -gt 0) { ($gF | Measure-Object -Property work_product -Minimum).Minimum } else { $null }
    $maxWorkLf = if ($lF.Count -gt 0) { ($lF | Measure-Object -Property work_product -Maximum).Maximum } else { $null }
    [pscustomobject]@{
        numExperts              = $kExp
        repeat                  = $rep
        cases_total             = $inGroup.Count
        cases_grouped_faster    = $gF.Count
        cases_looped_faster     = ($inGroup.Count - $gF.Count)
        min_work_if_grouped_faster = $minWorkGf
        max_work_if_looped_faster   = $maxWorkLf
    }
}

$agg | Sort-Object numExperts, repeat | Export-Csv -LiteralPath $OutGroupedThresholds -NoTypeInformation -Encoding utf8
Write-Host "Wrote grouped thresholds: $OutGroupedThresholds ($($agg.Count) groups)"
Write-Host "Grouped-faster rows in full sweep: $($gf.Count) / $($enriched.Count)"
