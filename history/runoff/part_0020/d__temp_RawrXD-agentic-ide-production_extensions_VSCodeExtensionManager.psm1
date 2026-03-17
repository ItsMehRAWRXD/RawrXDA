# ============================================
# VSCode Extension Manager for RawrXD
# ============================================
# Category: IDE Integration
# Purpose: Load and manage VS Code extensions in RawrXD IDE
# Author: RawrXD
# Version: 1.0.0
# ============================================

# Extension Metadata
$global:VSCodeExtensionManager = @{
    Id           = "vscode-extension-manager"
    Name         = "VSCode Extension Manager"
    Description  = "Load and manage VS Code extensions in RawrXD IDE with full marketplace integration"
    Author       = "RawrXD"
    Version      = "1.0.0"
    Language     = 0  # LANG_CUSTOM
    Capabilities = @("IDE", "Extensions", "Marketplace", "VSCode", "Integration")
    EditorType   = $null
    Dependencies = @()
    Enabled      = $true
}

# Global variables
$global:VSCodeExtensions = @{}
$global:VSCodeMarketplaceUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
$global:VSCodeExtensionsPath = "$PSScriptRoot\vscode-extensions"

# Ensure extensions directory exists
if (!(Test-Path $global:VSCodeExtensionsPath)) {
    New-Item -ItemType Directory -Path $global:VSCodeExtensionsPath -Force | Out-Null
}

# ============================================
# VS CODE EXTENSION MANAGEMENT
# ============================================

function Get-VSCodeExtension {
    <#
    .SYNOPSIS
        Get information about a VS Code extension
    .PARAMETER ExtensionId
        The extension ID (e.g., ms-python.python)
    #>
    param([string]$ExtensionId)
    
    if ($global:VSCodeExtensions.ContainsKey($ExtensionId)) {
        return $global:VSCodeExtensions[$ExtensionId]
    }
    return $null
}

function Install-VSCodeExtension {
    <#
    .SYNOPSIS
        Install a VS Code extension
    .PARAMETER ExtensionId
        The extension ID (e.g., ms-python.python)
    .PARAMETER Version
        Specific version to install (optional)
    #>
    param([string]$ExtensionId, [string]$Version = "latest")
    
    try {
        Write-Host "Installing VS Code extension: $ExtensionId" -ForegroundColor Cyan
        
        # Download extension
        $extensionPath = Download-VSCodeExtension -ExtensionId $ExtensionId -Version $Version
        if (!$extensionPath) {
            throw "Failed to download extension"
        }
        
        # Extract and install
        $installResult = Install-VSCodeExtensionFromFile -ExtensionPath $extensionPath
        if (!$installResult) {
            throw "Failed to install extension"
        }
        
        Write-Host "Successfully installed: $ExtensionId" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "Failed to install VS Code extension: $_"
        return $false
    }
}

function Uninstall-VSCodeExtension {
    <#
    .SYNOPSIS
        Uninstall a VS Code extension
    .PARAMETER ExtensionId
        The extension ID to uninstall
    #>
    param([string]$ExtensionId)
    
    try {
        if ($global:VSCodeExtensions.ContainsKey($ExtensionId)) {
            $extension = $global:VSCodeExtensions[$ExtensionId]
            
            # Remove extension files
            if (Test-Path $extension.InstallPath) {
                Remove-Item -Path $extension.InstallPath -Recurse -Force
            }
            
            # Remove from registry
            $global:VSCodeExtensions.Remove($ExtensionId)
            
            Write-Host "Uninstalled: $ExtensionId" -ForegroundColor Yellow
            return $true
        }
        else {
            Write-Warning "Extension not found: $ExtensionId"
            return $false
        }
    }
    catch {
        Write-Error "Failed to uninstall VS Code extension: $_"
        return $false
    }
}

function Get-InstalledVSCodeExtensions {
    <#
    .SYNOPSIS
        Get list of installed VS Code extensions
    #>
    return $global:VSCodeExtensions.Values | Sort-Object Name
}

# ============================================
# VS CODE MARKETPLACE INTEGRATION
# ============================================

function Search-VSCodeMarketplace {
    <#
    .SYNOPSIS
        Search the VS Code marketplace for extensions
    .PARAMETER Query
        Search query
    .PARAMETER Category
        Category filter (optional)
    .PARAMETER Page
        Page number (default: 1)
    .PARAMETER PageSize
        Results per page (default: 20)
    #>
    param(
        [string]$Query,
        [string]$Category = "",
        [int]$Page = 1,
        [int]$PageSize = 20
    )
    
    try {
        $body = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 8; value = $Query }  # Search text
                    )
                    pageNumber = $Page
                    pageSize = $PageSize
                    sortBy = 0  # Relevance
                    sortOrder = 0  # Descending
                }
            )
            flags = 0x1FF  # Include most extension details
        } | ConvertTo-Json -Depth 10
        
        $response = Invoke-RestMethod -Uri $global:VSCodeMarketplaceUrl -Method Post -Body $body -ContentType "application/json"
        
        return @{
            Success = $true
            Results = $response.results
            TotalCount = $response.totalCount
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
            Results = @()
            TotalCount = 0
        }
    }
}

function Get-VSCodeExtensionDetails {
    <#
    .SYNOPSIS
        Get detailed information about a VS Code extension
    .PARAMETER ExtensionId
        The extension ID
    #>
    param([string]$ExtensionId)
    
    try {
        $body = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 4; value = $ExtensionId }  # Extension ID
                    )
                    pageNumber = 1
                    pageSize = 1
                }
            )
            flags = 0x1FF
        } | ConvertTo-Json -Depth 10
        
        $response = Invoke-RestMethod -Uri $global:VSCodeMarketplaceUrl -Method Post -Body $body -ContentType "application/json"
        
        if ($response.results.Count -gt 0 -and $response.results[0].extensions.Count -gt 0) {
            return @{
                Success = $true
                Extension = $response.results[0].extensions[0]
            }
        }
        else {
            return @{
                Success = $false
                Error = "Extension not found"
            }
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

# ============================================
# EXTENSION LOADING AND INTEGRATION
# ============================================

function Load-VSCodeExtension {
    <#
    .SYNOPSIS
        Load a VS Code extension into the IDE
    .PARAMETER ExtensionId
        The extension ID to load
    #>
    param([string]$ExtensionId)
    
    try {
        $extension = Get-VSCodeExtension -ExtensionId $ExtensionId
        if (!$extension) {
            Write-Warning "Extension not installed: $ExtensionId"
            return $false
        }
        
        # Load extension manifest
        $manifestPath = Join-Path $extension.InstallPath "package.json"
        if (!(Test-Path $manifestPath)) {
            Write-Error "Extension manifest not found: $manifestPath"
            return $false
        }
        
        $manifest = Get-Content $manifestPath | ConvertFrom-Json
        
        # Register extension capabilities
        Register-VSCodeExtensionCapabilities -ExtensionId $ExtensionId -Manifest $manifest
        
        Write-Host "Loaded VS Code extension: $ExtensionId" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "Failed to load VS Code extension: $_"
        return $false
    }
}

function Register-VSCodeExtensionCapabilities {
    <#
    .SYNOPSIS
        Register extension capabilities with the IDE
    #>
    param([string]$ExtensionId, [PSCustomObject]$Manifest)
    
    # Register commands
    if ($Manifest.contributes.commands) {
        foreach ($command in $Manifest.contributes.commands) {
            Register-VSCodeCommand -ExtensionId $ExtensionId -Command $command
        }
    }
    
    # Register languages
    if ($Manifest.contributes.languages) {
        foreach ($language in $Manifest.contributes.languages) {
            Register-VSCodeLanguage -ExtensionId $ExtensionId -Language $language
        }
    }
    
    # Register grammars
    if ($Manifest.contributes.grammars) {
        foreach ($grammar in $Manifest.contributes.grammars) {
            Register-VSCodeGrammar -ExtensionId $ExtensionId -Grammar $grammar
        }
    }
    
    # Register snippets
    if ($Manifest.contributes.snippets) {
        foreach ($snippet in $Manifest.contributes.snippets) {
            Register-VSCodeSnippet -ExtensionId $ExtensionId -Snippet $snippet
        }
    }
}

# ============================================
# HELPER FUNCTIONS
# ============================================

function Download-VSCodeExtension {
    <#
    .SYNOPSIS
        Download a VS Code extension
    #>
    param([string]$ExtensionId, [string]$Version = "latest")
    
    try {
        # Get extension details
        $details = Get-VSCodeExtensionDetails -ExtensionId $ExtensionId
        if (!$details.Success) {
            throw $details.Error
        }
        
        $extension = $details.Extension
        
        # Get download URL
        $downloadUrl = $extension.versions[0].files | Where-Object { $_.assetType -eq "Microsoft.VisualStudio.Services.VSIXPackage" } | Select-Object -First 1
        if (!$downloadUrl) {
            throw "Download URL not found"
        }
        
        # Download extension
        $downloadPath = Join-Path $global:VSCodeExtensionsPath "$ExtensionId.vsix"
        Invoke-WebRequest -Uri $downloadUrl.source -OutFile $downloadPath
        
        return $downloadPath
    }
    catch {
        Write-Error "Failed to download VS Code extension: $_"
        return $null
    }
}

function Install-VSCodeExtensionFromFile {
    <#
    .SYNOPSIS
        Install a VS Code extension from a .vsix file
    #>
    param([string]$ExtensionPath)
    
    try {
        # Extract .vsix file
        $extensionId = [System.IO.Path]::GetFileNameWithoutExtension($ExtensionPath)
        $installPath = Join-Path $global:VSCodeExtensionsPath $extensionId
        
        # Create extraction directory
        if (Test-Path $installPath) {
            Remove-Item -Path $installPath -Recurse -Force
        }
        New-Item -ItemType Directory -Path $installPath -Force | Out-Null
        
        # Extract .vsix (which is a zip file)
        Expand-Archive -Path $ExtensionPath -DestinationPath $installPath -Force
        
        # Register extension
        $global:VSCodeExtensions[$extensionId] = @{
            Id = $extensionId
            Name = $extensionId
            Version = "1.0.0"  # Would parse from manifest
            InstallPath = $installPath
            Enabled = $true
            Loaded = $false
        }
        
        # Clean up .vsix file
        Remove-Item -Path $ExtensionPath -Force
        
        return $true
    }
    catch {
        Write-Error "Failed to install VS Code extension from file: $_"
        return $false
    }
}

function Register-VSCodeCommand {
    <#
    .SYNOPSIS
        Register a VS Code command with the IDE
    #>
    param([string]$ExtensionId, [PSCustomObject]$Command)
    
    # Register command with IDE command system
    Write-Host "Registering command: $($Command.command) from $ExtensionId" -ForegroundColor Blue
}

function Register-VSCodeLanguage {
    <#
    .SYNOPSIS
        Register a VS Code language with the IDE
    #>
    param([string]$ExtensionId, [PSCustomObject]$Language)
    
    # Register language support
    Write-Host "Registering language: $($Language.id) from $ExtensionId" -ForegroundColor Blue
}

function Register-VSCodeGrammar {
    <#
    .SYNOPSIS
        Register a VS Code grammar with the IDE
    #>
    param([string]$ExtensionId, [PSCustomObject]$Grammar)
    
    # Register syntax highlighting
    Write-Host "Registering grammar: $($Grammar.scopeName) from $ExtensionId" -ForegroundColor Blue
}

function Register-VSCodeSnippet {
    <#
    .SYNOPSIS
        Register a VS Code snippet with the IDE
    #>
    param([string]$ExtensionId, [PSCustomObject]$Snippet)
    
    # Register code snippets
    Write-Host "Registering snippets from: $ExtensionId" -ForegroundColor Blue
}

# ============================================
# INITIALIZATION
# ============================================

function Initialize-VSCodeExtensionManager {
    <#
    .SYNOPSIS
        Initialize the VS Code extension manager
    #>
    Write-Host "Initializing VS Code Extension Manager..." -ForegroundColor Cyan
    
    # Load any previously installed extensions
    $extensionDirs = Get-ChildItem -Path $global:VSCodeExtensionsPath -Directory
    foreach ($dir in $extensionDirs) {
        $extensionId = $dir.Name
        $manifestPath = Join-Path $dir.FullName "package.json"
        
        if (Test-Path $manifestPath) {
            $global:VSCodeExtensions[$extensionId] = @{
                Id = $extensionId
                Name = $extensionId
                InstallPath = $dir.FullName
                Enabled = $true
                Loaded = $false
            }
        }
    }
    
    Write-Host "VS Code Extension Manager initialized with $($global:VSCodeExtensions.Count) extensions" -ForegroundColor Green
}

# Auto-initialize on module import
Initialize-VSCodeExtensionManager

# Export functions
Export-ModuleMember -Function *-VSCode*
Export-ModuleMember -Variable VSCodeExtensionManager