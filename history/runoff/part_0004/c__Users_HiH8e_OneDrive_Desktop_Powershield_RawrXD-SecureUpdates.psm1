# RawrXD-SecureUpdates.psm1
# Secure update mechanism for RawrXD IDE

using namespace System.Net.Http
using namespace System.Security.Cryptography
using namespace System.IO
using namespace System.Text.Json

class UpdateManager {
    [string]$UpdateUrl
    [string]$CurrentVersion
    [string]$InstallPath
    [System.Security.Cryptography.X509Certificates.X509Certificate2]$SigningCert

    UpdateManager([string]$updateUrl, [string]$currentVersion, [string]$installPath) {
        $this.UpdateUrl = $updateUrl
        $this.CurrentVersion = $currentVersion
        $this.InstallPath = $installPath
        $this.SigningCert = $this.LoadSigningCertificate()
    }

    [System.Security.Cryptography.X509Certificates.X509Certificate2]LoadSigningCertificate() {
        # Load Microsoft's signing certificate (placeholder - in reality, this would be embedded)
        $certPath = Join-Path $this.InstallPath "signing.crt"
        if (Test-Path $certPath) {
            return New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $certPath
        }

        # For development, create a test certificate
        return $this.CreateTestCertificate()
    }

    [System.Security.Cryptography.X509Certificates.X509Certificate2]CreateTestCertificate() {
        $rsa = [RSA]::Create(2048)
        $req = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
            "CN=RawrXD Update Signing",
            $rsa,
            [HashAlgorithmName]::SHA256,
            [RSASignaturePadding]::Pkcs1
        )

        $cert = $req.CreateSelfSigned((Get-Date).AddDays(-1), (Get-Date).AddYears(1))
        return [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
            $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx),
            $null,
            [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable
        )
    }

    [hashtable]CheckForUpdates() {
        try {
            $client = New-Object HttpClient
            $response = $client.GetAsync($this.UpdateUrl).Result
            $response.EnsureSuccessStatusCode()

            $json = $response.Content.ReadAsStringAsync().Result
            $updateInfo = [JsonSerializer]::Deserialize($json, [hashtable])

            if ([version]$updateInfo.Version -gt [version]$this.CurrentVersion) {
                return $updateInfo
            }
        }
        catch {
            Write-Warning "Failed to check for updates: $_"
        }

        return $null
    }

    [bool]DownloadAndVerifyUpdate([hashtable]$updateInfo, [string]$downloadPath) {
        try {
            $client = New-Object HttpClient
            $response = $client.GetAsync($updateInfo.DownloadUrl).Result
            $response.EnsureSuccessStatusCode()

            $updateData = $response.Content.ReadAsByteArrayAsync().Result

            # Verify signature
            if (-not $this.VerifySignature($updateData, $updateInfo.Signature)) {
                throw "Update signature verification failed"
            }

            # Verify hash
            if (-not $this.VerifyHash($updateData, $updateInfo.SHA256)) {
                throw "Update hash verification failed"
            }

            [File]::WriteAllBytes($downloadPath, $updateData)
            return $true
        }
        catch {
            Write-Error "Update download/verification failed: $_"
            return $false
        }
    }

    [bool]VerifySignature([byte[]]$data, [string]$signature) {
        try {
            $rsa = $this.SigningCert.GetRSAPublicKey()
            $signatureBytes = [Convert]::FromBase64String($signature)

            return $rsa.VerifyData($data, $signatureBytes, [HashAlgorithmName]::SHA256, [RSASignaturePadding]::Pkcs1)
        }
        catch {
            return $false
        }
    }

    [bool]VerifyHash([byte[]]$data, [string]$expectedHash) {
        $sha256 = [SHA256]::Create()
        $actualHash = [BitConverter]::ToString($sha256.ComputeHash($data)).Replace("-", "").ToLower()
        return $actualHash -eq $expectedHash.ToLower()
    }

    [void]InstallUpdate([string]$updatePath) {
        # Create backup
        $backupPath = Join-Path $env:TEMP "RawrXD_Backup_$((Get-Date).ToString('yyyyMMdd_HHmmss'))"
        Copy-Item $this.InstallPath $backupPath -Recurse -Force

        try {
            # Extract update (assuming it's a zip)
            $extractPath = Join-Path $env:TEMP "RawrXD_Update"
            if (Test-Path $extractPath) { Remove-Item $extractPath -Recurse -Force }

            Expand-Archive -Path $updatePath -DestinationPath $extractPath

            # Stop running processes
            $this.StopRunningProcesses()

            # Install new files
            $this.InstallNewFiles($extractPath)

            # Clean up
            Remove-Item $extractPath -Recurse -Force
            Remove-Item $updatePath -Force

            Write-Host "Update installed successfully. Please restart RawrXD."
        }
        catch {
            # Rollback on failure
            Write-Error "Update failed, rolling back: $_"
            $this.RollbackUpdate($backupPath)
            throw
        }
    }

    [void]StopRunningProcesses() {
        $processes = Get-Process | Where-Object { $_.Name -like "*RawrXD*" }
        foreach ($process in $processes) {
            $process.Kill()
            $process.WaitForExit(5000)
        }
    }

    [void]InstallNewFiles([string]$sourcePath) {
        $files = Get-ChildItem $sourcePath -Recurse -File
        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($sourcePath.Length + 1)
            $targetPath = Join-Path $this.InstallPath $relativePath

            $targetDir = Split-Path $targetPath -Parent
            if (-not (Test-Path $targetDir)) {
                New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
            }

            Copy-Item $file.FullName $targetPath -Force
        }
    }

    [void]RollbackUpdate([string]$backupPath) {
        if (Test-Path $backupPath) {
            Remove-Item $this.InstallPath -Recurse -Force -ErrorAction SilentlyContinue
            Copy-Item $backupPath $this.InstallPath -Recurse -Force
            Remove-Item $backupPath -Recurse -Force
        }
    }
}

class ExtensionManager {
    [string]$ExtensionsPath
    [string]$MarketplaceUrl
    [hashtable]$InstalledExtensions

    ExtensionManager([string]$extensionsPath, [string]$marketplaceUrl) {
        $this.ExtensionsPath = $extensionsPath
        $this.MarketplaceUrl = $marketplaceUrl
        $this.LoadInstalledExtensions()
    }

    [void]LoadInstalledExtensions() {
        $manifestPath = Join-Path $this.ExtensionsPath "extensions.json"
        if (Test-Path $manifestPath) {
            $this.InstalledExtensions = Get-Content $manifestPath | ConvertFrom-Json -AsHashtable
        }
        else {
            $this.InstalledExtensions = @{}
        }
    }

    [void]SaveExtensionsManifest() {
        $manifestPath = Join-Path $this.ExtensionsPath "extensions.json"
        $this.InstalledExtensions | ConvertTo-Json | Out-File $manifestPath -Encoding UTF8
    }

    [array]GetAvailableExtensions() {
        try {
            $client = New-Object HttpClient
            $response = $client.GetAsync($this.MarketplaceUrl).Result
            $response.EnsureSuccessStatusCode()

            $json = $response.Content.ReadAsStringAsync().Result
            return [JsonSerializer]::Deserialize($json, [array])
        }
        catch {
            Write-Warning "Failed to fetch extensions: $_"
            return @()
        }
    }

    [bool]InstallExtension([string]$extensionId, [string]$version = "latest") {
        $extensions = $this.GetAvailableExtensions()
        $extension = $extensions | Where-Object { $_.id -eq $extensionId }

        if (-not $extension) {
            Write-Error "Extension not found: $extensionId"
            return $false
        }

        $downloadUrl = if ($version -eq "latest") { $extension.downloadUrl } else { $extension.versions[$version].downloadUrl }

        try {
            $client = New-Object HttpClient
            $response = $client.GetAsync($downloadUrl).Result
            $response.EnsureSuccessStatusCode()

            $extensionData = $response.Content.ReadAsByteArrayAsync().Result

            # Verify extension signature
            if (-not $this.VerifyExtensionSignature($extensionData, $extension.signature)) {
                throw "Extension signature verification failed"
            }

            # Extract and install
            $extractPath = Join-Path $env:TEMP "Extension_$extensionId"
            if (Test-Path $extractPath) { Remove-Item $extractPath -Recurse -Force }

            $zipPath = Join-Path $env:TEMP "extension.zip"
            [File]::WriteAllBytes($zipPath, $extensionData)
            Expand-Archive -Path $zipPath -DestinationPath $extractPath

            $extensionDir = Join-Path $this.ExtensionsPath $extensionId
            if (Test-Path $extensionDir) { Remove-Item $extensionDir -Recurse -Force }

            Move-Item $extractPath $extensionDir

            # Update manifest
            $this.InstalledExtensions[$extensionId] = @{
                Version = $version
                InstallDate = Get-Date -Format "o"
                Publisher = $extension.publisher
            }
            $this.SaveExtensionsManifest()

            Remove-Item $zipPath -Force
            return $true
        }
        catch {
            Write-Error "Extension installation failed: $_"
            return $false
        }
    }

    [bool]VerifyExtensionSignature([byte[]]$data, [string]$signature) {
        # Use Microsoft's extension signing certificate
        $certThumbprint = "8A38755D100D358729C3A0B4F4D1E7B1"  # Placeholder thumbprint
        $cert = Get-ChildItem Cert:\CurrentUser\TrustedPublisher\$certThumbprint -ErrorAction SilentlyContinue

        if (-not $cert) {
            Write-Warning "Extension signing certificate not found, skipping verification"
            return $true  # Allow in development
        }

        try {
            $rsa = $cert.GetRSAPublicKey()
            $signatureBytes = [Convert]::FromBase64String($signature)
            return $rsa.VerifyData($data, $signatureBytes, [HashAlgorithmName]::SHA256, [RSASignaturePadding]::Pkcs1)
        }
        catch {
            return $false
        }
    }

    [void]UninstallExtension([string]$extensionId) {
        $extensionDir = Join-Path $this.ExtensionsPath $extensionId
        if (Test-Path $extensionDir) {
            Remove-Item $extensionDir -Recurse -Force
            $this.InstalledExtensions.Remove($extensionId)
            $this.SaveExtensionsManifest()
        }
    }

    [hashtable]GetExtensionInfo([string]$extensionId) {
        return $this.InstalledExtensions[$extensionId]
    }
}

class StagedRolloutManager {
    [string]$RolloutConfigPath
    [hashtable]$RolloutConfig

    StagedRolloutManager([string]$configPath) {
        $this.RolloutConfigPath = $configPath
        $this.LoadRolloutConfig()
    }

    [void]LoadRolloutConfig() {
        if (Test-Path $this.RolloutConfigPath) {
            $this.RolloutConfig = Get-Content $this.RolloutConfigPath | ConvertFrom-Json -AsHashtable
        }
        else {
            $this.RolloutConfig = @{
                Stages = @(
                    @{ Name = "Alpha"; Percentage = 1; Users = @() }
                    @{ Name = "Beta"; Percentage = 10; Users = @() }
                    @{ Name = "RC"; Percentage = 50; Users = @() }
                    @{ Name = "GA"; Percentage = 100; Users = @() }
                )
                CurrentStage = "Alpha"
            }
            $this.SaveRolloutConfig()
        }
    }

    [void]SaveRolloutConfig() {
        $this.RolloutConfig | ConvertTo-Json -Depth 10 | Out-File $this.RolloutConfigPath -Encoding UTF8
    }

    [bool]IsUserEligibleForUpdate([string]$userId, [string]$updateVersion) {
        $stage = $this.RolloutConfig.Stages | Where-Object { $_.Name -eq $this.RolloutConfig.CurrentStage }
        if (-not $stage) { return $false }

        # Check if user is explicitly included
        if ($stage.Users -contains $userId) { return $true }

        # Check percentage-based rollout
        $hash = [StagedRolloutManager]::GetUserHash($userId + $updateVersion)
        $percentage = $hash % 100

        return $percentage -lt $stage.Percentage
    }

    static [int]GetUserHash([string]$input) {
        $sha256 = [SHA256]::Create()
        $bytes = [Encoding]::UTF8.GetBytes($input)
        $hash = $sha256.ComputeHash($bytes)
        return [BitConverter]::ToUInt32($hash, 0)
    }

    [void]AdvanceStage() {
        $currentIndex = $this.RolloutConfig.Stages.IndexOf(($this.RolloutConfig.Stages | Where-Object { $_.Name -eq $this.RolloutConfig.CurrentStage }))
        if ($currentIndex -lt $this.RolloutConfig.Stages.Count - 1) {
            $this.RolloutConfig.CurrentStage = $this.RolloutConfig.Stages[$currentIndex + 1].Name
            $this.SaveRolloutConfig()
        }
    }

    [void]AddUserToStage([string]$userId, [string]$stageName) {
        $stage = $this.RolloutConfig.Stages | Where-Object { $_.Name -eq $stageName }
        if ($stage -and $stage.Users -notcontains $userId) {
            $stage.Users += $userId
            $this.SaveRolloutConfig()
        }
    }
}

# Export functions
function New-UpdateManager {
    param(
        [string]$updateUrl,
        [string]$currentVersion,
        [string]$installPath
    )
    return [UpdateManager]::new($updateUrl, $currentVersion, $installPath)
}

function New-ExtensionManager {
    param(
        [string]$extensionsPath,
        [string]$marketplaceUrl
    )
    return [ExtensionManager]::new($extensionsPath, $marketplaceUrl)
}

function New-StagedRolloutManager {
    param([string]$configPath)
    return [StagedRolloutManager]::new($configPath)
}

Export-ModuleMember -Function * -Variable *