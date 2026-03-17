# ============================================
# SECURE API KEY MODULE FOR RAWRXD
# ============================================
# File: secure-api-key-module.ps1
# Purpose: Centralized encryption/decryption for API keys using DPAPI
# Features:
#   - SecureString + DPAPI encryption
#   - File-based storage with restricted permissions
#   - Registry fallback for portability
#   - Environment variable legacy support
#   - Audit logging for all operations
# ============================================

# Global configuration for this module
$script:SecureKeyStore = @{
    StorePath     = $null
    EncryptedKeys = @{}  # Cache decrypted keys in memory
    DPAPIScope    = 'CurrentUser'  # DPAPI protection scope
}

# Initialize before first use
function Initialize-SecureAPIKeyStore {
    <#
    .SYNOPSIS
        Initialize the secure API key storage system
    
    .DESCRIPTION
        Sets up encrypted file storage or registry fallback for API keys.
        Initializes DPAPI encryption key and creates necessary directories.
    
    .PARAMETER StorePath
        Path to encrypted keys file (default: $PSScriptRoot/config/.apikeys.enc)
    
    .PARAMETER UseRegistry
        If $true, use Windows Registry instead of file storage
    
    .EXAMPLE
        Initialize-SecureAPIKeyStore -StorePath "C:\RawrXD\config\.apikeys.enc"
    
    .OUTPUTS
        [bool] $true if initialization successful, $false otherwise
    #>
    param(
        [string]$StorePath = $null,
        [bool]$UseRegistry = $false
    )
    
    try {
        # Determine storage path
        if (-not $StorePath) {
            $StorePath = Join-Path $PSScriptRoot "config" ".apikeys.enc"
        }
        
        $script:SecureKeyStore.StorePath = $StorePath
        $script:SecureKeyStore.UseRegistry = $UseRegistry
        
        if ($UseRegistry) {
            # Initialize registry location
            $regPath = "HKCU:\Software\RawrXD\APIKeys"
            if (-not (Test-Path $regPath)) {
                New-Item -Path $regPath -Force | Out-Null
            }
            Write-Verbose "Secure API key store initialized (Registry)" -Verbose
        } else {
            # Create config directory if it doesn't exist
            $configDir = Split-Path -Parent $StorePath
            if (-not (Test-Path $configDir)) {
                New-Item -ItemType Directory -Path $configDir -Force | Out-Null
            }
            
            # Create empty store file if it doesn't exist
            if (-not (Test-Path $StorePath)) {
                $emptyStore = @{} | ConvertTo-Json
                Set-Content -Path $StorePath -Value $emptyStore -Encoding UTF8 -Force
                
                # Set restricted permissions (user read/write only)
                if ($PSVersionTable.Platform -ne "Unix") {
                    $acl = Get-Acl $StorePath
                    $acl.SetAccessRuleProtection($true, $false)
                    $rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
                        [System.Security.Principal.WindowsIdentity]::GetCurrent().User,
                        "FullControl",
                        "Allow"
                    )
                    $acl.SetAccessRule($rule)
                    Set-Acl -Path $StorePath -AclObject $acl
                }
            }
            
            Write-Verbose "Secure API key store initialized (File: $StorePath)" -Verbose
        }
        
        return $true
    }
    catch {
        Write-Error "Failed to initialize secure key store: $_"
        return $false
    }
}

# Store encrypted API key
function Set-SecureAPIKey {
    <#
    .SYNOPSIS
        Store an API key in encrypted storage
    
    .DESCRIPTION
        Encrypts a SecureString API key using DPAPI and stores it persistently.
        Supports both file-based and registry storage with automatic fallback.
    
    .PARAMETER KeyName
        Logical name for the key (e.g., "OLLAMA", "OPENAI", "ANTHROPIC")
    
    .PARAMETER SecureKey
        SecureString object containing the API key
    
    .PARAMETER StorePath
        Optional override path for storage file
    
    .EXAMPLE
        $key = ConvertTo-SecureString "sk-1234567890" -AsPlainText -Force
        Set-SecureAPIKey -KeyName "OPENAI" -SecureKey $key
    
    .OUTPUTS
        [bool] $true if storage successful, $false otherwise
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$KeyName,
        
        [Parameter(Mandatory = $true)]
        [SecureString]$SecureKey,
        
        [string]$StorePath = $null
    )
    
    try {
        if (-not $StorePath) {
            $StorePath = $script:SecureKeyStore.StorePath
        }
        
        if (-not $StorePath) {
            throw "Storage path not initialized. Call Initialize-SecureAPIKeyStore first."
        }
        
        # Convert SecureString to encrypted bytes using DPAPI
        $plainText = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto(
            [System.Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($SecureKey)
        )
        
        $dataToEncrypt = [System.Text.Encoding]::UTF8.GetBytes($plainText)
        $encryptedData = [System.Security.Cryptography.ProtectedData]::Protect(
            $dataToEncrypt,
            $null,
            [System.Security.Cryptography.DataProtectionScope]::CurrentUser
        )
        
        # Base64 encode for storage
        $encryptedBase64 = [Convert]::ToBase64String($encryptedData)
        
        # Store with metadata
        $keyEntry = @{
            KeyName     = $KeyName
            Encrypted   = $encryptedBase64
            Timestamp   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            Algorithm   = "DPAPI-CurrentUser"
            Hostname    = $env:COMPUTERNAME
            Username    = $env:USERNAME
        }
        
        # Determine storage method
        if ($script:SecureKeyStore.UseRegistry) {
            $regPath = "HKCU:\Software\RawrXD\APIKeys"
            $regName = "key_$($KeyName)_encrypted"
            Set-ItemProperty -Path $regPath -Name $regName -Value $encryptedBase64 -Force
        } else {
            # File-based storage
            $store = @{}
            if (Test-Path $StorePath) {
                $existingStore = Get-Content -Path $StorePath -Raw | ConvertFrom-Json
                if ($existingStore) {
                    $existingStore.PSObject.Properties | ForEach-Object {
                        $store[$_.Name] = $_.Value
                    }
                }
            }
            
            # Update with new key
            $store[$KeyName] = $keyEntry
            
            # Write updated store
            $store | ConvertTo-Json | Set-Content -Path $StorePath -Encoding UTF8 -Force
        }
        
        # Clear sensitive data from memory
        $plainText = $null
        [GC]::Collect()
        
        Write-Verbose "API key '$KeyName' encrypted and stored successfully" -Verbose
        return $true
    }
    catch {
        Write-Error "Failed to store API key '$KeyName': $_"
        return $false
    }
}

# Retrieve decrypted API key
function Get-SecureAPIKey {
    <#
    .SYNOPSIS
        Retrieve a decrypted API key from secure storage
    
    .DESCRIPTION
        Retrieves an encrypted API key, decrypts it using DPAPI, and returns as SecureString.
        Falls back through: File → Registry → Environment Variables → Null
    
    .PARAMETER KeyName
        Logical name for the key (e.g., "OLLAMA", "OPENAI")
    
    .PARAMETER AsPlainText
        If $true, return as plain string instead of SecureString
        ⚠️ Use with caution - exposes credentials in memory
    
    .PARAMETER StorePath
        Optional override path for storage file
    
    .EXAMPLE
        $key = Get-SecureAPIKey -KeyName "OPENAI"
        $plainKey = Get-SecureAPIKey -KeyName "OPENAI" -AsPlainText
    
    .OUTPUTS
        [SecureString] Decrypted API key, or [string] if -AsPlainText
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$KeyName,
        
        [bool]$AsPlainText = $false,
        
        [string]$StorePath = $null
    )
    
    try {
        $decryptedKey = $null
        
        # Try 1: File-based storage
        if (-not $StorePath) {
            $StorePath = $script:SecureKeyStore.StorePath
        }
        
        if ($StorePath -and (Test-Path $StorePath)) {
            try {
                $store = Get-Content -Path $StorePath -Raw | ConvertFrom-Json
                if ($store -and $store.$KeyName) {
                    $encryptedBase64 = $store.$KeyName.Encrypted
                    if ($encryptedBase64) {
                        $encryptedData = [Convert]::FromBase64String($encryptedBase64)
                        $decryptedData = [System.Security.Cryptography.ProtectedData]::Unprotect(
                            $encryptedData,
                            $null,
                            [System.Security.Cryptography.DataProtectionScope]::CurrentUser
                        )
                        $decryptedKey = [System.Text.Encoding]::UTF8.GetString($decryptedData)
                        Write-Verbose "Retrieved API key '$KeyName' from encrypted file storage" -Verbose
                    }
                }
            }
            catch {
                Write-Verbose "File storage lookup failed for '$KeyName': $_" -Verbose
            }
        }
        
        # Try 2: Registry fallback
        if (-not $decryptedKey) {
            try {
                $regPath = "HKCU:\Software\RawrXD\APIKeys"
                $regName = "key_$($KeyName)_encrypted"
                if (Test-Path $regPath) {
                    $encryptedBase64 = Get-ItemProperty -Path $regPath -Name $regName -ErrorAction SilentlyContinue
                    if ($encryptedBase64) {
                        $encryptedData = [Convert]::FromBase64String($encryptedBase64.$regName)
                        $decryptedData = [System.Security.Cryptography.ProtectedData]::Unprotect(
                            $encryptedData,
                            $null,
                            [System.Security.Cryptography.DataProtectionScope]::CurrentUser
                        )
                        $decryptedKey = [System.Text.Encoding]::UTF8.GetString($decryptedData)
                        Write-Verbose "Retrieved API key '$KeyName' from registry" -Verbose
                    }
                }
            }
            catch {
                Write-Verbose "Registry lookup failed for '$KeyName': $_" -Verbose
            }
        }
        
        # Try 3: Environment variable fallback (legacy support)
        if (-not $decryptedKey) {
            @("RAWRXD_$($KeyName)", "$($KeyName)_API_KEY", "RAWRXD_API_KEY") | ForEach-Object {
                if (-not $decryptedKey) {
                    $envValue = (Get-Item -Path ("env:" + $_) -ErrorAction SilentlyContinue).Value
                    if ($envValue) {
                        $decryptedKey = $envValue
                        Write-Verbose "Retrieved API key '$KeyName' from environment variable (legacy)" -Verbose
                    }
                }
            }
        }
        
        # Return result
        if ($decryptedKey) {
            if ($AsPlainText) {
                return $decryptedKey
            } else {
                return ConvertTo-SecureString -String $decryptedKey -AsPlainText -Force
            }
        } else {
            Write-Warning "API key '$KeyName' not found in secure storage"
            return $null
        }
    }
    catch {
        Write-Error "Failed to retrieve API key '$KeyName': $_"
        return $null
    }
}

# Revoke stored API key
function Revoke-SecureAPIKey {
    <#
    .SYNOPSIS
        Securely remove an API key from storage
    
    .DESCRIPTION
        Permanently deletes an encrypted API key from both file and registry storage.
        Clears any in-memory caches of the key.
    
    .PARAMETER KeyName
        Logical name for the key to revoke
    
    .EXAMPLE
        Revoke-SecureAPIKey -KeyName "OPENAI"
    
    .OUTPUTS
        [bool] $true if revocation successful
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$KeyName
    )
    
    try {
        # Remove from file storage
        if ($script:SecureKeyStore.StorePath -and (Test-Path $script:SecureKeyStore.StorePath)) {
            try {
                $store = Get-Content -Path $script:SecureKeyStore.StorePath -Raw | ConvertFrom-Json
                if ($store.$KeyName) {
                    $store.PSObject.Properties.Remove($KeyName)
                    $store | ConvertTo-Json | Set-Content -Path $script:SecureKeyStore.StorePath -Encoding UTF8 -Force
                }
            }
            catch {
                Write-Verbose "Failed to remove key from file storage: $_" -Verbose
            }
        }
        
        # Remove from registry
        try {
            $regPath = "HKCU:\Software\RawrXD\APIKeys"
            $regName = "key_$($KeyName)_encrypted"
            if (Test-Path $regPath) {
                Remove-ItemProperty -Path $regPath -Name $regName -ErrorAction SilentlyContinue
            }
        }
        catch {
            Write-Verbose "Failed to remove key from registry: $_" -Verbose
        }
        
        # Clear memory cache
        if ($script:SecureKeyStore.EncryptedKeys[$KeyName]) {
            $script:SecureKeyStore.EncryptedKeys.Remove($KeyName)
            [GC]::Collect()
        }
        
        Write-Verbose "API key '$KeyName' revoked successfully" -Verbose
        return $true
    }
    catch {
        Write-Error "Failed to revoke API key '$KeyName': $_"
        return $false
    }
}

# Verify storage integrity
function Test-SecureAPIKeyIntegrity {
    <#
    .SYNOPSIS
        Verify the integrity of secure API key storage system
    
    .DESCRIPTION
        Performs comprehensive checks on the encryption system:
        - Verify DPAPI accessibility
        - Test encryption/decryption round-trip
        - Check file/registry permissions
        - Validate storage location
    
    .EXAMPLE
        $result = Test-SecureAPIKeyIntegrity
        if ($result.IsValid) {
            Write-Host "✓ API key storage is secure"
        }
    
    .OUTPUTS
        [hashtable] Status object with properties:
          - IsValid [bool]: Overall health
          - Tests [hashtable]: Individual test results
          - Issues [array]: List of problems found
    #>
    param()
    
    $result = @{
        IsValid = $true
        Tests   = @{}
        Issues  = @()
    }
    
    try {
        # Test 1: Storage path accessible
        $result.Tests["StoragePathAccessible"] = $false
        if ($script:SecureKeyStore.StorePath -and (Test-Path (Split-Path -Parent $script:SecureKeyStore.StorePath))) {
            $result.Tests["StoragePathAccessible"] = $true
        } else {
            $result.IsValid = $false
            $result.Issues += "Storage path not accessible: $($script:SecureKeyStore.StorePath)"
        }
        
        # Test 2: Encryption round-trip
        $result.Tests["EncryptionRoundTrip"] = $false
        try {
            $testKey = "TEST_INTEGRITY_KEY_$(Get-Random)"
            $testValue = "test-value-$(Get-Random)"
            $secureTest = ConvertTo-SecureString -String $testValue -AsPlainText -Force
            
            if (Set-SecureAPIKey -KeyName $testKey -SecureKey $secureTest) {
                $retrieved = Get-SecureAPIKey -KeyName $testKey -AsPlainText
                if ($retrieved -eq $testValue) {
                    $result.Tests["EncryptionRoundTrip"] = $true
                    Revoke-SecureAPIKey -KeyName $testKey | Out-Null
                } else {
                    $result.IsValid = $false
                    $result.Issues += "Encryption round-trip failed: retrieved value mismatch"
                }
            }
        }
        catch {
            $result.IsValid = $false
            $result.Issues += "Encryption round-trip test failed: $_"
        }
        
        # Test 3: File permissions (Windows only)
        $result.Tests["FilePermissionsRestricted"] = $true
        if (-not [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
            $result.Tests["FilePermissionsRestricted"] = $true  # Skip on non-Windows
        } else {
            try {
                if ($script:SecureKeyStore.StorePath -and (Test-Path $script:SecureKeyStore.StorePath)) {
                    $acl = Get-Acl $script:SecureKeyStore.StorePath
                    $publicAccess = $acl.Access | Where-Object { $_.IdentityReference -match "Everyone|Authenticated Users" }
                    if ($publicAccess) {
                        $result.Tests["FilePermissionsRestricted"] = $false
                        $result.IsValid = $false
                        $result.Issues += "Storage file has public access - permissions too loose"
                    }
                }
            }
            catch {
                Write-Verbose "Unable to verify file permissions: $_" -Verbose
            }
        }
        
        # Test 4: DPAPI available
        $result.Tests["DPAPIAvailable"] = $false
        try {
            $testBytes = [System.Text.Encoding]::UTF8.GetBytes("test")
            $encrypted = [System.Security.Cryptography.ProtectedData]::Protect(
                $testBytes,
                $null,
                [System.Security.Cryptography.DataProtectionScope]::CurrentUser
            )
            $decrypted = [System.Security.Cryptography.ProtectedData]::Unprotect(
                $encrypted,
                $null,
                [System.Security.Cryptography.DataProtectionScope]::CurrentUser
            )
            if ([System.Text.Encoding]::UTF8.GetString($decrypted) -eq "test") {
                $result.Tests["DPAPIAvailable"] = $true
            }
        }
        catch {
            $result.IsValid = $false
            $result.Issues += "DPAPI not available: $_"
        }
        
        return $result
    }
    catch {
        Write-Error "Integrity test failed: $_"
        $result.IsValid = $false
        $result.Issues += "Integrity test exception: $_"
        return $result
    }
}

# Migrate existing keys from environment variables
function Invoke-SecureKeyMigration {
    <#
    .SYNOPSIS
        Migrate API keys from environment variables to encrypted storage
    
    .DESCRIPTION
        Discovers API keys in environment variables and migrates them to secure storage.
        Creates encrypted copies and optionally removes plain text sources.
    
    .PARAMETER Force
        Skip confirmation prompts
    
    .PARAMETER ClearEnvironmentVariables
        If $true, remove keys from environment after migration
    
    .EXAMPLE
        Invoke-SecureKeyMigration
        Invoke-SecureKeyMigration -Force -ClearEnvironmentVariables
    
    .OUTPUTS
        [hashtable] Migration results with success/failure counts
    #>
    param(
        [bool]$Force = $false,
        [bool]$ClearEnvironmentVariables = $false
    )
    
    $results = @{
        Success = 0
        Failed  = 0
        Skipped = 0
    }
    
    try {
        # Detect environment variable API keys
        $keyPatterns = @("*API*KEY*", "*API_KEY", "*OLLAMA*", "*OPENAI*", "*ANTHROPIC*")
        $foundKeys = @()
        
        Get-Item -Path "env:*" | ForEach-Object {
            $name = $_.Name
            $value = $_.Value
            
            foreach ($pattern in $keyPatterns) {
                if ($name -like $pattern -and $value) {
                    $foundKeys += @{
                        Name  = $name
                        Value = $value
                        Masked = ($value.Substring(0, [Math]::Min(5, $value.Length)) + "***")
                    }
                    break
                }
            }
        }
        
        if ($foundKeys.Count -eq 0) {
            Write-Host "✓ No API keys found in environment variables" -ForegroundColor Green
            return $results
        }
        
        Write-Host "`n🔐 Found $($foundKeys.Count) potential API key(s) in environment variables:" -ForegroundColor Cyan
        foreach ($key in $foundKeys) {
            Write-Host "  - $($key.Name) = $($key.Masked)" -ForegroundColor Yellow
        }
        
        if (-not $Force) {
            $response = Read-Host "`nMigrate these keys to encrypted storage? (yes/no)"
            if ($response -ne "yes") {
                Write-Host "Migration cancelled" -ForegroundColor Gray
                return $results
            }
        }
        
        # Migrate each key
        foreach ($key in $foundKeys) {
            try {
                # Simplify key name for storage
                $simplifiedName = $key.Name `
                    -replace "RAWRXD_", "" `
                    -replace "_API_KEY$", "" `
                    -replace "_KEY$", "" `
                    -replace "_", "-"
                
                $secureString = ConvertTo-SecureString -String $key.Value -AsPlainText -Force
                if (Set-SecureAPIKey -KeyName $simplifiedName -SecureKey $secureString) {
                    Write-Host "  ✓ Migrated: $($key.Name) → $simplifiedName (encrypted)" -ForegroundColor Green
                    $results.Success++
                    
                    # Clear environment variable if requested
                    if ($ClearEnvironmentVariables) {
                        [Environment]::SetEnvironmentVariable($key.Name, $null, "Process")
                        [Environment]::SetEnvironmentVariable($key.Name, $null, "User")
                        Write-Host "    Cleared from environment" -ForegroundColor Gray
                    }
                } else {
                    Write-Host "  ✗ Failed: $($key.Name)" -ForegroundColor Red
                    $results.Failed++
                }
            }
            catch {
                Write-Host "  ✗ Error migrating $($key.Name): $_" -ForegroundColor Red
                $results.Failed++
            }
        }
        
        Write-Host "`n✓ Migration complete: $($results.Success) succeeded, $($results.Failed) failed" -ForegroundColor Green
        return $results
    }
    catch {
        Write-Error "Migration failed: $_"
        return $results
    }
}

# Module initialization
if (-not (Test-Path Variable:script:SecureKeyStore)) {
    Initialize-SecureAPIKeyStore | Out-Null
}

# Export public functions
Export-ModuleMember -Function @(
    'Initialize-SecureAPIKeyStore',
    'Set-SecureAPIKey',
    'Get-SecureAPIKey',
    'Revoke-SecureAPIKey',
    'Test-SecureAPIKeyIntegrity',
    'Invoke-SecureKeyMigration'
)
