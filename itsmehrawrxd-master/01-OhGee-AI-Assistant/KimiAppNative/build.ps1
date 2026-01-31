# PowerShell build script for AI Assistant Hub

Write-Host "Building AI Assistant Hub..." -ForegroundColor Green

# Clean previous builds
if (Test-Path "bin") { Remove-Item -Recurse -Force "bin" }
if (Test-Path "obj") { Remove-Item -Recurse -Force "obj" }
if (Test-Path "publish") { Remove-Item -Recurse -Force "publish" }

# Restore packages
Write-Host "Restoring packages..." -ForegroundColor Yellow
dotnet restore

# Build the application
Write-Host "Building application..." -ForegroundColor Yellow
dotnet build --configuration Release

# Publish the application
Write-Host "Publishing application..." -ForegroundColor Yellow
dotnet publish --configuration Release --runtime win-x64 --self-contained true --output "publish"

Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Output directory: publish\" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run the application:" -ForegroundColor Cyan
Write-Host "cd publish" -ForegroundColor White
Write-Host ".\AIAssistantHub.exe" -ForegroundColor White

# Optional: Open the publish directory
$openDir = Read-Host "Open publish directory? (y/n)"
if ($openDir -eq "y" -or $openDir -eq "Y") {
    Start-Process "publish"
}
