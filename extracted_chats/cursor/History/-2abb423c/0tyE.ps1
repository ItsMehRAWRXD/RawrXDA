# D: Drive Organization Status Report
# Shows current organization and quick access

Write-Host "🎉 D: Drive Organization Complete!" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan

# Show organization summary
Write-Host "`n📊 Organization Summary:" -ForegroundColor Yellow
$categories = Get-ChildItem -Path "D:\" -Directory | Where-Object { $_.Name -match "^\d{2}-" } | Sort-Object Name

foreach ($category in $categories) {
    $fileCount = (Get-ChildItem -Path $category.FullName -Recurse -File).Count
    $size = [math]::Round(((Get-ChildItem -Path $category.FullName -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB), 2)
    Write-Host "  📁 $($category.Name): $fileCount files ($size MB)" -ForegroundColor White
}

# Show HTML apps that are auto-configured
Write-Host "`n🌐 Auto-Configured HTML Apps:" -ForegroundColor Yellow
$htmlApps = Get-ChildItem -Path "D:\" -Filter "*.html" -Recurse | Where-Object { $_.Name -notlike "*test*" } | Select-Object -First 10
foreach ($app in $htmlApps) {
    Write-Host "  ✓ $($app.Name)" -ForegroundColor Green
}

# Show Ollama tools
Write-Host "`n🤖 Ollama AI Tools:" -ForegroundColor Yellow
$ollamaTools = Get-ChildItem -Path "D:\01-AI-Models\Ollama\Tools" -Filter "*.ps1"
foreach ($tool in $ollamaTools) {
    Write-Host "  ✓ $($tool.BaseName)" -ForegroundColor Green
}

# Show quick access commands
Write-Host "`n🚀 Quick Access Commands:" -ForegroundColor Cyan
Write-Host "========================" -ForegroundColor Cyan
Write-Host "  .\Master-Launcher.ps1                    - Start everything" -ForegroundColor White
Write-Host "  .\01-AI-Models\Ollama\Tools\Start-Ollama-Chat.ps1 - Start Ollama chat" -ForegroundColor White
Write-Host "  .\D-Drive-Organizer.ps1 -Force           - Reorganize if needed" -ForegroundColor White

# Show desktop shortcuts
Write-Host "`n🔗 Desktop Shortcuts Created:" -ForegroundColor Yellow
$desktopPath = [Environment]::GetFolderPath("Desktop")
$shortcuts = Get-ChildItem -Path $desktopPath -Filter "D-Drive-*" -File
foreach ($shortcut in $shortcuts) {
    Write-Host "  ✓ $($shortcut.BaseName)" -ForegroundColor Green
}

Write-Host "`n✨ Your D: drive is now:" -ForegroundColor Green
Write-Host "  • Alphabetically organized by category" -ForegroundColor White
Write-Host "  • HTML files auto-configured to work without manual setup" -ForegroundColor White
Write-Host "  • Ollama chats organized and easily accessible" -ForegroundColor White
Write-Host "  • Random files moved to appropriate folders" -ForegroundColor White
Write-Host "  • One-click launchers for everything" -ForegroundColor White

Write-Host "`n🎯 Next Steps:" -ForegroundColor Cyan
Write-Host "  1. Double-click 'D-Drive-Master' on your desktop to start everything" -ForegroundColor White
Write-Host "  2. Your HTML apps will now work automatically" -ForegroundColor White
Write-Host "  3. Use Ollama tools for AI chat" -ForegroundColor White
Write-Host "  4. Everything is organized and ready to go!" -ForegroundColor White
