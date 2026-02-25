#Requires -Version 5.1
<#
.SYNOPSIS
    Automatically fix all markdown linting errors
.DESCRIPTION
    Fixes MD022, MD032, MD031, MD040, MD026, MD009, MD012, MD047, MD034, MD028, MD029
.EXAMPLE
    .\fix-all-markdown-lints.ps1
#>

param(
    [string[]]$Files = @(),
    [switch]$DryRun
)

$ErrorActionPreference = 'Continue'

function Fix-MarkdownFile {
    param(
        [string]$FilePath,
        [switch]$DryRun
    )
    
    if (-not (Test-Path $FilePath)) {
        Write-Warning "File not found: $FilePath"
        return
    }
    
    Write-Host "`n📝 Processing: $FilePath" -ForegroundColor Cyan
    
    $content = Get-Content $FilePath -Raw
    $originalContent = $content
    $fixCount = 0
    
    # Fix 1: MD047 - Ensure file ends with single newline
    if (-not $content.EndsWith("`n") -or $content.EndsWith("`n`n")) {
        $content = $content.TrimEnd() + "`n"
        $fixCount++
        Write-Host "  ✅ Fixed MD047: File ending" -ForegroundColor Green
    }
    
    # Fix 2: MD012 - Remove multiple consecutive blank lines
    while ($content -match "`n`n`n") {
        $content = $content -replace "`n`n`n", "`n`n"
        $fixCount++
    }
    if ($originalContent -ne $content) {
        Write-Host "  ✅ Fixed MD012: Multiple blank lines" -ForegroundColor Green
    }
    
    # Fix 3: MD009 - Remove trailing spaces
    $lines = $content -split "`n"
    $fixedLines = @()
    $trailingSpacesFixes = 0
    
    foreach ($line in $lines) {
        if ($line -match '\s+$' -and $line -notmatch '  $') {
            $fixedLines += $line.TrimEnd()
            $trailingSpacesFixes++
        } else {
            $fixedLines += $line
        }
    }
    
    if ($trailingSpacesFixes -gt 0) {
        $content = $fixedLines -join "`n"
        $fixCount += $trailingSpacesFixes
        Write-Host "  ✅ Fixed MD009: $trailingSpacesFixes trailing spaces" -ForegroundColor Green
    }
    
    # Fix 4: MD022 - Add blank lines around headings
    $lines = $content -split "`n"
    $fixedLines = @()
    $headingFixes = 0
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Check if line is a heading
        if ($line -match '^#{1,6}\s+') {
            # Add blank line before if needed
            if ($i -gt 0 -and $fixedLines[-1] -ne '') {
                $fixedLines += ''
                $headingFixes++
            }
            
            $fixedLines += $line
            
            # Add blank line after if needed
            if ($i -lt $lines.Count - 1 -and $lines[$i + 1] -ne '' -and $lines[$i + 1] -notmatch '^#{1,6}\s+') {
                $fixedLines += ''
                $headingFixes++
            }
        } else {
            $fixedLines += $line
        }
    }
    
    if ($headingFixes -gt 0) {
        $content = $fixedLines -join "`n"
        $fixCount += $headingFixes
        Write-Host "  ✅ Fixed MD022: $headingFixes heading blank lines" -ForegroundColor Green
    }
    
    # Fix 5: MD026 - Remove trailing punctuation from headings
    $punctuationFixes = 0
    $content = $content -replace '(^#{1,6}\s+.+)[!:;,.](\s*)$', '$1$2'
    if ($content -ne ($content -replace '(^#{1,6}\s+.+)[!:;,.](\s*)$', '$1$2')) {
        $punctuationFixes++
        $fixCount++
        Write-Host "  ✅ Fixed MD026: Heading punctuation" -ForegroundColor Green
    }
    
    # Fix 6: MD032 & MD031 - Add blank lines around lists and code blocks
    $lines = $content -split "`n"
    $fixedLines = @()
    $listFixes = 0
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $prevLine = if ($i -gt 0) { $lines[$i - 1] } else { '' }
        $nextLine = if ($i -lt $lines.Count - 1) { $lines[$i + 1] } else { '' }
        
        # Check if line is list item or code fence
        $isList = $line -match '^\s*[-*+]\s+' -or $line -match '^\s*\d+\.\s+'
        $isCodeFence = $line -match '^```'
        
        if ($isList -or $isCodeFence) {
            # Add blank line before if needed
            if ($prevLine -ne '' -and -not ($prevLine -match '^\s*[-*+]\s+') -and -not ($prevLine -match '^\s*\d+\.\s+') -and -not ($prevLine -match '^```') -and -not ($prevLine -match '^#{1,6}\s+')) {
                $fixedLines += ''
                $listFixes++
            }
        }
        
        $fixedLines += $line
        
        # Add blank line after lists/code if needed
        if ($isList -or $isCodeFence) {
            if ($nextLine -ne '' -and -not ($nextLine -match '^\s*[-*+]\s+') -and -not ($nextLine -match '^\s*\d+\.\s+') -and -not ($nextLine -match '^```') -and -not ($nextLine -match '^#{1,6}\s+')) {
                $fixedLines += ''
                $listFixes++
            }
        }
    }
    
    if ($listFixes -gt 0) {
        $content = $fixedLines -join "`n"
        $fixCount += $listFixes
        Write-Host "  ✅ Fixed MD032/MD031: $listFixes list/code blank lines" -ForegroundColor Green
    }
    
    # Fix 7: MD040 - Add language to code blocks
    $codeFixes = 0
    $content = $content -replace '(?m)^```\s*$', '```plaintext'
    if ($originalContent -ne $content) {
        $codeFixes = ([regex]::Matches($originalContent, '(?m)^```\s*$')).Count
        $fixCount += $codeFixes
        Write-Host "  ✅ Fixed MD040: $codeFixes code block languages" -ForegroundColor Green
    }
    
    # Fix 8: MD029 - Fix ordered list numbering (reset to 1, 2, 3...)
    $lines = $content -split "`n"
    $fixedLines = @()
    $numberingFixes = 0
    $currentNumber = 1
    $inList = $false
    
    foreach ($line in $lines) {
        if ($line -match '^\s*(\d+)\.\s+(.+)$') {
            $actualNumber = [int]$matches[1]
            if ($actualNumber -ne $currentNumber) {
                $fixedLines += $line -replace '^\s*\d+\.', "  $currentNumber."
                $numberingFixes++
            } else {
                $fixedLines += $line
            }
            $currentNumber++
            $inList = $true
        } else {
            if ($inList -and $line.Trim() -eq '') {
                $currentNumber = 1
                $inList = $false
            }
            $fixedLines += $line
        }
    }
    
    if ($numberingFixes -gt 0) {
        $content = $fixedLines -join "`n"
        $fixCount += $numberingFixes
        Write-Host "  ✅ Fixed MD029: $numberingFixes list numbering issues" -ForegroundColor Green
    }
    
    # Fix 9: MD034 - Wrap bare URLs in angle brackets
    $urlFixes = 0
    $content = $content -replace '(?<![\[<])(https?://[^\s\)]+)(?![\]>])', '<$1>'
    if ($originalContent -ne $content) {
        $urlFixes = ([regex]::Matches($originalContent, '(?<![\[<])(https?://[^\s\)]+)(?![\]>])')).Count
        $fixCount += $urlFixes
        Write-Host "  ✅ Fixed MD034: $urlFixes bare URLs" -ForegroundColor Green
    }
    
    # Fix 10: MD028 - Remove blank lines inside blockquotes
    $blockquoteFixes = 0
    while ($content -match '(?m)^>\s*$\n^>\s') {
        $content = $content -replace '(?m)^>\s*$\n(?=^>)', ''
        $blockquoteFixes++
    }
    if ($blockquoteFixes -gt 0) {
        $fixCount += $blockquoteFixes
        Write-Host "  ✅ Fixed MD028: $blockquoteFixes blockquote blank lines" -ForegroundColor Green
    }
    
    # Apply changes
    if ($content -ne $originalContent) {
        if (-not $DryRun) {
            Set-Content -Path $FilePath -Value $content -NoNewline -Encoding UTF8
            Write-Host "  💾 Saved $fixCount fixes" -ForegroundColor Green
        } else {
            Write-Host "  🔍 DRY RUN: Would save $fixCount fixes" -ForegroundColor Yellow
        }
        return $fixCount
    } else {
        Write-Host "  ✨ No fixes needed" -ForegroundColor Gray
        return 0
    }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║   📝 Markdown Linter Auto-Fix Tool 📝                        ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

# Find markdown files if not specified
if ($Files.Count -eq 0) {
    Write-Host "🔍 Scanning for markdown files..." -ForegroundColor Yellow
    
    $searchPaths = @(
        "D:\Security Research aka GitHub Repos\ProjectIDEAI\*.md",
        "C:\Users\HiH8e\OneDrive\Desktop\*.md",
        "D:\Security Research aka GitHub Repos\neuro-symphonic-workspace\*.md"
    )
    
    foreach ($path in $searchPaths) {
        $foundFiles = Get-ChildItem -Path $path -ErrorAction SilentlyContinue
        $Files += $foundFiles.FullName
    }
    
    Write-Host "  Found $($Files.Count) markdown files" -ForegroundColor Cyan
}

# Process each file
$totalFixes = 0
$filesProcessed = 0

foreach ($file in $Files) {
    $fixes = Fix-MarkdownFile -FilePath $file -DryRun:$DryRun
    $totalFixes += $fixes
    if ($fixes -gt 0) {
        $filesProcessed++
    }
}

# Summary
Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║                        SUMMARY                               ║
╚══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

Write-Host "Files Scanned:    " -NoNewline
Write-Host $Files.Count -ForegroundColor Cyan

Write-Host "Files Fixed:      " -NoNewline
Write-Host $filesProcessed -ForegroundColor Green

Write-Host "Total Fixes:      " -NoNewline
Write-Host $totalFixes -ForegroundColor Green

if ($DryRun) {
    Write-Host "`n⚠️  DRY RUN MODE - No files were modified" -ForegroundColor Yellow
    Write-Host "Run without -DryRun to apply changes" -ForegroundColor Gray
} else {
    Write-Host "`n✅ All markdown files fixed!" -ForegroundColor Green
}

Write-Host ""

