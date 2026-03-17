param(
    [string]$InputPath = "bench/bench_results_final.json",
    [switch]$Raw,
    [string[]]$Metrics = @('parse_ms','gpu_init_ms','matmul_avg_ms','rmsnorm_avg_ms','silu_avg_ms','attention_avg_ms')
)

function Get-Percentile {
    param([double[]]$Values,[double]$P)
    if (-not $Values -or $Values.Count -eq 0) { return $null }
    $sorted = $Values | Sort-Object
    $rank = ($P/100.0)*($sorted.Count - 1)
    $low = [math]::Floor($rank)
    $high = [math]::Ceiling($rank)
    if ($low -eq $high) { return $sorted[$low] }
    $weight = $rank - $low
    return $sorted[$low] + ($sorted[$high]-$sorted[$low])*$weight
}

if ($Raw) { $InputPath = "bench/bench_results.json" }
if (!(Test-Path $InputPath)) { Write-Host "Input file not found: $InputPath" -ForegroundColor Red; exit 2 }

# Attempt to read JSON (final form should be array; raw may have trailing comma)
$content = Get-Content $InputPath -Raw
if ($content.TrimEnd() -notmatch '\]$') {
    # Try to fix raw: remove trailing comma and add bracket
    $content = ($content -replace ',\s*$','')
    if ($content.TrimEnd() -notmatch '\]$') { $content += '\n]'}
}

try {
    $json = $content | ConvertFrom-Json
} catch {
    Write-Host "Failed to parse JSON: $($_.Exception.Message)" -ForegroundColor Red
    exit 3
}

if (-not $json) { Write-Host "No records parsed" -ForegroundColor Yellow; exit 0 }

$result = @{}
foreach ($m in $Metrics) {
    $vals = @()
    foreach ($row in $json) {
        if ($row.PSObject.Properties[$m] -and ($row.$m -is [double] -or $row.$m -is [int])) {
            $vals += [double]$row.$m
        }
    }
    if ($vals.Count -gt 0) {
        $result[$m] = [pscustomobject]@{
            count = $vals.Count
            median = Get-Percentile -Values $vals -P 50
            p95 = Get-Percentile -Values $vals -P 95
            max = ($vals | Measure-Object -Maximum).Maximum
            min = ($vals | Measure-Object -Minimum).Minimum
        }
    }
}

# Output summary
Write-Host "Benchmark Percentile Summary" -ForegroundColor Cyan
$result.GetEnumerator() | ForEach-Object {
    $name = $_.Key; $data = $_.Value
    Write-Host ("{0}: count={1} median={2:N2} p95={3:N2} min={4:N2} max={5:N2}" -f $name,$data.count,$data.median,$data.p95,$data.min,$data.max)
}

# Emit JSON file
$outFile = "bench/bench_summary.json"
($result | ConvertTo-Json -Depth 4) | Set-Content $outFile
Write-Host "Summary written: $outFile" -ForegroundColor Green
