# Validate-License.ps1
# License Validation System for OMEGA-REVERSER TOOLKIT

param(
    [string]$LicenseFile = "",
    [string]$ExchangeKeyFile = "",
    [string]$MasterKeyFile = "MASTER_EXCHANGE_KEY.json",
    [switch]$CheckExpiration,
    [switch]$CheckFeatures,
    [switch]$Verbose
)

Add-Type -TypeDefinition @"
using System;
using System.Security.Cryptography;
using System.Text;

public class LicenseValidator {
    public static bool VerifySignature(string licenseJson, string signatureBase64, byte[] validationKey) {
        try {
            byte[] licenseBytes = Encoding.UTF8.GetBytes(licenseJson);
            byte[] signature = Convert.FromBase64String(signatureBase64);
            
            using (HMACSHA256 hmac = new HMACSHA256(validationKey)) {
                byte[] computedSignature = hmac.ComputeHash(licenseBytes);
                return Convert.ToBase64String(computedSignature) == signatureBase64;
            }
        }
        catch {
            return false;
        }
    }
    
    public static byte[] DecryptKey(string encryptedKeyBase64, byte[] masterKey, byte[] iv) {
        try {
            byte[] encryptedKey = Convert.FromBase64String(encryptedKeyBase64);
            
            using (Aes aes = Aes.Create()) {
                aes.Key = masterKey;
                aes.IV = iv;
                aes.Mode = CipherMode.CBC;
                aes.Padding = PaddingMode.PKCS7;
                
                ICryptoTransform decryptor = aes.CreateDecryptor();
                byte[] decryptedKey = decryptor.TransformFinalBlock(encryptedKey, 0, encryptedKey.Length);
                return decryptedKey;
            }
        }
        catch {
            return null;
        }
    }
    
    public static string ComputeHash(string input) {
        using (SHA256 sha256 = SHA256.Create()) {
            byte[] bytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(input));
            return Convert.ToBase64String(bytes);
        }
    }
}
"@

function Test-LicenseExpiration {
    param($License)
    
    $expirationDate = [DateTime]::ParseExact($License.ExpirationDate, "yyyy-MM-dd HH:mm:ss", $null)
    $currentDate = Get-Date
    
    if ($currentDate -gt $expirationDate) {
        Write-Error "License expired! Expiration date: $($License.ExpirationDate)"
        return $false
    }
    
    $daysRemaining = ($expirationDate - $currentDate).Days
    if ($daysRemaining -le 30) {
        Write-Warning "License expires in $daysRemaining days!"
    }
    
    return $true
}

function Test-LicenseFeatures {
    param($License, $RequiredFeatures)
    
    $availableFeatures = $License.Features
    $missingFeatures = @()
    
    foreach ($feature in $RequiredFeatures) {
        if ($feature -notin $availableFeatures) {
            $missingFeatures += $feature
        }
    }
    
    if ($missingFeatures.Count -gt 0) {
        Write-Error "License missing required features: $($missingFeatures -join ', ')"
        return $false
    }
    
    return $true
}

function Test-LicenseRestrictions {
    param($License, $CurrentUsage)
    
    $restrictions = $License.Restrictions
    
    # Check concurrent users
    if ($CurrentUsage.ConcurrentUsers -gt $restrictions.MaxConcurrentUsers) {
        Write-Error "License exceeded max concurrent users: $($restrictions.MaxConcurrentUsers)"
        return $false
    }
    
    # Check extracted files
    if ($CurrentUsage.ExtractedFiles -gt $restrictions.MaxExtractedFiles) {
        Write-Error "License exceeded max extracted files: $($restrictions.MaxExtractedFiles)"
        return $false
    }
    
    return $true
}

function Show-LicenseInfo {
    param($License)
    
    Write-Host "`nLICENSE INFORMATION:" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "License ID:    $($License.LicenseId)" -ForegroundColor White
    Write-Host "Licensee:      $($License.LicenseeName)" -ForegroundColor White
    Write-Host "Email:         $($License.LicenseeEmail)" -ForegroundColor White
    Write-Host "Type:          $($License.LicenseType)" -ForegroundColor White
    Write-Host "Issued:        $($License.IssuedDate)" -ForegroundColor White
    Write-Host "Expires:       $($License.ExpirationDate)" -ForegroundColor $(if (([DateTime]::ParseExact($License.ExpirationDate, "yyyy-MM-dd HH:mm:ss", $null) -lt (Get-Date).AddDays(30)) { "Red" } else { "Green" })
    Write-Host "Duration:      $($License.DurationDays) days" -ForegroundColor White
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    Write-Host "`nFEATURES ENABLED:" -ForegroundColor Cyan
    foreach ($feature in $License.Features) {
        Write-Host "  ✓ $feature" -ForegroundColor Green
    }
    
    Write-Host "`nRESTRICTIONS:" -ForegroundColor Cyan
    Write-Host "  Max Concurrent Users: $($License.Restrictions.MaxConcurrentUsers)" -ForegroundColor White
    Write-Host "  Max Extracted Files:  $($License.Restrictions.MaxExtractedFiles)" -ForegroundColor White
    Write-Host "  Commercial Use:       $($License.Restrictions.CommercialUse)" -ForegroundColor White
}

# Main validation logic
Write-Host @"
╔══════════════════════════════════════════════════════════════════╗
║     OMEGA-REVERSER TOOLKIT - LICENSE VALIDATION                ║
║     Cryptographic License Verification Engine                  ║
╚══════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

$startTime = Get-Date
$validationPassed = $true

# Load master exchange key
if (-not (Test-Path $MasterKeyFile)) {
    Write-Error "Master exchange key not found: $MasterKeyFile"
    Write-Error "Run Generate-Exchange-Key.ps1 -GenerateMasterKey first"
    exit 1
}

$masterKeyData = Get-Content $MasterKeyFile -Raw | ConvertFrom-Json
$masterKey = [Convert]::FromBase64String($masterKeyData.MasterKey)
$masterIV = [Convert]::FromBase64String($masterKeyData.MasterIV)
$masterValidation = [Convert]::FromBase64String($masterKeyData.MasterValidation)

Write-Host "`n[1/4] Master exchange key loaded" -ForegroundColor Green

# Load and validate license
if ($LicenseFile) {
    if (-not (Test-Path $LicenseFile)) {
        Write-Error "License file not found: $LicenseFile"
        exit 1
    }
    
    $licenseData = Get-Content $LicenseFile -Raw | ConvertFrom-Json
    
    # Verify signature
    $licenseJson = $licenseData.License | ConvertTo-Json -Depth 10 -Compress
    $signatureValid = [LicenseValidator]::VerifySignature($licenseJson, $licenseData.Signature, $masterValidation)
    
    if (-not $signatureValid) {
        Write-Error "License signature verification FAILED!"
        Write-Error "The license file may be tampered or corrupted."
        $validationPassed = $false
    } else {
        Write-Host "`n[2/4] License signature verified" -ForegroundColor Green
    }
    
    # Decrypt master key from license
    $encryptedMasterKey = $licenseData.License.MasterKey
    $decryptedMasterKey = [LicenseValidator]::DecryptKey($encryptedMasterKey, $masterKey, $masterIV)
    
    if ($null -eq $decryptedMasterKey) {
        Write-Error "Failed to decrypt master key from license"
        $validationPassed = $false
    } else {
        Write-Host "`n[3/4] Master key decrypted from license" -ForegroundColor Green
    }
    
    # Check expiration
    if ($CheckExpiration) {
        $expirationValid = Test-LicenseExpiration -License $licenseData.License
        if (-not $expirationValid) {
            $validationPassed = $false
        } else {
            Write-Host "`n[4/4] License expiration checked" -ForegroundColor Green
        }
    }
    
    # Show license info
    if ($Verbose) {
        Show-LicenseInfo -License $licenseData.License
    }
    
    $global:ValidatedLicense = $licenseData.License
    $global:DecryptedMasterKey = $decryptedMasterKey
}

# Load exchange key
if ($ExchangeKeyFile) {
    if (-not (Test-Path $ExchangeKeyFile)) {
        Write-Error "Exchange key file not found: $ExchangeKeyFile"
        exit 1
    }
    
    $exchangeKeyData = Get-Content $ExchangeKeyFile -Raw | ConvertFrom-Json
    
    # Verify exchange key signature
    $exchangeKeyJson = $exchangeKeyData | ConvertTo-Json -Depth 10 -Compress
    $exchangeSignatureValid = [LicenseValidator]::VerifySignature($exchangeKeyJson, $exchangeKeyData.Signature, $masterValidation)
    
    if (-not $exchangeSignatureValid) {
        Write-Error "Exchange key signature verification FAILED!"
        $validationPassed = $false
    } else {
        Write-Host "`n[✓] Exchange key signature verified" -ForegroundColor Green
    }
    
    # Extract exchange key
    $exchangeKey = [Convert]::FromBase64String($exchangeKeyData.ExchangeKey)
    $exchangeIV = [Convert]::FromBase64String($exchangeKeyData.IV)
    $exchangeValidation = [Convert]::FromBase64String($exchangeKeyData.ValidationKey)
    
    $global:ExchangeKey = $exchangeKey
    $global:ExchangeIV = $exchangeIV
    $global:ExchangeValidation = $exchangeValidation
    $global:ExchangeLicenseId = $exchangeKeyData.LicenseId
}

$duration = (Get-Date) - $startTime

if ($validationPassed) {
    Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║                    LICENSE VALIDATION PASSED                   ║
╚══════════════════════════════════════════════════════════════════╝
Duration: $($duration.ToString('hh\:mm\:ss'))

The license has been validated and is ready for use.
All cryptographic signatures have been verified.

READY TO USE: OMEGA-REVERSER TOOLKIT
"@ -ForegroundColor Green
    
    exit 0
} else {
    Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║                    LICENSE VALIDATION FAILED                   ║
╚══════════════════════════════════════════════════════════════════╝
Duration: $($duration.ToString('hh\:mm\:ss'))

The license validation failed. Please check:
- License file exists and is valid
- Exchange key file exists and is valid
- Master exchange key exists and is valid
- License has not expired
- License signature is valid

ERROR: Cannot use OMEGA-REVERSER TOOLKIT
"@ -ForegroundColor Red
    
    exit 1
}