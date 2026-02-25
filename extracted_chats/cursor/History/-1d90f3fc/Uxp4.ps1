# Find Eon ASM Conversation
# Search specifically for "eon asm" conversation

param(
    [string]$OutputPath = "D:\01-AI-Models\Chat-History\Downloaded\GitHub-Copilot"
)

Write-Host "Searching for Eon ASM Conversation" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan

# Create output directory
if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Write-Host "Created output directory: $OutputPath" -ForegroundColor Green
}

# More specific search paths for VS 2022 and Copilot
$searchPaths = @(
    "$env:LOCALAPPDATA\Microsoft\VisualStudio\17.0",
    "$env:APPDATA\Microsoft\VisualStudio\17.0",
    "$env:LOCALAPPDATA\Microsoft\VisualStudio\2022",
    "$env:APPDATA\Microsoft\VisualStudio\2022",
    "$env:USERPROFILE\.vscode\extensions\github.copilot*",
    "$env:APPDATA\Code\User\globalStorage\github.copilot*",
    "$env:LOCALAPPDATA\Programs\Microsoft VS Code\User\globalStorage\github.copilot*",
    "$env:USERPROFILE\Documents\Visual Studio 2022",
    "$env:USERPROFILE\Documents\Visual Studio 2019"
)

$searchTerms = @("eon asm", "eon assembly", "eon compiler", "assembly compiler", "eon", "asm")

Write-Host "`nSearching for 'eon asm' content..." -ForegroundColor Yellow

$foundFiles = @()

foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        Write-Host "Checking: $path" -ForegroundColor White
        
        try {
            # Search for various file types
            $filePatterns = @("*.json", "*.log", "*.txt", "*.db", "*.sqlite", "*.cache", "*.xml")
            
            foreach ($pattern in $filePatterns) {
                $files = Get-ChildItem -Path $path -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Where-Object {
                    $_.Length -gt 100 -and $_.Length -lt 50MB
                }
                
                foreach ($file in $files) {
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
                                }
                                Write-Host "  Found: $($file.Name) (matched: $($matchedTerms -join ', '))" -ForegroundColor Green
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

# Also search in common project directories
$projectPaths = @(
    "$env:USERPROFILE\Documents",
    "$env:USERPROFILE\Desktop",
    "$env:USERPROFILE\Downloads",
    "D:\"
)

Write-Host "`nSearching project directories..." -ForegroundColor Yellow

foreach ($path in $projectPaths) {
    if (Test-Path $path) {
        Write-Host "Checking projects: $path" -ForegroundColor White
        
        try {
            $files = Get-ChildItem -Path $path -Recurse -Filter "*.json" -ErrorAction SilentlyContinue | Where-Object {
                $_.Name -match "(copilot|github|ai|chat|conversation)" -and
                $_.Length -gt 100 -and $_.Length -lt 10MB
            }
            
            foreach ($file in $files) {
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
                            }
                            Write-Host "  Found: $($file.Name) (matched: $($matchedTerms -join ', '))" -ForegroundColor Green
                        }
                    }
                } catch {
                    # Skip files that can't be read
                }
            }
        } catch {
            # Skip directories that can't be accessed
        }
    }
}

if ($foundFiles.Count -gt 0) {
    Write-Host "`nFound $($foundFiles.Count) files with 'eon asm' content!" -ForegroundColor Green
    
    foreach ($file in $foundFiles) {
        $timestamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
        $baseName = "eon_asm_$($file.Name)_$timestamp"
        
        # Save file info
        $info = @{
            FilePath = $file.Path
            FileName = $file.Name
            FileSize = $file.Size
            LastWrite = $file.LastWriteTime
            MatchedTerms = $file.MatchedTerms
            FoundAt = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
            SearchTerms = $searchTerms
        }
        
        $jsonFile = Join-Path $OutputPath "$baseName.json"
        $info | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonFile -Encoding UTF8
        
        # Also try to extract the actual content if it's a reasonable size
        if ($file.Size -lt 5MB) {
            try {
                $content = Get-Content $file.Path -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
                if ($content) {
                    $contentFile = Join-Path $OutputPath "$baseName_content.txt"
                    Set-Content -Path $contentFile -Value $content -Encoding UTF8
                    Write-Host "  Saved content: $baseName_content.txt" -ForegroundColor Cyan
                }
            } catch {
                # Skip if content can't be extracted
            }
        }
        
        Write-Host "  Saved info: $baseName" -ForegroundColor Cyan
    }
} else {
    Write-Host "`nNo 'eon asm' conversations found" -ForegroundColor Yellow
    Write-Host "`nTips:" -ForegroundColor Yellow
    Write-Host "  - Check if the conversation was in a different VS version" -ForegroundColor White
    Write-Host "  - Look in your project folders for any saved conversations" -ForegroundColor White
    Write-Host "  - Check if it was saved in a different format" -ForegroundColor White
    Write-Host "  - The conversation might be in VS Code instead of VS 2022" -ForegroundColor White
}

Write-Host "`nSearch complete!" -ForegroundColor Green
Write-Host "Results saved to: $OutputPath" -ForegroundColor White
