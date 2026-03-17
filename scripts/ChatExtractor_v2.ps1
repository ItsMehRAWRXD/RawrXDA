<#
.SYNOPSIS
    ChatExtractor v2 — High-speed robocopy-based extraction of IDE chat artifacts.
.DESCRIPTION
    Extracts Cursor, VS Code, VS Code Insiders, and GitHub Copilot chat artifacts
    using robocopy for speed, with LevelDB/IndexedDB detection and content scanning.
.PARAMETER Targets
    Comma-separated: cursor, vscode, insiders, github, all  (default: all)
.PARAMETER OutDir
    Destination directory (default: D:\rawrxd\extracted_chats)
.PARAMETER Scan
    If set, scan extracted files for chat-like JSON content after copying.
.EXAMPLE
    .\ChatExtractor_v2.ps1 -Targets cursor,vscode -Scan
#>
[CmdletBinding()]
param(
    [string]$Targets = "all",
    [string]$OutDir  = "D:\rawrxd\extracted_chats",
    [switch]$Scan
)

$ErrorActionPreference = "SilentlyContinue"

# ─── Source paths ─────────────────────────────────────────────────────────
$Sources = @{
    cursor   = @(
        "$env:APPDATA\Cursor"
        "$env:LOCALAPPDATA\Cursor"
        "$env:APPDATA\cursor"
        "$env:LOCALAPPDATA\cursor"
    )
    vscode   = @(
        "$env:APPDATA\Code"
        "$env:LOCALAPPDATA\Code"
        "$env:APPDATA\Code\User"
    )
    insiders = @(
        "$env:APPDATA\Code - Insiders"
        "$env:LOCALAPPDATA\Code - Insiders"
    )
    github   = @(
        "$env:LOCALAPPDATA\GitHub Desktop"
        "$env:APPDATA\GitHub Copilot"
        "$env:LOCALAPPDATA\github-copilot"
        "$env:USERPROFILE\.config\github-copilot"
    )
}

# Subfolders of interest
$SubDirs = @(
    "User\globalStorage"
    "User\workspaceStorage"
    "blob_storage"
    "Local Storage"
    "IndexedDB"
    "Session Storage"
    "Cache"
    "Code Cache"
    "Logs"
    "CachedData"
    "CachedExtensions"
    "History"
    "Backups"
    "databases"
    "GPUCache"
)

# File patterns that usually contain chat data
$ChatPatterns = @("*.json", "*.sqlite", "*.sqlite3", "*.db", "*.ldb", "*.log",
                  "*.vscdb", "*.backup", "*.jsonl", "MANIFEST*", "CURRENT", "LOG*")

# ─── Resolve targets ─────────────────────────────────────────────────────
$targetList = if ($Targets -eq "all") {
    @("cursor", "vscode", "insiders", "github")
} else {
    $Targets -split "," | ForEach-Object { $_.Trim().ToLower() }
}

Write-Host "`n===== ChatExtractor v2 =====" -ForegroundColor Cyan
Write-Host "Targets : $($targetList -join ', ')"
Write-Host "Output  : $OutDir"
Write-Host ""

$totalFiles = 0
$totalBytes = 0
$results    = @()

foreach ($target in $targetList) {
    if (-not $Sources.ContainsKey($target)) {
        Write-Host "[SKIP] Unknown target: $target" -ForegroundColor Yellow
        continue
    }

    $destBase = Join-Path $OutDir $target
    $found    = $false

    foreach ($srcRoot in $Sources[$target]) {
        if (-not (Test-Path $srcRoot)) { continue }
        $found = $true

        Write-Host "[+] $target : $srcRoot" -ForegroundColor Green

        # ── robocopy top-level chat files ──
        $destTop = Join-Path $destBase (Split-Path $srcRoot -Leaf)
        if (-not (Test-Path $destTop)) { New-Item -ItemType Directory -Path $destTop -Force | Out-Null }

        foreach ($pat in $ChatPatterns) {
            $roboArgs = @(
                "`"$srcRoot`""
                "`"$destTop`""
                $pat
                "/S"            # recurse
                "/NP"           # no progress
                "/NFL"          # no file list to stdout (speed)
                "/NDL"          # no dir list
                "/NJH"          # no job header
                "/NJS"          # no job summary
                "/R:0"          # no retries
                "/W:0"          # no wait
                "/MT:8"         # 8 threads
                "/XO"           # exclude older files (only copy newer/new)
            )
            $proc = Start-Process robocopy -ArgumentList $roboArgs -NoNewWindow -Wait -PassThru 2>$null
        }

        # ── Targeted subdirectories ──
        foreach ($sub in $SubDirs) {
            $subPath = Join-Path $srcRoot $sub
            if (-not (Test-Path $subPath)) { continue }

            $destSub = Join-Path $destTop $sub
            if (-not (Test-Path $destSub)) { New-Item -ItemType Directory -Path $destSub -Force | Out-Null }

            $roboArgs = @(
                "`"$subPath`""
                "`"$destSub`""
                "/E"            # all subdirs including empty
                "/NP"
                "/NFL"
                "/NDL"
                "/NJH"
                "/NJS"
                "/R:0"
                "/W:0"
                "/MT:8"
                "/XO"
            )
            $proc = Start-Process robocopy -ArgumentList $roboArgs -NoNewWindow -Wait -PassThru 2>$null
        }
    }

    if (-not $found) {
        Write-Host "[-] $target : no source dirs found" -ForegroundColor DarkGray
    }

    # Count results
    if (Test-Path $destBase) {
        $stats = Get-ChildItem -Path $destBase -Recurse -File -ErrorAction SilentlyContinue |
                 Measure-Object -Property Length -Sum
        $fc = $stats.Count
        $fb = $stats.Sum
        $totalFiles += $fc
        $totalBytes += $fb
        $results += [PSCustomObject]@{
            Target = $target
            Files  = $fc
            SizeMB = [math]::Round($fb / 1MB, 2)
        }
        Write-Host "    → $fc files ($([math]::Round($fb / 1MB, 2)) MB)" -ForegroundColor White
    }
}

# ─── LevelDB / IndexedDB detection ──────────────────────────────────────
Write-Host "`n--- LevelDB/IndexedDB scan ---" -ForegroundColor Cyan
$ldbFiles = Get-ChildItem -Path $OutDir -Recurse -Include "*.ldb","*.log","MANIFEST-*","CURRENT" -File -ErrorAction SilentlyContinue
if ($ldbFiles) {
    $ldbDirs = $ldbFiles | ForEach-Object { $_.DirectoryName } | Sort-Object -Unique
    Write-Host "Found $($ldbFiles.Count) LevelDB files across $($ldbDirs.Count) databases:" -ForegroundColor Green
    foreach ($d in $ldbDirs) {
        $relPath = $d.Replace($OutDir, "").TrimStart("\")
        Write-Host "  $relPath"
    }
} else {
    Write-Host "No LevelDB artifacts found." -ForegroundColor DarkGray
}

# ─── Content scan (optional) ────────────────────────────────────────────
if ($Scan) {
    Write-Host "`n--- Content scan for chat artifacts ---" -ForegroundColor Cyan
    $chatKeywords = @('"messages"', '"role"', '"content"', '"assistant"', '"user"',
                      '"model"', '"prompt"', '"completion"', '"conversation"',
                      '"copilot"', '"chat"', '"response"')

    $jsonFiles = Get-ChildItem -Path $OutDir -Recurse -Include "*.json","*.jsonl","*.vscdb" -File -ErrorAction SilentlyContinue |
                 Where-Object { $_.Length -gt 0 -and $_.Length -lt 50MB }

    $hitCount = 0
    $hitFiles = @()

    foreach ($jf in $jsonFiles) {
        try {
            $head = Get-Content $jf.FullName -TotalCount 50 -Raw -ErrorAction SilentlyContinue
            if (-not $head) { continue }

            foreach ($kw in $chatKeywords) {
                if ($head -match [regex]::Escape($kw)) {
                    $hitCount++
                    $hitFiles += $jf.FullName.Replace($OutDir, "").TrimStart("\")
                    break
                }
            }
        } catch { }
    }

    Write-Host "Chat-like content found in $hitCount files:" -ForegroundColor Green
    $hitFiles | Select-Object -First 30 | ForEach-Object { Write-Host "  $_" }
    if ($hitFiles.Count -gt 30) {
        Write-Host "  ... and $($hitFiles.Count - 30) more"
    }
}

# ─── Summary ────────────────────────────────────────────────────────────
Write-Host "`n===== Summary =====" -ForegroundColor Cyan
$results | Format-Table -AutoSize
Write-Host "Total: $totalFiles files ($([math]::Round($totalBytes / 1MB, 2)) MB)" -ForegroundColor White
Write-Host "Output: $OutDir`n"
