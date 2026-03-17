# rawr1024_test_runner.ps1 - Test Runner for Rawr1024 Engine Tests
# Assembles with ML64, links with link.exe, and runs the test executable

param(
    [string]$ML64Path = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe",
    [string]$LinkPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe",
    [string]$Kernel32Lib = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.Lib"
)

# Set paths
$TestDir = "$PSScriptRoot\tests"
$SourceDir = "$PSScriptRoot"
$TestAsm = "$TestDir\rawr1024_engine_tests.asm"
$MainAsm = "$SourceDir\rawr1024_dual_engine_custom.asm"

Write-Host "=== Rawr1024 Engine Test Runner ===" -ForegroundColor Cyan
Write-Host "Test Directory: $TestDir"
Write-Host "Source Directory: $SourceDir"

# Step 1: Assemble the main engine file
Write-Host "`n1. Assembling main engine file..." -ForegroundColor Yellow
& $ML64Path /c /Fo"$SourceDir\rawr1024_dual_engine_custom.obj" "$MainAsm"
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to assemble main engine file" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Main engine assembled successfully" -ForegroundColor Green

# Step 2: Assemble the test file
Write-Host "`n2. Assembling test file..." -ForegroundColor Yellow
& $ML64Path /c /Fo"$TestDir\rawr1024_engine_tests.obj" "$TestAsm"
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to assemble test file" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Test file assembled successfully" -ForegroundColor Green

# Step 3: Link the test executable
Write-Host "`n3. Linking test executable..." -ForegroundColor Yellow
& $LinkPath /OUT:"$TestDir\rawr1024_engine_tests.exe" "$TestDir\rawr1024_engine_tests.obj" "$SourceDir\rawr1024_dual_engine_custom.obj" /ENTRY:main /SUBSYSTEM:CONSOLE
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to link test executable" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Test executable linked successfully" -ForegroundColor Green

# Step 4: Run the tests
Write-Host "`n4. Running tests..." -ForegroundColor Yellow
$TestResult = & "$TestDir\rawr1024_engine_tests.exe"
$ExitCode = $LASTEXITCODE

if ($ExitCode -eq 0) {
    Write-Host "`n=== ALL TESTS PASSED ===" -ForegroundColor Green
    Write-Host "Exit Code: $ExitCode" -ForegroundColor Green
} else {
    Write-Host "`n=== TESTS FAILED ===" -ForegroundColor Red
    Write-Host "Exit Code: $ExitCode" -ForegroundColor Red
}

# Step 5: Clean up intermediate files
Write-Host "`n5. Cleaning up intermediate files..." -ForegroundColor Yellow
Remove-Item "$SourceDir\rawr1024_dual_engine_custom.obj" -ErrorAction SilentlyContinue
Remove-Item "$TestDir\rawr1024_engine_tests.obj" -ErrorAction SilentlyContinue
Write-Host "✓ Cleanup complete" -ForegroundColor Green

Write-Host "`n=== Test Runner Complete ===" -ForegroundColor Cyan
exit $ExitCode