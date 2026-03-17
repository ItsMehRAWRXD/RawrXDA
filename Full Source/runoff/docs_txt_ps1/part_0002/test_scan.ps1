Write-Host "Starting simplified test..."
Set-Location 'd:\lazy init ide'

Write-Host "Scanning for modules..."
$modules = @(Get-ChildItem -Path 'd:\lazy init ide\scripts' -Filter *.psm1)
Write-Host "Found $($modules.Count) modules"

Write-Host "Scanning for source files (this may take a while)..."
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$files = @(Get-ChildItem -Path 'd:\lazy init ide' -Recurse -Include *.ps1,*.psm1 -File -ErrorAction SilentlyContinue | Select-Object -First 100)
$timer.Stop()
Write-Host "Scanned $($files.Count) files in $($timer.Elapsed.TotalSeconds) seconds"

Write-Host "Test complete!"
