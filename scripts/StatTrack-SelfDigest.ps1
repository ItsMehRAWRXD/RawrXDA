# StatTrack-SelfDigest.ps1
# Digests the entire D:\rawrxd (or RepoRoot) codebase and writes stats so the stat track can "stat itself and track itself".
# Run: .\scripts\StatTrack-SelfDigest.ps1  or  .\scripts\StatTrack-SelfDigest.ps1 -RepoRoot D:\rawrxd

param(
    [string]$RepoRoot = $PSScriptRoot + "\..",
    [string]$OutFile  = ""  # default: RepoRoot\docs\STAT_TRACK_GENERATED.md
)

$ErrorActionPreference = "Stop"
if (-not $OutFile) {
    $OutFile = Join-Path $RepoRoot "docs\STAT_TRACK_GENERATED.md"
}

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
$srcDir   = Join-Path $RepoRoot "src"
$docsDir  = Join-Path $RepoRoot "docs"
$cmakePath = Join-Path $RepoRoot "CMakeLists.txt"

# Ensure output dir exists
$outDir = [IO.Path]::GetDirectoryName($OutFile)
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

# --- Gather stats ---
$when = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$stats = @{}

# 1) File counts under repo (excluding common noise)
$excludeDirs = @("node_modules", ".git", "build", "build_ide", "cmake-build", "out", ".vs", "x64", "Debug", "Release", "dist", "3rdparty")
$extCounts = @{}
$getFiles = {
    param($root, $pattern)
    Get-ChildItem -Path $root -Recurse -Include $pattern -File -ErrorAction SilentlyContinue |
        Where-Object {
            $rel = $_.FullName.Substring($root.Length)
            $skip = $false
            foreach ($d in $excludeDirs) {
                if ($rel -match ([regex]::Escape($d))) { $skip = $true; break }
            }
            -not $skip
        }
}

foreach ($ext in @("*.cpp", "*.c", "*.h", "*.hpp", "*.asm", "*.rc", "*.cmake", "CMakeLists.txt", "*.ps1", "*.md", "*.json", "*.py", "*.bat", "*.sh")) {
    $files = & $getFiles $RepoRoot $ext
    $cnt = ($files | Measure-Object).Count
    $key = $ext -replace "\*\.", ""
    if ($key -eq "CMakeLists.txt") { $key = "cmake_lists" }
    $extCounts[$key] = $cnt
}
$stats["by_extension"] = $extCounts

# 2) src/ only (main codebase)
$srcCpp  = (& $getFiles $srcDir "*.cpp" | Measure-Object).Count
$srcC    = (& $getFiles $srcDir "*.c"   | Measure-Object).Count
$srcH    = (& $getFiles $srcDir "*.h"   | Measure-Object).Count
$srcHpp  = (& $getFiles $srcDir "*.hpp" | Measure-Object).Count
$srcAsm  = (& $getFiles $srcDir "*.asm" | Measure-Object).Count
$stats["src_cpp"] = $srcCpp
$stats["src_c"]   = $srcC
$stats["src_h"]   = $srcH
$stats["src_hpp"] = $srcHpp
$stats["src_asm"] = $srcAsm
$stats["src_compilable"] = $srcCpp + $srcC + $srcAsm

# 3) Line counts (src/*.cpp + src/*.c + src/*.asm only, sampled or full)
$lineTotal = 0
$countLines = {
    param($root, $pattern)
    $files = & $getFiles $root $pattern
    $sum = 0
    foreach ($f in $files) {
        $lines = Get-Content -LiteralPath $f.FullName -ErrorAction SilentlyContinue
        if ($lines) { $sum += $lines.Count }
    }
    $sum
}
$stats["lines_cpp"] = & $countLines $srcDir "*.cpp"
$stats["lines_c"]   = & $countLines $srcDir "*.c"
$stats["lines_asm"] = & $countLines $srcDir "*.asm"
$stats["lines_src_total"] = $stats["lines_cpp"] + $stats["lines_c"] + $stats["lines_asm"]

# 4) CMake SOURCES / WIN32IDE_SOURCES approximate counts (grep src/ in CMakeLists)
if (Test-Path $cmakePath) {
    $content = Get-Content -LiteralPath $cmakePath -Raw -ErrorAction SilentlyContinue
    $sourcesLines = ([regex]::Matches($content, "^\s+src/", "Multiline")).Count
    $win32IdeLines = ([regex]::Matches($content, "WIN32IDE_SOURCES|src/win32app/|src/asm/RawrXD_DualEngine|src/asm/quantum_beaconism")).Count
    $stats["cmake_src_refs"] = $sourcesLines
    $stats["cmake_win32ide_refs"] = $win32IdeLines
    $soourcesBlock = if ($content -match "set\s*\(\s*SOURCES\s+([\s\S]*?)\)\s*(?=\s*set\s*\()") { $Matches[1] } else { "" }
    $stats["sources_block_src_count"] = ([regex]::Matches($soourcesBlock, "src/")).Count
}

# 5) Git (if present)
$gitDir = Join-Path $RepoRoot ".git"
if (Test-Path $gitDir) {
    try {
        $branch = git -C $RepoRoot rev-parse --abbrev-ref HEAD 2>$null
        $rev    = git -C $RepoRoot rev-parse --short HEAD 2>$null
        $last   = git -C $RepoRoot log -1 --format="%ci %s" 2>$null
        $stats["git_branch"] = $branch
        $stats["git_rev"]    = $rev
        $stats["git_last"]   = $last
    } catch {
        $stats["git_branch"] = ""
        $stats["git_rev"]    = ""
        $stats["git_last"]   = ""
    }
} else {
    $stats["git_branch"] = ""
    $stats["git_rev"]    = ""
    $stats["git_last"]   = ""
}

# 6) Databases / data files (optional)
$dbFiles = Get-ChildItem -Path $RepoRoot -Recurse -Include "*.sqlite", "*.sqlite3", "*.db" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch "\.git|node_modules|build" }
$stats["db_files_count"] = ($dbFiles | Measure-Object).Count
$stats["db_files"] = @($dbFiles | ForEach-Object { $_.FullName.Substring($RepoRoot.Length).TrimStart("\", "/") })

# --- Write STAT_TRACK_GENERATED.md ---
$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine("# Stat track — self-generated")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("**Generated:** $when  |  **Repo:** $RepoRoot")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Run `.\scripts\StatTrack-SelfDigest.ps1` to refresh. No todos.")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## File counts (repo-wide, ex. build/.git)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Extension | Count |")
[void]$sb.AppendLine("|-----------|-------|")
foreach ($k in @("cpp", "c", "h", "hpp", "asm", "rc", "cmake", "cmake_lists", "ps1", "md", "json", "py", "bat", "sh")) {
    $v = $stats["by_extension"][$k]
    if ($null -ne $v) { [void]$sb.AppendLine("| $k | $v |") }
}
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## src/ only")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Metric | Value |")
[void]$sb.AppendLine("|--------|-------|")
[void]$sb.AppendLine("| .cpp | $($stats['src_cpp']) |")
[void]$sb.AppendLine("| .c   | $($stats['src_c']) |")
[void]$sb.AppendLine("| .h   | $($stats['src_h']) |")
[void]$sb.AppendLine("| .hpp | $($stats['src_hpp']) |")
[void]$sb.AppendLine("| .asm | $($stats['src_asm']) |")
[void]$sb.AppendLine("| Compilable (cpp+c+asm) | $($stats['src_compilable']) |")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Line counts (src/)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Kind | Lines |")
[void]$sb.AppendLine("|------|-------|")
[void]$sb.AppendLine("| .cpp | $($stats['lines_cpp']) |")
[void]$sb.AppendLine("| .c   | $($stats['lines_c']) |")
[void]$sb.AppendLine("| .asm | $($stats['lines_asm']) |")
[void]$sb.AppendLine("| **Total** | **$($stats['lines_src_total'])** |")
[void]$sb.AppendLine("")
if ($stats["cmake_src_refs"]) {
    [void]$sb.AppendLine("## CMake (CMakeLists.txt)")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("| Metric | Value |")
    [void]$sb.AppendLine("|--------|-------|")
    [void]$sb.AppendLine("| Lines with \`src/\` | $($stats['cmake_src_refs']) |")
    if ($stats["sources_block_src_count"]) {
        [void]$sb.AppendLine("| SOURCES block \`src/\` refs | $($stats['sources_block_src_count']) |")
    }
    [void]$sb.AppendLine("")
}
if ($stats["git_rev"]) {
    [void]$sb.AppendLine("## Git")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("| | |")
    [void]$sb.AppendLine("|---|-----|")
    [void]$sb.AppendLine("| Branch | $($stats['git_branch']) |")
    [void]$sb.AppendLine("| Rev | $($stats['git_rev']) |")
    [void]$sb.AppendLine("| Last commit | $($stats['git_last']) |")
    [void]$sb.AppendLine("")
}
if ($stats["db_files_count"] -gt 0) {
    [void]$sb.AppendLine("## Database files")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Count: $($stats['db_files_count'])")
    foreach ($p in $stats["db_files"]) {
        [void]$sb.AppendLine("- $p")
    }
}
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("*End of self-generated stat track.*")

$sb.ToString() | Set-Content -LiteralPath $OutFile -Encoding UTF8
Write-Host "Wrote: $OutFile" -ForegroundColor Green
Write-Host "  src compilable: $($stats['src_compilable'])  |  lines (src): $($stats['lines_src_total'])  |  git: $($stats['git_rev'])" -ForegroundColor Cyan
