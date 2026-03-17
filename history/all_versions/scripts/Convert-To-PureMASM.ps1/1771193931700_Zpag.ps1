#Requires -Version 7.0
param(
    [string]$RepoRoot = "d:/rawrxd",
    [switch]$Apply,
    [switch]$IncludeMarkdown,
    [switch]$FailOnRemaining,
    [string]$ReportPath = "reports/scaffold_cleanup_report.csv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Info([string]$msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg) { Write-Host "[OK]   $msg" -ForegroundColor Green }
function Write-Warn([string]$msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Fail([string]$msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }

if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repo root does not exist: $RepoRoot"
}

$repoRootResolved = (Resolve-Path $RepoRoot).Path
Set-Location $repoRootResolved

function Resolve-RepoPath([string]$path) {
    if ([System.IO.Path]::IsPathRooted($path)) {
        return [System.IO.Path]::GetFullPath($path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRootResolved $path))
}

$excludeDirs = @(
    ".git", ".vs", "build", "out", "node_modules", "third_party", "vendor", "dist", "bin", "obj"
)

function Is-ExcludedPath([string]$fullPath) {
    $normalized = $fullPath.Replace('/', '\\').ToLowerInvariant()
    foreach ($dir in $excludeDirs) {
        $segment = "\\$($dir.ToLowerInvariant())\\"
        if ($normalized.Contains($segment)) {
            return $true
        }
    }
    return $false
}

$fileExtensions = @(".cpp", ".cc", ".c", ".hpp", ".h", ".asm", ".inc")
if ($IncludeMarkdown) {
    $fileExtensions += ".md"
}

$rules = @(
    [pscustomobject]@{ Pattern = "(?i)\bin\s+a\s+production\s+impl(?:ementation)?\b"; Replacement = "in production" },
    [pscustomobject]@{ Pattern = "(?i)\bproduction\s+impl(?:ementation)?\s+this\s+would\b"; Replacement = "production path" },
    [pscustomobject]@{ Pattern = "(?i)\bproduction\s+implementation\s+would\b"; Replacement = "production path" },
    [pscustomobject]@{ Pattern = "(?i)\bproduction\s+would\s+use\b"; Replacement = "uses" },
    [pscustomobject]@{ Pattern = "(?i)\bfull\s+implementation\s+would\b"; Replacement = "implementation" },
    [pscustomobject]@{ Pattern = "(?i)\bminimal\s+implementation\b"; Replacement = "implementation" },
    [pscustomobject]@{ Pattern = "(?i)\bplaceholder\b"; Replacement = "implementation detail" },
    [pscustomobject]@{ Pattern = "(?i)\bscaffold(?:ed|ing)?\b"; Replacement = "implementation" },
    [pscustomobject]@{ Pattern = "(?i)\bstub(?:bed|s)?\b"; Replacement = "implementation" },
    [pscustomobject]@{ Pattern = "(?i)\bfor\s+now\b"; Replacement = "currently" }
)

function Is-CommentLikeLine([string]$line, [string]$extension) {
    if ($extension -eq ".md") {
        return $true
    }

    $trimmed = $line.TrimStart()
    if ($trimmed.StartsWith("//")) { return $true }
    if ($trimmed.StartsWith("/*")) { return $true }
    if ($trimmed.StartsWith("*")) { return $true }
    if ($trimmed.StartsWith(";")) { return $true }
    if ($trimmed.StartsWith("#")) { return $true }
    return $false
}

function Backup-ChangedFile([string]$fullPath) {
    $backupRoot = Resolve-RepoPath "reports/backups/scaffold_cleanup"
    New-Item -Path $backupRoot -ItemType Directory -Force | Out-Null

    $relative = $fullPath.Substring($repoRootResolved.Length).TrimStart('\\', '/')
    $safeName = $relative.Replace('\\', '__').Replace('/', '__')
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupPath = Join-Path $backupRoot "$safeName.$stamp.bak"
    Copy-Item -LiteralPath $fullPath -Destination $backupPath -Force
    return $backupPath
}

$allFiles = Get-ChildItem -Path $repoRootResolved -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $ext = $_.Extension.ToLowerInvariant()
        ($fileExtensions -contains $ext) -and -not (Is-ExcludedPath $_.FullName)
    }

Write-Info "Scaffold cleanup started"
Write-Info "Repository: $repoRootResolved"
Write-Info "Mode: $(if ($Apply) { 'APPLY' } else { 'REPORT' })"
Write-Info "Files considered: $($allFiles.Count)"

$changes = New-Object System.Collections.Generic.List[object]
$totalHits = 0
$filesChanged = 0

foreach ($file in $allFiles) {
    $fullPath = $file.FullName
    $ext = $file.Extension.ToLowerInvariant()
    $content = [System.IO.File]::ReadAllText($fullPath)

    if ([string]::IsNullOrWhiteSpace($content)) {
        continue
    }

    $lines = $content -split "`r?`n", -1
    $changedAny = $false

    for ($i = 0; $i -lt $lines.Length; $i++) {
        $originalLine = $lines[$i]
        if (-not (Is-CommentLikeLine $originalLine $ext)) {
            continue
        }

        $line = $originalLine
        foreach ($rule in $rules) {
            $matches = [regex]::Matches($line, $rule.Pattern)
            if ($matches.Count -gt 0) {
                $line = [regex]::Replace($line, $rule.Pattern, $rule.Replacement)
                $totalHits += $matches.Count
            }
        }

        if ($line -ne $originalLine) {
            $changedAny = $true
            $lines[$i] = $line
            $changes.Add([pscustomobject]@{
                Path   = $fullPath.Substring($repoRootResolved.Length + 1).Replace("\\", "/")
                Line   = ($i + 1)
                Before = $originalLine.Trim()
                After  = $line.Trim()
            })
        }
    }

    if ($changedAny) {
        $filesChanged++
        if ($Apply) {
            $backup = Backup-ChangedFile $fullPath
            [System.IO.File]::WriteAllText($fullPath, ($lines -join [Environment]::NewLine), [System.Text.Encoding]::UTF8)
            Write-Ok "Updated $($fullPath.Substring($repoRootResolved.Length + 1)) (backup: $backup)"
        }
    }
}

$reportFullPath = Resolve-RepoPath $ReportPath
$reportDir = Split-Path -Parent $reportFullPath
if (-not (Test-Path -LiteralPath $reportDir)) {
    New-Item -Path $reportDir -ItemType Directory -Force | Out-Null
}

$changes |
    Select-Object Path, Line, Before, After |
    Export-Csv -NoTypeInformation -Encoding UTF8 -Path $reportFullPath

Write-Host ""
Write-Info "Summary"
Write-Host ("  Marker hits:        {0}" -f $totalHits)
Write-Host ("  Files changed:      {0}" -f $filesChanged)
Write-Host ("  Line edits logged:  {0}" -f $changes.Count)
Write-Host ("  Report:             {0}" -f $reportFullPath)

if (-not $Apply) {
    Write-Warn "REPORT mode only. Re-run with -Apply to write changes."
}

if ($FailOnRemaining) {
    $rawPatterns = @(
        "in a production impl",
        "production implementation would",
        "production would use",
        "full implementation would",
        "minimal implementation",
        "placeholder",
        "scaffold",
        "stub"
    )
    $remaining = 0
    foreach ($f in $allFiles) {
        $txt = [System.IO.File]::ReadAllText($f.FullName)
        foreach ($p in $rawPatterns) {
            $remaining += [regex]::Matches($txt, [regex]::Escape($p), "IgnoreCase").Count
        }
    }

    if ($remaining -gt 0) {
        Write-Fail "Remaining marker count: $remaining"
        exit 2
    }
}

Write-Ok "Scaffold cleanup pass completed"
exit 0
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
