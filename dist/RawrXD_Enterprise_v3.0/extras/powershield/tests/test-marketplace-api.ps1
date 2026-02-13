# Standalone test for VSCode Marketplace API
$ErrorActionPreference = "Continue"

Write-Host "Testing VSCode Marketplace API Integration" -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host ""

function Test-VSCodeMarketplaceAPI {
    param(
        [string]$Query = "",
        [int]$PageSize = 10
    )

    try {
        Write-Host "📡 Connecting to VSCode Marketplace API..." -ForegroundColor Yellow

        # VSCode Marketplace API endpoint
        $apiUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"

        # Build the request body
        $requestBody = @{
            filters = @(
                @{
                    criteria = @(
                        @{
                            filterType = 8  # Extension name
                            value      = if ($Query) { $Query } else { "" }
                        }
                        @{
                            filterType = 10  # Target platform
                            value      = "Microsoft.VisualStudio.Code"
                        }
                    )
                    pageNumber = 1
                    pageSize   = $PageSize
                    sortBy     = 4  # Downloads
                    sortOrder  = 2  # Descending
                }
            )
            # Flags: Include versions (0x1) + Include files (0x2) + Include statistics (0x4) + Include latest version only (0x80) + Include version properties (0x100)
            flags = 0x187  # 1 + 2 + 4 + 128 + 256 = 391
        } | ConvertTo-Json -Depth 10

        # Make the API request
        $headers = @{
            "Content-Type" = "application/json"
            "Accept"       = "application/json;api-version=7.1-preview.1"
            "User-Agent"   = "RawrXD-Editor/1.0"
        }

        $webResponse = Invoke-WebRequest -Uri $apiUrl -Method Post -Body $requestBody -Headers $headers -TimeoutSec 15 -UseBasicParsing
        $response = $webResponse.Content | ConvertFrom-Json

        Write-Host "   Response received. Checking results..." -ForegroundColor Gray
        Write-Host "   Content length: $($webResponse.Content.Length) bytes" -ForegroundColor Gray

        if ($response.results) {
            Write-Host "   Found $($response.results.Count) result groups" -ForegroundColor Gray
            if ($response.results[0].extensions) {
                Write-Host "   Found $($response.results[0].extensions.Count) extensions in first group" -ForegroundColor Gray
            }
            else {
                Write-Host "   No extensions in results[0]" -ForegroundColor Yellow
                Write-Host "   Response structure: $($response | ConvertTo-Json -Depth 2)" -ForegroundColor DarkGray
            }
        }
        else {
            Write-Host "   No results in response" -ForegroundColor Yellow
        }

        $extensions = @()

        if ($response.results -and $response.results[0].extensions) {
            foreach ($ext in $response.results[0].extensions) {
                $publisher = $ext.publisher.publisherName
                $extName = $ext.extensionName
                $displayName = $ext.displayName
                $shortDesc = $ext.shortDescription
                $version = if ($ext.versions -and $ext.versions[0]) { $ext.versions[0].version } else { "1.0.0" }

                # Get statistics
                $downloads = 0
                $rating = 0
                if ($ext.statistics) {
                    $downloadStat = $ext.statistics | Where-Object { $_.statisticName -eq "install" } | Select-Object -First 1
                    if ($downloadStat) { $downloads = [int]$downloadStat.value }

                    $ratingStat = $ext.statistics | Where-Object { $_.statisticName -eq "averagerating" } | Select-Object -First 1
                    if ($ratingStat) { $rating = [math]::Round([double]$ratingStat.value, 1) }
                }

                $category = if ($ext.categories -and $ext.categories[0]) { $ext.categories[0] } else { "Other" }

                $extensions += [PSCustomObject]@{
                    Id          = "$publisher.$extName"
                    Name        = $displayName
                    Description = $shortDesc
                    Author      = $publisher
                    Version     = $version
                    Downloads   = $downloads
                    Rating      = $rating
                    Category    = $category
                }
            }
        }

        return $extensions
    }
    catch {
        Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
        return @()
    }
}

# Test 1: Get popular extensions
Write-Host "Test 1: Fetching top 10 popular VSCode extensions..." -ForegroundColor Cyan
$popular = Test-VSCodeMarketplaceAPI -PageSize 10

if ($popular.Count -gt 0) {
    Write-Host "✅ SUCCESS: Fetched $($popular.Count) extensions" -ForegroundColor Green
    Write-Host ""

    $index = 1
    foreach ($ext in $popular) {
        Write-Host "[$index] 📦 $($ext.Name) v$($ext.Version)" -ForegroundColor White
        Write-Host "     $($ext.Description)" -ForegroundColor Gray
        Write-Host "     By: $($ext.Author) | Downloads: $("{0:N0}" -f $ext.Downloads) | Rating: ⭐ $($ext.Rating)/5.0" -ForegroundColor DarkGray
        Write-Host "     ID: $($ext.Id)" -ForegroundColor DarkGray
        Write-Host ""
        $index++
    }
}
else {
    Write-Host "⚠️ No extensions returned" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Test 2: Searching for 'copilot'..." -ForegroundColor Cyan
$searchResults = Test-VSCodeMarketplaceAPI -Query "copilot" -PageSize 5

if ($searchResults.Count -gt 0) {
    Write-Host "✅ SUCCESS: Found $($searchResults.Count) extensions matching 'copilot'" -ForegroundColor Green
    Write-Host ""

    foreach ($ext in $searchResults) {
        Write-Host "📦 $($ext.Name)" -ForegroundColor White
        Write-Host "   $($ext.Description)" -ForegroundColor Gray
        Write-Host "   ID: $($ext.Id) | Downloads: $("{0:N0}" -f $ext.Downloads)" -ForegroundColor DarkGray
        Write-Host ""
    }
}
else {
    Write-Host "⚠️ No results found" -ForegroundColor Yellow
}

Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "✅ VSCode Marketplace API Integration Test Complete!" -ForegroundColor Green
