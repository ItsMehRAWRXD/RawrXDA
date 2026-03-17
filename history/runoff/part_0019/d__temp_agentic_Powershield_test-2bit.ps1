Write-Host "🧠 Starting 2-Bit Drive Digest..." -ForegroundColor Cyan

# Simple test - scan current directory
$files = Get-ChildItem -Path "." -Recurse -File -Filter "*.ps1" | Select-Object -First 5

Write-Host "Found $($files.Count) PowerShell files" -ForegroundColor Green

foreach ($file in $files) {
    Write-Host "Processing: $($file.Name)" -ForegroundColor Gray
}

Write-Host "✅ Test completed!" -ForegroundColor Green