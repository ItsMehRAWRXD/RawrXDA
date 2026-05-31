param(
    [string]$RepoRoot = "",
    [string]$Prefer = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}

function Resolve-Candidate {
    param([string]$Path)
    $cache = Join-Path $Path "CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return $null
}

if ($Prefer) {
    $resolved = Resolve-Candidate -Path $Prefer
    if ($resolved) {
        Write-Output $resolved
        exit 0
    }
}

$candidates = @(
    (Join-Path $RepoRoot "build-win32"),
    (Join-Path $RepoRoot "build-ninja"),
    (Join-Path $RepoRoot "build-ninja-ctx2"),
    (Join-Path $RepoRoot "build_smoke_auto"),
    (Join-Path $RepoRoot "build")
)

foreach ($c in $candidates) {
    $resolved = Resolve-Candidate -Path $c
    if ($resolved) {
        Write-Output $resolved
        exit 0
    }
}

exit 1
