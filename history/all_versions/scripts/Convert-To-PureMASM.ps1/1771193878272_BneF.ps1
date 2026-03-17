param(
    [string]$RepoRoot = "",
    [switch]$Apply,
    [switch]$IncludeDocs,
    [int]$BatchSize = 0,
    [string]$InventoryPath = "",
    [switch]$FailIfRemaining
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Info([string]$Message) { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
function Write-Ok([string]$Message) { Write-Host "[OK]   $Message" -ForegroundColor Green }
function Write-Warn([string]$Message) { Write-Host "[WARN] $Message" -ForegroundColor Yellow }
function Write-Fail([string]$Message) { Write-Host "[FAIL] $Message" -ForegroundColor Red }

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
}

if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repo root does not exist: $RepoRoot"
}

$repoRootResolved = (Resolve-Path $RepoRoot).Path

if ([string]::IsNullOrWhiteSpace($InventoryPath)) {
    $InventoryPath = Join-Path $repoRootResolved "reports/scaffold-marker-inventory.json"
}

Write-Info "Convert-To-PureMASM started"
Write-Info "Repository: $repoRootResolved"
Write-Info "Mode: $(if ($Apply) { 'APPLY' } else { 'REPORT' })"

$scanDirs = @("src", "scripts", "include", "CMakeLists.txt")
if ($IncludeDocs) {
    $scanDirs += @("README.md", "docs")
}

$excludedDirNames = @(
    ".git", "build", "build_ide", "build_agentic", "node_modules", "third_party", "vendor", "legacy", "out"
)

$codeExtensions = @(".cpp", ".cc", ".c", ".hpp", ".h", ".asm", ".inc", ".ps1", ".cmake", ".txt")
if ($IncludeDocs) {
    $codeExtensions += @(".md")
}

function Should-SkipDirectory([System.IO.DirectoryInfo]$Directory) {
    $name = $Directory.Name.ToLowerInvariant()
    return $excludedDirNames -contains $name
}

function Get-ScanFiles {
    $files = New-Object System.Collections.Generic.List[string]

    foreach ($entry in $scanDirs) {
        $full = Join-Path $repoRootResolved $entry
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $item = Get-Item -LiteralPath $full
        if ($item.PSIsContainer) {
            Get-ChildItem -LiteralPath $full -Recurse -File -ErrorAction SilentlyContinue |
                Where-Object {
                    $ext = $_.Extension.ToLowerInvariant()
                    if (-not ($codeExtensions -contains $ext)) { return $false }

                    $parent = $_.Directory
                    while ($null -ne $parent) {
                        if ($parent.FullName -eq $repoRootResolved) { break }
                        if (Should-SkipDirectory $parent) { return $false }
                        $parent = $parent.Parent
                    }
                    return $true
                } |
                ForEach-Object { $files.Add($_.FullName) }
        }
        else {
            $files.Add($item.FullName)
        }
    }

    return @($files | Sort-Object -Unique)
}

$markerPatterns = @(
    '(?im)^\s*(//|#|;|\*)\s*TODO\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bplaceholder\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bscaffold(ing)?\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bminimal implementation\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bin a production impl(?:ementation)?\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bproduction implementation would\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bfull implementation would\b.*$',
    '(?im)^\s*(//|#|;|\*)\s*.*\bstub\b.*$'
)

function Count-Markers([string]$Text) {
    $count = 0
    foreach ($pattern in $markerPatterns) {
        $count += [regex]::Matches($Text, $pattern).Count
    }
    return $count
}

function Remove-Markers([string]$Text) {
    $updated = $Text
    foreach ($pattern in $markerPatterns) {
        $updated = [regex]::Replace($updated, $pattern, "")
    }
    $updated = [regex]::Replace($updated, '(\r?\n){3,}', "`r`n`r`n")
    return $updated
}

function To-Relative([string]$Path) {
    $prefix = $repoRootResolved.TrimEnd('\\') + '\\'
    if ($Path.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $Path.Substring($prefix.Length).Replace('\\', '/')
    }
    return $Path
}

function Save-Inventory([object]$Inventory) {
    $targetDir = Split-Path -Parent $InventoryPath
    if (-not (Test-Path -LiteralPath $targetDir)) {
        New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
    }
    $json = $Inventory | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($InventoryPath, $json, [System.Text.Encoding]::UTF8)
    Write-Ok "Inventory written: $InventoryPath"
}

$scanFiles = Get-ScanFiles
Write-Info "Candidate files: $($scanFiles.Count)"

$items = New-Object System.Collections.Generic.List[object]
$totalBefore = 0

foreach ($path in $scanFiles) {
    $text = [System.IO.File]::ReadAllText($path)
    $before = Count-Markers $text
    if ($before -le 0) { continue }

    $items.Add([PSCustomObject]@{
        file = To-Relative $path
        before = $before
    })
    $totalBefore += $before
}

$inventory = [PSCustomObject]@{
    timestamp = (Get-Date).ToString('o')
    mode = if ($Apply) { 'APPLY' } else { 'REPORT' }
    repoRoot = $repoRootResolved
    includeDocs = [bool]$IncludeDocs
    candidateFiles = $scanFiles.Count
    filesWithMarkers = $items.Count
    markersBefore = $totalBefore
    files = $items
}

Save-Inventory $inventory
Write-Info "Found markers: $totalBefore in $($items.Count) files"

if (-not $Apply) {
    if ($FailIfRemaining -and $totalBefore -gt 0) {
        Write-Fail "Markers remain and FailIfRemaining is set"
        exit 2
    }
    Write-Ok "REPORT complete"
    exit 0
}

$ordered = @($items | Sort-Object -Property before -Descending)
if ($BatchSize -gt 0) {
    $ordered = @($ordered | Select-Object -First $BatchSize)
    Write-Info "Applying batch size: $BatchSize files"
}

$changed = New-Object System.Collections.Generic.List[object]
$removedTotal = 0

foreach ($entry in $ordered) {
    $full = Join-Path $repoRootResolved ($entry.file -replace '/', '\\')
    if (-not (Test-Path -LiteralPath $full)) { continue }

    $original = [System.IO.File]::ReadAllText($full)
    $before = Count-Markers $original
    if ($before -le 0) { continue }

    $updated = Remove-Markers $original
    $after = Count-Markers $updated
    if ($updated -ne $original) {
        [System.IO.File]::WriteAllText($full, $updated, [System.Text.Encoding]::UTF8)
        $removed = $before - $after
        $removedTotal += $removed
        $changed.Add([PSCustomObject]@{
            file = $entry.file
            before = $before
            after = $after
            removed = $removed
        })
    }
}

$postItems = New-Object System.Collections.Generic.List[object]
$totalAfter = 0
foreach ($path in $scanFiles) {
    $text = [System.IO.File]::ReadAllText($path)
    $after = Count-Markers $text
    if ($after -le 0) { continue }
    $postItems.Add([PSCustomObject]@{ file = To-Relative $path; after = $after })
    $totalAfter += $after
}

$applyReport = [PSCustomObject]@{
    timestamp = (Get-Date).ToString('o')
    mode = 'APPLY'
    repoRoot = $repoRootResolved
    includeDocs = [bool]$IncludeDocs
    filesChanged = $changed.Count
    markersRemoved = $removedTotal
    markersRemaining = $totalAfter
    changed = $changed
    remaining = $postItems
}

Save-Inventory $applyReport

Write-Ok "APPLY complete"
Write-Host "  Files changed:     $($changed.Count)"
Write-Host "  Markers removed:   $removedTotal"
Write-Host "  Markers remaining: $totalAfter"

if ($FailIfRemaining -and $totalAfter -gt 0) {
    Write-Fail "Markers remain after apply and FailIfRemaining is set"
    exit 3
}

exit 0
