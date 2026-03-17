#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Complete Extension & Plugin Management System
    
.DESCRIPTION
    Unified system for creating, installing, enabling/disabling IDE extensions and plugins.
    Integrates plugin_craft_room (creator) with ModuleLifecycleManager (lifecycle).
    
.EXAMPLE
    Import-Module .\ExtensionManager.psm1
    Show-ExtensionMenu
#>

param()

$script:ExtensionRoot = "d:\lazy init ide\extensions"
$script:PluginCraftRoom = "d:\lazy init ide\craft_room"
$script:ModuleStore = "d:\lazy init ide\scripts\modules"

# Ensure directories exist
@($script:ExtensionRoot, $script:PluginCraftRoom, $script:ModuleStore) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -Path $_ -ItemType Directory -Force | Out-Null }
}

# Import dependencies
$pluginCraftPath = Join-Path (Split-Path $PSScriptRoot -Parent) "scripts\plugin_craft_room.psm1"
$lifecyclePath = Join-Path (Split-Path $PSScriptRoot -Parent) "scripts\ModuleLifecycleManager.psm1"

if (Test-Path $pluginCraftPath) {
    Import-Module $pluginCraftPath -Force -DisableNameChecking
}
if (Test-Path $lifecyclePath) {
    Import-Module $lifecyclePath -Force -DisableNameChecking
}

# ============================================================================
# EXTENSION REGISTRY
# ============================================================================

$script:ExtensionRegistry = @{}
$script:RegistryPath = Join-Path $script:ExtensionRoot "registry.json"

function Load-ExtensionRegistry {
    if (Test-Path $script:RegistryPath) {
        try {
            $content = Get-Content $script:RegistryPath -Raw | ConvertFrom-Json
            $script:ExtensionRegistry = @{}
            foreach ($prop in $content.PSObject.Properties) {
                $script:ExtensionRegistry[$prop.Name] = $prop.Value
            }
        } catch {
            Write-Warning "Failed to load extension registry: $_"
        }
    }
}

function Save-ExtensionRegistry {
    $script:ExtensionRegistry | ConvertTo-Json -Depth 10 | Set-Content $script:RegistryPath
}

# ============================================================================
# EXTENSION CREATION
# ============================================================================

function New-Extension {
    <#
    .SYNOPSIS
        Create a new IDE extension/plugin
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,
        
        [Parameter(Mandatory=$true)]
        [ValidateSet('Translator','Analyzer','Connector','Generator','Processor','Custom')]
        [string]$Type,
        
        [hashtable]$Parameters = @{},
        
        [switch]$AutoInstall,
        [switch]$AutoEnable
    )
    
    Write-Host "🎨 Creating extension: $Name ($Type)" -ForegroundColor Cyan
    
    # Use plugin craft room to generate the extension
    $pluginPath = Join-Path $script:PluginCraftRoom "$Name.psm1"
    
    $template = switch ($Type) {
        'Translator' { Get-TranslatorTemplate -Languages $Parameters.Languages -PluginName $Name }
        'Analyzer' { Get-AnalyzerTemplate -AnalysisType $Parameters.AnalysisType -PluginName $Name }
        'Connector' { Get-ConnectorTemplate -ServiceName $Parameters.ServiceName -Endpoint $Parameters.Endpoint -PluginName $Name }
        'Generator' { Get-GeneratorTemplate -OutputType $Parameters.OutputType -PluginName $Name }
        'Processor' { Get-ProcessorTemplate -ProcessType $Parameters.ProcessType -PluginName $Name }
        'Custom' { Get-CustomTemplate -PluginName $Name }
    }
    
    Set-Content -Path $pluginPath -Value $template
    
    # Register extension
    $script:ExtensionRegistry[$Name] = @{
        Type = $Type
        Path = $pluginPath
        Created = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        Enabled = $false
        Installed = $false
        Parameters = $Parameters
    }
    Save-ExtensionRegistry
    
    Write-Host "✓ Extension created: $pluginPath" -ForegroundColor Green
    
    if ($AutoInstall) {
        Install-Extension -Name $Name
    }
    
    if ($AutoEnable) {
        Enable-Extension -Name $Name
    }
    
    return $pluginPath
}

function Get-CustomTemplate {
    param([string]$PluginName)
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Custom Extension
#>

param()

function Invoke-$PluginName {
    Write-Host "Running $PluginName..." -ForegroundColor Cyan
    # Add your custom logic here
}

Export-ModuleMember -Function @(
    'Invoke-$PluginName'
)
"@
}

# ============================================================================
# EXTENSION LIFECYCLE
# ============================================================================

function Install-Extension {
    <#
    .SYNOPSIS
        Install an extension to the module store
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )
    
    if (-not $script:ExtensionRegistry.ContainsKey($Name)) {
        throw "Extension '$Name' not found in registry"
    }
    
    $ext = $script:ExtensionRegistry[$Name]
    
    Write-Host "📦 Installing extension: $Name" -ForegroundColor Cyan
    
    # Use ModuleLifecycleManager to install
    if (Get-Command Install-LocalModule -ErrorAction SilentlyContinue) {
        $installedPath = Install-LocalModule -SourcePath $ext.Path -ModuleName $Name
        $ext.Installed = $true
        $ext.InstalledPath = $installedPath
        Save-ExtensionRegistry
        Write-Host "✓ Installed to: $installedPath" -ForegroundColor Green
    } else {
        # Fallback: copy manually
        $destination = Join-Path $script:ModuleStore "$Name.psm1"
        Copy-Item -Path $ext.Path -Destination $destination -Force
        $ext.Installed = $true
        $ext.InstalledPath = $destination
        Save-ExtensionRegistry
        Write-Host "✓ Installed to: $destination" -ForegroundColor Green
    }
}

function Uninstall-Extension {
    <#
    .SYNOPSIS
        Uninstall an extension from the module store
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )
    
    if (-not $script:ExtensionRegistry.ContainsKey($Name)) {
        throw "Extension '$Name' not found in registry"
    }
    
    $ext = $script:ExtensionRegistry[$Name]
    
    Write-Host "🗑️ Uninstalling extension: $Name" -ForegroundColor Yellow
    
    if ($ext.Enabled) {
        Disable-Extension -Name $Name
    }
    
    if (Get-Command Uninstall-LocalModule -ErrorAction SilentlyContinue) {
        Uninstall-LocalModule -ModuleName $Name
    } else {
        if ($ext.InstalledPath -and (Test-Path $ext.InstalledPath)) {
            Remove-Item -Path $ext.InstalledPath -Force
        }
    }
    
    $ext.Installed = $false
    $ext.InstalledPath = $null
    Save-ExtensionRegistry
    
    Write-Host "✓ Uninstalled: $Name" -ForegroundColor Green
}

function Enable-Extension {
    <#
    .SYNOPSIS
        Enable (activate) an extension
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )
    
    if (-not $script:ExtensionRegistry.ContainsKey($Name)) {
        throw "Extension '$Name' not found in registry"
    }
    
    $ext = $script:ExtensionRegistry[$Name]
    
    if (-not $ext.Installed) {
        Write-Host "Extension not installed. Installing first..." -ForegroundColor Yellow
        Install-Extension -Name $Name
    }
    
    Write-Host "🔌 Enabling extension: $Name" -ForegroundColor Cyan
    
    if (Get-Command Enable-LocalModule -ErrorAction SilentlyContinue) {
        Enable-LocalModule -ModuleName $Name
    } else {
        # Fallback: import directly
        if ($ext.InstalledPath -and (Test-Path $ext.InstalledPath)) {
            Import-Module -Path $ext.InstalledPath -Force -DisableNameChecking
        }
    }
    
    $ext.Enabled = $true
    $ext.LastEnabled = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    Save-ExtensionRegistry
    
    Write-Host "✓ Enabled: $Name" -ForegroundColor Green
}

function Disable-Extension {
    <#
    .SYNOPSIS
        Disable (deactivate) an extension
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )
    
    if (-not $script:ExtensionRegistry.ContainsKey($Name)) {
        throw "Extension '$Name' not found in registry"
    }
    
    Write-Host "⏸️ Disabling extension: $Name" -ForegroundColor Yellow
    
    if (Get-Command Disable-LocalModule -ErrorAction SilentlyContinue) {
        Disable-LocalModule -ModuleName $Name
    } else {
        Remove-Module -Name $Name -Force -ErrorAction SilentlyContinue
    }
    
    $ext = $script:ExtensionRegistry[$Name]
    $ext.Enabled = $false
    Save-ExtensionRegistry
    
    Write-Host "✓ Disabled: $Name" -ForegroundColor Green
}

function Remove-Extension {
    <#
    .SYNOPSIS
        Completely remove an extension (uninstall + delete source)
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,
        
        [switch]$Force
    )
    
    if (-not $script:ExtensionRegistry.ContainsKey($Name)) {
        throw "Extension '$Name' not found in registry"
    }
    
    $ext = $script:ExtensionRegistry[$Name]
    
    if (-not $Force) {
        $confirm = Read-Host "Are you sure you want to remove extension '$Name'? (y/N)"
        if ($confirm -ne 'y') {
            Write-Host "Cancelled" -ForegroundColor Yellow
            return
        }
    }
    
    Write-Host "🗑️ Removing extension: $Name" -ForegroundColor Red
    
    # Uninstall if installed
    if ($ext.Installed) {
        Uninstall-Extension -Name $Name
    }
    
    # Delete source
    if ($ext.Path -and (Test-Path $ext.Path)) {
        Remove-Item -Path $ext.Path -Force
    }
    
    # Remove from registry
    $script:ExtensionRegistry.Remove($Name)
    Save-ExtensionRegistry
    
    Write-Host "✓ Removed: $Name" -ForegroundColor Green
}

function Get-Extension {
    <#
    .SYNOPSIS
        List all extensions
    #>
    [CmdletBinding()]
    param(
        [string]$Name,
        [switch]$EnabledOnly,
        [switch]$InstalledOnly
    )
    
    $extensions = $script:ExtensionRegistry.GetEnumerator()
    
    if ($Name) {
        $extensions = $extensions | Where-Object { $_.Key -like "*$Name*" }
    }
    
    if ($EnabledOnly) {
        $extensions = $extensions | Where-Object { $_.Value.Enabled -eq $true }
    }
    
    if ($InstalledOnly) {
        $extensions = $extensions | Where-Object { $_.Value.Installed -eq $true }
    }
    
    $extensions | ForEach-Object {
        [PSCustomObject]@{
            Name = $_.Key
            Type = $_.Value.Type
            Installed = $_.Value.Installed
            Enabled = $_.Value.Enabled
            Created = $_.Value.Created
            Path = $_.Value.Path
        }
    }
}

# ============================================================================
# INTERACTIVE MENU
# ============================================================================

function Show-ExtensionMenu {
    <#
    .SYNOPSIS
        Interactive extension management menu
    #>
    
    Load-ExtensionRegistry
    
    while ($true) {
        Clear-Host
        Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║         IDE EXTENSION & PLUGIN MANAGER                       ║" -ForegroundColor Cyan
        Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        Write-Host ""
        
        # Show current extensions
        $exts = Get-Extension
        if ($exts) {
            Write-Host "Current Extensions:" -ForegroundColor White
            foreach ($ext in $exts) {
                $statusIcon = if ($ext.Enabled) { "🟢" } elseif ($ext.Installed) { "🟡" } else { "⚪" }
                $statusText = if ($ext.Enabled) { "ENABLED" } elseif ($ext.Installed) { "INSTALLED" } else { "CREATED" }
                Write-Host "  $statusIcon [$statusText] " -NoNewline
                Write-Host "$($ext.Name) " -NoNewline -ForegroundColor Cyan
                Write-Host "($($ext.Type))" -ForegroundColor Gray
            }
        } else {
            Write-Host "  No extensions yet" -ForegroundColor Gray
        }
        
        Write-Host ""
        Write-Host "Actions:" -ForegroundColor White
        Write-Host "  [1] Create new extension"
        Write-Host "  [2] Install extension"
        Write-Host "  [3] Enable extension"
        Write-Host "  [4] Disable extension"
        Write-Host "  [5] Uninstall extension"
        Write-Host "  [6] Remove extension (delete)"
        Write-Host "  [7] List all extensions"
        Write-Host "  [Q] Quit"
        Write-Host ""
        
        $choice = Read-Host "Select action"
        
        switch ($choice) {
            '1' {
                Write-Host ""
                $name = Read-Host "Extension name"
                Write-Host "Types: 1=Translator, 2=Analyzer, 3=Connector, 4=Generator, 5=Processor, 6=Custom"
                $typeChoice = Read-Host "Select type"
                $type = @('Translator','Analyzer','Connector','Generator','Processor','Custom')[[int]$typeChoice - 1]
                
                $params = @{}
                switch ($type) {
                    'Translator' { $params.Languages = @(Read-Host "Languages (comma-separated)").Split(',').Trim() }
                    'Analyzer' { $params.AnalysisType = Read-Host "Analysis type" }
                    'Connector' { 
                        $params.ServiceName = Read-Host "Service name"
                        $params.Endpoint = Read-Host "Endpoint URL"
                    }
                    'Generator' { $params.OutputType = Read-Host "Output type" }
                    'Processor' { $params.ProcessType = Read-Host "Process type" }
                }
                
                New-Extension -Name $name -Type $type -Parameters $params
                Read-Host "Press Enter to continue"
            }
            '2' {
                $name = Read-Host "Extension name to install"
                if ($name) { Install-Extension -Name $name }
                Read-Host "Press Enter to continue"
            }
            '3' {
                $name = Read-Host "Extension name to enable"
                if ($name) { Enable-Extension -Name $name }
                Read-Host "Press Enter to continue"
            }
            '4' {
                $name = Read-Host "Extension name to disable"
                if ($name) { Disable-Extension -Name $name }
                Read-Host "Press Enter to continue"
            }
            '5' {
                $name = Read-Host "Extension name to uninstall"
                if ($name) { Uninstall-Extension -Name $name }
                Read-Host "Press Enter to continue"
            }
            '6' {
                $name = Read-Host "Extension name to remove"
                if ($name) { Remove-Extension -Name $name }
                Read-Host "Press Enter to continue"
            }
            '7' {
                Get-Extension | Format-Table -AutoSize
                Read-Host "Press Enter to continue"
            }
            'Q' { return }
            'q' { return }
        }
    }
}

# Load registry on import
Load-ExtensionRegistry

Export-ModuleMember -Function @(
    'New-Extension',
    'Install-Extension',
    'Uninstall-Extension',
    'Enable-Extension',
    'Disable-Extension',
    'Remove-Extension',
    'Get-Extension',
    'Show-ExtensionMenu'
)
