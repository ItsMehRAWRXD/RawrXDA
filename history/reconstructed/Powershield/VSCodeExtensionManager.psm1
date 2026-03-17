# VSCodeExtensionManager.psm1
# Simple PowerShell module for managing VS Code extensions via the Marketplace API

function Get-VSCodeExtensionStatus {
    param([string]$ExtensionId)
    # Check if extension is installed in the local extensions folder
    $extensionsPath = "$env:USERPROFILE\.vscode\extensions"
    if (Test-Path $extensionsPath) {
        $installed = Get-ChildItem $extensionsPath -Directory | Where-Object { $_.Name -like "${ExtensionId}*" }
        if ($installed) {
            return "Installed"
        }
    }
    return "NotInstalled"
}

function Search-VSCodeMarketplace {
    param([string]$Query)
    $url = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
    $body = @{
        filters = @(@{ criteria = @(@{ filterType = 8; value = $Query }) })
        flags = 914
        pageSize = 20
        pageNumber = 1
    } | ConvertTo-Json -Depth 5
    $response = Invoke-RestMethod -Method Post -Uri $url -ContentType "application/json" -Body $body
    return $response.results[0].extensions | Select-Object -Property extensionId, displayName, shortDescription
}

function Install-VSCodeExtension {
    param([string]$ExtensionId)
    # Use code CLI to install extension
    $codeExe = "code"
    $result = & $codeExe --install-extension $ExtensionId --force
    return $result
}

function Uninstall-VSCodeExtension {
    param([string]$ExtensionId)
    $codeExe = "code"
    $result = & $codeExe --uninstall-extension $ExtensionId
    return $result
}

function Load-VSCodeExtension {
    param([string]$ExtensionId)
    # Loading is essentially ensuring it's installed; no extra action needed
    return "Loaded $ExtensionId"
}

function Get-VSCodeExtensionInfo {
    param([string]$ExtensionId)
    $url = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
    $body = @{
        filters = @(@{ criteria = @(@{ filterType = 7; value = $ExtensionId }) })
        flags = 914
        pageSize = 1
        pageNumber = 1
    } | ConvertTo-Json -Depth 5
    $response = Invoke-RestMethod -Method Post -Uri $url -ContentType "application/json" -Body $body
    $ext = $response.results[0].extensions[0]
    return $ext | Select-Object -Property extensionId, displayName, version, publisher, description, installCount, rating
}

Export-ModuleMember -Function *
