# Automated Chat Log Extractor
# Extracts ALL chat logs from browser storage and saves them locally as text files

param(
    [string]$OutputPath = "D:\01-AI-Models\Chat-History\Downloaded\Auto-Extracted"
)

Write-Host "🤖 Automated Chat Log Extractor" -ForegroundColor Green
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host ""

if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

# Service patterns
$serviceConfigs = @{
    "ChatGPT" = @{
        Patterns = @("openai.com", "chatgpt")
        Domains = @("chat.openai.com", "openai.com")
    }
    "Kimi" = @{
        Patterns = @("moonshot.cn", "kimi")
        Domains = @("kimi.moonshot.cn")
    }
    "DeepSeek" = @{
        Patterns = @("deepseek.com")
        Domains = @("chat.deepseek.com")
    }
    "Claude" = @{
        Patterns = @("claude.ai", "anthropic")
        Domains = @("claude.ai")
    }
    "Gemini" = @{
        Patterns = @("gemini", "bard")
        Domains = @("gemini.google.com")
    }
}

$browserPaths = @(
    @{ Name = "Chrome"; Path = "$env:LOCALAPPDATA\Google\Chrome\User Data\Default" }
    @{ Name = "Edge"; Path = "$env:LOCALAPPDATA\Microsoft\Edge\User Data\Default" }
)

$allExtractedData = @{}

foreach ($serviceName in $serviceConfigs.Keys) {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Yellow
    Write-Host "📱 Extracting: $serviceName" -ForegroundColor Yellow
    Write-Host ("=" * 60) -ForegroundColor Yellow
    
    $serviceData = @()
    $config = $serviceConfigs[$serviceName]
    $patterns = $config.Patterns
    
    foreach ($browser in $browserPaths) {
        if (!(Test-Path $browser.Path)) {
            continue
        }
        
        Write-Host "  🔍 Scanning $($browser.Name)..." -ForegroundColor Cyan
        
        # Extract from IndexedDB
        $indexedDBPath = Join-Path $browser.Path "IndexedDB"
        if (Test-Path $indexedDBPath) {
            $dbDirs = Get-ChildItem -Path $indexedDBPath -Directory -ErrorAction SilentlyContinue
            
            foreach ($dbDir in $dbDirs) {
                $dbName = $dbDir.Name
                $matches = $false
                
                foreach ($pattern in $patterns) {
                    if ($dbName -like "*$pattern*") {
                        $matches = $true
                        break
                    }
                }
                
                if ($matches) {
                    Write-Host "    ✓ Found: $dbName" -ForegroundColor Green
                    
                    # Read all files in the database directory
                    $allFiles = Get-ChildItem -Path $dbDir.FullName -File -ErrorAction SilentlyContinue
                    $extractedText = @()
                    $fileCount = 0
                    
                    foreach ($file in $allFiles) {
                        try {
                            # Try to read as text first
                            $content = Get-Content $file.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
                            if ($content) {
                                # Clean up null bytes and look for JSON/data
                                $cleanContent = $content -replace "`0", "" -replace "[^\x20-\x7E\n\r\t]", ""
                                
                                # Look for conversation-related content
                                if ($cleanContent -match '(conversation|message|chat|prompt|response|user|assistant)' -and $cleanContent.Length -gt 100) {
                                    $extractedText += "=== File: $($file.Name) ==="
                                    $extractedText += $cleanContent
                                    $extractedText += ""
                                    $fileCount++
                                }
                            }
                        } catch {
                            # Try binary read
                            try {
                                $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
                                $text = [System.Text.Encoding]::UTF8.GetString($bytes) -replace "`0", ""
                                $cleanText = $text -replace "[^\x20-\x7E\n\r\t]", ""
                                
                                if ($cleanText -match '(conversation|message|chat)' -and $cleanText.Length -gt 100) {
                                    $extractedText += "=== File: $($file.Name) ==="
                                    $extractedText += $cleanText
                                    $extractedText += ""
                                    $fileCount++
                                }
                            } catch {
                                # Skip binary files we can't read
                            }
                        }
                    }
                    
                    if ($extractedText.Count -gt 0) {
                        $serviceData += @{
                            Browser = $browser.Name
                            Source = "IndexedDB"
                            Database = $dbName
                            Files = $fileCount
                            Content = $extractedText -join "`n"
                        }
                        Write-Host "      ✓ Extracted data from $fileCount file(s)" -ForegroundColor Green
                    }
                }
            }
        }
        
        # Extract from Local Storage LevelDB
        $localStoragePath = Join-Path $browser.Path "Local Storage\leveldb"
        if (Test-Path $localStoragePath) {
            Write-Host "    📦 Checking Local Storage..." -ForegroundColor Cyan
            
            $lsFiles = Get-ChildItem -Path $localStoragePath -Filter "*.ldb" -ErrorAction SilentlyContinue
            $lsData = @()
            
            foreach ($file in $lsFiles) {
                try {
                    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
                    $text = [System.Text.Encoding]::UTF8.GetString($bytes) -replace "`0", ""
                    
                    # Look for service-specific patterns
                    foreach ($pattern in $patterns) {
                        if ($text -like "*$pattern*") {
                            # Try to extract JSON-like structures
                            $jsonMatches = [regex]::Matches($text, '\{[^{}]*"(conversation|message|chat|prompt|response)"[^{}]*\}', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                            
                            if ($jsonMatches.Count -gt 0 -or ($text.Length -gt 500 -and $text -match '(conversation|message|chat)')) {
                                $lsData += "=== File: $($file.Name) ==="
                                $lsData += $text
                                $lsData += ""
                                break
                            }
                        }
                    }
                } catch {
                    # Skip files we can't read
                }
            }
            
            if ($lsData.Count -gt 0) {
                $serviceData += @{
                    Browser = $browser.Name
                    Source = "LocalStorage"
                    Database = "leveldb"
                    Files = $lsData.Count
                    Content = $lsData -join "`n"
                }
                Write-Host "      ✓ Found data in Local Storage" -ForegroundColor Green
            }
        }
    }
    
    # Save all extracted data for this service
    if ($serviceData.Count -gt 0) {
        $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $serviceOutputDir = Join-Path $OutputPath $serviceName
        if (!(Test-Path $serviceOutputDir)) {
            New-Item -ItemType Directory -Path $serviceOutputDir -Force | Out-Null
        }
        
        $allText = @()
        $allText += "=" * 80
        $allText += "$serviceName Chat History - Automated Extraction"
        $allText += "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
        $allText += "Total Sources Found: $($serviceData.Count)"
        $allText += "=" * 80
        $allText += ""
        
        foreach ($data in $serviceData) {
            $allText += "-" * 80
            $allText += "Source: $($data.Source)"
            $allText += "Browser: $($data.Browser)"
            $allText += "Database: $($data.Database)"
            $allText += "Files Processed: $($data.Files)"
            $allText += "-" * 80
            $allText += ""
            $allText += $data.Content
            $allText += ""
            $allText += ""
        }
        
        $outputFile = Join-Path $serviceOutputDir "${serviceName}_ALL_CHAT_LOGS_${timestamp}.txt"
        $allText -join "`n" | Out-File -FilePath $outputFile -Encoding UTF8
        
        Write-Host "  ✅ Saved to: $outputFile" -ForegroundColor Green
        Write-Host "  📊 Total size: $([math]::Round((Get-Item $outputFile).Length / 1KB, 2)) KB" -ForegroundColor Cyan
        
        $allExtractedData[$serviceName] = @{
            File = $outputFile
            Sources = $serviceData.Count
            Size = (Get-Item $outputFile).Length
        }
    } else {
        Write-Host "  ⚠️ No data found for $serviceName" -ForegroundColor Yellow
    }
}

# Summary
Write-Host ""
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host "📊 Extraction Summary" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host ""

$totalSize = 0
foreach ($service in $allExtractedData.Keys) {
    $data = $allExtractedData[$service]
    $sizeKB = [math]::Round($data.Size / 1KB, 2)
    $totalSize += $data.Size
    
    Write-Host "  ✅ $service" -ForegroundColor Green
    Write-Host "     Sources: $($data.Sources) | Size: $sizeKB KB" -ForegroundColor Gray
    Write-Host "     File: $($data.File)" -ForegroundColor Gray
    Write-Host ""
}

$totalSizeMB = [math]::Round($totalSize / 1MB, 2)
Write-Host "📁 Total Extracted: $totalSizeMB MB" -ForegroundColor Cyan
Write-Host "📂 All files saved to: $OutputPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "💡 To view files:" -ForegroundColor Yellow
Write-Host "   explorer `"$OutputPath`"" -ForegroundColor White

