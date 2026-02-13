# RawrXD Real Marketplace Integration Demo
# This script demonstrates how to integrate the real marketplace system with RawrXD.ps1

# Import the marketplace module
if (Test-Path ".\RawrXD-Marketplace.psm1") {
  Import-Module ".\RawrXD-Marketplace.psm1" -Force
  Write-Host "✅ Real Marketplace module loaded" -ForegroundColor Green
}
else {
  Write-Error "RawrXD-Marketplace.psm1 not found!"
  exit 1
}

function Show-RealMarketplaceDemo {
  <#
    .SYNOPSIS
    Demonstrates the RawrXD Real Marketplace functionality
    #>

  Clear-Host
  Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
  Write-Host "║              🦊 RawrXD REAL Marketplace Demo 🦊                ║" -ForegroundColor Cyan
  Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
  Write-Host ""

  # Initialize marketplace with real data
  Write-Host "🔄 Initializing REAL marketplace system..." -ForegroundColor Yellow
  Write-Host "   This will fetch live data from VS Code Marketplace, PowerShell Gallery, and GitHub!" -ForegroundColor Cyan
  Write-Host ""

  # Ask user if they want to fetch real data or use cached
  $choice = Read-Host "Fetch fresh real data? (Y/N) [This may take 30-60 seconds]"
  $forceRefresh = $choice -eq 'Y' -or $choice -eq 'y'

  if ($forceRefresh) {
    $initialized = Initialize-RawrXDMarketplace -ForceRefresh
  }
  else {
    $initialized = Initialize-RawrXDMarketplace
  }

  if (-not $initialized) {
    Write-Error "Failed to initialize real marketplace"
    return
  }

  Write-Host ""

  # Demo 1: Show real marketplace overview
  Write-Host "📊 Demo 1: Real Marketplace Overview" -ForegroundColor Magenta
  Write-Host "─────────────────────────────────────" -ForegroundColor Gray
  Show-RawrXDMarketplace

  # Wait for user input
  Write-Host ""
  Write-Host "Press any key to continue to Demo 2..." -ForegroundColor Yellow
  $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

  # Demo 2: VS Code Marketplace extensions
  Clear-Host
  Write-Host "📦 Demo 2: VS Code Marketplace Extensions" -ForegroundColor Magenta
  Write-Host "──────────────────────────────────────────" -ForegroundColor Gray

  $vscodeExtensions = Get-RawrXDExtensions -Source "vscode-marketplace" -SortBy "downloads" -MaxResults 8

  if ($vscodeExtensions.Count -gt 0) {
    Write-Host "Found $($vscodeExtensions.Count) real VS Code extensions:" -ForegroundColor Green
    Write-Host ""

    foreach ($ext in $vscodeExtensions) {
      Write-Host "📦 $($ext.displayName) v$($ext.version)" -ForegroundColor Cyan
      Write-Host "   👤 by $($ext.author.name)$(if($ext.author.verified) { ' ✓' })" -ForegroundColor Gray
      Write-Host "   📝 $($ext.description)" -ForegroundColor White
      Write-Host "   ⭐ $($ext.stats.rating) | ⬇️ $($ext.stats.downloads.ToString('N0')) downloads" -ForegroundColor Green
      Write-Host "   🔗 $($ext.homepage)" -ForegroundColor Blue
      Write-Host ""
    }
  }
  else {
    Write-Host "No VS Code extensions loaded. Try running with -ForceRefresh" -ForegroundColor Yellow
  }

  Write-Host ""
  Write-Host "Press any key to continue to Demo 3..." -ForegroundColor Yellow
  $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

  # Demo 3: PowerShell Gallery modules
  Clear-Host
  Write-Host "🔷 Demo 3: PowerShell Gallery Modules" -ForegroundColor Magenta
  Write-Host "─────────────────────────────────────" -ForegroundColor Gray

  $psModules = Get-RawrXDExtensions -Source "powershell-gallery" -SortBy "downloads" -MaxResults 5

  if ($psModules.Count -gt 0) {
    Write-Host "Found $($psModules.Count) PowerShell modules from PowerShell Gallery:" -ForegroundColor Green
    Write-Host ""

    foreach ($module in $psModules) {
      Write-Host "🔷 $($module.displayName) v$($module.version)" -ForegroundColor Blue
      Write-Host "   👤 by $($module.author.name)" -ForegroundColor Gray
      Write-Host "   📝 $($module.description)" -ForegroundColor White
      Write-Host "   ⬇️ $($module.stats.downloads.ToString('N0')) downloads" -ForegroundColor Green
      Write-Host "   🔗 $($module.homepage)" -ForegroundColor Blue
      Write-Host ""
    }
  }
  else {
    Write-Host "No PowerShell modules loaded. Try running with -ForceRefresh" -ForegroundColor Yellow
  }

  Write-Host ""
  Write-Host "Press any key to continue to Demo 4..." -ForegroundColor Yellow
  $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

  # Demo 4: GitHub repositories
  Clear-Host
  Write-Host "🐙 Demo 4: GitHub Extension Repositories" -ForegroundColor Magenta
  Write-Host "────────────────────────────────────────" -ForegroundColor Gray

  $githubRepos = Get-RawrXDExtensions -Source "github" -SortBy "rating" -MaxResults 5

  if ($githubRepos.Count -gt 0) {
    Write-Host "Found $($githubRepos.Count) extension repositories from GitHub:" -ForegroundColor Green
    Write-Host ""

    foreach ($repo in $githubRepos) {
      Write-Host "🐙 $($repo.displayName)" -ForegroundColor Green
      Write-Host "   👤 by $($repo.author.name)" -ForegroundColor Gray
      Write-Host "   📝 $($repo.description)" -ForegroundColor White
      Write-Host "   ⭐ $($repo.stats.ratingCount) stars | Rating: $($repo.stats.rating)" -ForegroundColor Green
      Write-Host "   🔗 $($repo.homepage)" -ForegroundColor Blue
      Write-Host ""
    }
  }
  else {
    Write-Host "No GitHub repositories loaded. Try running with -ForceRefresh" -ForegroundColor Yellow
  }

  Write-Host ""
  Write-Host "Press any key to continue to Demo 5..." -ForegroundColor Yellow
  $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

  # Demo 5: Real extension installation
  Clear-Host
  Write-Host "📦 Demo 5: Real Extension Installation" -ForegroundColor Magenta
  Write-Host "─────────────────────────────────────" -ForegroundColor Gray

  # Find a popular extension to install
  $allExtensions = Get-RawrXDExtensions -SortBy "downloads" -MaxResults 10
  if ($allExtensions.Count -gt 0) {
    $extensionToInstall = $allExtensions[0]
    Write-Host "Installing top extension: $($extensionToInstall.displayName)" -ForegroundColor Cyan
    Write-Host "Source: $($extensionToInstall.source)" -ForegroundColor Gray
    Write-Host ""

    $installResult = Install-RawrXDExtension -ExtensionId $extensionToInstall.id
  }
  else {
    Write-Host "No extensions available for installation demo" -ForegroundColor Yellow
  }

  Write-Host ""
  Write-Host "Press any key to continue to Demo 6..." -ForegroundColor Yellow
  $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

  # Demo 6: Real marketplace search
  Clear-Host
  Write-Host "🔍 Demo 6: Real Marketplace Search" -ForegroundColor Magenta
  Write-Host "─────────────────────────────────" -ForegroundColor Gray

  $searchTerms = @("PowerShell", "Python", "JavaScript", "Theme")

  foreach ($searchTerm in $searchTerms | Select-Object -First 2) {
    Write-Host "🔍 Searching for: '$searchTerm'" -ForegroundColor Cyan
    $searchResults = Search-RawrXDMarketplace -Query $searchTerm -Type "extensions"

    Write-Host "Found $($searchResults.Count) results:" -ForegroundColor Green
    foreach ($result in $searchResults | Select-Object -First 3) {
      Write-Host "  📦 [$($result.Source)] $($result.Name)" -ForegroundColor Cyan
      Write-Host "     📝 $($result.Description)" -ForegroundColor White
      Write-Host "     👤 $($result.Author) | ⭐ $($result.Rating) | ⬇️ $($result.Downloads.ToString('N0'))" -ForegroundColor Gray
      Write-Host ""
    }
  }

  # Demo 7: Real-time statistics
  Write-Host ""
  Write-Host "📊 Demo 7: Real Marketplace Statistics" -ForegroundColor Magenta
  Write-Host "─────────────────────────────────────" -ForegroundColor Gray

  $allExtensions = Get-RawrXDExtensions -MaxResults 1000
  if ($allExtensions.Count -gt 0) {
    $totalExtensions = $allExtensions.Count
    $totalDownloads = ($allExtensions | Measure-Object -Property { $_.stats.downloads } -Sum).Sum
    $avgRating = ($allExtensions | Measure-Object -Property { $_.stats.rating } -Average).Average

    # Group by source
    $sourceStats = $allExtensions | Group-Object source | Sort-Object Count -Descending

    Write-Host "📈 Real Marketplace Statistics:" -ForegroundColor Green
    Write-Host "   • Total Real Extensions: $totalExtensions" -ForegroundColor White
    Write-Host "   • Total Real Downloads: $($totalDownloads.ToString('N0'))" -ForegroundColor White
    Write-Host "   • Average Rating: $($avgRating.ToString('F1'))/5.0" -ForegroundColor White
    Write-Host ""

    Write-Host "📂 Source Breakdown:" -ForegroundColor Green
    foreach ($source in $sourceStats) {
      $percentage = [math]::Round($source.Count / $totalExtensions * 100)
      Write-Host "   • $($source.Name): $($source.Count) extensions ($percentage%)" -ForegroundColor White
    }

    # Top categories
    $categoryStats = $allExtensions | Group-Object category | Sort-Object Count -Descending | Select-Object -First 5
    Write-Host ""
    Write-Host "🏷️ Top Categories:" -ForegroundColor Green
    foreach ($cat in $categoryStats) {
      Write-Host "   • $($cat.Name): $($cat.Count) extensions" -ForegroundColor White
    }
  }

  Write-Host ""
  Write-Host "🎉 Real Marketplace Demo Complete!" -ForegroundColor Green
  Write-Host "─────────────────────────────────────" -ForegroundColor Gray
  Write-Host "The RawrXD Real Marketplace system is now functional with:" -ForegroundColor White
  Write-Host "• Live data from VS Code Marketplace, PowerShell Gallery, and GitHub" -ForegroundColor Gray
  Write-Host "• Real extension metadata, download counts, and ratings" -ForegroundColor Gray
  Write-Host "• Automatic caching and refresh capabilities" -ForegroundColor Gray
  Write-Host "• Multi-source search and filtering" -ForegroundColor Gray
  Write-Host "• Source-specific installation methods" -ForegroundColor Gray
  Write-Host ""
  Write-Host "Ready for integration into RawrXD.ps1 with REAL marketplace data!" -ForegroundColor Cyan
}

function Test-RealMarketplaceIntegration {
  <#
    .SYNOPSIS
    Tests the real marketplace integration functions
    #>

  Write-Host "🧪 Testing Real Marketplace Integration..." -ForegroundColor Yellow

  # Test 1: Initialization with real data
  Write-Host "Test 1: Real Marketplace Initialization"
  $result1 = Initialize-RawrXDMarketplace -ForceRefresh
  Write-Host "   Result: $(if ($result1) { '✅ PASS' } else { '❌ FAIL' })" -ForegroundColor $(if ($result1) { 'Green' } else { 'Red' })

  # Test 2: VS Code extensions retrieval
  Write-Host "Test 2: VS Code Extensions Retrieval"
  $vscodeExts = Get-RawrXDExtensions -Source "vscode-marketplace" -MaxResults 5
  $result2 = $vscodeExts.Count -gt 0
  Write-Host "   Result: $(if ($result2) { "✅ PASS ($($vscodeExts.Count) VS Code extensions)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result2) { 'Green' } else { 'Red' })

  # Test 3: PowerShell Gallery modules retrieval
  Write-Host "Test 3: PowerShell Gallery Modules Retrieval"
  $psModules = Get-RawrXDExtensions -Source "powershell-gallery" -MaxResults 5
  $result3 = $psModules.Count -gt 0
  Write-Host "   Result: $(if ($result3) { "✅ PASS ($($psModules.Count) PS modules)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result3) { 'Green' } else { 'Red' })

  # Test 4: GitHub repositories retrieval
  Write-Host "Test 4: GitHub Repositories Retrieval"
  $githubRepos = Get-RawrXDExtensions -Source "github" -MaxResults 5
  $result4 = $githubRepos.Count -gt 0
  Write-Host "   Result: $(if ($result4) { "✅ PASS ($($githubRepos.Count) GitHub repos)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result4) { 'Green' } else { 'Red' })

  # Test 5: Real marketplace search
  Write-Host "Test 5: Real Marketplace Search"
  $searchResults = Search-RawrXDMarketplace -Query "PowerShell" -Type "extensions"
  $result5 = $searchResults.Count -gt 0
  Write-Host "   Result: $(if ($result5) { "✅ PASS ($($searchResults.Count) search results)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result5) { 'Green' } else { 'Red' })

  # Test 6: Data refresh functionality
  Write-Host "Test 6: Data Refresh Functionality"
  $refreshResult = Update-RawrXDMarketplace
  $result6 = $refreshResult -eq $true
  Write-Host "   Result: $(if ($result6) { "✅ PASS (Data refresh successful)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result6) { 'Green' } else { 'Red' })

  $allPassed = $result1 -and $result2 -and $result3 -and $result4 -and $result5 -and $result6

  Write-Host ""
  Write-Host "🎯 Overall Real Marketplace Test Result: $(if ($allPassed) { '✅ ALL TESTS PASSED' } else { '❌ SOME TESTS FAILED' })" -ForegroundColor $(if ($allPassed) { 'Green' } else { 'Red' })

  if ($allPassed) {
    Write-Host "🚀 Real marketplace integration is working perfectly!" -ForegroundColor Green
    Write-Host "   Your RawrXD IDE now has access to thousands of real extensions!" -ForegroundColor Cyan
  }

  return $allPassed
}

function Show-QuickRealMarketplace {
  <#
    .SYNOPSIS
    Quick demo of real marketplace without full demo
    #>

  Write-Host "⚡ Quick Real Marketplace Demo" -ForegroundColor Yellow

  # Initialize
  Initialize-RawrXDMarketplace | Out-Null

  # Show quick stats
  $allExtensions = Get-RawrXDExtensions -MaxResults 100
  if ($allExtensions.Count -gt 0) {
    Write-Host "✅ Loaded $($allExtensions.Count) real extensions!" -ForegroundColor Green

    $sources = $allExtensions | Group-Object source
    Write-Host "📊 Sources: $($sources.Name -join ', ')" -ForegroundColor Cyan

    $totalDownloads = ($allExtensions | Measure-Object -Property { $_.stats.downloads } -Sum).Sum
    Write-Host "⬇️ Total Downloads: $($totalDownloads.ToString('N0'))" -ForegroundColor Blue

    # Show top extension
    $topExtension = $allExtensions | Sort-Object { $_.stats.downloads } -Descending | Select-Object -First 1
    Write-Host ""
    Write-Host "🏆 Top Extension:" -ForegroundColor Yellow
    Write-Host "   $($topExtension.displayName) by $($topExtension.author.name)" -ForegroundColor White
    Write-Host "   $($topExtension.stats.downloads.ToString('N0')) downloads from $($topExtension.source)" -ForegroundColor Gray
  }
  else {
    Write-Host "No real extensions loaded. Run 'Initialize-RawrXDMarketplace -ForceRefresh' to fetch live data." -ForegroundColor Yellow
  }
}
<#
    .SYNOPSIS
    Demonstrates the RawrXD Marketplace functionality
    #>

Clear-Host
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                🦊 RawrXD Marketplace Demo 🦊                  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Initialize marketplace
Write-Host "🔄 Initializing marketplace system..." -ForegroundColor Yellow
$initialized = Initialize-RawrXDMarketplace

if (-not $initialized) {
  Write-Error "Failed to initialize marketplace"
  return
}

Write-Host ""

# Demo 1: Show marketplace overview
Write-Host "📊 Demo 1: Marketplace Overview" -ForegroundColor Magenta
Write-Host "─────────────────────────────────" -ForegroundColor Gray
Show-RawrXDMarketplace

# Wait for user input
Write-Host ""
Write-Host "Press any key to continue to Demo 2..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Demo 2: Search for programming language extensions
Clear-Host
Write-Host "🔍 Demo 2: Programming Language Extensions" -ForegroundColor Magenta
Write-Host "───────────────────────────────────────────" -ForegroundColor Gray

$languageExtensions = Get-RawrXDExtensions -Category "Languages" -SortBy "rating" -MaxResults 5

foreach ($ext in $languageExtensions) {
  Write-Host ""
  Write-Host "📦 $($ext.displayName) v$($ext.version)" -ForegroundColor Cyan
  Write-Host "   👤 by $($ext.author.name)" -ForegroundColor Gray
  Write-Host "   📝 $($ext.description)" -ForegroundColor White
  Write-Host "   ⭐ $($ext.stats.rating) ($($ext.stats.ratingCount) reviews)" -ForegroundColor Green
  Write-Host "   ⬇️  $($ext.stats.downloads.ToString('N0')) downloads" -ForegroundColor Yellow
  Write-Host "   🏷️  $($ext.tags -join ', ')" -ForegroundColor DarkGray

  if ($ext.pricing.type -eq "paid") {
    Write-Host "   💰 $($ext.pricing.price) $($ext.pricing.currency)" -ForegroundColor Red
  }
  else {
    Write-Host "   🆓 Free" -ForegroundColor Green
  }
}

Write-Host ""
Write-Host "Press any key to continue to Demo 3..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Demo 3: Community themes
Clear-Host
Write-Host "🎨 Demo 3: Community Themes" -ForegroundColor Magenta
Write-Host "──────────────────────────────" -ForegroundColor Gray

$themes = Get-RawrXDCommunityContent -ContentType "theme" -MaxResults 3

foreach ($theme in $themes) {
  Write-Host ""
  Write-Host "🎯 $($theme.displayName) v$($theme.version)" -ForegroundColor Cyan
  Write-Host "   👤 by $($theme.author.name)" -ForegroundColor Gray
  Write-Host "   📝 $($theme.description)" -ForegroundColor White
  Write-Host "   ⭐ $($theme.stats.rating) | ❤️  $($theme.stats.likes) likes" -ForegroundColor Green
  Write-Host "   ⬇️  $($theme.stats.downloads.ToString('N0')) downloads" -ForegroundColor Yellow
  Write-Host "   🏷️  $($theme.tags -join ', ')" -ForegroundColor DarkGray

  if ($theme.social.featured) {
    Write-Host "   ✨ Featured Content" -ForegroundColor Yellow
  }
  if ($theme.social.trending) {
    Write-Host "   🔥 Trending" -ForegroundColor Red
  }
  if ($theme.social.staffPick) {
    Write-Host "   👑 Staff Pick" -ForegroundColor Magenta
  }
}

Write-Host ""
Write-Host "Press any key to continue to Demo 4..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Demo 4: Extension installation simulation
Clear-Host
Write-Host "📦 Demo 4: Extension Installation" -ForegroundColor Magenta
Write-Host "─────────────────────────────────" -ForegroundColor Gray

$extensionToInstall = "rawrxd-powershell-pro"
Write-Host "Installing popular extension: $extensionToInstall" -ForegroundColor Cyan
Write-Host ""

$installResult = Install-RawrXDExtension -ExtensionId $extensionToInstall

Write-Host ""
Write-Host "Press any key to continue to Demo 5..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Demo 5: Search functionality
Clear-Host
Write-Host "🔍 Demo 5: Marketplace Search" -ForegroundColor Magenta
Write-Host "─────────────────────────────" -ForegroundColor Gray

$searchTerm = "PowerShell"
Write-Host "Searching for: '$searchTerm'" -ForegroundColor Cyan
Write-Host ""

$searchResults = Search-RawrXDMarketplace -Query $searchTerm -Type "all"

foreach ($result in $searchResults | Select-Object -First 5) {
  Write-Host "🔍 [$($result.Type)] $($result.Name)" -ForegroundColor Cyan
  Write-Host "   📝 $($result.Description)" -ForegroundColor White
  Write-Host "   👤 $($result.Author) | 📂 $($result.Category)" -ForegroundColor Gray
  Write-Host "   ⭐ $($result.Rating) | ⬇️  $($result.Downloads.ToString('N0'))" -ForegroundColor Green
  Write-Host ""
}

# Demo 6: Analytics and stats
Write-Host ""
Write-Host "📊 Demo 6: Marketplace Statistics" -ForegroundColor Magenta
Write-Host "─────────────────────────────────" -ForegroundColor Gray

# Calculate some interesting statistics
$allExtensions = Get-RawrXDExtensions -MaxResults 1000
$allCommunity = Get-RawrXDCommunityContent -MaxResults 1000

$totalExtensions = $allExtensions.Count
$totalCommunityItems = $allCommunity.Count
$totalDownloads = ($allExtensions | Measure-Object -Property { $_.stats.downloads } -Sum).Sum
$avgRating = ($allExtensions | Measure-Object -Property { $_.stats.rating } -Average).Average
$freeExtensions = ($allExtensions | Where-Object { $_.pricing.type -eq "free" }).Count
$paidExtensions = ($allExtensions | Where-Object { $_.pricing.type -eq "paid" }).Count

Write-Host "📈 Marketplace Statistics:" -ForegroundColor Green
Write-Host "   • Total Extensions: $totalExtensions" -ForegroundColor White
Write-Host "   • Community Items: $totalCommunityItems" -ForegroundColor White
Write-Host "   • Total Downloads: $($totalDownloads.ToString('N0'))" -ForegroundColor White
Write-Host "   • Average Rating: $($avgRating.ToString('F1'))/5.0" -ForegroundColor White
Write-Host "   • Free Extensions: $freeExtensions ($([math]::Round($freeExtensions/$totalExtensions*100))%)" -ForegroundColor White
Write-Host "   • Paid Extensions: $paidExtensions ($([math]::Round($paidExtensions/$totalExtensions*100))%)" -ForegroundColor White

# Category breakdown
Write-Host ""
Write-Host "📂 Category Breakdown:" -ForegroundColor Green
$categories = $allExtensions | Group-Object category | Sort-Object Count -Descending
foreach ($cat in $categories) {
  Write-Host "   • $($cat.Name): $($cat.Count) extensions" -ForegroundColor White
}

Write-Host ""
Write-Host "🎉 Demo Complete!" -ForegroundColor Green
Write-Host "───────────────────" -ForegroundColor Gray
Write-Host "The RawrXD Marketplace system is fully functional with:" -ForegroundColor White
Write-Host "• Comprehensive extension metadata and real functionality descriptions" -ForegroundColor Gray
Write-Host "• Rich community content including themes, snippets, and templates" -ForegroundColor Gray
Write-Host "• PowerShell integration for browsing, searching, and installing" -ForegroundColor Gray
Write-Host "• API schema for future web-based marketplace development" -ForegroundColor Gray
Write-Host "• Security features and configuration management" -ForegroundColor Gray
Write-Host ""
Write-Host "Ready for integration into RawrXD.ps1!" -ForegroundColor Cyan
}

function Test-MarketplaceIntegration {
  <#
    .SYNOPSIS
    Tests the marketplace integration functions
    #>

  Write-Host "🧪 Testing Marketplace Integration..." -ForegroundColor Yellow

  # Test 1: Initialization
  Write-Host "Test 1: Marketplace Initialization"
  $result1 = Initialize-RawrXDMarketplace
  Write-Host "   Result: $(if ($result1) { '✅ PASS' } else { '❌ FAIL' })" -ForegroundColor $(if ($result1) { 'Green' } else { 'Red' })

  # Test 2: Extension retrieval
  Write-Host "Test 2: Extension Retrieval"
  $extensions = Get-RawrXDExtensions -MaxResults 5
  $result2 = $extensions.Count -gt 0
  Write-Host "   Result: $(if ($result2) { "✅ PASS ($($extensions.Count) extensions)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result2) { 'Green' } else { 'Red' })

  # Test 3: Community content retrieval
  Write-Host "Test 3: Community Content Retrieval"
  $community = Get-RawrXDCommunityContent -MaxResults 5
  $result3 = $community.Count -gt 0
  Write-Host "   Result: $(if ($result3) { "✅ PASS ($($community.Count) items)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result3) { 'Green' } else { 'Red' })

  # Test 4: Search functionality
  Write-Host "Test 4: Search Functionality"
  $searchResults = Search-RawrXDMarketplace -Query "PowerShell"
  $result4 = $searchResults.Count -gt 0
  Write-Host "   Result: $(if ($result4) { "✅ PASS ($($searchResults.Count) results)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result4) { 'Green' } else { 'Red' })

  # Test 5: Category filtering
  Write-Host "Test 5: Category Filtering"
  $languageExts = Get-RawrXDExtensions -Category "Languages"
  $result5 = $languageExts.Count -gt 0
  Write-Host "   Result: $(if ($result5) { "✅ PASS ($($languageExts.Count) language extensions)" } else { '❌ FAIL' })" -ForegroundColor $(if ($result5) { 'Green' } else { 'Red' })

  $allPassed = $result1 -and $result2 -and $result3 -and $result4 -and $result5

  Write-Host ""
  Write-Host "🎯 Overall Test Result: $(if ($allPassed) { '✅ ALL TESTS PASSED' } else { '❌ SOME TESTS FAILED' })" -ForegroundColor $(if ($allPassed) { 'Green' } else { 'Red' })

  return $allPassed
}

# Main execution
if ($args.Count -gt 0) {
  switch ($args[0].ToLower()) {
    "demo" { Show-RealMarketplaceDemo }
    "test" { Test-RealMarketplaceIntegration }
    "quick" { Show-QuickRealMarketplace }
    "init" { Initialize-RawrXDMarketplace }
    "refresh" { Initialize-RawrXDMarketplace -ForceRefresh }
    "show" { Show-RawrXDMarketplace }
    "update" { Update-RawrXDMarketplace }
    default {
      Write-Host "Usage: $($MyInvocation.MyCommand.Name) [demo|test|quick|init|refresh|show|update]" -ForegroundColor Yellow
      Write-Host "  demo    - Run full real marketplace demonstration"
      Write-Host "  test    - Run real marketplace integration tests"
      Write-Host "  quick   - Quick demo of real marketplace"
      Write-Host "  init    - Initialize marketplace (use cached if available)"
      Write-Host "  refresh - Force refresh from real marketplace sources"
      Write-Host "  show    - Show current marketplace overview"
      Write-Host "  update  - Update marketplace data from real sources"
    }
  }
} else {
  Show-RealMarketplaceDemo
}
