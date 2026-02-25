# Download All AI Chat History
# One-click downloader for all your AI conversations

Write-Host "🤖 Download All AI Chat History" -ForegroundColor Green
Write-Host "===============================" -ForegroundColor Cyan

# Create main launcher
$launcherScript = @"
# AI Chat History Master Downloader
# Downloads from all major AI services

Write-Host "🚀 Starting AI Chat History Download..." -ForegroundColor Green

# Download from all services
& "D:\01-AI-Models\Chat-History-Downloader.ps1" -AllServices

Write-Host "`n📋 Manual Extraction Steps:" -ForegroundColor Yellow
Write-Host "=========================" -ForegroundColor Yellow

Write-Host "`n1️⃣ ChatGPT (OpenAI):" -ForegroundColor Cyan
Write-Host "   • Go to: https://chat.openai.com" -ForegroundColor White
Write-Host "   • Log in to your account" -ForegroundColor White
Write-Host "   • Press F12 to open browser console" -ForegroundColor White
Write-Host "   • Copy script from: D:\01-AI-Models\Chat-History\Downloaded\OpenAI\ChatGPT\chatgpt_extractor.js" -ForegroundColor White
Write-Host "   • Paste and press Enter to download" -ForegroundColor White

Write-Host "`n2️⃣ Gemini (Google):" -ForegroundColor Cyan
Write-Host "   • Go to: https://gemini.google.com" -ForegroundColor White
Write-Host "   • Log in to your Google account" -ForegroundColor White
Write-Host "   • Press F12 to open browser console" -ForegroundColor White
Write-Host "   • Copy script from: D:\01-AI-Models\Chat-History\Downloaded\Google\Gemini\gemini_extractor.js" -ForegroundColor White
Write-Host "   • Paste and press Enter to download" -ForegroundColor White

Write-Host "`n3️⃣ Amazon Q (AWS):" -ForegroundColor Cyan
Write-Host "   • Go to your Amazon Q workspace" -ForegroundColor White
Write-Host "   • Log in to your AWS account" -ForegroundColor White
Write-Host "   • Press F12 to open browser console" -ForegroundColor White
Write-Host "   • Copy script from: D:\01-AI-Models\Chat-History\Downloaded\AWS\AmazonQ\amazonq_extractor.js" -ForegroundColor White
Write-Host "   • Paste and press Enter to download" -ForegroundColor White

Write-Host "`n4️⃣ Kimi (Moonshot):" -ForegroundColor Cyan
Write-Host "   • Go to: https://kimi.moonshot.cn" -ForegroundColor White
Write-Host "   • Log in to your account" -ForegroundColor White
Write-Host "   • Press F12 to open browser console" -ForegroundColor White
Write-Host "   • Copy script from: D:\01-AI-Models\Chat-History\Downloaded\Moonshot\Kimi\kimi_extractor.js" -ForegroundColor White
Write-Host "   • Paste and press Enter to download" -ForegroundColor White

Write-Host "`n5️⃣ Claude (Anthropic):" -ForegroundColor Cyan
Write-Host "   • Go to: https://claude.ai" -ForegroundColor White
Write-Host "   • Log in to your account" -ForegroundColor White
Write-Host "   • Press F12 to open browser console" -ForegroundColor White
Write-Host "   • Copy script from: D:\01-AI-Models\Chat-History\Downloaded\Anthropic\Claude\claude_extractor.js" -ForegroundColor White
Write-Host "   • Paste and press Enter to download" -ForegroundColor White

Write-Host "`n📁 Download Locations:" -ForegroundColor Yellow
Write-Host "   • Raw files: D:\01-AI-Models\Chat-History\Downloaded\" -ForegroundColor White
Write-Host "   • Processed: D:\01-AI-Models\Chat-History\Processed-Conversations\" -ForegroundColor White
Write-Host "   • Auto extracted: D:\01-AI-Models\Chat-History\Auto-Extracted\" -ForegroundColor White

Write-Host "`n✨ Tips:" -ForegroundColor Green
Write-Host "   • Make sure you're logged in to each service" -ForegroundColor White
Write-Host "   • Some services may require you to scroll through conversations" -ForegroundColor White
Write-Host "   • Check browser console for any error messages" -ForegroundColor White
Write-Host "   • Files will be downloaded to your default download folder" -ForegroundColor White

Write-Host "`n🎉 Happy downloading!" -ForegroundColor Green
"@

Set-Content -Path "D:\01-AI-Models\Chat-History\Master-Downloader.ps1" -Value $launcherScript -Encoding UTF8

# Run the downloader
& "D:\01-AI-Models\Chat-History-Downloader.ps1" -AllServices

Write-Host "`n🎉 Chat History Downloader Setup Complete!" -ForegroundColor Green
Write-Host "`n📋 Next Steps:" -ForegroundColor Yellow
Write-Host "1. Run: .\01-AI-Models\Chat-History\Master-Downloader.ps1" -ForegroundColor White
Write-Host "2. Follow the step-by-step instructions for each service" -ForegroundColor White
Write-Host "3. Download your chat history from all AI services" -ForegroundColor White
Write-Host "4. All files will be organized in: D:\01-AI-Models\Chat-History\" -ForegroundColor White

Write-Host "`n🚀 Quick Access:" -ForegroundColor Cyan
Write-Host "  .\01-AI-Models\Chat-History\Master-Downloader.ps1 - Start downloading" -ForegroundColor White
Write-Host "  .\01-AI-Models\Chat-History-Downloader.ps1 -ServiceName - Download specific service" -ForegroundColor White
Write-Host "  .\01-AI-Models\Auto-Chat-Extractor.ps1 - Automated extraction (requires setup)" -ForegroundColor White
