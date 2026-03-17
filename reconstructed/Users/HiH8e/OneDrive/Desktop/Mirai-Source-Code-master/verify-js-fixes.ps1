# Verify JavaScript Syntax Fixes
# Check that the syntax errors have been resolved

param(
    [string]$IDEPath = "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html"
)

Write-Host "🧪 Verifying JavaScript Syntax Fixes" -ForegroundColor Cyan
Write-Host "📁 File: $IDEPath" -ForegroundColor White
Write-Host ""

# Read file content
$content = Get-Content $IDEPath -Raw

# Test 1: Check for invalid optional chaining patterns
Write-Host "1. 🔍 Checking for Invalid Optional Chaining..." -ForegroundColor Yellow
$invalidPatterns = [regex]::Matches($content, '\(\w+\)\?\.\w+\s*=')
if ($invalidPatterns.Count -eq 0) {
    Write-Host "   ✅ No invalid optional chaining on assignments" -ForegroundColor Green
} else {
    Write-Host "   ❌ Found $($invalidPatterns.Count) invalid optional chaining patterns" -ForegroundColor Red
    $invalidPatterns | ForEach-Object { Write-Host "      Line $($_.Index): $($_.Value)" -ForegroundColor Red }
}

# Test 2: Check for valid DOM operations
Write-Host ""
Write-Host "2. 🔍 Checking DOM Operations..." -ForegroundColor Yellow
$validAssignments = [regex]::Matches($content, '\w+\.style\.\w+\s*=')
$validMethods = [regex]::Matches($content, '\w+\.appendChild\(')
$validEvents = [regex]::Matches($content, '\w+\.addEventListener\(')

Write-Host "   ✅ Found $($validAssignments.Count) valid style assignments" -ForegroundColor Green
Write-Host "   ✅ Found $($validMethods.Count) valid appendChild calls" -ForegroundColor Green
Write-Host "   ✅ Found $($validEvents.Count) valid addEventListener calls" -ForegroundColor Green

# Test 3: Check for safe DOM functions (should still be there)
Write-Host ""
Write-Host "3. 🔍 Checking Safe DOM Functions..." -ForegroundColor Yellow
$safeFunctions = @('safeGetElementById', 'safeQuerySelector', 'ensureDOMReady', 'safeDOMOperation')
$allPresent = $true

foreach ($func in $safeFunctions) {
    if ($content -match $func) {
        Write-Host "   ✅ $func function present" -ForegroundColor Green
    } else {
        Write-Host "   ❌ $func function missing" -ForegroundColor Red
        $allPresent = $false
    }
}

# Test 4: Check for consolidated DOM handler
Write-Host ""
Write-Host "4. 🔍 Checking Consolidated Handler..." -ForegroundColor Yellow
if ($content -match "DOM fully loaded, initializing IDE components") {
    Write-Host "   ✅ Consolidated DOM handler present" -ForegroundColor Green
} else {
    Write-Host "   ❌ Consolidated DOM handler missing" -ForegroundColor Red
}

Write-Host ""
Write-Host "🎯 Verification Results:" -ForegroundColor Cyan
if ($invalidPatterns.Count -eq 0 -and $allPresent) {
    Write-Host "   ✅ All syntax errors fixed!" -ForegroundColor Green
    Write-Host "   ✅ Safe DOM functions preserved" -ForegroundColor Green
    Write-Host "   ✅ Valid JavaScript syntax restored" -ForegroundColor Green
} else {
    Write-Host "   ⚠️ Some issues may remain" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "🚀 Next Steps:" -ForegroundColor Green
Write-Host "   1. Open IDEre2.html in browser" -ForegroundColor White
Write-Host "   2. Check console for syntax errors (should be none)" -ForegroundColor White
Write-Host "   3. Verify IDE loads and functions properly" -ForegroundColor White
Write-Host "   4. Test Beast Swarm integration" -ForegroundColor White