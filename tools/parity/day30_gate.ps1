param(
    [string]$MatrixPath = "tools/parity/day30_top50_ai_ide_matrix.csv",
    [switch]$FailOnOpenStatus = $true
)

if (-not (Test-Path $MatrixPath)) {
    Write-Error "Matrix not found: $MatrixPath"
    exit 2
}

$rows = Import-Csv -Path $MatrixPath
if (-not $rows -or $rows.Count -eq 0) {
    Write-Error "Matrix is empty: $MatrixPath"
    exit 2
}

$domainCols = @(
    "editor_core",
    "nav_refactor",
    "build_debug",
    "ai_agentic",
    "tooling_mcp",
    "git_collab",
    "ux_perf",
    "security_controls"
)

$missing = @()
$sum = 0.0
$count = 0
$openCount = 0

foreach ($r in $rows) {
    foreach ($c in $domainCols) {
        $v = 0
        if (-not [int]::TryParse(($r.$c).ToString(), [ref]$v)) {
            $missing += "rank=$($r.rank) ide=$($r.reference_ide) col=$c invalid=$($r.$c)"
            continue
        }

        if ($v -lt 0 -or $v -gt 3) {
            $missing += "rank=$($r.rank) ide=$($r.reference_ide) col=$c out_of_range=$v"
            continue
        }

        if ($v -eq 0) {
            $missing += "rank=$($r.rank) ide=$($r.reference_ide) col=$c score=0"
        }

        $sum += $v
        $count += 1
    }

    if (($r.status + "").Trim().ToLowerInvariant() -eq "open") {
        $openCount += 1
    }
}

$avg = 0.0
if ($count -gt 0) {
    $avg = $sum / $count
}

Write-Host "=== Day 30 Parity Gate ==="
Write-Host "Matrix: $MatrixPath"
Write-Host "Rows: $($rows.Count)"
Write-Host "Domain cells scored: $count"
Write-Host ("Average score: {0:N2}" -f $avg)
Write-Host "Open status rows: $openCount"

if ($missing.Count -gt 0) {
    Write-Host "\nBlocking findings:"
    $missing | Select-Object -First 50 | ForEach-Object { Write-Host "- $_" }
    if ($missing.Count -gt 50) {
        Write-Host "- ... and $($missing.Count - 50) more"
    }
    exit 1
}

if ($avg -lt 2.0) {
    Write-Host "\nGate failed: average score < 2.0"
    exit 1
}

if ($FailOnOpenStatus -and $openCount -gt 0) {
    Write-Host "\nGate failed: open status rows remain"
    exit 1
}

Write-Host "\nGate passed."
exit 0
