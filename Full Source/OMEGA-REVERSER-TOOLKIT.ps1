# OMEGA-REVERSER-TOOLKIT.ps1
# Main wrapper for OMEGA-REVERSER TOOLKIT with license validation

param(
    [string]$Command = "",
    [string]$InputPath = "",
    [string]$OutputPath = "",
    [string]$LicenseFile = "",
    [string]$ExchangeKeyFile = "",
    [string]$MasterKeyFile = "MASTER_EXCHANGE_KEY.json",
    [switch]$ValidateOnly,
    [switch]$ShowLicenseInfo,
    [switch]$Help
)

# Global variables for license validation
$global:ValidatedLicense = $null
$global:DecryptedMasterKey = $null
$global:ExchangeKey = $null
$global:ExchangeIV = $null
$global:ExchangeValidation = $null
$global:ExchangeLicenseId = $null
$global:LicenseValidated = $false

function Show-Help {
    Write-Host @"
╔══════════════════════════════════════════════════════════════════╗
║     OMEGA-REVERSER TOOLKIT v5.0                                ║
║     The Ultimate Reverse Engineering Suite                     ║
╚══════════════════════════════════════════════════════════════════╝

USAGE:
    .\OMEGA-REVERSER-TOOLKIT.ps1 -Command <command> [options]

COMMANDS:
    reverse-install    Reverse an installation to buildable source
    deobfuscate        Deobfuscate code (50+ languages)
    extract-features   Extract specific features (chat/agent)
    validate-license   Validate license only
    show-license       Show license information
    help               Show this help

OPTIONS:
    -InputPath         Input path (installation, files, etc.)
    -OutputPath        Output path for extracted files
    -LicenseFile       License file for validation
    -ExchangeKeyFile   Exchange key file
    -MasterKeyFile     Master exchange key file (default: MASTER_EXCHANGE_KEY.json)
    -ValidateOnly      Validate license only, don't run command
    -ShowLicenseInfo   Show detailed license information
    -Help              Show this help

EXAMPLES:
    # Reverse an installation
    .\OMEGA-REVERSER-TOOLKIT.ps1 -Command reverse-install -InputPath "C:\Program Files\MyApp" -OutputPath "MyApp_Reversed" -LicenseFile LICENSE.json -ExchangeKeyFile EXCHANGE_KEY.json
    
    # Deobfuscate code
    .\OMEGA-REVERSER-TOOLKIT.ps1 -Command deobfuscate -InputPath "obfuscated.js" -OutputPath "deobfuscated" -LicenseFile LICENSE.json -ExchangeKeyFile EXCHANGE_KEY.json
    
    # Validate license only
    .\OMEGA-REVERSER-TOOLKIT.ps1 -Command validate-license -LicenseFile LICENSE.json -ExchangeKeyFile EXCHANGE_KEY.json
    
    # Show license info
    .\OMEGA-REVERSER-TOOLKIT.ps1 -Command show-license -LicenseFile LICENSE.json -ExchangeKeyFile EXCHANGE_KEY.json

LICENSE REQUIREMENT:
    All commands require valid license and exchange key files.
    Run Generate-Exchange-Key.ps1 to create keys.
    Run Validate-License.ps1 to validate keys.

SECURITY:
    The toolkit uses 256-bit AES encryption with HMAC-SHA256 signatures.
    All operations require cryptographic key exchange.
    Unauthorized use is prevented by license validation.

OMEGA-REVERSER TOOLKIT v5.0
"Extract anything. Protect everything."
"@ -ForegroundColor Cyan
}

function Test-LicenseValidation {
    if (-not $global:LicenseValidated) {
        Write-Error "License not validated! Run Validate-License.ps1 first."
        Write-Error "Or use -LicenseFile and -ExchangeKeyFile parameters."
        return $false
    }
    return $true
}

function Invoke-LicenseValidation {
    param(
        [string]$LicenseFile,
        [string]$ExchangeKeyFile,
        [string]$MasterKeyFile
    )
    
    if (-not $LicenseFile -or -not $ExchangeKeyFile) {
        Write-Error "License file and exchange key file are required!"
        return $false
    }
    
    # Run validation script
    $validationResult = .\Validate-License.ps1 -LicenseFile $LicenseFile -ExchangeKeyFile $ExchangeKeyFile -MasterKeyFile $MasterKeyFile
    
    if ($LASTEXITCODE -eq 0) {
        $global:LicenseValidated = $true
        Write-Host "✓ License validation successful" -ForegroundColor Green
        return $true
    } else {
        Write-Error "License validation failed!"
        return $false
    }
}

function Invoke-ReverseInstall {
    param(
        [string]$InputPath,
        [string]$OutputPath
    )
    
    if (-not (Test-LicenseValidation)) {
        return
    }
    
    Write-Host "`n[✓] License validated, proceeding with installation reversal..." -ForegroundColor Green
    
    # Check license restrictions
    $maxFiles = $global:ValidatedLicense.Restrictions.MaxExtractedFiles
    
    # Run the actual reversal
    $reversalArgs = @{
        InstallPath = $InputPath
        OutputPath = $OutputPath
        GenerateBuildSystem = $true
        DeepTypeRecovery = $true
        ExtractResources = $true
        ReconstructCOM = $true
        MapDependencies = $true
        ProjectName = "ReversedProject"
    }
    
    Write-Host "`n[1/6] Reversing installation..." -ForegroundColor Yellow
    .\Omega-Install-Reverser.ps1 @reversalArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Installation reversal complete" -ForegroundColor Green
        
        # Update usage statistics
        $extractedFiles = (Get-ChildItem $OutputPath -Recurse -File).Count
        Write-Host "✓ Extracted $extractedFiles files" -ForegroundColor Green
        
        if ($extractedFiles -gt $maxFiles) {
            Write-Warning "License limit exceeded! Max files: $maxFiles, Extracted: $extractedFiles"
        }
    } else {
        Write-Error "Installation reversal failed!"
    }
}

function Invoke-Deobfuscate {
    param(
        [string]$InputPath,
        [string]$OutputPath
    )
    
    if (-not (Test-LicenseValidation)) {
        return
    }
    
    Write-Host "`n[✓] License validated, proceeding with deobfuscation..." -ForegroundColor Green
    
    # Check license restrictions
    $maxFiles = $global:ValidatedLicense.Restrictions.MaxExtractedFiles
    
    # Run deobfuscation
    $deobfuscateArgs = @{
        InputPath = $InputPath
        OutputPath = $OutputPath
        MaxTPS = 10
        HumanLikeDelays = $true
        JitterSimulation = $true
    }
    
    Write-Host "`n[1/4] Deobfuscating code..." -ForegroundColor Yellow
    .\Omega-Deobfuscator.ps1 @deobfuscateArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Deobfuscation complete" -ForegroundColor Green
        
        # Update usage statistics
        $deobfuscatedFiles = (Get-ChildItem $OutputPath -Recurse -File).Count
        Write-Host "✓ Deobfuscated $deobfuscatedFiles files" -ForegroundColor Green
        
        if ($deobfuscatedFiles -gt $maxFiles) {
            Write-Warning "License limit exceeded! Max files: $maxFiles, Deobfuscated: $deobfuscatedFiles"
        }
    } else {
        Write-Error "Deobfuscation failed!"
    }
}

function Invoke-ExtractFeatures {
    param(
        [string]$InputPath,
        [string]$OutputPath
    )
    
    if (-not (Test-LicenseValidation)) {
        return
    }
    
    Write-Host "`n[✓] License validated, proceeding with feature extraction..." -ForegroundColor Green
    
    # Check license features
    $requiredFeatures = @("ReverseEngineering", "Deobfuscation")
    $hasFeatures = $true
    
    foreach ($feature in $requiredFeatures) {
        if ($feature -notin $global:ValidatedLicense.Features) {
            Write-Error "License missing required feature: $feature"
            $hasFeatures = $false
        }
    }
    
    if (-not $hasFeatures) {
        return
    }
    
    # Run feature extraction
    $extractArgs = @{
        SourcePath = $InputPath
        OutputPath = $OutputPath
        ExtractChatPane = $true
        ExtractAgentFeatures = $true
        ExtractCopilot = $true
        ExtractAIServices = $true
    }
    
    Write-Host "`n[1/5] Extracting features..." -ForegroundColor Yellow
    .\Extract-Chat-Agent-Features.ps1 @extractArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Feature extraction complete" -ForegroundColor Green
        
        # Update usage statistics
        $extractedFiles = (Get-ChildItem $OutputPath -Recurse -File).Count
        Write-Host "✓ Extracted $extractedFiles feature files" -ForegroundColor Green
    } else {
        Write-Error "Feature extraction failed!"
    }
}

function Invoke-ShowLicenseInfo {
    if ($global:ValidatedLicense) {
        Write-Host "`nLICENSE INFORMATION:" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "License ID:    $($global:ValidatedLicense.LicenseId)" -ForegroundColor White
        Write-Host "Licensee:      $($global:ValidatedLicense.LicenseeName)" -ForegroundColor White
        Write-Host "Email:         $($global:ValidatedLicense.LicenseeEmail)" -ForegroundColor White
        Write-Host "Type:          $($global:ValidatedLicense.LicenseType)" -ForegroundColor White
        Write-Host "Issued:        $($global:ValidatedLicense.IssuedDate)" -ForegroundColor White
        Write-Host "Expires:       $($global:ValidatedLicense.ExpirationDate)" -ForegroundColor $(if (([DateTime]::ParseExact($global:ValidatedLicense.ExpirationDate, "yyyy-MM-dd HH:mm:ss", $null) -lt (Get-Date).AddDays(30)) { "Red" } else { "Green" })
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        
        Write-Host "`nFEATURES ENABLED:" -ForegroundColor Cyan
        foreach ($feature in $global:ValidatedLicense.Features) {
            Write-Host "  ✓ $feature" -ForegroundColor Green
        }
        
        Write-Host "`nRESTRICTIONS:" -ForegroundColor Cyan
        Write-Host "  Max Concurrent Users: $($global:ValidatedLicense.Restrictions.MaxConcurrentUsers)" -ForegroundColor White
        Write-Host "  Max Extracted Files:  $($global:ValidatedLicense.Restrictions.MaxExtractedFiles)" -ForegroundColor White
        Write-Host "  Commercial Use:       $($global:ValidatedLicense.Restrictions.CommercialUse)" -ForegroundColor White
    } else {
        Write-Error "No license loaded. Run validation first."
    }
}

# Main execution
Write-Host @"
╔══════════════════════════════════════════════════════════════════╗
║     OMEGA-REVERSER TOOLKIT v5.0                                ║
║     The Ultimate Reverse Engineering Suite                     ║
╚══════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

$startTime = Get-Date

# Show help if requested
if ($Help) {
    Show-Help
    exit 0
}

# Validate license if files provided
if ($LicenseFile -and $ExchangeKeyFile) {
    Write-Host "`n[1/2] Validating license..." -ForegroundColor Yellow
    
    $validationResult = Invoke-LicenseValidation -LicenseFile $LicenseFile -ExchangeKeyFile $ExchangeKeyFile -MasterKeyFile $MasterKeyFile
    
    if (-not $validationResult) {
        Write-Error "License validation failed!"
        exit 1
    }
    
    Write-Host "✓ License validation successful" -ForegroundColor Green
}

# Show license info if requested
if ($ShowLicenseInfo) {
    Invoke-ShowLicenseInfo
    exit 0
}

# Validate only mode
if ($ValidateOnly) {
    if ($global:LicenseValidated) {
        Write-Host "✓ License is valid and ready for use" -ForegroundColor Green
        exit 0
    } else {
        Write-Error "License not validated!"
        exit 1
    }
}

# Execute command
if ($Command) {
    if (-not $global:LicenseValidated) {
        Write-Error "License must be validated before running commands!"
        Write-Error "Use -LicenseFile and -ExchangeKeyFile parameters."
        exit 1
    }
    
    Write-Host "`n[2/2] Executing command: $Command" -ForegroundColor Yellow
    
    switch ($Command.ToLower()) {
        "reverse-install" {
            if (-not $InputPath -or -not $OutputPath) {
                Write-Error "InputPath and OutputPath required for reverse-install"
                exit 1
            }
            Invoke-ReverseInstall -InputPath $InputPath -OutputPath $OutputPath
        }
        "deobfuscate" {
            if (-not $InputPath -or -not $OutputPath) {
                Write-Error "InputPath and OutputPath required for deobfuscate"
                exit 1
            }
            Invoke-Deobfuscate -InputPath $InputPath -OutputPath $OutputPath
        }
        "extract-features" {
            if (-not $InputPath -or -not $OutputPath) {
                Write-Error "InputPath and OutputPath required for extract-features"
                exit 1
            }
            Invoke-ExtractFeatures -InputPath $InputPath -OutputPath $OutputPath
        }
        "validate-license" {
            # Already validated above
            Write-Host "✓ License is valid" -ForegroundColor Green
        }
        default {
            Write-Error "Unknown command: $Command"
            Write-Error "Use -Help to see available commands"
            exit 1
        }
    }
} elseif (-not $Help -and -not $ShowLicenseInfo -and -not $ValidateOnly) {
    Write-Error "No command specified!"
    Write-Error "Use -Help to see available commands"
    exit 1
}

$duration = (Get-Date) - $startTime

Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║                    OPERATION COMPLETE                            ║
╚══════════════════════════════════════════════════════════════════╝
Duration: $($duration.ToString('hh\:mm\:ss'))

$(if ($global:LicenseValidated) { "✓ License validated and active" } else { "✗ License not validated" })
$(if ($Command) { "✓ Command executed: $Command" })

OMEGA-REVERSER TOOLKIT v5.0
"Extract anything. Protect everything."
"@ -ForegroundColor $(if ($global:LicenseValidated) { "Green" } else { "Red" })
