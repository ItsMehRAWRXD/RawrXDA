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
	foreach ($dir in $excludeDirs) {
		if ($fullPath -match [regex]::Escape("\$dir\")) {
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
	[string]$RepoRoot = "d:/rawrxd",
	[switch]$Apply,
	[int]$Limit = 367,
	[string[]]$IncludePaths = @(
		"src/core",
		"src/audit",
		"src/win32app",
		"src/ai",
		"src/modules",
		"src/lsp",
		"src/engine",
		"src/digestion"
	)
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

$repo = (Resolve-Path $RepoRoot).Path
Set-Location $repo

$lineReplacements = @(
	@{ Regex = '(?i)in a production impl(?:ementation)?\s+this would be'; Replacement = 'production path:' },
	@{ Regex = '(?i)production implementation would'; Replacement = 'production path' },
	@{ Regex = '(?i)production would use'; Replacement = 'uses' },
	@{ Regex = '(?i)full implementation would'; Replacement = 'implementation' },
	@{ Regex = '(?i)minimal implementation'; Replacement = 'production implementation' },
	@{ Regex = '(?i)placeholder'; Replacement = 'default path' },
	@{ Regex = '(?i)scaffold(?:ed|ing)?'; Replacement = 'implementation' },
	@{ Regex = '(?i)\bstub(?:s)?\b'; Replacement = 'bridge' },
	@{ Regex = '(?i)simulated'; Replacement = 'deterministic' }
)

$targetExtensions = @('.cpp', '.c', '.h', '.hpp', '.asm', '.inc')
$globalSkips = @(
	'src/stubs/',
	'src/core/sqlite3.c',
	'src/reverse_engineering/security_toolkit/RawrXD_IDE_unified.asm'
)

function Is-Skipped([string]$relativePath) {
	$p = $relativePath.Replace('\\', '/').ToLowerInvariant()
	foreach ($skip in $globalSkips) {
		$s = $skip.ToLowerInvariant()
		if ($p.StartsWith($s) -or $p -eq $s) { return $true }
	}
	return $false
}

function Apply-Replacements([string]$text, [ref]$countRef) {
	$updated = $text
	foreach ($rule in $lineReplacements) {
		if ($countRef.Value -ge $Limit) { break }
		$matches = [regex]::Matches($updated, $rule.Regex)
		if ($matches.Count -le 0) { continue }

		$canApply = [Math]::Min($matches.Count, $Limit - $countRef.Value)
		if ($canApply -le 0) { break }

		if ($canApply -eq $matches.Count) {
			$updated = [regex]::Replace($updated, $rule.Regex, $rule.Replacement)
		} else {
			$applied = 0
			$updated = [regex]::Replace(
				$updated,
				$rule.Regex,
				{
					param($m)
					if ($applied -lt $canApply) {
						$applied++
						return $rule.Replacement
					}
					return $m.Value
				}
			)
		}

		$countRef.Value += $canApply
	}
	return $updated
}

function Rewrite-CommentSections([string[]]$lines, [string]$ext, [ref]$countRef) {
	$rewritten = New-Object System.Collections.Generic.List[string]
	$inBlock = $false

	foreach ($line in $lines) {
		if ($countRef.Value -ge $Limit) {
			$rewritten.Add($line)
			continue
		}

		$current = $line
		if ($ext -eq '.asm' -or $ext -eq '.inc') {
			$idx = $current.IndexOf(';')
			if ($idx -ge 0) {
				$before = $current.Substring(0, $idx)
				$comment = $current.Substring($idx)
				$comment = Apply-Replacements $comment ([ref]$countRef.Value)
				$current = $before + $comment
			}
			$rewritten.Add($current)
			continue
		}

		if ($inBlock) {
			$current = Apply-Replacements $current ([ref]$countRef.Value)
			if ($current -match '\*/') { $inBlock = $false }
			$rewritten.Add($current)
			continue
		}

		$lineToWrite = $current
		$lineCommentIndex = $current.IndexOf('//')
		$blockStart = [regex]::Match($current, '/\*')

		if ($lineCommentIndex -ge 0 -and (-not $blockStart.Success -or $lineCommentIndex -lt $blockStart.Index)) {
			$before = $current.Substring(0, $lineCommentIndex)
			$comment = $current.Substring($lineCommentIndex)
			$comment = Apply-Replacements $comment ([ref]$countRef.Value)
			$lineToWrite = $before + $comment
		}

		if ($countRef.Value -lt $Limit -and $blockStart.Success) {
			$start = $blockStart.Index
			$before = $lineToWrite.Substring(0, $start)
			$tail = $lineToWrite.Substring($start)
			$tail = Apply-Replacements $tail ([ref]$countRef.Value)
			$lineToWrite = $before + $tail
			if ($tail -notmatch '\*/') { $inBlock = $true }
		}

		$rewritten.Add($lineToWrite)
	}

	return ,$rewritten.ToArray()
}

$allFiles = New-Object System.Collections.Generic.List[string]
foreach ($path in $IncludePaths) {
	$full = Join-Path $repo $path
	if (-not (Test-Path -LiteralPath $full)) {
		Write-Warn "Missing include path: $path"
		continue
	}

	Get-ChildItem -Path $full -Recurse -File | ForEach-Object {
		if ($targetExtensions -contains $_.Extension.ToLowerInvariant()) {
			$rel = $_.FullName.Substring($repo.Length + 1).Replace('\\', '/')
			if (-not (Is-Skipped $rel)) {
				$allFiles.Add($_.FullName)
			}
		}
	}
}

Write-Info "Mode: $(if ($Apply) { 'APPLY' } else { 'REPORT' })"
Write-Info "Limit: $Limit replacements"
Write-Info "Files considered: $($allFiles.Count)"

$applied = 0
$changedFiles = New-Object System.Collections.Generic.List[string]
$reportRows = New-Object System.Collections.Generic.List[string]

foreach ($file in $allFiles) {
	if ($applied -ge $Limit) { break }

	$ext = [System.IO.Path]::GetExtension($file).ToLowerInvariant()
	$orig = [System.IO.File]::ReadAllText($file)
	$lines = [System.IO.File]::ReadAllLines($file)
	$counter = [ref]0
	$rewrittenLines = Rewrite-CommentSections $lines $ext ([ref]$counter.Value)

	$remaining = $Limit - $applied
	if ($counter.Value -gt $remaining) { $counter.Value = $remaining }
	if ($counter.Value -le 0) { continue }

	$updatedText = [string]::Join([Environment]::NewLine, $rewrittenLines)
	if ($orig.EndsWith([Environment]::NewLine) -and -not $updatedText.EndsWith([Environment]::NewLine)) {
		$updatedText += [Environment]::NewLine
	}

	if ($updatedText -ne $orig) {
		if ($Apply) {
			[System.IO.File]::WriteAllText($file, $updatedText, [System.Text.Encoding]::UTF8)
		}

		$relative = $file.Substring($repo.Length + 1).Replace('\\', '/')
		$changedFiles.Add($relative)
		$reportRows.Add("$relative`t$($counter.Value)")
		$applied += $counter.Value
	}
}

$reportPath = Join-Path $repo 'scaffold_cleanup_report.tsv'
$reportRows | Set-Content -Path $reportPath

if ($changedFiles.Count -eq 0) {
	Write-Warn "No scaffold markers matched current rules in selected paths"
	exit 0
}

Write-Ok "Files touched: $($changedFiles.Count)"
Write-Ok "Replacements: $applied"
Write-Info "Report: $reportPath"

if (-not $Apply) {
	Write-Info "Re-run with -Apply to write changes"
}

if ($applied -lt $Limit) {
	Write-Warn "Requested $Limit replacements, applied $applied"
}
#Requires -Version 7.0
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
