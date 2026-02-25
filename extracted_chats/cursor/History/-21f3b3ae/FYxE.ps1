# Test Internet Fallback System
# Run this in the 3-panel workspace console

Write-Host "🌐 Testing Internet Fallback System" -ForegroundColor Cyan
Write-Host "=" * 50 -ForegroundColor Cyan

# Test 1: Local first, internet second
Write-Host "`n📋 Test 1: Local first, internet second"
Write-Host "Running bigBrowseDWithFallback()..."
bigBrowseDWithFallback

# Test 2: Direct internet fetch
Write-Host "`n📋 Test 2: Direct internet fetch"
Write-Host "Testing safeInternetFetch with GitHub README..."
$result = safeInternetFetch("pytorch/vision/main/README.md")
if ($result) {
    Write-Host "✅ Internet fetch successful" -ForegroundColor Green
    Write-Host "Content preview: $($result.Substring(0, 200))..."
} else {
    Write-Host "❌ Internet fetch failed" -ForegroundColor Red
}

# Test 3: Cache test
Write-Host "`n📋 Test 3: Cache test"
Write-Host "Running same fetch again (should hit cache)..."
$result2 = safeInternetFetch("pytorch/vision/main/README.md")
if ($result2) {
    Write-Host "✅ Cache hit successful" -ForegroundColor Green
} else {
    Write-Host "❌ Cache hit failed" -ForegroundColor Red
}

# Test 4: Different content types
Write-Host "`n📋 Test 4: Different content types"
$testPaths = @(
    "tensorflow/tensorflow/master/README.md",
    "microsoft/vscode/main/README.md",
    "facebook/react/main/README.md"
)

foreach ($path in $testPaths) {
    Write-Host "Testing: $path"
    $result = safeInternetFetch($path)
    if ($result) {
        Write-Host "✅ $path - Success" -ForegroundColor Green
    } else {
        Write-Host "❌ $path - Failed" -ForegroundColor Red
    }
}

# Test 5: Error handling
Write-Host "`n📋 Test 5: Error handling"
Write-Host "Testing invalid URL..."
$result = safeInternetFetch("invalid-url-that-should-fail")
if (-not $result) {
    Write-Host "✅ Error handling working correctly" -ForegroundColor Green
} else {
    Write-Host "❌ Error handling failed" -ForegroundColor Red
}

Write-Host "`n🎉 Internet Fallback Test Complete!" -ForegroundColor Green
Write-Host "Check the console for detailed logs" -ForegroundColor Yellow
