# Comprehensive File Consolidation Script
# This script consolidates all C++/header files from D:\rawrxd\src into a single file

param(
    [string]$SourceDir = "D:\rawrxd\src",
    [string]$OutputFile = "D:\rawrxd\Ship\Win32_IDE_Complete.cpp"
)

$ErrorActionPreference = "Stop"

Write-Host "Starting file consolidation..."
Write-Host "Source: $SourceDir"
Write-Host "Output: $OutputFile"

# Get all .cpp and .h files
$allFiles = @()
$allFiles += Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp" -File
$allFiles += Get-ChildItem -Path $SourceDir -Recurse -Include "*.h" -File

Write-Host "Found $($allFiles.Count) files to consolidate"

# Collect all content
$consolidatedContent = New-Object System.Text.StringBuilder
$processedIncludes = @{}
$processedFiles = @()

# Track function/class definitions to avoid duplicates
$definedSymbols = @{}

# First pass: Collect all standard includes and system includes
$standardIncludes = @{
    "<windows.h>" = $true
    "<string>" = $true
    "<vector>" = $true
    "<map>" = $true
    "<memory>" = $true
    "<iostream>" = $true
    "<fstream>" = $true
    "<algorithm>" = $true
    "<chrono>" = $true
    "<ctime>" = $true
    "<sstream>" = $true
    "<thread>" = $true
    "<mutex>" = $true
    "<filesystem>" = $true
    "<array>" = $true
    "<queue>" = $true
    "<deque>" = $true
    "<set>" = $true
    "<functional>" = $true
    "<optional>" = $true
    "<variant>" = $true
    "<exception>" = $true
    "<stdexcept>" = $true
    "<cstring>" = $true
    "<cmath>" = $true
    "<limits>" = $true
    "<numeric>" = $true
    "<random>" = $true
    "<regex>" = $true
    "<atomic>" = $true
    "<condition_variable>" = $true
    "<shared_mutex>" = $true
    "<commdlg.h>" = $true
    "<richedit.h>" = $true
    "<commctrl.h>" = $true
    "<shlobj.h>" = $true
    "<shellapi.h>" = $true
    "<winhttp.h>" = $true
    "<wininet.h>" = $true
    "<winsock2.h>" = $true
    "<ws2tcpip.h>" = $true
    "<iphlpapi.h>" = $true
    "<tlhelp32.h>" = $true
    "<psapi.h>" = $true
    "<dbghelp.h>" = $true
    "<intrin.h>" = $true
    "<immintrin.h>" = $true
}

$collectedIncludes = New-Object 'System.Collections.Generic.HashSet[string]'

# Function to extract includes from file content
function Get-IncludesFromContent {
    param([string]$Content)
    $includes = @()
    $lines = $Content -split "`n"
    foreach ($line in $lines) {
        if ($line -match '^\s*#include\s+[<"](.+)[>"]') {
            $includes += $matches[1]
        }
    }
    return $includes
}

# Function to check if content is mostly duplicate
function Is-DuplicateContent {
    param([string]$Content1, [string]$Content2)
    $hash1 = [System.Security.Cryptography.SHA256]::ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Content1))
    $hash2 = [System.Security.Cryptography.SHA256]::ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Content2))
    return -not (Compare-Object $hash1 $hash2)
}

# Collect and deduplicate content
Write-Host "Collecting file content..."
$fileContents = @()

foreach ($file in $allFiles) {
    try {
        $content = Get-Content -Path $file.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        if ($null -eq $content) {
            $content = Get-Content -Path $file.FullName -Raw -Encoding Default -ErrorAction SilentlyContinue
        }
        
        if ($content) {
            # Extract includes
            $includes = Get-IncludesFromContent -Content $content
            foreach ($inc in $includes) {
                if ($standardIncludes.ContainsKey("<$inc>")) {
                    $collectedIncludes.Add("<$inc>") | Out-Null
                } elseif ($standardIncludes.ContainsKey("`"$inc`"")) {
                    $collectedIncludes.Add("`"$inc`"") | Out-Null
                } elseif (-not $inc.StartsWith("..")) {
                    $collectedIncludes.Add("`"$inc`"") | Out-Null
                }
            }
            
            $fileContents += @{
                File = $file.FullName
                Content = $content
                Includes = $includes
                RelativePath = $file.FullName.Replace($SourceDir, "").TrimStart("\")
            }
        }
    } catch {
        Write-Host "Error processing file $($file.FullName): $_" -ForegroundColor Yellow
    }
}

Write-Host "Processed $($fileContents.Count) files successfully"

# Start building consolidated output
[void]$consolidatedContent.AppendLine("// ================================================================================")
[void]$consolidatedContent.AppendLine("// CONSOLIDATED C++ IDE IMPLEMENTATION")
[void]$consolidatedContent.AppendLine("// Generated from $(($fileContents.Count)) source files")
[void]$consolidatedContent.AppendLine("// Date: $(Get-Date)")
[void]$consolidatedContent.AppendLine("// ================================================================================")
[void]$consolidatedContent.AppendLine("")
[void]$consolidatedContent.AppendLine("#pragma once")
[void]$consolidatedContent.AppendLine("")

# Add all collected unique includes
[void]$consolidatedContent.AppendLine("// Standard and System Includes")
[void]$consolidatedContent.AppendLine("// ================================================================================")
foreach ($inc in ($collectedIncludes | Sort-Object)) {
    [void]$consolidatedContent.AppendLine("#include $inc")
}
[void]$consolidatedContent.AppendLine("")

# Remove pragma once and includes from file contents
Write-Host "Consolidating file contents..."
$consolidatedLines = 0
$skippedLines = 0
$processedSymbols = New-Object 'System.Collections.Generic.HashSet[string]'

# Sort files to process header files first, then implementation files
$sortedFiles = $fileContents | Sort-Object {
    $path = $_.RelativePath
    if ($path.EndsWith('.h')) { 0 }
    else { 1 }
}

foreach ($fileData in $sortedFiles) {
    try {
        $content = $fileData.Content
        $lines = $content -split "`n"
        
        [void]$consolidatedContent.AppendLine("// ================================================================================")
        [void]$consolidatedContent.AppendLine("// Source: $($fileData.RelativePath)")
        [void]$consolidatedContent.AppendLine("// ================================================================================")
        [void]$consolidatedContent.AppendLine("")
        
        $inContent = $false
        foreach ($line in $lines) {
            # Skip pragma once
            if ($line -match "^\s*#pragma\s+once") {
                $skippedLines++
                continue
            }
            
            # Skip local includes (relative paths)
            if ($line -match '^\s*#include\s+"([^"]*)"') {
                $incPath = $matches[1]
                if ($incPath.StartsWith("..") -or $incPath.Contains("\")) {
                    continue
                }
                # Include local headers inline
                if ($incPath.EndsWith(".h")) {
                    $headerContent = $fileData.Content | Where-Object { $_ -match "#include.*$incPath" }
                    if ($null -ne $headerContent) {
                        [void]$consolidatedContent.AppendLine($line)
                    }
                    continue
                }
            }
            
            # Skip system includes (already collected at top)
            if ($line -match "^\s*#include\s+<") {
                continue
            }
            
            # Track definitions to avoid duplicates
            if ($line -match "^\s*(class|struct|namespace|int|void|bool|auto|const)\s+(\w+)\s*[{:;(]") {
                $symbol = $matches[2]
                if ($processedSymbols.Contains($symbol)) {
                    # Check if this is definitely a duplicate
                    continue
                }
                $processedSymbols.Add($symbol) | Out-Null
            }
            
            [void]$consolidatedContent.AppendLine($line)
            $consolidatedLines++
        }
        
        [void]$consolidatedContent.AppendLine("")
        Write-Host "Consolidated: $($fileData.RelativePath)" -ForegroundColor Green
    } catch {
        Write-Host "Error consolidating $($fileData.File): $_" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Writing consolidated file..."

# Save the file
try {
    # Ensure output directory exists
    $outputDir = Split-Path -Path $OutputFile
    if (-not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    }
    
    $consolidatedContent.ToString() | Out-File -Encoding UTF8 -FilePath $OutputFile -Force
    $fileSize = (Get-Item $OutputFile).Length / 1MB
    Write-Host "✓ Consolidated file created: $OutputFile" -ForegroundColor Green
    Write-Host "  File size: $([Math]::Round($fileSize, 2)) MB"
    Write-Host "  Lines consolidated: $consolidatedLines"
    Write-Host "  Lines skipped: $skippedLines"
    Write-Host "  Unique includes: $($collectedIncludes.Count)"
} catch {
    Write-Host "Error writing output file: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Consolidation complete!" -ForegroundColor Green
Write-Host "Summary:"
Write-Host "  Total source files: $($fileContents.Count)"
Write-Host "  Output file: $OutputFile"
Write-Host "  File size: $([Math]::Round($fileSize, 2)) MB"
