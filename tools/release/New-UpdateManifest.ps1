[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Channel,
    [Parameter(Mandatory = $true)][string]$BaseUrl,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [Parameter(Mandatory = $true)][string[]]$Artifacts
)

$ErrorActionPreference = "Stop"

function New-ArtifactEntry([string]$path, [string]$baseUrl) {
    if (-not (Test-Path $path -PathType Leaf)) {
        throw "Artifact not found: $path"
    }
    $file = Get-Item $path
    $hash = (Get-FileHash -Algorithm SHA256 -Path $path).Hash.ToLowerInvariant()
    $name = $file.Name
    $url = ($baseUrl.TrimEnd('/') + "/" + $name)
    return [ordered]@{
        name = $name
        size = [UInt64]$file.Length
        sha256 = $hash
        url = $url
    }
}

$entries = @()
foreach ($artifact in $Artifacts) {
    $entries += (New-ArtifactEntry -path $artifact -baseUrl $BaseUrl)
}

$manifest = [ordered]@{
    product = "RawrXD"
    channel = $Channel
    version = $Version
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    artifacts = $entries
}

$json = $manifest | ConvertTo-Json -Depth 6
$outDir = Split-Path -Parent $OutputPath
if ($outDir) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}
Set-Content -Path $OutputPath -Value ($json + [Environment]::NewLine) -Encoding UTF8

Write-Host "Manifest written: $OutputPath"
