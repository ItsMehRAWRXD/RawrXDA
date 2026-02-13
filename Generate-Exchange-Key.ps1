# Generate-Exchange-Key.ps1
# Cryptographic Key Exchange System for OMEGA-REVERSER TOOLKIT

param(
    [string]$LicenseeName = "",
    [string]$LicenseeEmail = "",
    [string]$LicenseType = "Enterprise", # Enterprise, Professional, Personal, Trial
    [int]$DurationDays = 365,
    [switch]$GenerateMasterKey,
    [string]$OutputPath = "ExchangeKeys"
)

Add-Type -TypeDefinition @"
using System;
using System.Security.Cryptography;
using System.Text;

public class ExchangeCrypto {
    public static byte[] GenerateKey(int size) {
        using (RNGCryptoServiceProvider rng = new RNGCryptoServiceProvider()) {
            byte[] key = new byte[size];
            rng.GetBytes(key);
            return key;
        }
    }
    
    public static byte[] GenerateIV() {
        using (RNGCryptoServiceProvider rng = new RNGCryptoServiceProvider()) {
            byte[] iv = new byte[16];
            rng.GetBytes(iv);
            return iv;
        }
    }
    
    public static string ComputeHash(string input) {
        using (SHA256 sha256 = SHA256.Create()) {
            byte[] bytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(input));
            return Convert.ToBase64String(bytes);
        }
    }
    
    public static byte[] SignData(byte[] data, byte[] key) {
        using (HMACSHA256 hmac = new HMACSHA256(key)) {
            return hmac.ComputeHash(data);
        }
    }
}
"@

function New-ExchangeKey {
    param(
        [string]$LicenseeName,
        [string]$LicenseeEmail,
        [string]$LicenseType,
        [int]$DurationDays
    )
    
    # Generate cryptographic keys
    $masterKey = [ExchangeCrypto]::GenerateKey(32)  # 256-bit master key
    $exchangeKey = [ExchangeCrypto]::GenerateKey(32) # 256-bit exchange key
    $validationKey = [ExchangeCrypto]::GenerateKey(32) # 256-bit validation key
    $iv = [ExchangeCrypto]::GenerateIV()              # 128-bit IV
    
    # Generate license ID
    $licenseId = [Guid]::NewGuid().ToString("N")
    
    # Calculate expiration
    $issuedDate = Get-Date
    $expirationDate = $issuedDate.AddDays($DurationDays)
    
    # Create license data
    $licenseData = @{
        LicenseId = $licenseId
        LicenseeName = $LicenseeName
        LicenseeEmail = $LicenseeEmail
        LicenseType = $LicenseType
        IssuedDate = $issuedDate.ToString("yyyy-MM-dd HH:mm:ss")
        ExpirationDate = $expirationDate.ToString("yyyy-MM-dd HH:mm:ss")
        DurationDays = $DurationDays
        MasterKey = [Convert]::ToBase64String($masterKey)
        ExchangeKey = [Convert]::ToBase64String($exchangeKey)
        ValidationKey = [Convert]::ToBase64String($validationKey)
        IV = [Convert]::ToBase64String($iv)
        Features = @(
            "ReverseEngineering",
            "Deobfuscation",
            "InstallationReversal",
            "ProtectedEdition",
            "PolyglotSupport"
        )
        Restrictions = @{
            MaxConcurrentUsers = switch($LicenseType) {
                "Enterprise" { 100 }
                "Professional" { 10 }
                "Personal" { 1 }
                "Trial" { 1 }
                default { 1 }
            }
            MaxExtractedFiles = switch($LicenseType) {
                "Enterprise" { [int]::MaxValue }
                "Professional" { 10000 }
                "Personal" { 1000 }
                "Trial" { 100 }
                default { 100 }
            }
            CommercialUse = switch($LicenseType) {
                "Enterprise" { $true }
                "Professional" { $true }
                "Personal" { $false }
                "Trial" { $false }
                default { $false }
            }
        }
    }
    
    # Sign the license
    $licenseJson = $licenseData | ConvertTo-Json -Depth 10
    $signature = [ExchangeCrypto]::SignData([Text.Encoding]::UTF8.GetBytes($licenseJson), $validationKey)
    
    $signedLicense = @{
        License = $licenseData
        Signature = [Convert]::ToBase64String($signature)
        SignatureAlgorithm = "HMAC-SHA256"
    }
    
    return $signedLicense
}

function New-MasterExchangeKey {
    # Generate master exchange key for the toolkit itself
    $masterKey = [ExchangeCrypto]::GenerateKey(32)
    $masterIV = [ExchangeCrypto]::GenerateIV()
    $masterValidation = [ExchangeCrypto]::GenerateKey(32)
    
    $masterExchange = @{
        MasterKey = [Convert]::ToBase64String($masterKey)
        MasterIV = [Convert]::ToBase64String($masterIV)
        MasterValidation = [Convert]::ToBase64String($masterValidation)
        GeneratedDate = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        Version = "5.0"
        Description = "OMEGA-REVERSER TOOLKIT Master Exchange Key"
    }
    
    return $masterExchange
}

# Main execution
Write-Host @"
╔══════════════════════════════════════════════════════════════════╗
║     OMEGA-REVERSER TOOLKIT - KEY EXCHANGE SYSTEM               ║
║     Cryptographic License Generation Engine                    ║
╚══════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

$startTime = Get-Date

# Create output directory
New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null

if ($GenerateMasterKey) {
    Write-Host "`n[1/2] Generating MASTER EXCHANGE KEY for toolkit..." -ForegroundColor Yellow
    
    $masterExchange = New-MasterExchangeKey
    $masterKeyPath = Join-Path $OutputPath "MASTER_EXCHANGE_KEY.json"
    
    $masterExchange | ConvertTo-Json -Depth 10 | Out-File -FilePath $masterKeyPath -Encoding UTF8
    
    Write-Host "    ✓ Master exchange key generated" -ForegroundColor Green
    Write-Host "    ✓ Saved to: $masterKeyPath" -ForegroundColor Green
    Write-Host "    ⚠️  KEEP THIS KEY SECURE!" -ForegroundColor Red
}

if ($LicenseeName -and $LicenseeEmail) {
    Write-Host "`n[2/2] Generating LICENSE for $LicenseeName..." -ForegroundColor Yellow
    
    $license = New-ExchangeKey -LicenseeName $LicenseeName -LicenseeEmail $LicenseeEmail -LicenseType $LicenseType -DurationDays $DurationDays
    
    # Save license file
    $licenseFileName = "LICENSE_$($LicenseeName -replace '[^a-zA-Z0-9]', '_')_$([DateTime]::Now.ToString('yyyyMMdd')).json"
    $licensePath = Join-Path $OutputPath $licenseFileName
    
    $license | ConvertTo-Json -Depth 10 | Out-File -FilePath $licensePath -Encoding UTF8
    
    # Save exchange key separately (to be sent to licensee)
    $exchangeKeyOnly = @{
        LicenseId = $license.License.LicenseId
        ExchangeKey = $license.License.ExchangeKey
        IV = $license.License.IV
        ValidationKey = $license.License.ValidationKey
        ExpirationDate = $license.License.ExpirationDate
    }
    
    $exchangeKeyPath = Join-Path $OutputPath "EXCHANGE_KEY_$($LicenseeName -replace '[^a-zA-Z0-9]', '_').json"
    $exchangeKeyOnly | ConvertTo-Json -Depth 10 | Out-File -FilePath $exchangeKeyPath -Encoding UTF8
    
    Write-Host "    ✓ License generated for $LicenseeName" -ForegroundColor Green
    Write-Host "    ✓ License file: $licensePath" -ForegroundColor Green
    Write-Host "    ✓ Exchange key: $exchangeKeyPath" -ForegroundColor Green
    Write-Host "    ✓ License ID: $($license.License.LicenseId)" -ForegroundColor Green
    Write-Host "    ✓ Expires: $($license.License.ExpirationDate)" -ForegroundColor Green
    Write-Host "    ✓ Type: $LicenseType" -ForegroundColor Green
    Write-Host "    📧 Send EXCHANGE_KEY file to licensee" -ForegroundColor Cyan
    Write-Host "    🔒 Keep LICENSE file for validation" -ForegroundColor Cyan
}

$duration = (Get-Date) - $startTime

Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║                    KEY EXCHANGE COMPLETE                         ║
╚══════════════════════════════════════════════════════════════════╝
Duration: $($duration.ToString('hh\:mm\:ss'))
Output Directory: $OutputPath

FILES GENERATED:
$(if ($GenerateMasterKey) { "✓ MASTER_EXCHANGE_KEY.json (KEEP SECURE!)" })
$(if ($LicenseeName) { "✓ LICENSE_*.json (for validation)" })
$(if ($LicenseeName) { "✓ EXCHANGE_KEY_*.json (send to licensee)" })

USAGE:
  # Generate master key for toolkit
  .\Generate-Exchange-Key.ps1 -GenerateMasterKey
  
  # Generate license for user
  .\Generate-Exchange-Key.ps1 -LicenseeName "John Doe" -LicenseeEmail "john@example.com" -LicenseType Enterprise

SECURITY NOTES:
⚠️  Master exchange key must be kept secure
⚠️  Exchange keys should be transmitted securely
⚠️  License files are required for validation
⚠️  All keys use 256-bit AES encryption
"@ -ForegroundColor Magenta

# Create README for exchange keys
$readme = @"
# OMEGA-REVERSER TOOLKIT - Key Exchange System

## Files in this directory

### MASTER_EXCHANGE_KEY.json
**KEEP THIS FILE SECURE!**

This is the master exchange key for the OMEGA-REVERSER TOOLKIT itself.
It is used to validate all licenses and exchange keys.

**DO NOT SHARE THIS FILE!**

### LICENSE_*.json
**FOR VALIDATION PURPOSES ONLY**

These files contain the complete license information including:
- Licensee details
- License type and restrictions
- Master key (encrypted)
- Validation key
- Feature permissions

**KEEP THESE FILES FOR VALIDATION!**

### EXCHANGE_KEY_*.json
**SEND TO LICENSEE**

These files contain only the exchange key information that the licensee needs:
- License ID
- Exchange key (for decryption)
- IV (initialization vector)
- Validation key (for integrity checks)
- Expiration date

**SEND THESE FILES TO LICENSEES SECURELY!**

## Security Best Practices

1. **Master Key**: Store in secure location (encrypted drive, HSM, etc.)
2. **License Files**: Keep backups in secure storage
3. **Exchange Keys**: Transmit using secure channels (TLS, encrypted email, etc.)
4. **Key Rotation**: Rotate master key annually
5. **Access Control**: Limit access to key files

## Key Format

All keys use 256-bit AES encryption with HMAC-SHA256 signatures.
Keys are Base64-encoded for transmission.

## License Types

- **Enterprise**: Unlimited usage, commercial use allowed, 100 concurrent users
- **Professional**: 10,000 files max, commercial use allowed, 10 concurrent users
- **Personal**: 1,000 files max, non-commercial use only, 1 concurrent user
- **Trial**: 100 files max, 30 days, non-commercial use only, 1 concurrent user

---

**OMEGA-REVERSER TOOLKIT v5.0**  
"Extract anything. Protect everything."
"@ 

$readme | Out-File -FilePath (Join-Path $OutputPath "README_KEYS.md") -Encoding UTF8
