[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$Files,
    [string]$CertThumbprint = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [switch]$AllowUnsigned
)

$ErrorActionPreference = "Stop"

$signTool = Get-Command signtool -ErrorAction SilentlyContinue
if (-not $signTool) {
    if ($AllowUnsigned) {
        Write-Warning "signtool.exe not found; continuing unsigned."
        return
    }
    throw "signtool.exe not found in PATH."
}

if ([string]::IsNullOrWhiteSpace($CertThumbprint)) {
    if ($AllowUnsigned) {
        Write-Warning "No certificate thumbprint provided; continuing unsigned."
        return
    }
    throw "CertThumbprint is required for signing."
}

foreach ($file in $Files) {
    if (-not (Test-Path $file -PathType Leaf)) {
        throw "File not found: $file"
    }
    Write-Host "Signing: $file"
    & $signTool.Path sign `
        /sha1 $CertThumbprint `
        /fd SHA256 `
        /tr $TimestampUrl `
        /td SHA256 `
        /as `
        $file
    if ($LASTEXITCODE -ne 0) {
        throw "Signing failed: $file"
    }
}
