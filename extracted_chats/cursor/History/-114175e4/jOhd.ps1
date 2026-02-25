# GitHub Copilot Chat Recovery System - Simple Version
# Searches VS 2022 directories for lost Copilot conversations

param(
    [string]$OutputPath = "D:\01-AI-Models\Chat-History\Downloaded\GitHub-Copilot",
    [string[]]$SearchTerms = @("assembly", "compiler", "eon", "assembly compiler"),
    [switch]$SearchAll = $false,
    [switch]$DryRun = $false
)

Write-Host "GitHub Copilot Chat Recovery System" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Cyan

# Create output directories
if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Write-Host "Created output directory: $OutputPath" -ForegroundColor Green
}

# VS 2022 and Copilot storage locations
$searchPaths = @(
    "$env:LOCALAPPDATA\Microsoft\VisualStudio",
    "$env:APPDATA\Microsoft\VisualStudio",
    "$env:LOCALAPPDATA\GitHub\Copilot",
    "$env:APPDATA\GitHub\Copilot",
    "$env:USERPROFILE\.vscode\extensions\github.copilot*",
    "$env:APPDATA\Code\User\globalStorage\github.copilot*"
)

function Search-CopilotFiles {
    Write-Host "`nSearching for Copilot conversation files..." -ForegroundColor Yellow
    
    $foundFiles = @()
    
    foreach ($path in $searchPaths) {
        if (Test-Path $path) {
            Write-Host "  Checking: $path" -ForegroundColor White
            
            # Look for various file types that might contain conversations
            $filePatterns = @("*.json", "*.log", "*.txt", "*.db", "*.sqlite", "*.cache")
            
            foreach ($pattern in $filePatterns) {
                try {
                    $files = Get-ChildItem -Path $path -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Where-Object {
                        $_.Length -gt 100 -and $_.Length -lt 50MB  # Reasonable size range
                    }
                    
                    foreach ($file in $files) {
                        $foundFiles += @{
                            Path = $file.FullName
                            Name = $file.Name
                            Size = $file.Length
                            LastWrite = $file.LastWriteTime
                        }
                    }
                } catch {
                    # Ignore permission errors
                }
            }
        }
    }
    
    Write-Host "  Found $($foundFiles.Count) potential files" -ForegroundColor Cyan
    return $foundFiles
}

function Search-In-File {
    param([string]$FilePath, [string[]]$SearchTerms)
    
    try {
        $content = Get-Content $FilePath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        
        if ($content) {
            $matches = @()
            
            foreach ($term in $SearchTerms) {
                if ($content -match $term) {
                    $matches += $term
                }
            }
            
            if ($matches.Count -gt 0) {
                return @{
                    FilePath = $FilePath
                    Matches = $matches
                    Content = $content
                    MatchCount = $matches.Count
                }
            }
        }
    } catch {
        # File might be binary or locked
    }
    
    return $null
}

function Save-Conversation {
    param([object]$FileMatch, [string]$OutputDir)
    
    $timestamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
    $baseName = "copilot_$(Split-Path $FileMatch.FilePath -Leaf)_$timestamp"
    
    # Save as JSON
    $jsonFile = Join-Path $OutputDir "$baseName.json"
    $conversation = @{
        id = [System.Guid]::NewGuid().ToString()
        source = "GitHub Copilot (VS 2022)"
        timestamp = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
        filePath = $FileMatch.FilePath
        searchMatches = $FileMatch.Matches
        content = $FileMatch.Content
        metadata = @{
            fileName = Split-Path $FileMatch.FilePath -Leaf
            fileSize = (Get-Item $FileMatch.FilePath).Length
            lastModified = (Get-Item $FileMatch.FilePath).LastWriteTime
            matchCount = $FileMatch.MatchCount
        }
    }
    
    $conversation | ConvertTo-Json -Depth 10 | Set-Content -Path $jsonFile -Encoding UTF8
    
    # Save as Markdown
    $markdownFile = Join-Path $OutputDir "$baseName.md"
    $markdown = "# GitHub Copilot Conversation`n`n"
    $markdown += "**File:** $($conversation.metadata.fileName)`n"
    $markdown += "**Search Matches:** $($conversation.searchMatches -join ', ')`n"
    $markdown += "**File Size:** $($conversation.metadata.fileSize) bytes`n"
    $markdown += "**Last Modified:** $($conversation.metadata.lastModified)`n`n"
    $markdown += "## Content`n`n"
    $markdown += "```text`n$($conversation.content)`n```"
    
    Set-Content -Path $markdownFile -Value $markdown -Encoding UTF8
    
    Write-Host "  [OK] Saved: $baseName" -ForegroundColor Green
    Write-Host "    Matches: $($conversation.searchMatches -join ', ')" -ForegroundColor Cyan
}

# Main execution
try {
    Write-Host "`nStarting Copilot chat recovery..." -ForegroundColor Cyan
    
    if ($DryRun) {
        Write-Host "DRY RUN MODE - No files will be saved" -ForegroundColor Yellow
    }
    
    # Search for files
    $foundFiles = Search-CopilotFiles
    $assemblyMatches = @()
    
    # Search for assembly compiler conversation specifically
    Write-Host "`nSearching specifically for assembly compiler conversation..." -ForegroundColor Yellow
    
    foreach ($file in $foundFiles) {
        $match = Search-In-File -FilePath $file.Path -SearchTerms $SearchTerms
        
        if ($match) {
            $assemblyMatches += $match
            Write-Host "  Found assembly-related content in: $($file.Name)" -ForegroundColor Green
        }
    }
    
    if ($assemblyMatches.Count -gt 0) {
        Write-Host "`nFound $($assemblyMatches.Count) files with assembly compiler content!" -ForegroundColor Green
        
        foreach ($match in $assemblyMatches) {
            if (!$DryRun) {
                Save-Conversation -FileMatch $match -OutputDir $OutputPath
            } else {
                Write-Host "  [DRY RUN] Would process: $($match.FilePath)" -ForegroundColor Cyan
            }
        }
    } else {
        Write-Host "`nNo assembly compiler conversations found" -ForegroundColor Yellow
    }
    
    # Search for all Copilot conversations if requested
    if ($SearchAll) {
        Write-Host "`nSearching for all Copilot conversations..." -ForegroundColor Yellow
        
        $allMatches = @()
        
        foreach ($file in $foundFiles) {
            $match = Search-In-File -FilePath $file.Path -SearchTerms @("copilot", "github", "ai", "assistant")
            
            if ($match) {
                $allMatches += $match
            }
        }
        
        Write-Host "  Found $($allMatches.Count) total Copilot-related files" -ForegroundColor Cyan
        
        foreach ($match in $allMatches) {
            if (!$DryRun) {
                Save-Conversation -FileMatch $match -OutputDir $OutputPath
            } else {
                Write-Host "  [DRY RUN] Would process: $($match.FilePath)" -ForegroundColor Cyan
            }
        }
    }
    
    Write-Host "`nCopilot chat recovery complete!" -ForegroundColor Green
    Write-Host "Saved to: $OutputPath" -ForegroundColor White
    
} catch {
    Write-Host "Error during recovery: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
