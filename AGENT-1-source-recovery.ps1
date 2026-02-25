#!/usr/bin/env pwsh
<#
.SYNOPSIS
    AGENT-1: Universal Source File Recovery from Cursor Cache History
    Extracts and categorizes ALL unique source files with latest versions
.DESCRIPTION
    Parses Cursor cache history (137K+ lines), groups by unique filepath,
    finds MAX timestamp for each, organizes by category, generates YAML+CSV reports
#>

$ErrorActionPreference = "Stop"

# Input data location
$historyFile = "c:\Users\HiH8e\AppData\Roaming\Cursor\User\workspaceStorage\08d7c9c56e3f5cf7bd075a5bc790a314\anysphere.cursor-retrieval\embeddable_files.txt"
$outputDir = "d:\rawrxd"

Write-Host "[AGENT-1] Starting source recovery from Cursor cache..." -ForegroundColor Cyan

# Check if history file exists
if (-not (Test-Path $historyFile)) {
    Write-Error "History file not found: $historyFile"
    exit 1
}

Write-Host "[STEP 1] Reading history file (137K+ lines)..." -ForegroundColor Yellow
$lines = @(Get-Content $historyFile -ErrorAction Stop)
Write-Host "  ✓ Read $($lines.Count) lines" -ForegroundColor Green

# Parse all lines into a hashtable grouped by unique source filepath
Write-Host "[STEP 2] Parsing and grouping by unique source filepath..." -ForegroundColor Yellow

$fileMap = @{}  # key: source_filepath, value: @{timestamp, hash, ext}

foreach ($line in $lines) {
    # Format: history\all_versions\src\{FILEPATH}\{TIMESTAMP}_{HASH}.{EXT}
    # Extract parts
    if ($line -match 'history\\all_versions\\(.+?)\\(\d+)_([A-Za-z0-9]+)\.([a-z]+)$') {
        $sourcePath = $matches[1]
        $timestamp = [int64]$matches[2]
        $hash = $matches[3]
        $ext = $matches[4]
        
        # Initialize or update the entry with the latest timestamp
        if (-not $fileMap.ContainsKey($sourcePath)) {
            $fileMap[$sourcePath] = @{
                timestamp = $timestamp
                hash = $hash
                ext = $ext
                line = $line
            }
        } else {
            # Keep the entry with maximum timestamp
            if ($timestamp -gt $fileMap[$sourcePath].timestamp) {
                $fileMap[$sourcePath] = @{
                    timestamp = $timestamp
                    hash = $hash
                    ext = $ext
                    line = $line
                }
            }
        }
    }
}

Write-Host "  ✓ Found $($fileMap.Count) unique source files" -ForegroundColor Green

# Categorize files
Write-Host "[STEP 3] Categorizing files..." -ForegroundColor Yellow

$categories = @{
    "WIN32_IDE_SOURCES" = @()
    "AGENT_SOURCES" = @()
    "ASSEMBLY_SOURCES" = @()
    "AI_BACKEND_SOURCES" = @()
    "API_SERVER_SOURCES" = @()
    "CORE_INFRASTRUCTURE" = @()
    "QT_SOURCES" = @()
    "OTHER_SOURCES" = @()
}

foreach ($sourcePath in $fileMap.Keys) {
    $entry = $fileMap[$sourcePath]
    $filename = Split-Path $sourcePath -Leaf
    
    # Categorize based on patterns
    if ($sourcePath -match 'Win32IDE|CreateWindowEx|WndProc|MSG|HWND|Win32' -or 
        $filename -match 'Win32IDE') {
        $categories["WIN32_IDE_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match 'agent|agentic|orchestrator' -or
            $filename -match '^agent_|^agentic_|orchestrator') {
        $categories["AGENT_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match '\.asm$') {
        $categories["ASSEMBLY_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match 'ai_|model_|inference_|digestion|completion_provider|gguf' -or
            $filename -match '^ai_|^model_|^inference_|^digestion') {
        $categories["AI_BACKEND_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match 'api_server|http|bridge|server' -or
            $filename -match 'server|api|bridge|http') {
        $categories["API_SERVER_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match 'main|bootstrap|entry|init' -or
            $filename -match '^main|^bootstrap|^entry|^init') {
        $categories["CORE_INFRASTRUCTURE"] += @{ path = $sourcePath; entry = $entry }
    }
    elseif ($sourcePath -match 'Qt|qt|QMainWindow|MainWindow' -or
            $filename -match 'MainWindow|qt') {
        $categories["QT_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
    else {
        $categories["OTHER_SOURCES"] += @{ path = $sourcePath; entry = $entry }
    }
}

# Print categorization summary
foreach ($category in $categories.Keys) {
    $count = $categories[$category].Count
    Write-Host "  ✓ $category`: $count files" -ForegroundColor Green
}

# Generate YAML report
Write-Host "[STEP 4] Generating YAML report..." -ForegroundColor Yellow

$yamlReport = @"
RECOVERY_SUMMARY:
  timestamp_generated: 2026-02-21
  cursor_cache_location: c:\Users\HiH8e\AppData\Roaming\Cursor\User\workspaceStorage\08d7c9c56e3f5cf7bd075a5bc790a314\anysphere.cursor-retrieval
  total_unique_files: $($fileMap.Count)

CATEGORIZED_FILES:
"@

foreach ($category in $categories.Keys | Sort-Object) {
    $files = $categories[$category]
    $count = $files.Count
    
    $yamlReport += "`n`n  $($category):`n    count: $count`n    latest_files:`n"
    
    # Sort by source path and add top files
    $sortedFiles = $files | Sort-Object { $_.path }
    foreach ($file in $sortedFiles | Select-Object -First 50) {
        $yamlReport += "      - path: `"$($file.path)`"`n"
        $yamlReport += "        timestamp: $($file.entry.timestamp)`n"
        $yamlReport += "        hash: $($file.entry.hash)`n"
        $yamlReport += "        ext: $($file.entry.ext)`n"
    }
    
    if ($sortedFiles.Count -gt 50) {
        $yamlReport += "      ... and $($sortedFiles.Count - 50) more files`n"
    }
}

$yamlReport += @"

RECOVERY_PATHS:
  base_workspace_dir: d:\rawrxd
  src_cache_dir: c:\Users\HiH8e\AppData\Roaming\Cursor\User\workspaceStorage\08d7c9c56e3f5cf7bd075a5bc790a314\anysphere.cursor-retrieval
  history_dir: history\all_versions
"@

$yamlReportPath = Join-Path $outputDir "AGENT-1-SOURCE-RECOVERY-COMPLETE.md"
$yamlReport | Out-File $yamlReportPath -Encoding UTF8
Write-Host "  ✓ Saved YAML report to: $yamlReportPath" -ForegroundColor Green

# Generate CSV mapping
Write-Host "[STEP 5] Generating CSV mapping..." -ForegroundColor Yellow

$csvRecords = @()
$csvHeader = "ORIGINAL_SOURCE_PATH,CURSOR_CACHE_PATH,TIMESTAMP,FILE_EXTENSION,HASH"

foreach ($sourcePath in ($fileMap.Keys | Sort-Object)) {
    $entry = $fileMap[$sourcePath]
    $cacheRelativePath = "history\all_versions\$sourcePath\$($entry.timestamp)_$($entry.hash).$($entry.ext)"
    
    $csvRecords += "$sourcePath,$cacheRelativePath,$($entry.timestamp),$($entry.ext),$($entry.hash)"
}

$csvPath = Join-Path $outputDir "source_recovery_map.csv"
@($csvHeader) + $csvRecords | Out-File $csvPath -Encoding UTF8
Write-Host "  ✓ Saved CSV mapping to: $csvPath with $($fileMap.Count) entries" -ForegroundColor Green

# Search for Win32IDE files specifically
Write-Host "[STEP 6] Searching for Win32IDE files..." -ForegroundColor Yellow

$win32IdeFiles = @()
foreach ($sourcePath in $fileMap.Keys) {
    if ($sourcePath -match 'Win32IDE|WndProc|CreateWindowEx|GetMessage') {
        $win32IdeFiles += @{
            path = $sourcePath
            timestamp = $fileMap[$sourcePath].timestamp
            hash = $fileMap[$sourcePath].hash
            ext = $fileMap[$sourcePath].ext
        }
    }
}

Write-Host "  ✓ Found $($win32IdeFiles.Count) Win32IDE related files" -ForegroundColor Green

if ($win32IdeFiles.Count -gt 0) {
    Write-Host "`n  Win32IDE Files Found:"
    foreach ($file in ($win32IdeFiles | Sort-Object path)) {
        Write-Host "    • $($file.path) (timestamp: $($file.timestamp))"
    }
}

# Summary report
Write-Host "`n[STEP 7] Generation complete!" -ForegroundColor Cyan
Write-Host @"

RECOVERY SUMMARY:
  Total unique files: $($fileMap.Count)
  
  Categories:
    Win32 IDE: $($categories["WIN32_IDE_SOURCES"].Count)
    Agent/Agentic: $($categories["AGENT_SOURCES"].Count)
    Assembly: $($categories["ASSEMBLY_SOURCES"].Count)
    AI/Backend: $($categories["AI_BACKEND_SOURCES"].Count)
    API/Server: $($categories["API_SERVER_SOURCES"].Count)
    Core Infrastructure: $($categories["CORE_INFRASTRUCTURE"].Count)
    Qt Sources: $($categories["QT_SOURCES"].Count)
    Other: $($categories["OTHER_SOURCES"].Count)
  
  Output Files:
    ✓ $yamlReportPath
    ✓ $csvPath
"@ -ForegroundColor Green

exit 0
