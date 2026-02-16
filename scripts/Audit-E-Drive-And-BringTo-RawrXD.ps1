<#
.SYNOPSIS
    Audits the E: drive (and optional paths) for RawrXD source and compilers, then optionally copies everything to D:\RawrXD so the IDE is a single fortress.

.DESCRIPTION
    - Scans E:\RawrXD (and any additional roots you add) for every file.
    - Produces a manifest (CSV + summary) of all files (path, size, last write).
    - Optionally copies source files, compiler binaries, and/or "all" to D:\RawrXD with structure preserved.
    - Skips overwriting newer files at destination by default (configurable).

.PARAMETER AuditOnly
    Only generate the manifest; do not copy anything.

.PARAMETER CopySource
    Copy source files (*.cpp, *.h, *.hpp, *.c, *.asm, *.rev, *.eon, *.bat, *.ps1, CMakeLists.txt, etc.) to D:\RawrXD.

.PARAMETER CopyCompilers
    Copy folders that look like compilers (contain cl.exe, g++.exe, gcc.exe, ml64.exe, nasm.exe, link.exe) into D:\RawrXD\compilers.

.PARAMETER CopyAll
    Copy everything under the audit roots to D:\RawrXD (structure preserved). Use with care.

.PARAMETER DestRoot
    Destination root (default D:\RawrXD).

.PARAMETER SourceRoot
    Single source root to audit (alias for one -ERoots value). If set, overrides ERoots.

.PARAMETER ERoots
    Paths to audit (default is E:\RawrXD). Ignored if -SourceRoot is set.

.PARAMETER OverwriteNewer
    If set, overwrite destination files even when newer than source. Default is to skip overwriting newer.

.EXAMPLE
    .\Audit-E-Drive-And-BringTo-RawrXD.ps1 -AuditOnly
    .\Audit-E-Drive-And-BringTo-RawrXD.ps1 -CopySource -CopyCompilers
    .\Audit-E-Drive-And-BringTo-RawrXD.ps1 -CopyAll -OverwriteNewer
#>

[CmdletBinding()]
param(
    [switch]$AuditOnly,
    [switch]$CopySource,
    [switch]$CopyCompilers,
    [switch]$CopyAll,
    [string]$DestRoot = "D:\RawrXD",
    [string]$SourceRoot,        # If set, audit only this root (overrides ERoots)
    [string[]]$ERoots = @("E:\RawrXD"),
    [switch]$OverwriteNewer,
    [string]$ManifestDir,
    [switch]$AddHashToManifest  # Add SHA256 for files < 100MB (slower)
)

$ErrorActionPreference = "Stop"
if ($SourceRoot) { $ERoots = @($SourceRoot) }

if (-not $ManifestDir) {
    $ManifestDir = Join-Path $DestRoot "audit_manifest"
}
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$manifestCsv = Join-Path $ManifestDir "E_drive_audit_$timestamp.csv"
$summaryTxt = Join-Path $ManifestDir "E_drive_audit_$timestamp.txt"

# Compiler binary names we consider "compiler folders"
$compilerBinaries = @("cl.exe", "g++.exe", "gcc.exe", "ml64.exe", "nasm.exe", "link.exe", "ml.exe")

# Source extensions to copy when -CopySource
$sourceExtensions = @(".cpp", ".h", ".hpp", ".c", ".asm", ".rev", ".eon", ".bat", ".ps1", ".md", ".txt", ".json", ".cmake")
$sourceFiles = @("CMakeLists.txt", "Makefile", ".cursorrules")

function Get-RelativePath {
    param([string]$FullPath, [string]$BasePath)
    $base = $BasePath.TrimEnd("\")
    if ($FullPath.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($base.Length).TrimStart("\")
    }
    return $null
}

function Test-IsCompilerFolder {
    param([string]$DirPath)
    $children = Get-ChildItem -Path $DirPath -Recurse -File -ErrorAction SilentlyContinue
    foreach ($c in $children) {
        if ($compilerBinaries -contains $c.Name) { return $true }
    }
    return $false
}

function Get-Extension {
    param([string]$Path)
    return [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
}

function Test-IsSourceFile {
    param([string]$Path)
    $name = [System.IO.Path]::GetFileName($Path)
    if ($sourceFiles -contains $name) { return $true }
    $ext = Get-Extension $Path
    return $sourceExtensions -contains $ext
}

# Ensure manifest output dir
if (-not (Test-Path $ManifestDir)) {
    New-Item -Path $ManifestDir -ItemType Directory -Force | Out-Null
}

$allRows = [System.Collections.Generic.List[object]]::new()
$totalSize = 0
$totalCount = 0
$compilerDirs = [System.Collections.Generic.List[string]]::new()
$sourcePaths = [System.Collections.Generic.List[string]]::new()

Write-Host "=== RawrXD E-Drive Audit & Bring-To-D:\RawrXD ===" -ForegroundColor Cyan
Write-Host "  Roots: $($ERoots -join ', ')" -ForegroundColor Gray
Write-Host "  Dest:  $DestRoot" -ForegroundColor Gray
Write-Host "  Manifest: $manifestCsv" -ForegroundColor Gray
Write-Host ""

foreach ($root in $ERoots) {
    if (-not (Test-Path $root)) {
        Write-Host "  [SKIP] Root not found: $root" -ForegroundColor Yellow
        continue
    }
    Write-Host "  [SCAN] $root" -ForegroundColor Cyan
    $files = Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $rel = Get-RelativePath $f.FullName $root
        if (-not $rel) { continue }
        $totalCount++
        $totalSize += $f.Length
        $hash = ""
        if ($AddHashToManifest -and $f.Length -lt 100MB) {
            try { $hash = (Get-FileHash $f.FullName -Algorithm SHA256 -ErrorAction SilentlyContinue).Hash } catch { $hash = "N/A" }
        }
        $allRows.Add([PSCustomObject]@{
            Root       = $root
            RelativePath = $rel
            FullPath   = $f.FullName
            Length     = $f.Length
            LastWrite  = $f.LastWriteTimeUtc.ToString("o")
            IsSource   = Test-IsSourceFile $f.FullName
            Hash      = $hash
        })
        if (Test-IsSourceFile $f.FullName) {
            $sourcePaths.Add($f.FullName)
        }
    }
    # Detect compiler-like folders (direct children that contain compiler binaries)
    $dirs = Get-ChildItem -Path $root -Directory -Recurse -ErrorAction SilentlyContinue
    foreach ($d in $dirs) {
        if (Test-IsCompilerFolder $d.FullName) {
            $compilerDirs.Add($d.FullName)
        }
    }
}

# Dedupe compiler dirs (keep top-most)
$compilerDirs = $compilerDirs | Sort-Object | ForEach-Object {
    $keep = $true
    foreach ($o in $compilerDirs) {
        if ($o -ne $_ -and $_.StartsWith($o, [StringComparison]::OrdinalIgnoreCase)) { $keep = $false; break }
    }
    if ($keep) { $_ }
}

# Write manifest CSV
$allRows | Export-Csv -Path $manifestCsv -NoTypeInformation -Encoding UTF8
Write-Host "  [OK] Manifest: $totalCount files, $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor Green
Write-Host "  [OK] Source files: $($sourcePaths.Count)" -ForegroundColor Green
Write-Host "  [OK] Compiler-like dirs: $($compilerDirs.Count)" -ForegroundColor Green

# Summary file
@"
RawrXD E-Drive Audit — $timestamp
Roots: $($ERoots -join ', ')
Destination: $DestRoot
Total files: $totalCount
Total size (MB): $([math]::Round($totalSize/1MB, 2))
Source files: $($sourcePaths.Count)
Compiler-like directories: $($compilerDirs.Count)
$($compilerDirs | ForEach-Object { "  $_" })

Manifest CSV: $manifestCsv
"@ | Set-Content -Path $summaryTxt -Encoding UTF8

if ($AuditOnly) {
    Write-Host "  [DONE] Audit only. No copy. Use -CopySource / -CopyCompilers / -CopyAll to bring to $DestRoot" -ForegroundColor Cyan
    exit 0
}

# --- Copy actions ---
if (-not (Test-Path $DestRoot)) {
    New-Item -Path $DestRoot -ItemType Directory -Force | Out-Null
}

$copyCount = 0
$skipNewer = 0

if ($CopyAll) {
    Write-Host ""
    Write-Host "  [COPY] Full tree -> $DestRoot" -ForegroundColor Cyan
    foreach ($root in $ERoots) {
        if (-not (Test-Path $root)) { continue }
        $rootNorm = $root.TrimEnd("\")
        $items = Get-ChildItem -Path $rootNorm -Recurse -ErrorAction SilentlyContinue
        foreach ($item in $items) {
            $rel = $item.FullName.Substring($rootNorm.Length).TrimStart("\")
            if (-not $rel) { continue }
            $dest = Join-Path $DestRoot $rel
            if ($item.PSIsContainer) {
                if (-not (Test-Path $dest)) { New-Item -Path $dest -ItemType Directory -Force | Out-Null }
            } else {
                $destDir = [System.IO.Path]::GetDirectoryName($dest)
                if (-not (Test-Path $destDir)) { New-Item -Path $destDir -ItemType Directory -Force | Out-Null }
                if (Test-Path $dest) {
                    $destItem = Get-Item $dest
                    if (-not $OverwriteNewer -and $destItem.LastWriteTimeUtc -ge $item.LastWriteTimeUtc) { $skipNewer++; continue }
                }
                Copy-Item -Path $item.FullName -Destination $dest -Force
                $copyCount++
            }
        }
    }
    Write-Host "  [OK] Copied $copyCount files (skipped newer: $skipNewer)" -ForegroundColor Green
} else {
    if ($CopySource) {
        Write-Host ""
        Write-Host "  [COPY] Source files -> $DestRoot (structure preserved)" -ForegroundColor Cyan
        foreach ($path in $sourcePaths) {
            $rel = $null
            foreach ($r in $ERoots) {
                if ($path.StartsWith($r.TrimEnd("\"), [StringComparison]::OrdinalIgnoreCase)) {
                    $rel = $path.Substring($r.TrimEnd("\").Length).TrimStart("\")
                    break
                }
            }
            if (-not $rel) { continue }
            $dest = Join-Path $DestRoot $rel
            $destDir = [System.IO.Path]::GetDirectoryName($dest)
            if (-not (Test-Path $destDir)) {
                New-Item -Path $destDir -ItemType Directory -Force | Out-Null
            }
            if (Test-Path $dest) {
                $destItem = Get-Item $dest
                $srcItem = Get-Item $path
                if (-not $OverwriteNewer -and $destItem.LastWriteTimeUtc -ge $srcItem.LastWriteTimeUtc) {
                    $skipNewer++; continue
                }
            }
            Copy-Item -Path $path -Destination $dest -Force
            $copyCount++
        }
        Write-Host "  [OK] Copied $copyCount source files (skipped newer: $skipNewer)" -ForegroundColor Green
    }

    if ($CopyCompilers) {
    $compDest = Join-Path $DestRoot "compilers"
    if (-not (Test-Path $compDest)) { New-Item -Path $compDest -ItemType Directory -Force | Out-Null }
    foreach ($cdir in $compilerDirs) {
        $name = [System.IO.Path]::GetFileName($cdir)
        if (-not $name) { $name = "compiler_$copyCount" }
        $target = Join-Path $compDest $name
        Write-Host "  [COPY] Compiler: $cdir -> $target" -ForegroundColor Cyan
        if (Test-Path $target) {
            Copy-Item -Path "$cdir\*" -Destination $target -Recurse -Force
        } else {
            Copy-Item -Path $cdir -Destination $target -Recurse -Force
        }
        $copyCount++
    }
    }
}

# Update path references E: → D: in scripts under DestRoot (when any copy was done)
if ($CopyAll -or $CopySource -or $CopyCompilers) {
    Write-Host ""
    Write-Host "  [PATH] Updating E:\RawrXD -> $DestRoot in scripts..." -ForegroundColor Cyan
    $scriptExts = @("*.ps1", "*.psm1", "*.bat", "*.cmd")
    $updateCount = 0
    foreach ($ext in $scriptExts) {
        Get-ChildItem -Path $DestRoot -Recurse -Include $ext -ErrorAction SilentlyContinue | ForEach-Object {
            $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
            if ($content -and $content -match "E:\\\\RawrXD|E:/RawrXD") {
                $newContent = $content -replace "E:\\\\RawrXD", $DestRoot -replace "E:/RawrXD", ($DestRoot -replace "\\", "/")
                Set-Content -Path $_.FullName -Value $newContent -NoNewline
                $updateCount++
            }
        }
    }
    if ($updateCount -gt 0) { Write-Host "  [OK] Updated $updateCount script(s)" -ForegroundColor Green }
}

Write-Host ""
Write-Host "  [DONE] Audit and copy complete. Manifest: $manifestCsv" -ForegroundColor Green
