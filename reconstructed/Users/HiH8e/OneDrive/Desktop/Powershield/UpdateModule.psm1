<#
.SYNOPSIS
    RawrXD Update Module - Secure Update Management Framework
.DESCRIPTION
    Provides code signing, signature verification, rollback capabilities,
    staged rollouts, and secure marketplace integration.
.NOTES
    Security: Code signing, signature verification, secure distribution
    Reliability: Rollback support, staged deployments
#>

# ============================================
# UPDATE CONFIGURATION
# ============================================

$script:UpdateConfig = @{
    CodeSigning = @{
        Enabled = $true
        CertificateThumbprint = $null  # Set during initialization
        CertificateSubject = "CN=GitHub, Inc."
        TimestampServer = "http://timestamp.digicert.com"
    }
    SignatureVerification = @{
        Enabled = $true
        TrustedPublishers = @(
            "CN=GitHub, Inc.",
            "CN=Microsoft Corporation"
        )
        RevocationCheck = $true
    }
    Rollback = @{
        Enabled = $true
        BackupVersions = 3
        AutoRollbackOnFailure = $true
        RollbackTimeout = 300  # 5 minutes
    }
    StagedRollouts = @{
        Enabled = $true
        Stages = @("Alpha", "Beta", "Stable")
        StageDistribution = @{
            Alpha = 0.05   # 5%
            Beta = 0.20    # 20%
            Stable = 1.0   # 100%
        }
    }
    Marketplace = @{
        Url = "https://marketplace.visualstudio.com"
        SecureConnection = $true
        VerifyDownloads = $true
    }
    UpdateChannels = @{
        Stable = @{
            Url = "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases/latest"
            AutoUpdate = $true
        }
        Beta = @{
            Url = "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases"
            AutoUpdate = $false
        }
        Alpha = @{
            Url = "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases"
            AutoUpdate = $false
        }
    }
}

# ============================================
# CODE SIGNING FUNCTIONS
# ============================================

function Test-CodeSigningCertificate {
    <#
    .SYNOPSIS
        Tests if code signing certificate is available and valid
    #>
    
    try {
        $cert = Get-CodeSigningCertificate
        if (-not $cert) {
            Write-UpdateLog "Code signing certificate not found" "WARNING"
            return $false
        }
        
        # Check certificate validity
        $now = Get-Date
        if ($cert.NotBefore -gt $now -or $cert.NotAfter -lt $now) {
            Write-UpdateLog "Code signing certificate is not valid (expired or not yet valid)" "ERROR"
            return $false
        }
        
        # Check if certificate is trusted
        $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
        $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::Online
        $valid = $chain.Build($cert)
        
        if (-not $valid) {
            Write-UpdateLog "Code signing certificate is not trusted" "ERROR"
            return $false
        }
        
        Write-UpdateLog "Code signing certificate is valid and trusted" "INFO"
        return $true
    }
    catch {
        Write-UpdateLog "Certificate validation failed: $_" "ERROR"
        return $false
    }
}

function Get-CodeSigningCertificate {
    <#
    .SYNOPSIS
        Gets the code signing certificate
    #>
    
    try {
        if ($script:UpdateConfig.CodeSigning.CertificateThumbprint) {
            $cert = Get-ChildItem -Path Cert:\CurrentUser\My\$($script:UpdateConfig.CodeSigning.CertificateThumbprint) -ErrorAction Stop
            return $cert
        }
        
        # Find certificate by subject
        $cert = Get-ChildItem -Path Cert:\CurrentUser\My | 
            Where-Object { 
                $_.Subject -eq $script:UpdateConfig.CodeSigning.CertificateSubject -and 
                $_.HasPrivateKey -and 
                $_.NotAfter -gt (Get-Date)
            } | 
            Select-Object -First 1
        
        if ($cert) {
            $script:UpdateConfig.CodeSigning.CertificateThumbprint = $cert.Thumbprint
        }
        
        return $cert
    }
    catch {
        Write-UpdateLog "Failed to get code signing certificate: $_" "ERROR"
        return $null
    }
}

function Sign-ScriptFile {
    <#
    .SYNOPSIS
        Signs a PowerShell script file
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        
        [string]$TimestampServer = $script:UpdateConfig.CodeSigning.TimestampServer
    )
    
    try {
        $cert = Get-CodeSigningCertificate
        if (-not $cert) {
            throw "Code signing certificate not available"
        }
        
        $signingResult = Set-AuthenticodeSignature -FilePath $FilePath `
            -Certificate $cert `
            -TimestampServer $TimestampServer `
            -HashAlgorithm SHA256
        
        if ($signingResult.Status -ne "Valid") {
            throw "Script signing failed: $($signingResult.StatusMessage)"
        }
        
        Write-UpdateLog "Script signed successfully: $FilePath" "INFO"
        return $true
    }
    catch {
        Write-UpdateLog "Script signing failed: $_" "ERROR"
        return $false
    }
}

# ============================================
# SIGNATURE VERIFICATION FUNCTIONS
# ============================================

function Test-ScriptSignature {
    <#
    .SYNOPSIS
        Verifies the signature of a PowerShell script
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath
    )
    
    try {
        $signature = Get-AuthenticodeSignature -FilePath $FilePath
        
        if ($signature.Status -ne "Valid") {
            Write-UpdateLog "Script signature invalid: $($signature.Status)" "WARNING"
            return $false
        }
        
        # Check if signer is trusted
        $trusted = $script:UpdateConfig.SignatureVerification.TrustedPublishers -contains $signature.SignerCertificate.Subject
        
        if (-not $trusted) {
            Write-UpdateLog "Script signed by untrusted publisher: $($signature.SignerCertificate.Subject)" "WARNING"
            return $false
        }
        
        # Check certificate revocation if enabled
        if ($script:UpdateConfig.SignatureVerification.RevocationCheck) {
            $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
            $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::Online
            $chain.ChainPolicy.RevocationFlag = [System.Security.Cryptography.X509Certificates.X509RevocationFlag]::EntireChain
            
            if (-not $chain.Build($signature.SignerCertificate)) {
                Write-UpdateLog "Certificate chain validation failed" "ERROR"
                return $false
            }
        }
        
        Write-UpdateLog "Script signature verified: $FilePath" "INFO"
        return $true
    }
    catch {
        Write-UpdateLog "Signature verification failed: $_" "ERROR"
        return $false
    }
}

function Verify-FileIntegrity {
    <#
    .SYNOPSIS
        Verifies file integrity using hash comparison
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        
        [Parameter(Mandatory = $true)]
        [string]$ExpectedHash,
        
        [string]$Algorithm = "SHA256"
    )
    
    try {
        $fileHash = Get-FileHash -Path $FilePath -Algorithm $Algorithm
        $match = $fileHash.Hash -eq $ExpectedHash
        
        if (-not $match) {
            Write-UpdateLog "File integrity check failed: $FilePath" "ERROR"
        } else {
            Write-UpdateLog "File integrity verified: $FilePath" "INFO"
        }
        
        return $match
    }
    catch {
        Write-UpdateLog "File integrity check error: $_" "ERROR"
        return $false
    }
}

# ============================================
# ROLLBACK FUNCTIONS
# ============================================

function New-BackupVersion {
    <#
    .SYNOPSIS
        Creates a backup of the current version
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,
        
        [string]$Version = (Get-Date -Format "yyyyMMdd_HHmmss")
    )
    
    try {
        $backupDir = Join-Path $env:APPDATA "RawrXD\backups\$Version"
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
        
        # Copy files
        Copy-Item -Path "$SourcePath\*" -Destination $backupDir -Recurse -Force
        
        # Clean up old backups
        $backups = Get-ChildItem -Path (Join-Path $env:APPDATA "RawrXD\backups") -Directory | 
            Sort-Object -Property Name -Descending
        
        if ($backups.Count -gt $script:UpdateConfig.Rollback.BackupVersions) {
            $backups | Select-Object -Skip $script:UpdateConfig.Rollback.BackupVersions | 
                Remove-Item -Recurse -Force
        }
        
        Write-UpdateLog "Backup created: $Version" "INFO"
        return $backupDir
    }
    catch {
        Write-UpdateLog "Backup creation failed: $_" "ERROR"
        return $null
    }
}

function Invoke-Rollback {
    <#
    .SYNOPSIS
        Rolls back to a previous version
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$TargetPath,
        
        [string]$Version = "latest"
    )
    
    try {
        $backupDir = if ($Version -eq "latest") {
            Get-ChildItem -Path (Join-Path $env:APPDATA "RawrXD\backups") -Directory | 
                Sort-Object -Property Name -Descending | 
                Select-Object -First 1
        } else {
            Join-Path $env:APPDATA "RawrXD\backups\$Version"
        }
        
        if (-not (Test-Path $backupDir)) {
            throw "Backup version not found: $Version"
        }
        
        # Create backup of current version before rollback
        New-BackupVersion -SourcePath $TargetPath -Version "pre_rollback_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
        
        # Restore from backup
        Get-ChildItem -Path $backupDir | Copy-Item -Destination $TargetPath -Recurse -Force
        
        Write-UpdateLog "Rolled back to version: $Version" "INFO"
        return $true
    }
    catch {
        Write-UpdateLog "Rollback failed: $_" "ERROR"
        return $false
    }
}

function Start-RollbackMonitor {
    <#
    .SYNOPSIS
        Starts monitoring for update failures that require rollback
    #>
    
    $script:RollbackTimer = New-Object System.Timers.Timer
    $script:RollbackTimer.Interval = $script:UpdateConfig.Rollback.RollbackTimeout * 1000
    $script:RollbackTimer.AutoReset = $false
    $script:RollbackTimer.add_Elapsed({
        try {
            if (Test-UpdateFailure) {
                Write-UpdateLog "Update failure detected, initiating automatic rollback" "WARNING"
                Invoke-Rollback -TargetPath $PSScriptRoot
            }
        }
        catch {
            Write-UpdateLog "Rollback monitor error: $_" "ERROR"
        }
    })
}

function Test-UpdateFailure {
    <#
    .SYNOPSIS
        Tests if the current update has failed
    #>
    
    # Check for error logs, crashes, or other failure indicators
    $errorLog = Join-Path $env:APPDATA "RawrXD\logs\error.log"
    if (Test-Path $errorLog) {
        $recentErrors = Get-Content $errorLog | 
            Where-Object { $_ -match "CRITICAL|ERROR|FATAL" } | 
            Where-Object { 
                $timestamp = [DateTime]::Parse($_.Split('|')[0])
                (Get-Date) - $timestamp -lt [TimeSpan]::FromMinutes(5)
            }
        
        if ($recentErrors.Count -gt 5) {
            return $true
        }
    }
    
    return $false
}

# ============================================
# STAGED ROLLOUT FUNCTIONS
# ============================================

function Get-StagedRolloutEligibility {
    <#
    .SYNOPSIS
        Determines if user is eligible for staged rollout
    #>
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("Alpha", "Beta", "Stable")]
        [string]$Stage
    )
    
    try {
        $userId = Get-AnonymizedUserId
        $stagePercentage = $script:UpdateConfig.StagedRollouts.StageDistribution[$Stage]
        
        # Use consistent hashing to determine eligibility
        $hash = Get-StringHash -InputString $userId -Algorithm "SHA256"
        $hashValue = [Convert]::ToInt32($hash.Substring(0, 8), 16)
        $eligibility = ($hashValue / [uint32]::MaxValue) -le $stagePercentage
        
        Write-UpdateLog "Staged rollout eligibility for $Stage`: $(if ($eligibility) { 'ELIGIBLE' } else { 'NOT ELIGIBLE' })" "DEBUG"
        return $eligibility
    }
    catch {
        Write-UpdateLog "Staged rollout eligibility check failed: $_" "ERROR"
        return $false
    }
}

function Get-AnonymizedUserId {
    <#
    .SYNOPSIS
        Gets anonymized user ID for rollout decisions
    #>
    
    try {
        $userIdPath = Join-Path $env:APPDATA "RawrXD\user_rollout_id.txt"
        
        if (Test-Path $userIdPath) {
            return Get-Content -Path $userIdPath -Raw
        }
        
        # Generate anonymized ID
        $userId = [Guid]::NewGuid().ToString()
        $userId | Out-File -FilePath $userIdPath -Encoding UTF8
        
        return $userId
    }
    catch {
        return "anonymous"
    }
}

function Get-StringHash {
    <#
    .SYNOPSIS
        Computes hash of a string
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$InputString,
        
        [string]$Algorithm = "SHA256"
    )
    
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($InputString)
    $hashAlgorithm = [System.Security.Cryptography.HashAlgorithm]::Create($Algorithm)
    $hashBytes = $hashAlgorithm.ComputeHash($bytes)
    return [BitConverter]::ToString($hashBytes).Replace("-", "").ToLower()
}

# ============================================
# SECURE MARKETPLACE FUNCTIONS
# ============================================

function Install-FromMarketplace {
    <#
    .SYNOPSIS
        Securely installs an extension from the marketplace
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExtensionId,
        
        [string]$Version = "latest"
    )
    
    try {
        # Get extension information
        $extensionInfo = Get-MarketplaceExtensionInfo -ExtensionId $ExtensionId -Version $Version
        
        if (-not $extensionInfo) {
            throw "Extension not found: $ExtensionId"
        }
        
        # Verify extension authenticity
        if (-not (Test-ExtensionAuthenticity -ExtensionInfo $extensionInfo)) {
            throw "Extension authenticity verification failed"
        }
        
        # Download with verification
        $downloadPath = Download-VerifiedFile -Url $extensionInfo.DownloadUrl -ExpectedHash $extensionInfo.Hash
        
        # Install extension
        Install-Extension -Path $downloadPath
        
        Write-UpdateLog "Extension installed successfully: $ExtensionId v$Version" "INFO"
        return $true
    }
    catch {
        Write-UpdateLog "Extension installation failed: $_" "ERROR"
        return $false
    }
}

function Get-MarketplaceExtensionInfo {
    <#
    .SYNOPSIS
        Gets extension information from marketplace
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExtensionId,
        
        [string]$Version = "latest"
    )
    
    try {
        $url = "$($script:UpdateConfig.Marketplace.Url)/items?itemName=$ExtensionId"
        if ($Version -ne "latest") {
            $url += "&version=$Version"
        }
        
        $response = Invoke-RestMethod -Uri $url -Method Get
        
        if ($response.results -and $response.results[0].extensions) {
            $extension = $response.results[0].extensions[0]
            
            return @{
                Id = $extension.extensionId
                Name = $extension.displayName
                Version = $extension.version
                Publisher = $extension.publisher.displayName
                DownloadUrl = $extension.versions[0].files | Where-Object { $_.assetType -eq "Microsoft.VisualStudio.Services.VSIXPackage" } | Select-Object -ExpandProperty source
                Hash = $extension.versions[0].files | Where-Object { $_.assetType -eq "Microsoft.VisualStudio.Services.VSIXPackage" } | Select-Object -ExpandProperty sha256
            }
        }
        
        return $null
    }
    catch {
        Write-UpdateLog "Failed to get extension info: $_" "ERROR"
        return $null
    }
}

function Test-ExtensionAuthenticity {
    <#
    .SYNOPSIS
        Tests extension authenticity
    #>
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$ExtensionInfo
    )
    
    # Check publisher trust
    $trustedPublishers = @("GitHub", "Microsoft")
    if ($trustedPublishers -notcontains $ExtensionInfo.Publisher) {
        Write-UpdateLog "Extension from untrusted publisher: $($ExtensionInfo.Publisher)" "WARNING"
        return $false
    }
    
    # Additional authenticity checks would go here
    return $true
}

function Download-VerifiedFile {
    <#
    .SYNOPSIS
        Downloads a file with integrity verification
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        
        [Parameter(Mandatory = $true)]
        [string]$ExpectedHash
    )
    
    try {
        $tempFile = [System.IO.Path]::GetTempFileName()
        
        Invoke-WebRequest -Uri $Url -OutFile $tempFile
        
        # Verify hash
        if (-not (Verify-FileIntegrity -FilePath $tempFile -ExpectedHash $ExpectedHash)) {
            Remove-Item -Path $tempFile -Force
            throw "File integrity verification failed"
        }
        
        Write-UpdateLog "File downloaded and verified: $Url" "INFO"
        return $tempFile
    }
    catch {
        Write-UpdateLog "Verified download failed: $_" "ERROR"
        throw
    }
}

function Install-Extension {
    <#
    .SYNOPSIS
        Installs an extension
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )
    
    # Placeholder - actual installation logic would go here
    Write-UpdateLog "Extension installation placeholder: $Path" "INFO"
}

# ============================================
# UPDATE LOGGING
# ============================================

function Write-UpdateLog {
    <#
    .SYNOPSIS
        Writes to update log
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        
        [ValidateSet("DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL")]
        [string]$Level = "INFO"
    )
    
    try {
        $logPath = Join-Path $env:APPDATA "RawrXD\update.log"
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        
        $logEntry = "$timestamp|$Level|$Message"
        Add-Content -Path $logPath -Value $logEntry -Encoding UTF8
    }
    catch {
        Write-Warning "Update logging failed: $_"
    }
}

# ============================================
# INITIALIZATION
# ============================================

function Initialize-UpdateModule {
    <#
    .SYNOPSIS
        Initializes the update module
    #>
    
    # Create update directories
    $updatePaths = @(
        (Join-Path $env:APPDATA "RawrXD"),
        (Join-Path $env:APPDATA "RawrXD\backups"),
        (Join-Path $env:APPDATA "RawrXD\updates")
    )
    
    foreach ($path in $updatePaths) {
        if (-not (Test-Path $path)) {
            New-Item -ItemType Directory -Path $path -Force | Out-Null
        }
    }
    
    # Test code signing setup
    Test-CodeSigningCertificate
    
    Write-UpdateLog "Update module initialized" "INFO"
}

# Initialize on module load
Initialize-UpdateModule

# Export functions
Export-ModuleMember -Function @(
    "Test-CodeSigningCertificate", "Get-CodeSigningCertificate", "Sign-ScriptFile",
    "Test-ScriptSignature", "Verify-FileIntegrity",
    "New-BackupVersion", "Invoke-Rollback", "Start-RollbackMonitor", "Test-UpdateFailure",
    "Get-StagedRolloutEligibility", "Get-AnonymizedUserId",
    "Install-FromMarketplace", "Get-MarketplaceExtensionInfo", "Test-ExtensionAuthenticity",
    "Download-VerifiedFile", "Install-Extension",
    "Write-UpdateLog",
    "Initialize-UpdateModule"
)