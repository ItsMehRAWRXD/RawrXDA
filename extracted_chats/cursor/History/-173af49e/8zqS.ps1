# Complete Automated Chat Archiver
# Extracts ALL chats + ALL messages from ALL AI platforms

param(
    [string]$OutputPath = "D:\01-AI-Models\Chat-History\Downloaded\Complete-Archive"
)

Write-Host "🤖 Complete Automated Chat Archiver" -ForegroundColor Green
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host ""

if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

# All AI chat services
$services = @(
    @{ Name = "ChatGPT"; Url = "https://chat.openai.com" }
    @{ Name = "Kimi"; Url = "https://kimi.moonshot.cn" }
    @{ Name = "DeepSeek"; Url = "https://chat.deepseek.com" }
    @{ Name = "Claude"; Url = "https://claude.ai" }
    @{ Name = "Gemini"; Url = "https://gemini.google.com" }
    @{ Name = "Perplexity"; Url = "https://www.perplexity.ai" }
    @{ Name = "Poe"; Url = "https://poe.com" }
    @{ Name = "HuggingChat"; Url = "https://huggingface.co/chat" }
    @{ Name = "Mistral"; Url = "https://chat.mistral.ai" }
    @{ Name = "Grok"; Url = "https://x.com/i/grok" }
)

# Read extractor scripts
$universalExtractor = "D:\01-AI-Models\Chat-History\Downloaded\Real-Chat-Logs\UNIVERSAL_CHAT_EXTRACTOR.js"
$fullMessageExtractor = "D:\01-AI-Models\Chat-History\Downloaded\Real-Chat-Logs\FULL_MESSAGE_EXTRACTOR.js"

if (!(Test-Path $universalExtractor) -or !(Test-Path $fullMessageExtractor)) {
    Write-Host "❌ Extractor scripts not found!" -ForegroundColor Red
    Write-Host "   Please ensure both extractor scripts exist." -ForegroundColor Yellow
    exit 1
}

Write-Host "📋 This archiver will:" -ForegroundColor Yellow
Write-Host "  1. Open each AI service website" -ForegroundColor White
Write-Host "  2. Provide extraction scripts for you to run" -ForegroundColor White
Write-Host "  3. Extract ALL chat lists + ALL messages" -ForegroundColor White
Write-Host "  4. Save everything locally" -ForegroundColor White
Write-Host ""
Write-Host "⚠️  IMPORTANT: You need to manually run the scripts in browser console" -ForegroundColor Red
Write-Host "   (Browser security prevents automatic execution)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press Enter to continue, or Ctrl+C to cancel..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Create instruction file
$instructions = @"
# Complete Automated Chat Archiver - Instructions

## Quick Start

1. For each service that opens:
   - Wait for page to load
   - Press F12 → Console tab
   - Copy the appropriate extractor script below
   - Paste and press Enter
   - Files will download automatically

## Extractors Available

### 1. UNIVERSAL_CHAT_EXTRACTOR.js
   - Extracts chat list (titles, IDs, URLs)
   - Works on ALL services
   - Fast extraction

### 2. FULL_MESSAGE_EXTRACTOR.js
   - Extracts chat list + current chat messages
   - Extracts from browser storage
   - More comprehensive

## Services

$($services | ForEach-Object { "  • $($_.Name): $($_.Url)" } | Out-String)

## File Locations

Extractors:
  • $universalExtractor
  • $fullMessageExtractor

Downloaded files will be in your Downloads folder.

## Tips

- Start with UNIVERSAL_CHAT_EXTRACTOR for quick chat list
- Use FULL_MESSAGE_EXTRACTOR for complete message extraction
- Make sure you're logged into each service
- Allow downloads in your browser
"@

$instructionsFile = Join-Path $OutputPath "ARCHIVER_INSTRUCTIONS.txt"
Set-Content -Path $instructionsFile -Value $instructions -Encoding UTF8

# Open extractor scripts in Notepad
Write-Host ""
Write-Host "📝 Opening extractor scripts in Notepad..." -ForegroundColor Cyan
Start-Process "notepad.exe" -ArgumentList "`"$universalExtractor`""
Start-Sleep -Seconds 1
Start-Process "notepad.exe" -ArgumentList "`"$fullMessageExtractor`""

# Open all service websites
Write-Host ""
Write-Host "🌐 Opening all AI service websites..." -ForegroundColor Cyan
Write-Host ""

foreach ($service in $services) {
    Write-Host "  📱 Opening $($service.Name)..." -ForegroundColor Yellow
    Start-Process $service.Url
    Start-Sleep -Seconds 2
}

Write-Host ""
Write-Host "✅ All websites opened!" -ForegroundColor Green
Write-Host ""
Write-Host "📋 Next Steps:" -ForegroundColor Yellow
Write-Host ""
Write-Host "For EACH service:" -ForegroundColor Cyan
Write-Host "  1. Wait for page to load and log in if needed" -ForegroundColor White
Write-Host "  2. Press F12 → Console tab" -ForegroundColor White
Write-Host "  3. Copy one of the extractor scripts from Notepad" -ForegroundColor White
Write-Host "  4. Paste into console and press Enter" -ForegroundColor White
Write-Host "  5. Files will download automatically!" -ForegroundColor White
Write-Host ""
Write-Host "💡 Recommended:" -ForegroundColor Cyan
Write-Host "  • Use UNIVERSAL_CHAT_EXTRACTOR.js for quick chat list extraction" -ForegroundColor White
Write-Host "  • Use FULL_MESSAGE_EXTRACTOR.js for complete message extraction" -ForegroundColor White
Write-Host ""
Write-Host "📁 Files:" -ForegroundColor Cyan
Write-Host "  • Instructions: $instructionsFile" -ForegroundColor Gray
Write-Host "  • Universal Extractor: $universalExtractor" -ForegroundColor Gray
Write-Host "  • Full Message Extractor: $fullMessageExtractor" -ForegroundColor Gray
Write-Host ""
Write-Host "📥 Downloaded files will be in your Downloads folder" -ForegroundColor Cyan
Write-Host ""

# Create a summary script that can be run later
$summaryScript = @"
# Summary of Extracted Files
# Run this after extraction to see what was downloaded

`$downloadsPath = [Environment]::GetFolderPath("UserProfile") + "\Downloads"
`$files = Get-ChildItem -Path `$downloadsPath -Filter "*chat_history*.json","*FULL_MESSAGES*.json" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending

Write-Host "📊 Extracted Chat History Files:" -ForegroundColor Cyan
Write-Host ""
`$files | ForEach-Object {
    `$sizeKB = [math]::Round(`$_.Length / 1KB, 2)
    Write-Host "  ✅ `$(`$_.Name) - `$sizeKB KB" -ForegroundColor Green
}
"@

$summaryScriptPath = Join-Path $OutputPath "Check-Extracted-Files.ps1"
Set-Content -Path $summaryScriptPath -Value $summaryScript -Encoding UTF8

Write-Host "💡 Tip: Run this to check extracted files:" -ForegroundColor Yellow
Write-Host "   .\Check-Extracted-Files.ps1" -ForegroundColor White
Write-Host ""

