# Test script for VSCode marketplace commands
$ErrorActionPreference = "Continue"

Write-Host "Testing VSCode Marketplace Integration" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# Dot-source the main script to load functions (use help command which is valid)
Write-Host "Loading RawrXD functions..." -ForegroundColor Gray
$null = & ".\RawrXD.ps1" -CliMode -Command "help" 2>&1 | Out-Null

Write-Host "1. Testing Get-VSCodeMarketplaceExtensions function directly..." -ForegroundColor Yellow
Write-Host ""

# Test the vscode-popular command directly
try {
    $extensions = Get-VSCodeMarketplaceExtensions -PageSize 10

    if ($extensions.Count -gt 0) {
        Write-Host "✅ SUCCESS: Fetched $($extensions.Count) extensions from VSCode Marketplace" -ForegroundColor Green
        Write-Host ""
        Write-Host "Sample extensions:" -ForegroundColor Cyan

        foreach ($ext in ($extensions | Select-Object -First 5)) {
            Write-Host "  📦 $($ext.Name) v$($ext.Version)" -ForegroundColor White
            Write-Host "     By: $($ext.Author)" -ForegroundColor Gray
            Write-Host "     Downloads: $("{0:N0}" -f $ext.Downloads)" -ForegroundColor Gray
            if ($ext.Rating -gt 0) {
                Write-Host "     Rating: ⭐ $($ext.Rating)/5.0" -ForegroundColor Yellow
            }
            Write-Host "     ID: $($ext.Id)" -ForegroundColor DarkGray
            Write-Host ""
        }
    }
    else {
        Write-Host "⚠️ WARNING: No extensions returned from API" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "❌ ERROR: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "2. Testing /vscode-search command..." -ForegroundColor Yellow
Write-Host ""

try {
    $searchResults = Get-VSCodeMarketplaceExtensions -Query "copilot" -PageSize 5

    if ($searchResults.Count -gt 0) {
        Write-Host "✅ SUCCESS: Search returned $($searchResults.Count) results for 'copilot'" -ForegroundColor Green
        Write-Host ""

        foreach ($ext in $searchResults) {
            Write-Host "  📦 $($ext.Name)" -ForegroundColor White
            Write-Host "     $($ext.Description)" -ForegroundColor Gray
            Write-Host "     ID: $($ext.Id)" -ForegroundColor DarkGray
            Write-Host ""
        }
    }
    else {
        Write-Host "⚠️ WARNING: No results found" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "❌ ERROR: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Test Complete!" -ForegroundColor Green
