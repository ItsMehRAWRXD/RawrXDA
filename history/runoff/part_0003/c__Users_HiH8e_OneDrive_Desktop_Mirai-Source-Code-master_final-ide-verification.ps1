# Final verification of IDE fixes

param(
    [string]$IDEPath = "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html"
)

Write-Host "🎯 Final IDE Verification" -ForegroundColor Cyan
Write-Host "📁 File: $IDEPath" -ForegroundColor White
Write-Host ""

$content = Get-Content $IDEPath -Raw

# Test 1: Check for exposed JavaScript code
Write-Host "1. 🔍 Checking for Exposed JavaScript Code..." -ForegroundColor Yellow
$exposedCode = $content -match '(?s)<body>.*?(// Consolidated DOMContentLoaded Handler).*?</body>'
if ($exposedCode) {
    Write-Host "   ❌ Found exposed JavaScript code in body!" -ForegroundColor Red
} else {
    Write-Host "   ✅ No exposed JavaScript code found" -ForegroundColor Green
}

# Test 2: Check script tag structure
Write-Host ""
Write-Host "2. 🔍 Checking Script Tag Structure..." -ForegroundColor Yellow
$openScripts = [regex]::Matches($content, '<script[^>]*>').Count
$closeScripts = [regex]::Matches($content, '</script>').Count
Write-Host "   📊 Script tags: $openScripts opening, $closeScripts closing" -ForegroundColor White
if ($openScripts -eq $closeScripts) {
    Write-Host "   ✅ Script tags properly balanced" -ForegroundColor Green
} else {
    Write-Host "   ⚠️ Script tags may be unbalanced" -ForegroundColor Yellow
}

# Test 3: Check for consolidated handler
Write-Host ""
Write-Host "3. 🔍 Checking Consolidated Handler..." -ForegroundColor Yellow
$handlerInScript = $content -match '(?s)<script>.*// Consolidated DOMContentLoaded Handler.*</script>'
if ($handlerInScript) {
    Write-Host "   ✅ Consolidated handler properly wrapped in script tags" -ForegroundColor Green
} else {
    Write-Host "   ❌ Consolidated handler not properly wrapped" -ForegroundColor Red
}

# Test 4: Check for $1 references
Write-Host ""
Write-Host "4. 🔍 Checking for Invalid References..." -ForegroundColor Yellow
$dollarRefs = [regex]::Matches($content, '\$1\.').Count
if ($dollarRefs -eq 0) {
    Write-Host "   ✅ No invalid \$1 references found" -ForegroundColor Green
} else {
    Write-Host "   ⚠️ Found $dollarRefs invalid \$1 references" -ForegroundColor Yellow
}

# Test 5: Check for duplicate handlers
Write-Host ""
Write-Host "5. 🔍 Checking for Duplicate Handlers..." -ForegroundColor Yellow
$handlerCount = [regex]::Matches($content, '// Consolidated DOMContentLoaded Handler').Count
Write-Host "   📊 Handler instances found: $handlerCount" -ForegroundColor White
if ($handlerCount -eq 1) {
    Write-Host "   ✅ Only one consolidated handler (perfect!)" -ForegroundColor Green
} else {
    Write-Host "   ⚠️ Found $handlerCount handler instances" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "🎯 Verification Summary:" -ForegroundColor Cyan
$allGood = ($exposedCode -eq $false) -and ($openScripts -eq $closeScripts) -and ($handlerInScript) -and ($dollarRefs -eq 0) -and ($handlerCount -eq 1)

if ($allGood) {
    Write-Host "   ✅ ALL FIXES VERIFIED - IDE should load cleanly!" -ForegroundColor Green
    Write-Host ""
    Write-Host "🚀 Ready to test:" -ForegroundColor Green
    Write-Host "   1. Open IDEre2.html in browser" -ForegroundColor White
    Write-Host "   2. Check that no JavaScript code appears as text" -ForegroundColor White
    Write-Host "   3. Verify console shows proper initialization messages" -ForegroundColor White
} else {
    Write-Host "   ⚠️ Some issues may remain - check individual test results" -ForegroundColor Yellow
}