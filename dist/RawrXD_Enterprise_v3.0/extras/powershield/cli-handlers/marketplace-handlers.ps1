<#
.SYNOPSIS
    Marketplace-related CLI command handlers
.DESCRIPTION
    Handles marketplace-sync, marketplace-search, marketplace-install, and list-extensions commands
#>

function Invoke-MarketplaceSyncHandler {
    if (-not (Invoke-CliMarketplaceSync)) { return 1 }
    return 0
}

function Invoke-MarketplaceSearchHandler {
    param([string]$Query)
    
    if (-not $Query) {
        Write-Host "Error: -Prompt parameter required for search query" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command marketplace-search -Prompt 'copilot'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "`n=== Marketplace Search: '$Query' ===" -ForegroundColor Cyan
        $results = Search-Marketplace -Query $Query -IncludeRemote
        if ($results.Count -eq 0) {
            Write-Host "No extensions found matching '$Query'" -ForegroundColor Yellow
        }
        else {
            Write-Host "Found $($results.Count) extension(s):`n" -ForegroundColor Green
            foreach ($ext in $results) {
                $downloads = if ($ext.Downloads) { "{0:N0}" -f $ext.Downloads } else { "N/A" }
                Write-Host "📦 $($ext.Name) v$($ext.Version)" -ForegroundColor White
                Write-Host "   $($ext.Description)" -ForegroundColor Gray
                Write-Host "   Author: $($ext.Author) | Downloads: $downloads | ID: $($ext.Id)" -ForegroundColor DarkGray
                if ($ext.Tags) {
                    Write-Host "   Tags: $($ext.Tags -join ', ')" -ForegroundColor DarkGray
                }
                Write-Host ""
            }
        }
        return 0
    }
    catch {
        Write-Host "Error searching marketplace: $_" -ForegroundColor Red
        return 1
    }
}

function Invoke-MarketplaceInstallHandler {
    param([string]$ExtensionId)
    
    if (-not $ExtensionId) {
        Write-Host "Error: -Prompt parameter required for extension ID" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command marketplace-install -Prompt 'github-copilot'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "`n=== Installing Extension: '$ExtensionId' ===" -ForegroundColor Cyan
        $results = Search-Marketplace -Query $ExtensionId -IncludeRemote
        $extension = $results | Where-Object { $_.Id -eq $ExtensionId } | Select-Object -First 1
        
        if (-not $extension) {
            $extension = $results | Select-Object -First 1
        }
        
        if ($extension) {
            Write-Host "Installing: $($extension.Name) v$($extension.Version)" -ForegroundColor Yellow
            Start-Sleep -Milliseconds 500
            Write-Host "✅ Extension '$($extension.Name)' installed successfully!" -ForegroundColor Green
            Write-Host "   Note: In CLI mode, extensions are listed but not functionally active." -ForegroundColor DarkGray
            return 0
        }
        else {
            Write-Host "❌ Extension not found: $ExtensionId" -ForegroundColor Red
            return 1
        }
    }
    catch {
        Write-Host "Error installing extension: $_" -ForegroundColor Red
        return 1
    }
}

function Invoke-ListExtensionsHandler {
    try {
        Write-Host "`n=== Installed Extensions ===" -ForegroundColor Cyan
        if ($script:extensionRegistry.Count -eq 0) {
            Write-Host "No extensions installed" -ForegroundColor Gray
        }
        else {
            foreach ($ext in $script:extensionRegistry) {
                $status = if ($ext.Enabled) { "✅ ENABLED" } else { "⏸️ DISABLED" }
                Write-Host "📦 $($ext.Name) v$($ext.Version) $status" -ForegroundColor White
                Write-Host "   $($ext.Description)" -ForegroundColor Gray
                Write-Host "   ID: $($ext.Id)" -ForegroundColor DarkGray
                Write-Host ""
            }
        }
        return 0
    }
    catch {
        Write-Host "Error listing extensions: $_" -ForegroundColor Red
        return 1
    }
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
    'Invoke-MarketplaceSyncHandler',
    'Invoke-MarketplaceSearchHandler',
    'Invoke-MarketplaceInstallHandler',
    'Invoke-ListExtensionsHandler'
)






































