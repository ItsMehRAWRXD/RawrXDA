#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Source Code Digester - Creates searchable knowledge base from entire codebase

.DESCRIPTION
    Scans and indexes all source files to create a comprehensive knowledge base
    that the chatbot can use to answer questions about the actual implementation.
    
    Features:
    - Indexes all PowerShell, C++, Markdown files
    - Extracts functions, classes, comments
    - Creates searchable index by topic/keyword
    - Generates documentation snippets
    - Provides file location mapping

.PARAMETER Operation
    digest, search, update, export

.EXAMPLE
    .\source_digester.ps1 -Operation digest
    
.EXAMPLE
    .\source_digester.ps1 -Operation search -Query "swarm deployment"
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('digest', 'search', 'update', 'export', 'stats')]
    [string]$Operation = "digest",
    
    [Parameter(Mandatory=$false)]
    [string]$RootPath = (if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Split-Path $PSScriptRoot -Parent) }),
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = (Join-Path (if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Split-Path $PSScriptRoot -Parent) }) "data" "knowledge_base.json"),
    
    [Parameter(Mandatory=$false)]
    [string]$Query = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# SOURCE CODE DIGESTER
# ═══════════════════════════════════════════════════════════════════════════════

class SourceDigester {
    [hashtable]$KnowledgeBase
    [string]$RootPath
    [System.Collections.ArrayList]$ProcessedFiles
    [hashtable]$Statistics
    
    SourceDigester([string]$rootPath) {
        $this.RootPath = $rootPath
        $this.ProcessedFiles = [System.Collections.ArrayList]::new()
        $this.KnowledgeBase = @{
            Files = @{}
            Functions = @{}
            Classes = @{}
            Topics = @{}
            Keywords = @{}
            Documentation = @{}
            Metadata = @{
                DigestDate = Get-Date -Format "o"
                Version = "1.0.0"
                FileCount = 0
                TotalLines = 0
            }
        }
        $this.Statistics = @{
            FilesProcessed = 0
            FunctionsFound = 0
            ClassesFound = 0
            LinesProcessed = 0
            KeywordsExtracted = 0
        }
    }
    
    [void] DigestAllSources() {
        Write-Host "`n╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║                    SOURCE CODE DIGESTER                                       ║" -ForegroundColor Cyan
        Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
        
        Write-Host "  Root Path: $($this.RootPath)" -ForegroundColor Gray
        Write-Host "  Starting digestion...`n" -ForegroundColor Yellow
        
        # Find all relevant files
        $patterns = @("*.ps1", "*.psm1", "*.cpp", "*.h", "*.md")
        $allFiles = @()
        
        foreach ($pattern in $patterns) {
            $files = Get-ChildItem -Path $this.RootPath -Filter $pattern -Recurse -File -ErrorAction SilentlyContinue
            $allFiles += $files
        }
        
        Write-Host "  Found $($allFiles.Count) files to process" -ForegroundColor Cyan
        
        $progress = 0
        foreach ($file in $allFiles) {
            $progress++
            $percent = [Math]::Round(($progress / $allFiles.Count) * 100, 1)
            Write-Host "`r  Progress: $percent% ($progress/$($allFiles.Count))" -NoNewline -ForegroundColor Yellow
            
            $this.DigestFile($file)
        }
        
        Write-Host "`n`n  ✅ Digestion complete!" -ForegroundColor Green
        $this.DisplayStatistics()
        $this.BuildTopicIndex()
        $this.BuildKeywordIndex()
    }
    
    [void] DigestFile([System.IO.FileInfo]$file) {
        try {
            $content = Get-Content $file.FullName -Raw -ErrorAction Stop
            $lines = $content -split "`n"
            
            $fileInfo = @{
                Path = $file.FullName
                RelativePath = $file.FullName -replace [regex]::Escape($this.RootPath), ""
                Name = $file.Name
                Extension = $file.Extension
                Size = $file.Length
                Lines = $lines.Count
                Functions = @()
                Classes = @()
                Comments = @()
                Keywords = @()
                Synopsis = ""
            }
            
            # Process based on file type
            switch ($file.Extension) {
                ".ps1" { $this.DigestPowerShell($content, $lines, $fileInfo) }
                ".psm1" { $this.DigestPowerShell($content, $lines, $fileInfo) }
                ".cpp" { $this.DigestCpp($content, $lines, $fileInfo) }
                ".h" { $this.DigestCpp($content, $lines, $fileInfo) }
                ".md" { $this.DigestMarkdown($content, $lines, $fileInfo) }
            }
            
            $this.KnowledgeBase.Files[$file.FullName] = $fileInfo
            $this.ProcessedFiles.Add($file.FullName) | Out-Null
            
            $this.Statistics.FilesProcessed++
            $this.Statistics.LinesProcessed += $lines.Count
            
        }
        catch {
            Write-Verbose "Failed to process $($file.FullName): $_"
        }
    }
    
    [void] DigestPowerShell([string]$content, [string[]]$lines, [hashtable]$fileInfo) {
        # Extract functions
        $functionPattern = '(?m)^function\s+([A-Za-z0-9_-]+)\s*\{'
        $matches = [regex]::Matches($content, $functionPattern)
        
        foreach ($match in $matches) {
            $funcName = $match.Groups[1].Value
            $funcInfo = $this.ExtractFunctionDetails($content, $funcName, $match.Index)
            
            $fileInfo.Functions += $funcName
            $this.KnowledgeBase.Functions[$funcName] = $funcInfo
            $this.Statistics.FunctionsFound++
        }
        
        # Extract classes
        $classPattern = '(?m)^class\s+([A-Za-z0-9_]+)\s*\{'
        $matches = [regex]::Matches($content, $classPattern)
        
        foreach ($match in $matches) {
            $className = $match.Groups[1].Value
            $classInfo = $this.ExtractClassDetails($content, $className, $match.Index)
            
            $fileInfo.Classes += $className
            $this.KnowledgeBase.Classes[$className] = $classInfo
            $this.Statistics.ClassesFound++
        }
        
        # Extract synopsis from comment-based help
        if ($content -match '\.SYNOPSIS\s+(.+?)(?=\.DESCRIPTION|\.PARAMETER|\.EXAMPLE|#>)') {
            $fileInfo.Synopsis = $matches[1].Trim()
        }
        
        # Extract keywords from comments
        $commentPattern = '#\s*(.+?)$'
        foreach ($line in $lines) {
            if ($line -match $commentPattern) {
                $comment = $matches[1].Trim()
                $fileInfo.Comments += $comment
                
                # Extract significant keywords
                $words = $comment -split '\s+' | Where-Object { $_.Length -gt 3 }
                foreach ($word in $words) {
                    $fileInfo.Keywords += $word.ToLower()
                }
            }
        }
    }
    
    [void] DigestCpp([string]$content, [string[]]$lines, [hashtable]$fileInfo) {
        # Extract function declarations
        $funcPattern = '(?m)^\s*(?:static\s+)?(?:inline\s+)?(?:\w+\s+)+(\w+)\s*\([^)]*\)\s*[{;]'
        $matches = [regex]::Matches($content, $funcPattern)
        
        foreach ($match in $matches) {
            $funcName = $match.Groups[1].Value
            if ($funcName -notmatch '^(if|for|while|switch)$') {
                $funcInfo = $this.ExtractCppFunctionDetails($content, $funcName, $match.Index)
                $fileInfo.Functions += $funcName
                $this.KnowledgeBase.Functions[$funcName] = $funcInfo
                $this.Statistics.FunctionsFound++
            }
        }
        
        # Extract classes
        $classPattern = '(?m)^(?:class|struct)\s+(\w+)'
        $matches = [regex]::Matches($content, $classPattern)
        
        foreach ($match in $matches) {
            $className = $match.Groups[1].Value
            $classInfo = $this.ExtractCppClassDetails($content, $className, $match.Index)
            $fileInfo.Classes += $className
            $this.KnowledgeBase.Classes[$className] = $classInfo
            $this.Statistics.ClassesFound++
        }
        
        # Extract comments
        $commentPattern = '//\s*(.+?)$'
        foreach ($line in $lines) {
            if ($line -match $commentPattern) {
                $comment = $matches[1].Trim()
                $fileInfo.Comments += $comment
                
                $words = $comment -split '\s+' | Where-Object { $_.Length -gt 3 }
                foreach ($word in $words) {
                    $fileInfo.Keywords += $word.ToLower()
                }
            }
        }
    }
    
    [void] DigestMarkdown([string]$content, [string[]]$lines, [hashtable]$fileInfo) {
        # Extract headings
        $headingPattern = '(?m)^#+\s+(.+?)$'
        $matches = [regex]::Matches($content, $headingPattern)
        
        $headings = @()
        foreach ($match in $matches) {
            $headings += $match.Groups[1].Value
        }
        
        $fileInfo.Synopsis = if ($headings.Count -gt 0) { $headings[0] } else { "" }
        
        # Extract code blocks
        $codeBlockPattern = '```(\w+)\s+([\s\S]*?)```'
        $matches = [regex]::Matches($content, $codeBlockPattern)
        
        foreach ($match in $matches) {
            $lang = $match.Groups[1].Value
            $code = $match.Groups[2].Value
            $fileInfo.Keywords += $lang
        }
        
        # Extract keywords from text
        $words = $content -split '\s+' | Where-Object { $_.Length -gt 4 -and $_ -match '^[A-Za-z]+$' }
        $uniqueWords = $words | Select-Object -Unique
        foreach ($word in $uniqueWords) {
            $fileInfo.Keywords += $word.ToLower()
        }
    }
    
    [hashtable] ExtractFunctionDetails([string]$content, [string]$funcName, [int]$startIndex) {
        # Find function help comment
        $helpPattern = "<#[\s\S]*?\.SYNOPSIS\s+(.+?)(?=\.DESCRIPTION|\.PARAMETER|#>)"
        $beforeFunc = $content.Substring(0, $startIndex)
        
        $synopsis = ""
        if ($beforeFunc -match $helpPattern) {
            $synopsis = $matches[1].Trim()
        }
        
        # Extract parameters
        $paramPattern = "\[Parameter[^\]]*\]\s*\[(\w+)\]\s*\`$(\w+)"
        $params = @()
        
        $funcContent = $content.Substring($startIndex, [Math]::Min(2000, $content.Length - $startIndex))
        $paramMatches = [regex]::Matches($funcContent, $paramPattern)
        
        foreach ($match in $paramMatches) {
            $params += @{
                Type = $match.Groups[1].Value
                Name = $match.Groups[2].Value
            }
        }
        
        return @{
            Name = $funcName
            Synopsis = $synopsis
            Parameters = $params
            Location = "Index: $startIndex"
        }
    }
    
    [hashtable] ExtractClassDetails([string]$content, [string]$className, [int]$startIndex) {
        $classContent = $content.Substring($startIndex, [Math]::Min(3000, $content.Length - $startIndex))
        
        # Extract properties
        $propPattern = '\[(\w+)\]\s*\$(\w+)'
        $props = @()
        $propMatches = [regex]::Matches($classContent, $propPattern)
        
        foreach ($match in $propMatches) {
            $props += @{
                Type = $match.Groups[1].Value
                Name = $match.Groups[2].Value
            }
        }
        
        # Extract methods
        $methodPattern = '\[(\w+)\]\s+(\w+)\s*\('
        $methods = @()
        $methodMatches = [regex]::Matches($classContent, $methodPattern)
        
        foreach ($match in $methodMatches) {
            $methods += @{
                ReturnType = $match.Groups[1].Value
                Name = $match.Groups[2].Value
            }
        }
        
        return @{
            Name = $className
            Properties = $props
            Methods = $methods
            Location = "Index: $startIndex"
        }
    }
    
    [hashtable] ExtractCppFunctionDetails([string]$content, [string]$funcName, [int]$startIndex) {
        $funcContent = $content.Substring([Math]::Max(0, $startIndex - 200), [Math]::Min(500, $content.Length - [Math]::Max(0, $startIndex - 200)))
        
        # Look for comment above function
        $commentPattern = '//\s*(.+?)$'
        $lines = $funcContent -split "`n"
        $comment = ""
        
        for ($i = $lines.Count - 1; $i -ge 0; $i--) {
            if ($lines[$i] -match $commentPattern) {
                $comment = $matches[1].Trim()
                break
            }
        }
        
        return @{
            Name = $funcName
            Synopsis = $comment
            Location = "Index: $startIndex"
        }
    }
    
    [hashtable] ExtractCppClassDetails([string]$content, [string]$className, [int]$startIndex) {
        return @{
            Name = $className
            Synopsis = ""
            Location = "Index: $startIndex"
        }
    }
    
    [void] BuildTopicIndex() {
        Write-Host "`n  Building topic index..." -ForegroundColor Yellow
        
        $topics = @{
            "swarm" = @()
            "todo" = @()
            "model" = @()
            "benchmark" = @()
            "quantization" = @()
            "training" = @()
            "agent" = @()
            "win32" = @()
        }
        
        foreach ($filePath in $this.KnowledgeBase.Files.Keys) {
            $fileInfo = $this.KnowledgeBase.Files[$filePath]
            
            foreach ($topic in $topics.Keys) {
                $searchText = ($fileInfo.Name + " " + $fileInfo.Synopsis + " " + ($fileInfo.Keywords -join " ")).ToLower()
                
                if ($searchText -match $topic) {
                    $topics[$topic] += @{
                        File = $fileInfo.RelativePath
                        Name = $fileInfo.Name
                        Synopsis = $fileInfo.Synopsis
                        Functions = $fileInfo.Functions
                        Classes = $fileInfo.Classes
                    }
                }
            }
        }
        
        $this.KnowledgeBase.Topics = $topics
        Write-Host "  ✓ Indexed $($topics.Keys.Count) topics" -ForegroundColor Green
    }
    
    [void] BuildKeywordIndex() {
        Write-Host "  Building keyword index..." -ForegroundColor Yellow
        
        $keywordMap = @{}
        
        foreach ($filePath in $this.KnowledgeBase.Files.Keys) {
            $fileInfo = $this.KnowledgeBase.Files[$filePath]
            
            foreach ($keyword in $fileInfo.Keywords) {
                if ($keyword.Length -gt 3) {
                    if (-not $keywordMap.ContainsKey($keyword)) {
                        $keywordMap[$keyword] = @()
                    }
                    
                    $keywordMap[$keyword] += $fileInfo.RelativePath
                }
            }
        }
        
        $this.KnowledgeBase.Keywords = $keywordMap
        $this.Statistics.KeywordsExtracted = $keywordMap.Keys.Count
        Write-Host "  ✓ Indexed $($keywordMap.Keys.Count) unique keywords" -ForegroundColor Green
    }
    
    [void] DisplayStatistics() {
        Write-Host "`n  STATISTICS:" -ForegroundColor Magenta
        Write-Host "  ─────────────────────────────────────────" -ForegroundColor DarkGray
        Write-Host "  Files Processed:     $($this.Statistics.FilesProcessed)" -ForegroundColor Gray
        Write-Host "  Functions Found:     $($this.Statistics.FunctionsFound)" -ForegroundColor Gray
        Write-Host "  Classes Found:       $($this.Statistics.ClassesFound)" -ForegroundColor Gray
        Write-Host "  Lines Processed:     $($this.Statistics.LinesProcessed)" -ForegroundColor Gray
        Write-Host "  Keywords Extracted:  $($this.Statistics.KeywordsExtracted)" -ForegroundColor Gray
    }
    
    [void] Save([string]$outputPath) {
        Write-Host "`n  Saving knowledge base to: $outputPath" -ForegroundColor Yellow
        
        $outputDir = Split-Path $outputPath -Parent
        if (-not (Test-Path $outputDir)) {
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
        }
        
        $this.KnowledgeBase.Metadata.FileCount = $this.Statistics.FilesProcessed
        $this.KnowledgeBase.Metadata.TotalLines = $this.Statistics.LinesProcessed
        
        $json = $this.KnowledgeBase | ConvertTo-Json -Depth 10 -Compress
        $json | Set-Content $outputPath -Encoding UTF8
        
        $sizeMB = [Math]::Round((Get-Item $outputPath).Length / 1MB, 2)
        Write-Host "  ✓ Saved! Size: $sizeMB MB" -ForegroundColor Green
    }
    
    [hashtable] Search([string]$query) {
        $results = @{
            Files = @()
            Functions = @()
            Classes = @()
            Topics = @()
            Score = 0
        }
        
        $queryLower = $query.ToLower()
        $keywords = $queryLower -split '\s+' | Where-Object { $_.Length -gt 2 }
        
        # Search files
        foreach ($filePath in $this.KnowledgeBase.Files.Keys) {
            $fileInfo = $this.KnowledgeBase.Files[$filePath]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($fileInfo.Name.ToLower() -match $keyword) { $score += 5 }
                if ($fileInfo.Synopsis.ToLower() -match $keyword) { $score += 3 }
                if ($fileInfo.Keywords -contains $keyword) { $score += 2 }
            }
            
            if ($score -gt 0) {
                $results.Files += @{
                    Path = $fileInfo.RelativePath
                    Name = $fileInfo.Name
                    Synopsis = $fileInfo.Synopsis
                    Score = $score
                }
            }
        }
        
        # Search functions
        foreach ($funcName in $this.KnowledgeBase.Functions.Keys) {
            $funcInfo = $this.KnowledgeBase.Functions[$funcName]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($funcName.ToLower() -match $keyword) { $score += 5 }
                if ($funcInfo.Synopsis.ToLower() -match $keyword) { $score += 3 }
            }
            
            if ($score -gt 0) {
                $results.Functions += @{
                    Name = $funcName
                    Synopsis = $funcInfo.Synopsis
                    Score = $score
                }
            }
        }
        
        # Sort by score
        $results.Files = $results.Files | Sort-Object -Property Score -Descending
        $results.Functions = $results.Functions | Sort-Object -Property Score -Descending
        
        $results.Score = ($results.Files | Measure-Object -Property Score -Sum).Sum + 
                         ($results.Functions | Measure-Object -Property Score -Sum).Sum
        
        return $results
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

switch ($Operation) {
    "digest" {
        $digester = [SourceDigester]::new($RootPath)
        $digester.DigestAllSources()
        $digester.Save($OutputPath)
        
        Write-Host "`n╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
        Write-Host "║                    ✅ DIGESTION COMPLETE ✅                                   ║" -ForegroundColor Green
        Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green
    }
    
    "search" {
        if (-not $Query) {
            Write-Error "Query parameter required for search operation"
            exit 1
        }
        
        if (-not (Test-Path $OutputPath)) {
            Write-Error "Knowledge base not found. Run digest first."
            exit 1
        }
        
        Write-Host "`n🔍 Searching knowledge base for: $Query" -ForegroundColor Cyan
        
        $json = Get-Content $OutputPath -Raw | ConvertFrom-Json -AsHashtable
        $digester = [SourceDigester]::new($RootPath)
        $digester.KnowledgeBase = $json
        
        $results = $digester.Search($Query)
        
        Write-Host "`nRESULTS (Score: $($results.Score)):" -ForegroundColor Yellow
        Write-Host "─────────────────────────────────────────" -ForegroundColor DarkGray
        
        Write-Host "`nFiles:" -ForegroundColor Cyan
        foreach ($file in $results.Files | Select-Object -First 5) {
            Write-Host "  [$($file.Score)] $($file.Name)" -ForegroundColor Gray
            if ($file.Synopsis) {
                Write-Host "      $($file.Synopsis)" -ForegroundColor DarkGray
            }
        }
        
        Write-Host "`nFunctions:" -ForegroundColor Cyan
        foreach ($func in $results.Functions | Select-Object -First 5) {
            Write-Host "  [$($func.Score)] $($func.Name)" -ForegroundColor Gray
            if ($func.Synopsis) {
                Write-Host "      $($func.Synopsis)" -ForegroundColor DarkGray
            }
        }
    }
    
    "stats" {
        if (-not (Test-Path $OutputPath)) {
            Write-Error "Knowledge base not found. Run digest first."
            exit 1
        }
        
        $json = Get-Content $OutputPath -Raw | ConvertFrom-Json -AsHashtable
        
        Write-Host "`n📊 KNOWLEDGE BASE STATISTICS" -ForegroundColor Cyan
        Write-Host "─────────────────────────────────────────" -ForegroundColor DarkGray
        Write-Host "  Files:      $($json.Metadata.FileCount)" -ForegroundColor Gray
        Write-Host "  Lines:      $($json.Metadata.TotalLines)" -ForegroundColor Gray
        Write-Host "  Functions:  $($json.Functions.Count)" -ForegroundColor Gray
        Write-Host "  Classes:    $($json.Classes.Count)" -ForegroundColor Gray
        Write-Host "  Keywords:   $($json.Keywords.Count)" -ForegroundColor Gray
        Write-Host "  Topics:     $($json.Topics.Count)" -ForegroundColor Gray
        Write-Host "  Digest Date: $($json.Metadata.DigestDate)" -ForegroundColor Gray
        
        $sizeMB = [Math]::Round((Get-Item $OutputPath).Length / 1MB, 2)
        Write-Host "  Size:       $sizeMB MB" -ForegroundColor Gray
    }
    
    "export" {
        if (-not (Test-Path $OutputPath)) {
            Write-Error "Knowledge base not found. Run digest first."
            exit 1
        }
        
        $json = Get-Content $OutputPath -Raw | ConvertFrom-Json -AsHashtable
        
        # Export human-readable summary
        $summaryPath = $OutputPath -replace '\.json$', '_summary.txt'
        
        $summary = @"
RawrXD IDE Knowledge Base Summary
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
═══════════════════════════════════════════════════════════════

FILES: $($json.Metadata.FileCount)
LINES: $($json.Metadata.TotalLines)
FUNCTIONS: $($json.Functions.Count)
CLASSES: $($json.Classes.Count)
KEYWORDS: $($json.Keywords.Count)

TOPICS:
$($json.Topics.Keys | ForEach-Object { "  - $_" } | Out-String)

TOP FUNCTIONS:
$($json.Functions.Keys | Select-Object -First 20 | ForEach-Object { "  - $_" } | Out-String)

TOP CLASSES:
$($json.Classes.Keys | Select-Object -First 20 | ForEach-Object { "  - $_" } | Out-String)
"@
        
        $summary | Set-Content $summaryPath
        Write-Host "`n✅ Summary exported to: $summaryPath" -ForegroundColor Green
    }
}

Write-Host ""
