# ============================================
# CLI HANDLER: vscode-popular
# ============================================
# Category: vscode
# Command: vscode-popular
# Purpose: Get top VSCode extensions from live API
# ============================================

function Invoke-CliVSCodePopular {
    <#
    .SYNOPSIS
        Get top VSCode extensions from live API
    .DESCRIPTION
        Fetches popular extensions from the official VSCode Marketplace API.
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command vscode-popular
    .OUTPUTS
        [bool] $true if successful, $false otherwise
    #>
    
    Write-Host "`n=== Top VSCode Extensions (Live API) ===" -ForegroundColor Cyan
    Write-Host "Fetching from official VSCode Marketplace..." -ForegroundColor Gray
    Write-Host ""
    
    try {
        # VSCode Marketplace API
        $apiUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
        
        $body = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 8; value = "Microsoft.VisualStudio.Code" }
                    )
                    pageNumber = 1
                    pageSize = 15
                    sortBy = 4  # Sort by installs
                    sortOrder = 2  # Descending
                }
            )
            assetTypes = @()
            flags = 914
        } | ConvertTo-Json -Depth 5
        
        $headers = @{
            "Content-Type" = "application/json"
            "Accept" = "application/json;api-version=6.0-preview.1"
        }
        
        $response = Invoke-RestMethod -Uri $apiUrl -Method Post -Body $body -Headers $headers -TimeoutSec 30 -ErrorAction Stop
        
        $extensions = $response.results[0].extensions
        
        if ($extensions.Count -eq 0) {
            Write-Host "⚠️ No extensions returned. Check internet connection." -ForegroundColor Yellow
            return $false
        }
        
        Write-Host "Found $($extensions.Count) popular extensions:`n" -ForegroundColor Green
        
        $index = 1
        foreach ($ext in $extensions) {
            $displayName = $ext.displayName
            $publisher = $ext.publisher.publisherName
            $version = ($ext.versions | Select-Object -First 1).version
            
            # Get statistics
            $downloads = 0
            $rating = 0
            foreach ($stat in $ext.statistics) {
                if ($stat.statisticName -eq "install") { $downloads = $stat.value }
                if ($stat.statisticName -eq "averagerating") { $rating = [math]::Round($stat.value, 1) }
            }
            
            $downloadsFormatted = "{0:N0}" -f $downloads
            $ratingStars = if ($rating -gt 0) { "⭐ $rating/5.0" } else { "" }
            
            Write-Host "[$index] 📦 $displayName v$version $ratingStars" -ForegroundColor White
            Write-Host "     $($ext.shortDescription)" -ForegroundColor Gray
            Write-Host "     By: $publisher | Downloads: $downloadsFormatted" -ForegroundColor DarkGray
            Write-Host "     ID: $publisher.$($ext.extensionName)" -ForegroundColor DarkGray
            Write-Host ""
            $index++
        }
        
        return $true
    }
    catch {
        Write-Host "Error fetching VSCode extensions: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliVSCodePopular
}
