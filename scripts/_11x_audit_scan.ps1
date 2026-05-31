#Requires -Version 5.1
<#
.SYNOPSIS
  11-axis regex scan over C/C++/asm sources (default: src + include).

.PARAMETER Win32Only
  Scan only src/win32app (production Win32 IDE tree; excludes ggml, legacy/qtapp, CLI noise).

.PARAMETER MaxFileBytes
  Skip reading files larger than this (default 4MB). Large .asm/sqlite bodies are listed as skipped.
#>
param(
    [switch]$Win32Only,
    [int]$MaxFileBytes = (4 * 1024 * 1024)
)

$ErrorActionPreference = "SilentlyContinue"
$repoRoot = Split-Path -Parent $PSScriptRoot
$ext = @("*.cpp", "*.c", "*.cc", "*.cxx", "*.h", "*.hpp", "*.inl", "*.asm")
if ($Win32Only) {
    $roots = @(Join-Path $repoRoot "src\win32app")
}
else {
    $roots = @(
        (Join-Path $repoRoot "src"),
        (Join-Path $repoRoot "include")
    )
}
$acc = [System.Collections.Generic.List[string]]::new()
foreach ($r in $roots) {
    if (Test-Path -LiteralPath $r) {
        $acc.AddRange([string[]](Get-ChildItem -LiteralPath $r -Recurse -File -Include $ext | ForEach-Object { $_.FullName }))
    }
}
$files = $acc | Sort-Object -Unique

$axes = [ordered]@{
    "01_TODO_FIXME_HACK"   = "TODO:|FIXME:|XXX:|HACK:"
    "02_STUB_UNIMPL"       = "(?i)(STUB|not implemented|NotImplemented|PLACEHOLDER|unimplemented|\bWIP\b)"
    "03_NOOP_INCOMPLETE" = "(?i)(no-op|NOOP|not yet wired|not yet implemented)"
    "04_THROW"             = "\bthrow\b"
    "05_STDOUT_PRINTF"     = "std::cout|std::cerr|printf\s*\("
    "06_RAW_NEW_DELETE"    = "(?<!\/\/ )\bnew\s+(?!operator)|\bdelete\s+"
    "07_QT_INCLUDES"      = '#include\s*[<"]Q[A-Za-z]'
    "08_DISABLED_IF0"     = "#if\s+0\b|#ifdef\s+.*STUB|EXCLUDE_FROM_ALL"
    "09_OUTPUTDEBUG" = "OutputDebugString"
    "10_BLOCKING_WAIT"    = "WaitForSingleObject\s*\([^,]+,\s*INFINITE\s*\)|Sleep\s*\(\s*-?1\s*\)"
    "11_SECURITY_MARKERS" = "(?i)(SECURITY_|TODO.*sec|FIXME.*sec|hardcoded.*password|\beval\s*\()"
}

$perAxis = @{}
foreach ($k in $axes.Keys) { $perAxis[$k] = [System.Collections.Generic.List[object]]::new() }

$maxBytes = 4 * 1024 * 1024
$i = 0
$skippedLarge = [System.Collections.Generic.List[string]]::new()
foreach ($fp in $files) {
    $i++
    if (($i % 800) -eq 0) { Write-Host "progress $i / $($files.Count)" }
    try {
        $fi = [System.IO.FileInfo]::new($fp)
        if ($fi.Length -gt $maxBytes) {
            $skippedLarge.Add($fp.Substring($repoRoot.Length + 1))
            continue
        }
        $txt = [System.IO.File]::ReadAllText($fp)
    }
    catch { continue }
    foreach ($k in $axes.Keys) {
        $n = [regex]::Matches($txt, $axes[$k]).Count
        if ($n -gt 0) {
            $rel = $fp.Substring($repoRoot.Length + 1)
            [void]$perAxis[$k].Add([pscustomobject]@{ Path = $rel; Count = $n })
        }
    }
}

$logDir = Join-Path $repoRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$outName = if ($Win32Only) { "11x_audit_win32app_last.txt" } else { "11x_audit_last.txt" }
$outPath = Join-Path $logDir $outName

$sb = [System.Text.StringBuilder]::new()
$scopeLabel = if ($Win32Only) { "src/win32app only" } else { "src + include" }
[void]$sb.AppendLine("RawrXD 11-axis audit ($scopeLabel)")
[void]$sb.AppendLine("Generated: $(Get-Date -Format o)")
[void]$sb.AppendLine("Files scanned: $($files.Count)")
[void]$sb.AppendLine("Skipped (>4MB): $($skippedLarge.Count)")
if ($skippedLarge.Count -gt 0) {
    foreach ($s in $skippedLarge) { [void]$sb.AppendLine("  skip $s") }
}
[void]$sb.AppendLine("")

foreach ($k in $axes.Keys) {
    $list = $perAxis[$k]
    $filesWith = $list.Count
    $sum = 0
    foreach ($o in $list) { $sum += $o.Count }
    [void]$sb.AppendLine("=== $k ===")
    [void]$sb.AppendLine("Files with hits: $filesWith  Total matches (file-aggregated): $sum")
    foreach ($o in ($list | Sort-Object Count -Descending | Select-Object -First 15)) {
        [void]$sb.AppendLine(("  {0,6}  {1}" -f $o.Count, $o.Path))
    }
    [void]$sb.AppendLine("")
}

$text = $sb.ToString()
Set-Content -LiteralPath $outPath -Value $text -Encoding utf8
Write-Host $text
Write-Host ""
Write-Host "Wrote $outPath"
