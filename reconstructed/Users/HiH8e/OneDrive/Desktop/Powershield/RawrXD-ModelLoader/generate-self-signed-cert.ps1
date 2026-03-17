# Self-Signed Code Signing Certificate Generator for RawrXD v0.1.0
# Run this ONCE before pushing the v0.1.0 tag

Write-Host "🔐 Generating self-signed code signing certificate..." -ForegroundColor Cyan

# 1. Create self-signed cert (valid for 3 years)
$cert = New-SelfSignedCertificate `
    -Type CodeSigning `
    -Subject "CN=RawrXD, O=RawrXD Contributors, C=US" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(3) `
    -KeyExportPolicy Exportable

Write-Host "✓ Certificate created: $($cert.Thumbprint)" -ForegroundColor Green

# 2. Export to .pfx with password
$password = "RawrXD_SelfSign_2025"
$securePassword = ConvertTo-SecureString -String $password -Force -AsPlainText
$pfxPath = "$PSScriptRoot\RawrXD_self.pfx"

Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $securePassword | Out-Null
Write-Host "✓ Exported to: $pfxPath" -ForegroundColor Green

# 3. Convert to base64 for GitHub secret
$pfxBytes = [IO.File]::ReadAllBytes($pfxPath)
$base64 = [Convert]::ToBase64String($pfxBytes)
$base64Path = "$PSScriptRoot\RawrXD_cert_base64.txt"
$base64 | Out-File -FilePath $base64Path -Encoding ASCII

Write-Host "✓ Base64 encoded to: $base64Path" -ForegroundColor Green
Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Yellow
Write-Host "📋 GitHub Secrets Setup (copy these values)" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. Go to: https://github.com/ItsMehRAWRXD/RawrXD/settings/secrets/actions" -ForegroundColor White
Write-Host ""
Write-Host "2. Create secret: CODESIGN_PFX" -ForegroundColor Cyan
Write-Host "   Value: (copy from RawrXD_cert_base64.txt)" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Create secret: CODESIGN_PWD" -ForegroundColor Cyan
Write-Host "   Value: $password" -ForegroundColor Gray
Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Yellow
Write-Host ""

# 4. Test local signing (if signtool is available)
$signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if ($signtool) {
    $exePath = "$PSScriptRoot\build\bin-msvc\Release\RawrXD-QtShell.exe"
    if (Test-Path $exePath) {
        Write-Host "🔍 Testing local signing..." -ForegroundColor Cyan
        & signtool sign /f $pfxPath /p $password /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $exePath
        & signtool verify /pa $exePath
        Write-Host "✓ Local signing test complete" -ForegroundColor Green
    } else {
        Write-Host "⚠️  Binary not found at: $exePath" -ForegroundColor Yellow
        Write-Host "   Build first: cmake --build build-msvc --config Release --target RawrXD-QtShell" -ForegroundColor Gray
    }
} else {
    Write-Host "⚠️  signtool.exe not in PATH (install Windows SDK to test locally)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "✅ Self-signed cert ready! Delete RawrXD_self.pfx after uploading secrets." -ForegroundColor Green
Write-Host "   Files to secure: RawrXD_self.pfx, RawrXD_cert_base64.txt" -ForegroundColor Gray
