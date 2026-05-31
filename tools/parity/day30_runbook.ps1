param(
    [string]$RepoRoot = ".",
    [string]$MatrixPath = "tools/parity/day30_top50_ai_ide_matrix.csv",
    [switch]$SkipGate = $false
)

Set-Location $RepoRoot

Write-Host "=== Day 30 Runbook ==="
Write-Host "Repo: $(Get-Location)"
Write-Host "Date: $(Get-Date -Format s)"

$script:checks = @()

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Details)
    $script:checks += [pscustomobject]@{ Name = $Name; Ok = $Ok; Details = $Details }
}

# Required artifacts
$required = @(
    "DAY30_PARITY_MASTERPLAN.md",
    "DAY30_P0_EXECUTION_BACKLOG.md",
    "tools/parity/DAY30_EVIDENCE_TEMPLATE.md",
    "tools/parity/day30_top50_ai_ide_matrix.csv",
    "tools/parity/day30_gate.ps1"
)

foreach ($p in $required) {
    $ok = Test-Path $p
    Add-Check -Name "artifact:$p" -Ok $ok -Details ($(if ($ok) { "present" } else { "missing" }))
}

if (-not $SkipGate) {
    $gateScript = "tools/parity/day30_gate.ps1"
    if (Test-Path $gateScript) {
        & pwsh -NoProfile -ExecutionPolicy Bypass -File $gateScript -MatrixPath $MatrixPath
        $exitCode = $LASTEXITCODE
        Add-Check -Name "parity-gate" -Ok ($exitCode -eq 0) -Details "exit=$exitCode"
    } else {
        Add-Check -Name "parity-gate" -Ok $false -Details "gate script missing"
    }
} else {
    Add-Check -Name "parity-gate" -Ok $true -Details "skipped"
}

Write-Host "\n=== Summary ==="
$script:checks | ForEach-Object {
    $mark = if ($_.Ok) { "PASS" } else { "FAIL" }
    Write-Host ("[{0}] {1} :: {2}" -f $mark, $_.Name, $_.Details)
}

$failed = @($script:checks | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    Write-Host "\nRunbook result: FAIL"
    exit 1
}

Write-Host "\nRunbook result: PASS"
exit 0
