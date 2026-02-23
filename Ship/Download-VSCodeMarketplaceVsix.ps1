# Download-VSCodeMarketplaceVsix.ps1
# Fetches .vsix packages from the VS Code Marketplace (reverse-engineered API).
# Use for agentic tests with Amazon Q and GitHub Copilot, or any extension by id.
#
# Usage:
#   .\Ship\Download-VSCodeMarketplaceVsix.ps1
#   .\Ship\Download-VSCodeMarketplaceVsix.ps1 -ExtensionIds "AmazonWebServices.amazon-q-vscode","GitHub.copilot"
#   .\Ship\Download-VSCodeMarketplaceVsix.ps1 -OutputDir "D:\rawrxd\plugins"
#
# Requires: PowerShell 5.1+ (Invoke-RestMethod, Invoke-WebRequest)

param(
    [string[]] $ExtensionIds = @("AmazonWebServices.amazon-q-vscode", "GitHub.copilot"),
    [string]   $OutputDir    = (Join-Path $PSScriptRoot "..\plugins"),
    [switch]   $OpenFolder
)

$ErrorActionPreference = "Stop"
$apiUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery?api-version=3.0-preview.1"

function Get-ExtensionInfo {
    param([string] $PublisherDotExtension)
    $body = @{
        filters = @(
            @{
                criteria = @(
                    @{ filterType = 8; value = "Microsoft.VisualStudio.Code" },
                    @{ filterType = 7; value = $PublisherDotExtension }
                )
                pageSize   = 1
                pageNumber = 1
            }
        )
        flags = 513
    } | ConvertTo-Json -Depth 5 -Compress

    $response = Invoke-RestMethod -Uri $apiUrl -Method Post -Body $body -ContentType "application/json" -UseBasicParsing
    $ext = $response.results[0].extensions[0]
    if (-not $ext) { throw "Extension not found: $PublisherDotExtension" }
    $ver = $ext.versions[0].version
    $pub = $ext.publisher.publisherName
    $name = $ext.extensionName
    [PSCustomObject]@{
        Id          = $PublisherDotExtension
        Publisher   = $pub
        ExtensionName = $name
        Version     = $ver
        DisplayName = $ext.displayName
    }
}

function Get-VsixDownloadUrl {
    param([string] $Publisher, [string] $ExtensionName, [string] $Version)
    "https://$Publisher.gallery.vsassets.io/_apis/public/gallery/publisher/$Publisher/extension/$ExtensionName/$Version/assetbyname/Microsoft.VisualStudio.Services.VSIXPackage"
}

if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$downloaded = @()
foreach ($id in $ExtensionIds) {
    Write-Host "[$id] Resolving version..." -ForegroundColor Cyan
    try {
        $info = Get-ExtensionInfo -PublisherDotExtension $id
        $url = Get-VsixDownloadUrl -Publisher $info.Publisher -ExtensionName $info.ExtensionName -Version $info.Version
        $safeName = $info.ExtensionName -replace '[^\w\-.]', '_'
        $outFile = Join-Path $OutputDir "$safeName-$($info.Version).vsix"
        Write-Host "[$id] Downloading $($info.DisplayName) v$($info.Version) -> $outFile" -ForegroundColor Green
        Invoke-WebRequest -Uri $url -OutFile $outFile -UseBasicParsing
        $downloaded += $outFile
    } catch {
        Write-Host "[$id] ERROR: $_" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Downloaded $($downloaded.Count) .vsix file(s) to: $OutputDir" -ForegroundColor Green
$downloaded | ForEach-Object { Write-Host "  $_" }

if ($OpenFolder -and $downloaded.Count -gt 0) {
    explorer.exe (Split-Path -Parent $downloaded[0])
}
