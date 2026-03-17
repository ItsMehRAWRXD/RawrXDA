param(
    [string]$RepoRoot = "D:\rawrxd"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repo root not found: $RepoRoot"
}

$scanRoots = @(
    (Join-Path $RepoRoot "src\agentic"),
    (Join-Path $RepoRoot "src\agent"),
    (Join-Path $RepoRoot "src\core"),
    (Join-Path $RepoRoot "src\win32app")
)

$patterns = @(
    @{ id = "stub_marker"; severity = "high"; regex = "\bstub(s)?\b" },
    @{ id = "scaffold_marker"; severity = "high"; regex = "\bscaffold\b" },
    @{ id = "placeholder_marker"; severity = "high"; regex = "\bplaceholder\b" },
    @{ id = "fake_marker"; severity = "high"; regex = "\bfake|fiction\b" },
    @{ id = "todo_marker"; severity = "medium"; regex = "\bTODO|FIXME|NotImplemented|not implemented\b" },
    @{ id = "hardcoded_bool_return"; severity = "medium"; regex = "return\s+(true|false)\s*;" }
)

$allHits = @()

foreach ($p in $patterns) {
    $cmd = @(
        "rg", "-n", "-S",
        "--glob", "*.{cpp,h,hpp,c,asm,md}",
        $p.regex
    ) + $scanRoots

    $lines = & $cmd[0] $cmd[1..($cmd.Length - 1)] 2>$null
    if (-not $lines) { continue }

    foreach ($line in $lines) {
        if ($line -match "^(?<file>.:\\[^:]+):(?<ln>\d+):(?<txt>.*)$") {
            $allHits += [pscustomobject]@{
                file = $matches.file
                line = [int]$matches.ln
                pattern = $p.id
                severity = $p.severity
                text = $matches.txt.Trim()
            }
        }
    }
}

$allHits = $allHits | Sort-Object file, line, pattern -Unique

$byFile = $allHits | Group-Object file | ForEach-Object {
    $critical = ($_.Group | Where-Object { $_.severity -eq "high" }).Count
    $medium = ($_.Group | Where-Object { $_.severity -eq "medium" }).Count
    [pscustomobject]@{
        file = $_.Name
        hits = $_.Count
        high = $critical
        medium = $medium
    }
} | Sort-Object @{Expression = "hits"; Descending = $true}, @{Expression = "file"; Descending = $false}

$summary = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    repo_root = $RepoRoot
    total_hits = $allHits.Count
    high_hits = ($allHits | Where-Object { $_.severity -eq "high" }).Count
    medium_hits = ($allHits | Where-Object { $_.severity -eq "medium" }).Count
    files_with_hits = ($byFile | Measure-Object).Count
}

$manifest = [pscustomobject]@{
    summary = $summary
    by_file = $byFile
    hits = $allHits
}

$docsDir = Join-Path $RepoRoot "docs"
if (-not (Test-Path -LiteralPath $docsDir)) {
    New-Item -ItemType Directory -Path $docsDir | Out-Null
}

$jsonOut = Join-Path $docsDir "AGENTIC_REALITY_MANIFEST.json"
$mdOut = Join-Path $docsDir "AGENTIC_REALITY_AUDIT_AUTO.md"

$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonOut -Encoding UTF8

$topFiles = $byFile | Select-Object -First 40
$topHits = $allHits | Select-Object -First 200

$md = @()
$md += "# Agentic Reality Audit (Auto)"
$md += ""
$md += "Generated: $($summary.generated_at_utc)"
$md += ""
$md += "## Summary"
$md += "- Total hits: $($summary.total_hits)"
$md += "- High severity hits: $($summary.high_hits)"
$md += "- Medium severity hits: $($summary.medium_hits)"
$md += "- Files with hits: $($summary.files_with_hits)"
$md += ""
$md += "## Top Files"
foreach ($f in $topFiles) {
    $md += "- $($f.file): hits=$($f.hits), high=$($f.high), medium=$($f.medium)"
}
$md += ""
$md += "## Sample Findings (First 200)"
foreach ($h in $topHits) {
    $md += "- $($h.severity) [$($h.pattern)] $($h.file):$($h.line) :: $($h.text)"
}
$md += ""
$md += "## Notes"
$md += "- This report is lexical/static and intentionally conservative."
$md += "- Presence of a marker does not always mean runtime execution path is active."

$md -join "`r`n" | Set-Content -Path $mdOut -Encoding UTF8

Write-Host "Wrote: $jsonOut"
Write-Host "Wrote: $mdOut"
Write-Host ("Summary: total={0}, high={1}, medium={2}, files={3}" -f `
    $summary.total_hits, $summary.high_hits, $summary.medium_hits, $summary.files_with_hits)
