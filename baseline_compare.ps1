<#
.SYNOPSIS
    Side-by-side comparison of two RawrXD SWE-bench JSON report files.

.DESCRIPTION
    Reads two JSON report files produced by RawrXD-SWEBench.exe and prints a
    formatted diff-style table of key metrics.

.PARAMETER Left
    Path to the baseline (left-side) JSON report.

.PARAMETER Right
    Path to the comparison (right-side) JSON report.

.EXAMPLE
    .\baseline_compare.ps1 -Left reports\baseline_a.json -Right reports\baseline_b.json
#>
param(
    [Parameter(Mandatory=$true)]  [string]$Left,
    [Parameter(Mandatory=$true)]  [string]$Right
)

function Read-Report($path) {
    if (-not (Test-Path $path)) {
        Write-Error "Report not found: $path"
        exit 1
    }
    $raw = Get-Content $path -Raw
    return $raw | ConvertFrom-Json
}

function Fmt($v, $fmt = "F4") {
    if ($null -eq $v) { return "N/A" }
    return $v.ToString($fmt)
}

function Delta($a, $b) {
    if ($null -eq $a -or $null -eq $b) { return "" }
    $d = $b - $a
    if ($d -gt 0) { return "+$([math]::Round($d,4))" }
    return "$([math]::Round($d,4))"
}

$L = Read-Report $Left
$R = Read-Report $Right

$lName = [System.IO.Path]::GetFileNameWithoutExtension($Left)
$rName = [System.IO.Path]::GetFileNameWithoutExtension($Right)

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════════════╗"
Write-Host "║           RawrXD SWE-bench Baseline Comparison                       ║"
Write-Host "╠══════════════════════════════════════════════════════════════════════╣"
Write-Host ("║  {0,-34}  {1,-14}  {2,-14} ║" -f "Metric", $lName.Substring(0,[math]::Min(14,$lName.Length)), $rName.Substring(0,[math]::Min(14,$rName.Length)))
Write-Host "╠══════════════════════════════════════════════════════════════════════╣"

$metrics = @(
    @{ Name="total";                  L=$L.total;                  R=$R.total }
    @{ Name="completed";              L=$L.completed;              R=$R.completed }
    @{ Name="patch_correct";          L=$L.patch_correct;          R=$R.patch_correct }
    @{ Name="tests_passed";           L=$L.tests_passed;           R=$R.tests_passed }
    @{ Name="task_completion_rate";   L=$L.task_completion_rate;   R=$R.task_completion_rate }
    @{ Name="patch_correctness";      L=$L.patch_correctness;      R=$R.patch_correctness }
    @{ Name="test_pass_rate";         L=$L.test_pass_rate;         R=$R.test_pass_rate }
    @{ Name="overall_score";          L=$L.overall_score;          R=$R.overall_score }
    @{ Name="pass@1 (patch_correct)"; L=if ($L.total -gt 0) { $L.patch_correct / $L.total } else { 0 }
                                       R=if ($R.total -gt 0) { $R.patch_correct / $R.total } else { 0 } }
)

foreach ($m in $metrics) {
    $lv = Fmt $m.L
    $rv = Fmt $m.R
    $dv = Delta $m.L $m.R
    Write-Host ("║  {0,-34}  {1,-14}  {2,-14}  {3,6} ║" -f $m.Name, $lv, $rv, $dv)
}

Write-Host "╚══════════════════════════════════════════════════════════════════════╝"
Write-Host ""

# Per-task diff
if ($L.results -and $R.results) {
    Write-Host "Per-task delta (status changes):"
    $lMap = @{}
    foreach ($t in $L.results) { $lMap[$t.task_id] = $t }
    $rMap = @{}
    foreach ($t in $R.results) { $rMap[$t.task_id] = $t }
    $allIds = ($L.results + $R.results | Select-Object -ExpandProperty task_id | Sort-Object -Unique)
    foreach ($id in $allIds) {
        $lt = $lMap[$id]; $rt = $rMap[$id]
        $ls = if ($lt) { $lt.status } else { "(absent)" }
        $rs = if ($rt) { $rt.status } else { "(absent)" }
        if ($ls -ne $rs) {
            Write-Host ("  {0,-40}  {1,-12}  ->  {2}" -f $id, $ls, $rs)
        }
    }
    Write-Host ""
}
