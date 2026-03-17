<#
.SYNOPSIS
    Generate a "Pro" JWT for Cursor Local Bypass.
#>

$ErrorActionPreference = "Stop"

Write-Host "`n[1/3] Initializing Cryptography..." -ForegroundColor Yellow

# The private key (32 bytes)
$hex = '7a1c0d7f3b9f4c8e2f6d5a4b3c2e1f0d9a8b7c6e5f4d3c2b1a00987766554433'
$key = New-Object byte[] 32
for($i=0; $i -lt 32; $i++) {
    $key[$i] = [Convert]::ToByte($hex.Substring($i*2, 2), 16)
}

Add-Type -AssemblyName System.Security
Add-Type -AssemblyName System.Security.Cryptography

try {
    # Use EdDSA for Ed25519 (Available in .NET 9 / PowerShell 7.5)
    $e = [System.Security.Cryptography.EdDSA]::Create([System.Security.Cryptography.ECCurve+NamedCurves]::Ed25519)
    if ($null -eq $e) { throw "EdDSA.Create returned null" }
} catch {
    $err1 = $_.Exception.Message
    try {
        # Fallback for older .NET versions that might support it via ECDsa (rare)
        $e = [System.Security.Cryptography.ECDsa]::Create([System.Security.Cryptography.ECCurve+NamedCurves]::Ed25519)
        if ($null -eq $e) { throw "ECDsa.Create returned null" }
    } catch {
        $err2 = $_.Exception.Message
        Write-Host "❌ Your system does not support Ed25519 natively in .NET." -ForegroundColor Red
        Write-Host "   Error 1 (EdDSA): $err1" -ForegroundColor Gray
        Write-Host "   Error 2 (ECDsa): $err2" -ForegroundColor Gray
        Write-Host "   This script requires PowerShell 7.5+ (.NET 9) for Ed25519 support." -ForegroundColor Yellow
        Write-Host "   Current PowerShell version: $($PSVersionTable.PSVersion)" -ForegroundColor Gray
        exit 1
    }
}

# Import the key into the provider (PKCS#8 format)
$p = New-Object byte[] 48
[byte[]]$header = 0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x04, 0x22, 0x04, 0x20
[Array]::Copy($header, 0, $p, 0, 16)
[Array]::Copy($key, 0, $p, 16, 32)
$e.ImportPkcs8PrivateKey($p, [ref]$null)

Write-Host "[2/3] Generating Pro Payload..." -ForegroundColor Yellow

$iat = [DateTimeOffset]::UtcNow.AddSeconds(-30).ToUnixTimeSeconds()
$exp = $iat + 31536000 # 1 year
$pld = '{"aud":"https://copilot-proxy.githubusercontent.com","iss":"copilot-cursor","iat":'+$iat+',"exp":'+$exp+',"entitlements":{"model":"gpt-4-turbo","context":32768,"tools":true,"agent":true,"rapid":true,"unlimited":true},"deviceId":"'+[guid]::NewGuid()+'","userId":31337}'

$hdr = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('{"alg":"EdDSA","crv":"Ed25519","kid":7}')).TrimEnd('=').Replace('+','-').Replace('/','_')
$b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($pld)).TrimEnd('=').Replace('+','-').Replace('/','_')
$pre = "$hdr.$b64"

Write-Host "[3/3] Signing Token..." -ForegroundColor Yellow

# Sign the token (EdDSA doesn't use a separate hash algorithm parameter for Ed25519)
$sig = [Convert]::ToBase64String($e.Sign([Text.Encoding]::UTF8.GetBytes($pre))).TrimEnd('=').Replace('+','-').Replace('/','_')
$jwt = "$pre.$sig"

# Save to file to avoid terminal truncation
$jwt | Set-Content "$PSScriptRoot\cursor_token.txt" -NoNewline
$jwt | Set-Clipboard

Write-Host "`n✅ PRO JWT GENERATED SUCCESSFULLY!" -ForegroundColor Green
Write-Host "--------------------------------------------------------------------------------"
Write-Host "📄 Token saved to: $PSScriptRoot\cursor_token.txt" -ForegroundColor Cyan
Write-Host "📋 Token copied to clipboard." -ForegroundColor Cyan
Write-Host "--------------------------------------------------------------------------------"

Write-Host "`n🚀 NEXT STEPS:" -ForegroundColor Yellow
Write-Host "1. Open Cursor and press Ctrl+Shift+I" -ForegroundColor White
Write-Host "2. In the Console, paste the following and press Enter:" -ForegroundColor White
Write-Host "   await github.copilot.signIn(`"$jwt`")" -ForegroundColor Cyan
Write-Host "3. Restart Cursor" -ForegroundColor White
