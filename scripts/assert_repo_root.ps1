param(
    [string]$ExpectedRoot = "d:/rawrxd",
    [string]$CheckPath = "."
)

$ErrorActionPreference = "Stop"

function Normalize-PathForCompare([string]$path) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        return ""
    }

    $resolved = [System.IO.Path]::GetFullPath($path)
    $normalized = $resolved.Replace('\\', '/').TrimEnd('/')
    return $normalized.ToLowerInvariant()
}

$repoRoot = git -C $CheckPath rev-parse --show-toplevel 2>$null
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repoRoot)) {
    Write-Host "[GUARD] Unable to resolve git root from '$CheckPath'." -ForegroundColor Red
    exit 1
}

$actual = Normalize-PathForCompare $repoRoot
$expected = Normalize-PathForCompare $ExpectedRoot

if ($actual -ne $expected) {
    Write-Host "[GUARD] Repository root mismatch." -ForegroundColor Red
    Write-Host "[GUARD] Expected: $expected" -ForegroundColor Red
    Write-Host "[GUARD] Actual:   $actual" -ForegroundColor Red
    Write-Host "[GUARD] Aborting to prevent cross-repo contamination." -ForegroundColor Red
    exit 1
}

Write-Host "[GUARD] Repository root verified: $actual" -ForegroundColor Green
