# 🚀 BOTBUILDER - OPEN IN VISUAL STUDIO (SIMPLEST WAY)
# This script opens BotBuilder in Visual Studio 2022
# You then press F5 to build and run

$VS_PATH = "D:\Microsoft Visual Studio 2022\Common7\IDE\devenv.exe"
$SLN_PATH = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.sln"

Write-Host "`n🎯 Opening BotBuilder in Visual Studio 2022..." -ForegroundColor Green
Write-Host "When VS opens:" -ForegroundColor Cyan
Write-Host "  1. Wait for the project to load (30 seconds)" -ForegroundColor Gray
Write-Host "  2. Press F5 to build and run" -ForegroundColor Gray
Write-Host "  3. Test all 4 tabs" -ForegroundColor Gray
Write-Host "  4. Close when done`n" -ForegroundColor Gray

# Open VS with the solution
& $VS_PATH $SLN_PATH

Write-Host "`n✅ Visual Studio closed`n" -ForegroundColor Green
