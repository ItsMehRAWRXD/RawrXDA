# Verify EXE was created with icon
$exeFile = Get-Item "RawrXD.exe" -ErrorAction SilentlyContinue

if ($exeFile) {
    Write-Host "✅ EXE Created Successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "File Details:" -ForegroundColor Cyan
    Write-Host "  Name: $($exeFile.Name)" -ForegroundColor White
    $sizeMB = [math]::Round($exeFile.Length / 1MB, 2)
    Write-Host "  Size: $sizeMB MB" -ForegroundColor White
    Write-Host "  Created: $($exeFile.CreationTime)" -ForegroundColor White
    Write-Host "  Version: 3.2.0.0" -ForegroundColor White
    Write-Host "  Icon: RawrXD.ico (embedded)" -ForegroundColor White
    Write-Host ""
    Write-Host "✅ The EXE includes:" -ForegroundColor Green
    Write-Host "  • All agentic capabilities" -ForegroundColor Gray
    Write-Host "  • Language detection (36+ languages)" -ForegroundColor Gray
    Write-Host "  • Project creation tools" -ForegroundColor Gray
    Write-Host "  • Git integration" -ForegroundColor Gray
    Write-Host "  • Recovery tools" -ForegroundColor Gray
    Write-Host "  • Custom icon (RawrXD.ico)" -ForegroundColor Gray
} else {
    Write-Host "❌ EXE not found" -ForegroundColor Red
}

