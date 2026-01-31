# Quick test of real marketplace functionality
Write-Host "🔄 Testing Real Marketplace Integration..." -ForegroundColor Cyan
Write-Host ""

try {
  # Import module
  Import-Module ".\RawrXD-Marketplace.psm1" -Force -ErrorAction Stop
  Write-Host "✅ Module imported successfully" -ForegroundColor Green

  # Test marketplace configuration loading
  if (Test-Path ".\marketplace\marketplace-config.json") {
    $config = Get-Content ".\marketplace\marketplace-config.json" | ConvertFrom-Json
    if ($config.realMarketplaceSources) {
      Write-Host "✅ Real marketplace sources configured:" -ForegroundColor Green
      foreach ($source in $config.realMarketplaceSources.PSObject.Properties) {
        if ($source.Value.enabled) {
          Write-Host "   🔹 $($source.Value.name) - ENABLED" -ForegroundColor Blue
        }
        else {
          Write-Host "   🔸 $($source.Value.name) - disabled" -ForegroundColor DarkGray
        }
      }
    }
  }

  Write-Host ""
  Write-Host "🚀 Ready to test real marketplace data fetching!" -ForegroundColor Green
  Write-Host ""
  Write-Host "📝 Available Commands:" -ForegroundColor Yellow
  Write-Host "   .\marketplace-demo.ps1 quick   - Quick real marketplace test" -ForegroundColor White
  Write-Host "   .\marketplace-demo.ps1 refresh - Force refresh from real sources" -ForegroundColor White
  Write-Host "   .\marketplace-demo.ps1 demo    - Full real marketplace demo" -ForegroundColor White
  Write-Host "   .\marketplace-demo.ps1 test    - Run all integration tests" -ForegroundColor White
  Write-Host ""
  Write-Host "💡 The real marketplace will fetch live data from:" -ForegroundColor Cyan
  Write-Host "   • VS Code Marketplace (up to 100 popular extensions)" -ForegroundColor Gray
  Write-Host "   • PowerShell Gallery (up to 50 relevant modules)" -ForegroundColor Gray
  Write-Host "   • GitHub (up to 50 extension repositories)" -ForegroundColor Gray
  Write-Host ""
  Write-Host "⚠️  Note: First run may take 30-60 seconds to fetch live data" -ForegroundColor Yellow

}
catch {
  Write-Error "Failed to test marketplace integration: $($_.Exception.Message)"
}
