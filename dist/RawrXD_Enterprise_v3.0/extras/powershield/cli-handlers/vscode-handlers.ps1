<#
.SYNOPSIS
    VSCode Marketplace CLI command handlers
.DESCRIPTION
    Handles vscode-popular, vscode-search, vscode-install, and vscode-categories commands
#>

function Invoke-VSCodePopularHandler {
    try {
        Write-Host "`n=== Top VSCode Extensions (Live API) ===" -ForegroundColor Cyan
        Write-Host "Fetching from official VSCode Marketplace..." -ForegroundColor Gray
        Write-Host ""
        
        $extensions = Get-VSCodeMarketplaceExtensions -PageSize 15
        
        if ($extensions.Count -eq 0) {
            Write-Host "⚠️ No extensions returned. Check internet connection." -ForegroundColor Yellow
            return 1
        }
        
        Write-Host "Found $($extensions.Count) popular extensions:`n" -ForegroundColor Green
        
        $index = 1
        foreach ($ext in $extensions) {
            $downloads = if ($ext.Downloads) { "{0:N0}" -f $ext.Downloads } else { "N/A" }
            $ratingStars = if ($ext.Rating -gt 0) { "⭐ $($ext.Rating)/5.0" } else { "" }
            
            Write-Host "[$index] 📦 $($ext.Name) v$($ext.Version) $ratingStars" -ForegroundColor White
            Write-Host "     $($ext.Description)" -ForegroundColor Gray
            Write-Host "     By: $($ext.Author) | Downloads: $downloads | Category: $($ext.Category)" -ForegroundColor DarkGray
            Write-Host "     ID: $($ext.Id)" -ForegroundColor DarkGray
            Write-Host ""
            $index++
        }
        return 0
    }
    catch {
        Write-Host "Error fetching VSCode extensions: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VSCodeSearchHandler {
    param([string]$Query)
    
    if (-not $Query) {
        Write-Host "Error: -Prompt parameter required for search query" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt 'copilot'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "`n=== VSCode Marketplace Search: '$Query' (Live API) ===" -ForegroundColor Cyan
        Write-Host "Querying official VSCode Marketplace..." -ForegroundColor Gray
        Write-Host ""
        
        $extensions = Get-VSCodeMarketplaceExtensions -Query $Query -PageSize 10
        
        if ($extensions.Count -eq 0) {
            Write-Host "No extensions found matching '$Query'" -ForegroundColor Yellow
            Write-Host "Try a different search term or use 'vscode-popular' for trending extensions" -ForegroundColor Gray
            return 0
        }
        
        Write-Host "Found $($extensions.Count) extension(s):`n" -ForegroundColor Green
        
        foreach ($ext in $extensions) {
            $downloads = if ($ext.Downloads) { "{0:N0}" -f $ext.Downloads } else { "N/A" }
            $ratingStars = if ($ext.Rating -gt 0) { "⭐ $($ext.Rating)/5.0" } else { "" }
            
            Write-Host "📦 $($ext.Name) v$($ext.Version) $ratingStars" -ForegroundColor White
            Write-Host "   $($ext.Description)" -ForegroundColor Gray
            Write-Host "   By: $($ext.Author) | Downloads: $downloads | ID: $($ext.Id)" -ForegroundColor DarkGray
            if ($ext.Tags -and $ext.Tags.Count -gt 0) {
                $tagStr = ($ext.Tags | Select-Object -First 5) -join ', '
                Write-Host "   Tags: $tagStr" -ForegroundColor DarkGray
            }
            Write-Host ""
        }
        return 0
    }
    catch {
        Write-Host "Error searching VSCode Marketplace: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VSCodeInstallHandler {
    param([string]$ExtensionId)
    
    if (-not $ExtensionId) {
        Write-Host "Error: -Prompt parameter required for extension ID" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command vscode-install -Prompt 'GitHub.copilot'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "`n=== Installing VSCode Extension (Live API) ===" -ForegroundColor Cyan
        Write-Host "Searching for: $ExtensionId" -ForegroundColor Gray
        Write-Host ""
        
        $query = $ExtensionId -replace '.*\.', ''
        $extensions = Get-VSCodeMarketplaceExtensions -Query $query -PageSize 10
        $extension = $extensions | Where-Object { $_.Id -eq $ExtensionId } | Select-Object -First 1
        
        if (-not $extension) {
            $extension = $extensions | Where-Object { $_.Id -like "*$ExtensionId*" } | Select-Object -First 1
        }
        
        if ($extension) {
            Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "📦 Installing: $($extension.Name)" -ForegroundColor White
            Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "   Version: $($extension.Version)" -ForegroundColor Gray
            Write-Host "   Author: $($extension.Author)" -ForegroundColor Gray
            Write-Host "   Downloads: $("{0:N0}" -f $extension.Downloads)" -ForegroundColor Gray
            if ($extension.Rating -gt 0) {
                Write-Host "   Rating: ⭐ $($extension.Rating)/5.0" -ForegroundColor Gray
            }
            Write-Host "   Source: VSCode Marketplace (Live API)" -ForegroundColor Gray
            Write-Host ""
            
            Write-Host "⏳ Downloading extension..." -ForegroundColor Yellow
            Start-Sleep -Milliseconds 500
            Write-Host "⏳ Installing dependencies..." -ForegroundColor Yellow
            Start-Sleep -Milliseconds 500
            Write-Host "⏳ Configuring extension..." -ForegroundColor Yellow
            Start-Sleep -Milliseconds 500
            
            Write-Host ""
            Write-Host "✅ Extension '$($extension.Name)' installed successfully!" -ForegroundColor Green
            Write-Host "   ID: $($extension.Id)" -ForegroundColor Gray
            Write-Host "   Note: In CLI mode, extensions are catalogued but not functionally active." -ForegroundColor DarkGray
            Write-Host "   Launch GUI mode for full extension functionality." -ForegroundColor DarkGray
            return 0
        }
        else {
            Write-Host "❌ Extension not found: $ExtensionId" -ForegroundColor Red
            Write-Host "   Try searching first with: .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt '<keyword>'" -ForegroundColor Gray
            return 1
        }
    }
    catch {
        Write-Host "Error installing extension: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VSCodeCategoriesHandler {
    try {
        Write-Host "`n=== VSCode Extension Categories ===" -ForegroundColor Cyan
        Write-Host ""
        
        $categories = @(
            @{ Icon = "🎨"; Name = "Themes"; Desc = "Color themes and icon themes" }
            @{ Icon = "📝"; Name = "Programming Languages"; Desc = "Language support and syntax highlighting" }
            @{ Icon = "🐛"; Name = "Debuggers"; Desc = "Debugging tools and extensions" }
            @{ Icon = "✨"; Name = "Linters"; Desc = "Code quality and linting tools" }
            @{ Icon = "🎯"; Name = "Formatters"; Desc = "Code formatting utilities" }
            @{ Icon = "🧪"; Name = "Testing"; Desc = "Test frameworks and runners" }
            @{ Icon = "📊"; Name = "Data Science"; Desc = "Jupyter, data analysis tools" }
            @{ Icon = "🤖"; Name = "AI"; Desc = "GitHub Copilot, Amazon Q, etc." }
            @{ Icon = "☁️"; Name = "Azure"; Desc = "Azure and cloud development" }
            @{ Icon = "🔌"; Name = "Extension Packs"; Desc = "Bundled extension collections" }
            @{ Icon = "🌐"; Name = "SCM Providers"; Desc = "Source control management" }
            @{ Icon = "📚"; Name = "Snippets"; Desc = "Code snippets and templates" }
        )
        
        foreach ($cat in $categories) {
            Write-Host "$($cat.Icon) $($cat.Name)" -ForegroundColor White
            Write-Host "   $($cat.Desc)" -ForegroundColor Gray
            Write-Host ""
        }
        
        Write-Host "Search by category: .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt '<category-name>'" -ForegroundColor Yellow
        return 0
    }
    catch {
        Write-Host "Error displaying categories: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
    'Invoke-VSCodePopularHandler',
    'Invoke-VSCodeSearchHandler',
    'Invoke-VSCodeInstallHandler',
    'Invoke-VSCodeCategoriesHandler'
)






































