# ============================================
# VS Code Extension Manager for RawrXD IDE
# ============================================
# Provides PowerShell cmdlets to search, install, uninstall, and load
# VS Code extensions (.vsix) for the RawrXD IDE. Uses the official
# VS Marketplace public gallery API.
# ============================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------- Configuration ----------
$script:ExtensionRoot   = Join-Path $PSScriptRoot 'vscode-extensions'
$script:RegistryPath    = Join-Path $script:ExtensionRoot 'extensions.json'
$script:MarketplaceUri  = 'https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery'
$script:DefaultPageSize = 20

if (-not (Test-Path $script:ExtensionRoot)) {
    New-Item -ItemType Directory -Path $script:ExtensionRoot -Force | Out-Null
}

# ---------- Logging & Timing ----------
function Write-Log {
    param(
        [string]$Message,
        [ValidateSet('DEBUG','INFO','WARN','ERROR')][string]$Level = 'INFO'
    )
    $ts = (Get-Date).ToString('s')
    Write-Host "[$ts][$Level] $Message"
}

function Invoke-WithTiming {
    param(
        [string]$Operation,
        [scriptblock]$Script
    )
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $result = & $Script
        $sw.Stop()
        Write-Log "$Operation succeeded in $($sw.ElapsedMilliseconds) ms" 'DEBUG'
        return $result
    }
    catch {
        $sw.Stop()
        Write-Log "$Operation failed in $($sw.ElapsedMilliseconds) ms : $_" 'ERROR'
        throw
    }
}

# ---------- Registry Helpers ----------
function Get-Registry {
    if (Test-Path $script:RegistryPath) {
        try {
            return Get-Content $script:RegistryPath -Raw | ConvertFrom-Json -Depth 6
        }
        catch {
            Write-Log "Registry corrupt; recreating: $_" 'WARN'
        }
    }
    return @{}
}

function Save-Registry {
    param([object]$Registry)
    $json = $Registry | ConvertTo-Json -Depth 6
    $json | Set-Content -Path $script:RegistryPath -Encoding UTF8
}

# ---------- Marketplace Helpers ----------
function Invoke-MarketplaceQuery {
    param(
        [string]$Query,
        [string]$Category,
        [int]$Page = 1,
        [int]$PageSize = $script:DefaultPageSize,
        [string]$ExtensionId
    )

    $criteria = @()
    if ($Query) {
        $criteria += @{ filterType = 8; value = $Query }
    }
    if ($Category) {
        $criteria += @{ filterType = 3; value = $Category }
    }
    if ($ExtensionId) {
        $criteria += @{ filterType = 4; value = $ExtensionId }
    }

    $body = @{
        filters = @(
            @{
                criteria  = $criteria
                pageNumber = $Page
                pageSize   = $PageSize
                sortBy     = 0
                sortOrder  = 0
            }
        )
        flags = 0x1FF
    }

    $json = $body | ConvertTo-Json -Depth 10
    Write-Log "Marketplace query: Query='$Query' Category='$Category' ExtensionId='$ExtensionId' Page=$Page Size=$PageSize" 'DEBUG'

    $params = @{
        Uri         = $script:MarketplaceUri
        Method      = 'Post'
        Body        = $json
        ContentType = 'application/json'
        Headers     = @{ 'Accept' = 'application/json; api-version=7.1-preview.1' }
    }

    # Ensure TLS 1.2+
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

    Invoke-RestMethod @params
}

function Get-ExtensionDetails {
    param([string]$ExtensionId)
    $response = Invoke-WithTiming "Get details for $ExtensionId" { Invoke-MarketplaceQuery -ExtensionId $ExtensionId -Page 1 -PageSize 1 }
    if (-not $response.results.extensions) {
        throw "Extension '$ExtensionId' not found."
    }
    return $response.results.extensions[0]
}

function Get-DownloadInfo {
    param([object]$Extension, [string]$Version)
    $versionObj = if ($Version) { $Extension.versions | Where-Object { $_.version -eq $Version } | Select-Object -First 1 } else { $Extension.versions | Select-Object -First 1 }
    if (-not $versionObj) { throw "Version '$Version' not found for $($Extension.extensionId)." }
    $vsix = $versionObj.files | Where-Object { $_.assetType -eq 'Microsoft.VisualStudio.Services.VSIXPackage' } | Select-Object -First 1
    if (-not $vsix) { throw "No VSIX asset found for $($Extension.extensionId)." }
    return @{ Url = $vsix.source; Version = $versionObj.version }
}

# ---------- Public Cmdlets ----------
function Get-VSCodeExtensionStatus {
    [CmdletBinding()]
    param()
    $reg = Get-Registry()
    $installed = @($reg.PSObject.Properties | ForEach-Object { $_.Value })
    [pscustomobject]@{
        InstalledCount = $installed.Count
        Installed      = $installed
        ExtensionRoot  = $script:ExtensionRoot
        RegistryPath   = $script:RegistryPath
    }
}

function Search-VSCodeMarketplace {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Query,
        [string]$Category,
        [int]$Page = 1,
        [int]$PageSize = $script:DefaultPageSize
    )

    $response = Invoke-WithTiming "Search '$Query'" { Invoke-MarketplaceQuery -Query $Query -Category $Category -Page $Page -PageSize $PageSize }
    $exts = @()
    foreach ($e in $response.results.extensions) {
        $exts += [pscustomobject]@{
            Id          = $e.extensionId
            Publisher   = $e.publisher.publisherName
            Name        = $e.displayName
            ShortName   = "$($e.publisher.publisherName).$($e.extensionName)"
            Version     = $e.versions[0].version
            Description = $e.shortDescription
        }
    }
    return $exts
}

function Get-VSCodeExtensionInfo {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ExtensionId
    )

    $reg = Get-Registry()
    $entry = $reg.$ExtensionId
    $details = $null
    try { $details = Get-ExtensionDetails -ExtensionId $ExtensionId } catch {}

    [pscustomobject]@{
        Id          = $ExtensionId
        Installed   = [bool]$entry
        Version     = $entry?.Version
        Path        = $entry?.InstallPath
        DisplayName = $details?.displayName
        Publisher   = $details?.publisher?.publisherName
        MarketplaceVersion = $details?.versions?[0]?.version
        Description = $details?.shortDescription
    }
}

function Install-VSCodeExtension {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ExtensionId,
        [string]$Version
    )

    $reg = Get-Registry()
    if ($reg.$ExtensionId) {
        Write-Log "Extension $ExtensionId already installed at $($reg.$ExtensionId.InstallPath)" 'INFO'
        return $reg.$ExtensionId
    }

    $details = Get-ExtensionDetails -ExtensionId $ExtensionId
    $dlInfo  = Get-DownloadInfo -Extension $details -Version $Version

    $tempVsix = Join-Path $script:ExtensionRoot "$ExtensionId.vsix"
    Invoke-WithTiming "Download $ExtensionId" {
        Invoke-WebRequest -Uri $dlInfo.Url -OutFile $tempVsix -UseBasicParsing
    } | Out-Null

    $installPath = Join-Path $script:ExtensionRoot $ExtensionId
    if (Test-Path $installPath) { Remove-Item -Path $installPath -Recurse -Force }
    New-Item -ItemType Directory -Path $installPath -Force | Out-Null

    Invoke-WithTiming "Extract $ExtensionId" {
        Expand-Archive -Path $tempVsix -DestinationPath $installPath -Force
    } | Out-Null

    Remove-Item -Path $tempVsix -Force

    $manifestPath = Join-Path $installPath 'extension' 'package.json'
    if (-not (Test-Path $manifestPath)) {
        # Some packages place manifest at root
        $manifestPath = Join-Path $installPath 'package.json'
    }

    $manifest = $null
    if (Test-Path $manifestPath) {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json -Depth 6
    }

    $entry = [pscustomobject]@{
        Id          = $ExtensionId
        Version     = $dlInfo.Version
        InstallPath = $installPath
        Manifest    = $manifest
        InstalledAt = (Get-Date)
        Loaded      = $false
    }

    $reg | Add-Member -NotePropertyName $ExtensionId -NotePropertyValue $entry -Force
    Save-Registry -Registry $reg

    Write-Log "Installed $ExtensionId version $($entry.Version) to $installPath" 'INFO'
    return $entry
}

function Uninstall-VSCodeExtension {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ExtensionId
    )

    $reg = Get-Registry()
    $entry = $reg.$ExtensionId
    if (-not $entry) {
        throw "Extension '$ExtensionId' is not installed."
    }

    if (Test-Path $entry.InstallPath) {
        Remove-Item -Path $entry.InstallPath -Recurse -Force
    }

    $reg.PSObject.Properties.Remove($ExtensionId) | Out-Null
    Save-Registry -Registry $reg
    Write-Log "Uninstalled $ExtensionId" 'INFO'
    return $true
}

function Load-VSCodeExtension {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ExtensionId
    )

    $reg = Get-Registry()
    $entry = $reg.$ExtensionId
    if (-not $entry) { throw "Extension '$ExtensionId' is not installed." }

    $manifestPath = if ($entry.Manifest) { $null } else { Join-Path $entry.InstallPath 'extension' 'package.json' }
    if (-not $entry.Manifest -and $manifestPath -and (Test-Path $manifestPath)) {
        $entry.Manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json -Depth 6
    }

    $entry.Loaded = $true
    $reg.PSObject.Properties.Remove($ExtensionId) | Out-Null
    $reg | Add-Member -NotePropertyName $ExtensionId -NotePropertyValue $entry -Force
    Save-Registry -Registry $reg
    Write-Log "Loaded $ExtensionId" 'INFO'
    return $entry
}

Export-ModuleMember -Function Get-VSCodeExtensionStatus,Search-VSCodeMarketplace,Get-VSCodeExtensionInfo,Install-VSCodeExtension,Uninstall-VSCodeExtension,Load-VSCodeExtension
