# Wait for build to complete and immediately test

Write-Host "Waiting for build to finish..." -ForegroundColor Cyan

# Wait forprocesses to finish
while (Get-Process -Name cmake,cl,link,msbuild -ErrorAction SilentlyContinue) {
    Start-Sleep -Seconds 3
    Write-Host "." -NoNewline
}

Write-Host "`n✅ Build complete!" -ForegroundColor Green
Start-Sleep -Seconds 2

# Check binary age
$bin = Get-Item bin\RawrXD-Win32IDE.exe
$age = (Get-Date) - $bin.LastWriteTime
Write-Host "Binary modified: $($bin.LastWriteTime)" -ForegroundColor Yellow
Write-Host "Age: $([Math]::Round($age.TotalSeconds, 0)) seconds`n" -ForegroundColor Yellow

if ($age.TotalMinutes -gt 5) {
    Write-Host "❌ WARNING: Binary is more than 5 minutes old - build may have failed!" -ForegroundColor Red
    Write-Host "Check build logs for errors.`n"
}

# Run test
Write-Host "=== RUNNING FLAG DETECTION TEST ===" -ForegroundColor Cyan
Write-Host "Expected: [FAST] lines if test mode works"
Write-Host "Bad sign: [IDEConfig] lines if GUI mode`n" -ForegroundColor Yellow

$timeout = 5000
$proc = Start-Process -FilePath ".\bin\RawrXD-Win32IDE.exe" `
                      -ArgumentList "--test-inference-fast --test-model model.gguf" `
                      -RedirectStandardOutput "test_post_build.log" `
                      -RedirectStandardError "test_post_build_err.log" `
                      -NoNewWindow -PassThru

if (!$proc.WaitForExit($timeout)) {
    $proc.Kill()
    Write-Host "❌ TIMEOUT - Still in GUI mode (stale binary)" -ForegroundColor Red
} else {
    Write-Host "✅ EXIT $($proc.ExitCode) within ${timeout}ms" -ForegroundColor Green
}

Write-Host "`n=== STDOUT (first 15 lines) ===" -ForegroundColor Cyan
Get-Content test_post_build.log | Select-Object -First 15

Write-Host "`n=== ANALYSIS ===" -ForegroundColor Yellow
$stdout = Get-Content test_post_build.log -Raw
if ($stdout -match '\[FAST\]') {
    Write-Host "✅ TEST MODE WORKING - Binary has flag detection!" -ForegroundColor Green
    if ($stdout -match 'PASS FAST_GENERATE') {
        Write-Host "✅✅ INFERENCE WORKING - Tokens emitted!" -ForegroundColor Green
    } elseif ($stdout -match '\[FAST\] Generating token') {
        Write-Host "⚠️ INFERENCE HANG - Reaches generation but times out" -ForegroundColor Yellow
        Write-Host "Next step: Add layer instrumentation to find hang location"
    }
} elseif ($stdout -match '\[IDEConfig\]|\[EnterpriseLicense\]') {
    Write-Host "❌ GUI MODE - Flag detection NOT working (binary still stale)" -ForegroundColor Red
} else {
    Write-Host "⚠️ UNKNOWN - No recognizable output" -ForegroundColor Yellow
}
