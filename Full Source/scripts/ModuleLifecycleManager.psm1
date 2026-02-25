# Module Lifecycle Manager for IDE Modules
# Supports install, uninstall, enable, disable and inventory

param()

$script:ModuleRoot = Join-Path $PSScriptRoot "modules"

if (-not (Test-Path $script:ModuleRoot)) {
    New-Item -Path $script:ModuleRoot -ItemType Directory -Force | Out-Null
}

function Get-ModuleInventory {
    <#
    .SYNOPSIS
    Lists available modules in the local module store
    #>
    [CmdletBinding()]
    param()

    Get-ChildItem -Path $script:ModuleRoot -Filter "*.psm1" | ForEach-Object {
        [PSCustomObject]@{
            Name = $_.BaseName
            Path = $_.FullName
            LastWriteTime = $_.LastWriteTime
        }
    }
}

function Install-LocalModule {
    <#
    .SYNOPSIS
    Installs a module into the local module store
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,
        [string]$ModuleName
    )

    if (-not (Test-Path $SourcePath)) {
        throw "Source path not found: $SourcePath"
    }

    if (-not $ModuleName) {
        $ModuleName = [System.IO.Path]::GetFileNameWithoutExtension($SourcePath)
    }

    $destination = Join-Path $script:ModuleRoot "$ModuleName.psm1"
    Copy-Item -Path $SourcePath -Destination $destination -Force

    return $destination
}

function Uninstall-LocalModule {
    <#
    .SYNOPSIS
    Removes a module from the local module store
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleName
    )

    $path = Join-Path $script:ModuleRoot "$ModuleName.psm1"
    if (-not (Test-Path $path)) {
        throw "Module not found: $ModuleName"
    }

    Remove-Item -Path $path -Force
}

function Enable-LocalModule {
    <#
    .SYNOPSIS
    Imports a local module into the current session
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleName
    )

    $path = Join-Path $script:ModuleRoot "$ModuleName.psm1"
    if (-not (Test-Path $path)) {
        throw "Module not found: $ModuleName"
    }

    Import-Module -Path $path -Force -DisableNameChecking
}

function Disable-LocalModule {
    <#
    .SYNOPSIS
    Removes a module from the current session
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleName
    )

    Remove-Module -Name $ModuleName -Force -ErrorAction SilentlyContinue
}

Export-ModuleMember -Function @(
    'Get-ModuleInventory',
    'Install-LocalModule',
    'Uninstall-LocalModule',
    'Enable-LocalModule',
    'Disable-LocalModule'
)
