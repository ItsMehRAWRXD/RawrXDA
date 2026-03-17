# ============================================
# CLI HANDLER: vscode-search
# ============================================
# Category: vscode
# Command: vscode-search
# Purpose: Search VSCode Marketplace
# ============================================

function Invoke-CliVSCodeSearch {
    <#
    .SYNOPSIS
        Search VSCode Marketplace
    .DESCRIPTION
        Search for extensions in the official VSCode Marketplace API.
    .PARAMETER Prompt
        Search query string
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt 'python'
    .OUTPUTS
        [bool] $true if successful, $false otherwise
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Prompt
    )
    
    Write-Host "`n=== VSCode Marketplace Search: '$Prompt' (Live API) ===" -ForegroundColor Cyan
    Write-Host "Querying official VSCode Marketplace..." -ForegroundColor Gray
    Write-Host ""
    
    try {
        # VSCode Marketplace API
        $apiUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
        
        $body = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 8; value = "Microsoft.VisualStudio.Code" }
                        @{ filterType = 10; value = $Prompt }
                    )
                    pageNumber = 1
                    pageSize = 10
                    sortBy = 0  # Sort by relevance
                    sortOrder = 2
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
            Write-Host "No extensions found matching '$Prompt'" -ForegroundColor Yellow
            Write-Host "Try a different search term or use 'vscode-popular' for trending extensions" -ForegroundColor Gray
            return $true
        }
        
        Write-Host "Found $($extensions.Count) extension(s):`n" -ForegroundColor Green
        
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
            
            Write-Host "📦 $displayName v$version $ratingStars" -ForegroundColor White
            Write-Host "   $($ext.shortDescription)" -ForegroundColor Gray
            Write-Host "   By: $publisher | Downloads: $downloadsFormatted | ID: $publisher.$($ext.extensionName)" -ForegroundColor DarkGray
            
            # Show tags if available
            if ($ext.tags -and $ext.tags.Count -gt 0) {
                $tagStr = ($ext.tags | Select-Object -First 5) -join ', '
                Write-Host "   Tags: $tagStr" -ForegroundColor DarkGray
            }
            Write-Host ""
        }
        
        return $true
    }
    catch {
        Write-Host "Error searching VSCode Marketplace: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliVSCodeSearch
}
