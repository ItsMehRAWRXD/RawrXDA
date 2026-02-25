# Find Eon ASM Conversation - Search for lost assembly compiler work
# Run this script to find your "eon asm" conversation with GitHub Copilot

param(
    [string]$OutputPath = "D:\Eon-ASM-Search-Results"
)

Write-Host "🔍 Searching for Eon ASM Conversation" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Cyan

# Create output directory
if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Write-Host "Created output directory: $OutputPath" -ForegroundColor Green
}

# Search paths for VS 2022, Copilot, and Cursor
$searchPaths = @(
    "$env:LOCALAPPDATA\Microsoft\VisualStudio\17.0",
    "$env:APPDATA\Microsoft\VisualStudio\17.0", 
    "$env:LOCALAPPDATA\Microsoft\VisualStudio\2022",
    "$env:APPDATA\Microsoft\VisualStudio\2022",
    "$env:USERPROFILE\.vscode\extensions\github.copilot*",
    "$env:APPDATA\Code\User\globalStorage\github.copilot*",
    "$env:LOCALAPPDATA\Programs\Microsoft VS Code\User\globalStorage\github.copilot*",
    "$env:USERPROFILE\Documents\Visual Studio 2022",
    "$env:USERPROFILE\Documents\Visual Studio 2019",
    "$env:USERPROFILE\Documents",
    "$env:USERPROFILE\Desktop",
    "$env:USERPROFILE\Downloads",
    "D:\"
)

$searchTerms = @("eon asm", "eon assembly", "eon compiler", "assembly compiler", "eon", "asm")

Write-Host "`n🎯 Searching for 'eon asm' content..." -ForegroundColor Yellow

$foundFiles = @()
$totalFiles = 0

foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        Write-Host "`n📁 Checking: $path" -ForegroundColor White
        
        try {
            # Search for various file types that might contain conversations
            $filePatterns = @("*.json", "*.log", "*.txt", "*.db", "*.sqlite", "*.cache", "*.xml", "*.md")
            
            foreach ($pattern in $filePatterns) {
                $files = Get-ChildItem -Path $path -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Where-Object {
                    $_.Length -gt 100 -and $_.Length -lt 50MB
                }
                
                foreach ($file in $files) {
                    $totalFiles++
                    if ($totalFiles % 100 -eq 0) {
                        Write-Host "  📊 Processed $totalFiles files..." -ForegroundColor Gray
                    }
                    
                    try {
                        $content = Get-Content $file.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
                        
                        if ($content) {
                            $hasMatch = $false
                            $matchedTerms = @()
                            
                            foreach ($term in $searchTerms) {
                                if ($content -match $term) {
                                    $hasMatch = $true
                                    $matchedTerms += $term
                                }
                            }
                            
                            if ($hasMatch) {
                                $foundFiles += @{
                                    Path = $file.FullName
                                    Name = $file.Name
                                    Size = $file.Length
                                    LastWrite = $file.LastWriteTime
                                    MatchedTerms = $matchedTerms
                                    Directory = $file.DirectoryName
                                }
                                Write-Host "  ✅ Found: $($file.Name) (matched: $($matchedTerms -join ', '))" -ForegroundColor Green
                            }
                        }
                    } catch {
                        # Skip files that can't be read
                    }
                }
            }
        } catch {
            # Skip directories that can't be accessed
        }
    }
}

Write-Host "`n📈 Search Summary:" -ForegroundColor Cyan
Write-Host "  Total files processed: $totalFiles" -ForegroundColor White
Write-Host "  Files with matches: $($foundFiles.Count)" -ForegroundColor White

if ($foundFiles.Count -gt 0) {
    Write-Host "`n🎉 Found $($foundFiles.Count) files with 'eon asm' content!" -ForegroundColor Green
    
    # Save detailed results
    $resultsFile = Join-Path $OutputPath "Eon-ASM-Search-Results.json"
    $foundFiles | ConvertTo-Json -Depth 5 | Set-Content -Path $resultsFile -Encoding UTF8
    
    # Create a summary report
    $reportFile = Join-Path $OutputPath "Eon-ASM-Summary.md"
    $report = @"
# Eon ASM Search Results

## Search Summary
- **Total files processed:** $totalFiles
- **Files with matches:** $($foundFiles.Count)
- **Search completed:** $(Get-Date)

## Found Files

"@
    
    foreach ($file in $foundFiles) {
        $report += @"

### $($file.Name)
- **Path:** $($file.Path)
- **Size:** $($file.Size) bytes
- **Last Modified:** $($file.LastWrite)
- **Matched Terms:** $($file.MatchedTerms -join ', ')
- **Directory:** $($file.Directory)

---
"@
    }
    
    $report | Set-Content -Path $reportFile -Encoding UTF8
    
    Write-Host "`n📄 Results saved to:" -ForegroundColor Cyan
    Write-Host "  📊 Detailed JSON: $resultsFile" -ForegroundColor White
    Write-Host "  📋 Summary Report: $reportFile" -ForegroundColor White
    
    # Show most promising files
    Write-Host "`n🔍 Most Promising Files:" -ForegroundColor Yellow
    $promisingFiles = $foundFiles | Where-Object { 
        $_.MatchedTerms -contains "eon asm" -or 
        $_.MatchedTerms -contains "eon assembly" -or 
        $_.MatchedTerms -contains "eon compiler" 
    } | Sort-Object LastWrite -Descending | Select-Object -First 5
    
    foreach ($file in $promisingFiles) {
        Write-Host "  🎯 $($file.Name)" -ForegroundColor Green
        Write-Host "     Path: $($file.Path)" -ForegroundColor Gray
        Write-Host "     Matched: $($file.MatchedTerms -join ', ')" -ForegroundColor Gray
        Write-Host "     Modified: $($file.LastWrite)" -ForegroundColor Gray
        Write-Host ""
    }
    
} else {
    Write-Host "`n❌ No 'eon asm' conversations found" -ForegroundColor Yellow
    Write-Host "`n💡 Tips:" -ForegroundColor Yellow
    Write-Host "  - Check if the conversation was in a different VS version" -ForegroundColor White
    Write-Host "  - Look in your project folders for any saved conversations" -ForegroundColor White
    Write-Host "  - Check if it was saved in a different format" -ForegroundColor White
    Write-Host "  - The conversation might be in VS Code instead of VS 2022" -ForegroundColor White
}

Write-Host "`n✅ Search complete!" -ForegroundColor Green
Write-Host "Results saved to: $OutputPath" -ForegroundColor White
