# RawrXD IDE Full Test Harness
# Guides and validates OS-to-GUI connections for both Windows and zero-deps builds

param(
    [string]$Root = "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\src\masm"
)

$ErrorActionPreference = 'Stop'

$binFull = Join-Path $Root 'build_complete\bin\RawrXD_IDE_Complete.exe'
$binZero = Join-Path $Root 'build_zero_deps\bin\RawrXD_ZeroDeps.exe'
$binMin  = Join-Path $Root 'build_zero_deps\bin\RawrXD_MinimalDeps.exe'

$chatHist = Join-Path $Root 'chat_history.txt'
$termLog  = Join-Path $Root 'terminal.log'
$modelsJson = Join-Path $Root 'models.json'
$sampleDir = Join-Path $Root 'test_data'
$sampleFile = Join-Path $sampleDir 'hello_world.txt'

Write-Host "=== RawrXD IDE Full Test ===" -ForegroundColor Cyan

# Prepare test data
if (-not (Test-Path $sampleDir)) { New-Item -ItemType Directory -Path $sampleDir | Out-Null }
Set-Content -Path $sampleFile -Value "Hello RawrXD!`r`nThis is a test file.`r`n" -Encoding ASCII

# Place GGUF model files for combo box
$ggufs = @('gpt-4-turbo.gguf','claude-3-sonnet.gguf','meta-llama-3.1-8b-instruct.gguf')
foreach ($g in $ggufs) { Set-Content -Path (Join-Path $Root $g) -Value '' -Encoding ASCII }

# Seed chat history
Set-Content -Path $chatHist -Value "Previous chat line 1`r`nPrevious chat line 2" -Encoding ASCII

# Full Windows build presence
if (Test-Path $binFull) {
    Write-Host "✅ Full IDE found: $binFull" -ForegroundColor Green
} else {
    Write-Host "❌ Full IDE missing. Run RAWR_COMPLETE_IDE_BUILD.bat" -ForegroundColor Red
}

# Zero-deps binaries presence
$zeroPresent = Test-Path $binZero
$minPresent  = Test-Path $binMin
Write-Host "ZeroDeps: $zeroPresent  MinimalDeps: $minPresent" -ForegroundColor Yellow

Write-Host ""; Write-Host "Manual Steps:" -ForegroundColor Magenta
Write-Host "1) Launch full IDE: $binFull" -ForegroundColor White
Write-Host "   - Verify drives appear in explorer" -ForegroundColor Gray
Write-Host "   - Navigate to $sampleDir and open $sampleFile" -ForegroundColor Gray
Write-Host "   - Type a message in chat; close app" -ForegroundColor Gray
Write-Host "2) Relaunch full IDE: chat should show previous lines + your new message" -ForegroundColor White
Write-Host "3) Save editor content using File > Save; verify file updates" -ForegroundColor White

Write-Host ""; Write-Host "Validation:" -ForegroundColor Magenta
Write-Host ("chat_history.txt exists: {0}" -f (Test-Path $chatHist)) -ForegroundColor Green
Write-Host ("terminal.log exists: {0}" -f (Test-Path $termLog)) -ForegroundColor Green
Write-Host ("models.json present: {0}" -f (Test-Path $modelsJson)) -ForegroundColor Green
Write-Host ("Sample file exists: {0}" -f (Test-Path $sampleFile)) -ForegroundColor Green

Write-Host ""; Write-Host "Zero-Dependencies:" -ForegroundColor Magenta
if ($zeroPresent) {
    Write-Host "Launch: $binZero (requires VGA/BIOS-compatible environment)" -ForegroundColor Yellow
} elseif ($minPresent) {
    Write-Host "Launch fallback: $binMin (minimal kernel32)" -ForegroundColor Yellow
} else {
    Write-Host "Run build_zero_deps.bat to produce zero/minimal binaries." -ForegroundColor Yellow
}

Write-Host ""; Write-Host "=== Test Guide Complete ===" -ForegroundColor Cyan
