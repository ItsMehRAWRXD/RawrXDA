# RawrXD Real Marketplace Test Script
# Tests the marketplace functionality with real data sources

Write-Host "🧪 Testing RawrXD Real Marketplace Integration..." -ForegroundColor Cyan

# Test 1: Module Import
try {
    Import-Module .\RawrXD-Marketplace.psm1 -Force
    Write-Host "✅ Module imported successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ Module import failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Test 2: Initialize Marketplace
try {
    $init = Initialize-RawrXDMarketplace
    Write-Host "✅ Marketplace initialized: $init" -ForegroundColor Green
} catch {
    Write-Host "❌ Marketplace initialization failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 3: Get Extensions
try {
    $extensions = Get-RawrXDExtensions -MaxResults 5
    Write-Host "✅ Retrieved $($extensions.Count) extensions" -ForegroundColor Green
    
    Write-Host "`n📦 Sample Extensions:" -ForegroundColor Yellow
    $extensions | Select-Object -First 3 | ForEach-Object {
        Write-Host "   • $($_.name) - $($_.source) - $($_.stats.rating)⭐" -ForegroundColor Gray
    }
} catch {
    Write-Host "❌ Get extensions failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 4: Search Functionality
try {
    $searchResults = Search-RawrXDMarketplace -Query "vscode" -Type "extensions" | Select-Object -First 3
    Write-Host "`n🔍 Search Results: $($searchResults.Count) items" -ForegroundColor Green
    $searchResults | ForEach-Object {
        Write-Host "   • $($_.name) - $($_.source)" -ForegroundColor Gray
    }
} catch {
    Write-Host "❌ Search failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 5: Marketplace Display
Write-Host "`n🎯 Testing Marketplace Display..." -ForegroundColor Cyan
try {
    Show-RawrXDMarketplace | Out-Null
    Write-Host "✅ Marketplace display rendered successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ Marketplace display failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "`n🎉 Marketplace Tests Completed!" -ForegroundColor Green
Write-Host "`nℹ️  Available Commands:" -ForegroundColor Blue
Write-Host "   • Get-RawrXDExtensions" -ForegroundColor Gray
Write-Host "   • Search-RawrXDMarketplace" -ForegroundColor Gray
Write-Host "   • Show-RawrXDMarketplace" -ForegroundColor Gray
Write-Host "   • Install-RawrXDExtension" -ForegroundColor Gray
Write-Host "   • Update-RawrXDMarketplace" -ForegroundColor Gray