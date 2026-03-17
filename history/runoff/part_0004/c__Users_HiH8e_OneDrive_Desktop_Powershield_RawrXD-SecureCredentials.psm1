# RawrXD-SecureCredentials.psm1
# Secure credential management for RawrXD IDE

using namespace System.Security.Cryptography
using namespace System.Text
using namespace System.IO

class SecureCredentialManager {
    [string]$VaultPath
    [byte[]]$MasterKey

    SecureCredentialManager() {
        $this.VaultPath = Join-Path $env:APPDATA "RawrXD\credentials.vault"
        $this.InitializeVault()
    }

    [void]InitializeVault() {
        if (-not (Test-Path (Split-Path $this.VaultPath -Parent))) {
            New-Item -ItemType Directory -Path (Split-Path $this.VaultPath -Parent) -Force | Out-Null
        }

        if (-not (Test-Path $this.VaultPath)) {
            # Generate master key using DPAPI
            $entropy = [System.Security.Cryptography.ProtectedData]::Protect(
                [Encoding]::UTF8.GetBytes((Get-Random -Minimum 100000 -Maximum 999999).ToString()),
                $null,
                [System.Security.Cryptography.DataProtectionScope]::CurrentUser
            )
            [File]::WriteAllBytes($this.VaultPath, $entropy)
        }

        $this.MasterKey = [System.Security.Cryptography.ProtectedData]::Unprotect(
            [File]::ReadAllBytes($this.VaultPath),
            $null,
            [System.Security.Cryptography.DataProtectionScope]::CurrentUser
        )
    }

    [void]StoreCredential([string]$key, [string]$value) {
        $encrypted = $this.EncryptString($value)
        $vault = $this.LoadVault()
        $vault[$key] = $encrypted
        $this.SaveVault($vault)
    }

    [string]RetrieveCredential([string]$key) {
        $vault = $this.LoadVault()
        if ($vault.ContainsKey($key)) {
            return $this.DecryptString($vault[$key])
        }
        throw "Credential not found: $key"
    }

    [void]RemoveCredential([string]$key) {
        $vault = $this.LoadVault()
        $vault.Remove($key)
        $this.SaveVault($vault)
    }

    [hashtable]LoadVault() {
        $vaultFile = $this.VaultPath + ".data"
        if (Test-Path $vaultFile) {
            $encryptedData = [File]::ReadAllBytes($vaultFile)
            $json = $this.DecryptString($encryptedData)
            return $json | ConvertFrom-Json -AsHashtable
        }
        return @{}
    }

    [void]SaveVault([hashtable]$vault) {
        $json = $vault | ConvertTo-Json -Compress
        $encrypted = $this.EncryptString($json)
        $vaultFile = $this.VaultPath + ".data"
        [File]::WriteAllBytes($vaultFile, $encrypted)
    }

    [byte[]]EncryptString([string]$plainText) {
        using ($aes = [Aes]::Create()) {
            $aes.Key = $this.MasterKey
            $aes.GenerateIV()
            $encryptor = $aes.CreateEncryptor()

            $plainBytes = [Encoding]::UTF8.GetBytes($plainText)
            $encrypted = $encryptor.TransformFinalBlock($plainBytes, 0, $plainBytes.Length)

            # Prepend IV
            $result = New-Object byte[] ($aes.IV.Length + $encrypted.Length)
            [Array]::Copy($aes.IV, 0, $result, 0, $aes.IV.Length)
            [Array]::Copy($encrypted, 0, $result, $aes.IV.Length, $encrypted.Length)

            return $result
        }
    }

    [string]DecryptString([byte[]]$encryptedData) {
        using ($aes = [Aes]::Create()) {
            $aes.Key = $this.MasterKey
            $aes.IV = $encryptedData[0..15]
            $decryptor = $aes.CreateDecryptor()

            $cipherBytes = $encryptedData[16..($encryptedData.Length - 1)]
            $decrypted = $decryptor.TransformFinalBlock($cipherBytes, 0, $cipherBytes.Length)

            return [Encoding]::UTF8.GetString($decrypted)
        }
    }
}

# Input validation and sanitization
class InputValidator {
    static [bool]IsValidCode([string]$code) {
        # Check for dangerous patterns
        $dangerousPatterns = @(
            'Invoke-Expression',
            'Start-Process.*-Verb.*RunAs',
            'New-Object.*System.Diagnostics.Process',
            '\$executionContext\.SessionState\.LanguageMode',
            'Add-Type.*-TypeDefinition',
            '\[System\.Reflection\.Assembly\]::Load',
            '\[System\.Runtime\.InteropServices\.Marshal\]::GetDelegateForFunctionPointer'
        )

        foreach ($pattern in $dangerousPatterns) {
            if ($code -match $pattern) {
                return $false
            }
        }

        # Check syntax validity
        try {
            $null = [System.Management.Automation.PSParser]::Tokenize($code, [ref]$null)
            return $true
        }
        catch {
            return $false
        }
    }

    static [string]SanitizeInput([string]$input) {
        # Remove potentially dangerous characters and patterns
        $sanitized = $input -replace '[<>\[\]{}|\\^`]', ''
        $sanitized = $sanitized -replace '\$executionContext', '$null'
        $sanitized = $sanitized -replace 'Invoke-Expression', 'Write-Host'
        return $sanitized
    }
}

# Secure execution sandbox
class SecureExecutor {
    static [object]ExecuteInSandbox([string]$code, [hashtable]$parameters = @{}) {
        # Create constrained runspace
        $runspace = [System.Management.Automation.Runspaces.RunspaceFactory]::CreateRunspace()
        $runspace.ThreadOptions = [System.Management.Automation.Runspaces.PSThreadOptions]::UseCurrentThread

        # Set language mode to ConstrainedLanguage
        $runspace.SessionStateProxy.LanguageMode = [System.Management.Automation.PSLanguageMode]::ConstrainedLanguage

        $runspace.Open()

        try {
            $pipeline = $runspace.CreatePipeline()
            $pipeline.Commands.AddScript($code)

            # Add parameters
            foreach ($param in $parameters.GetEnumerator()) {
                $pipeline.Commands[0].Parameters.Add($param.Key, $param.Value)
            }

            $results = $pipeline.Invoke()
            return $results
        }
        finally {
            $runspace.Close()
        }
    }
}

# Certificate management
class CertificateManager {
    static [System.Security.Cryptography.X509Certificates.X509Certificate2]GetOrCreateCertificate([string]$subjectName) {
        $store = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2Collection
        $storeLocation = [System.Security.Cryptography.X509Certificates.StoreLocation]::CurrentUser
        $storeName = [System.Security.Cryptography.X509Certificates.StoreName]::My

        $certStore = New-Object System.Security.Cryptography.X509Certificates.X509Store $storeName, $storeLocation
        $certStore.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)

        # Check if certificate already exists
        $existingCert = $certStore.Certificates | Where-Object { $_.Subject -eq "CN=$subjectName" -and $_.NotAfter -gt (Get-Date) }
        if ($existingCert) {
            $certStore.Close()
            return $existingCert[0]
        }

        # Create new self-signed certificate
        $cert = [CertificateManager]::CreateSelfSignedCertificate($subjectName)
        $certStore.Add($cert)
        $certStore.Close()

        return $cert
    }

    static [System.Security.Cryptography.X509Certificates.X509Certificate2]CreateSelfSignedCertificate([string]$subjectName) {
        $eku = [System.Security.Cryptography.OidCollection]::new()
        $eku.Add([System.Security.Cryptography.Oid]::new("1.3.6.1.5.5.7.3.1")) # Server Authentication

        $san = [System.Security.Cryptography.SubjectAlternativeNameBuilder]::new()
        $san.AddDnsName("localhost")
        $san.AddIpAddress([System.Net.IPAddress]::Parse("127.0.0.1"))

        $rsa = [System.Security.Cryptography.RSA]::Create(2048)

        $req = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
            "CN=$subjectName",
            $rsa,
            [System.Security.Cryptography.HashAlgorithmName]::SHA256,
            [System.Security.Cryptography.RSASignaturePadding]::Pkcs1
        )

        $req.CertificateExtensions.Add($eku)
        $req.CertificateExtensions.Add($san.Build())

        $cert = $req.CreateSelfSigned((Get-Date).AddDays(-1), (Get-Date).AddYears(1))
        return [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx), $null, [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable)
    }
}

# OAuth 2.0 token management
class OAuthManager {
    [string]$ClientId
    [string]$ClientSecret
    [string]$TokenEndpoint
    [string]$RefreshToken
    [DateTime]$TokenExpiry

    OAuthManager([string]$clientId, [string]$clientSecret, [string]$tokenEndpoint) {
        $this.ClientId = $clientId
        $this.ClientSecret = $clientSecret
        $this.TokenEndpoint = $tokenEndpoint
    }

    [string]GetAccessToken() {
        if ((Get-Date) -lt $this.TokenExpiry.AddMinutes(-5)) {
            return $this.AccessToken
        }

        return $this.RefreshAccessToken()
    }

    [string]RefreshAccessToken() {
        $body = @{
            grant_type = "refresh_token"
            refresh_token = $this.RefreshToken
            client_id = $this.ClientId
            client_secret = $this.ClientSecret
        }

        try {
            $response = Invoke-RestMethod -Uri $this.TokenEndpoint -Method Post -Body $body -ContentType "application/x-www-form-urlencoded"
            $this.AccessToken = $response.access_token
            $this.RefreshToken = $response.refresh_token
            $this.TokenExpiry = (Get-Date).AddSeconds($response.expires_in)
            return $this.AccessToken
        }
        catch {
            throw "Failed to refresh access token: $_"
        }
    }

    [void]SetTokens([string]$accessToken, [string]$refreshToken, [int]$expiresIn) {
        $this.AccessToken = $accessToken
        $this.RefreshToken = $refreshToken
        $this.TokenExpiry = (Get-Date).AddSeconds($expiresIn)
    }
}

# TLS configuration
class TLSManager {
    static [void]ConfigureTLS() {
        # Force TLS 1.3 when available, fallback to 1.2
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
        if ([System.Net.SecurityProtocolType]::Tls13) {
            [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls13
        }

        # Custom certificate validation for development
        [System.Net.ServicePointManager]::ServerCertificateValidationCallback = {
            param($sender, $certificate, $chain, $sslPolicyErrors)
            # In production, implement proper certificate validation
            return $true
        }
    }
}

# Secure logging
class SecureLogger {
    [string]$LogPath
    [System.Security.Cryptography.Aes]$Aes

    SecureLogger([string]$logPath) {
        $this.LogPath = $logPath
        $this.InitializeEncryption()
    }

    [void]InitializeEncryption() {
        $this.Aes = [System.Security.Cryptography.Aes]::Create()
        $this.Aes.KeySize = 256
        $this.Aes.GenerateKey()
        $this.Aes.GenerateIV()
    }

    [void]Log([string]$message, [string]$level = "INFO") {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $logEntry = "[$timestamp] [$level] $message"

        # Redact sensitive information
        $redactedEntry = [SecureLogger]::RedactSensitiveInfo($logEntry)

        # Encrypt and write
        $encrypted = $this.EncryptString($redactedEntry)
        Add-Content -Path $this.LogPath -Value $encrypted -Encoding UTF8
    }

    static [string]RedactSensitiveInfo([string]$text) {
        # Redact API keys, passwords, tokens
        $patterns = @(
            'password["\s]*:[\s]*["\'][^"\']*["\']',
            'api[_-]?key["\s]*:[\s]*["\'][^"\']*["\']',
            'token["\s]*:[\s]*["\'][^"\']*["\']',
            'secret["\s]*:[\s]*["\'][^"\']*["\']'
        )

        foreach ($pattern in $patterns) {
            $text = $text -replace $pattern, '$1: [REDACTED]'
        }

        return $text
    }

    [byte[]]EncryptString([string]$plainText) {
        $encryptor = $this.Aes.CreateEncryptor()
        $plainBytes = [Encoding]::UTF8.GetBytes($plainText)
        $encrypted = $encryptor.TransformFinalBlock($plainBytes, 0, $plainBytes.Length)

        # Prepend IV
        $result = New-Object byte[] ($this.Aes.IV.Length + $encrypted.Length)
        [Array]::Copy($this.Aes.IV, 0, $result, 0, $this.Aes.IV.Length)
        [Array]::Copy($encrypted, 0, $result, $this.Aes.IV.Length, $encrypted.Length)

        return [Convert]::ToBase64String($result)
    }
}

# Multi-factor authentication
class MFAManager {
    [string]$SecretKey

    MFAManager([string]$secretKey) {
        $this.SecretKey = $secretKey
    }

    [string]GenerateTOTP() {
        $timestamp = [Math]::Floor((Get-Date -UFormat %s))
        $timeStep = [Math]::Floor($timestamp / 30)
        $timeBytes = [BitConverter]::GetBytes([long]$timeStep)
        [Array]::Reverse($timeBytes)

        $hmac = New-Object System.Security.Cryptography.HMACSHA1
        $hmac.Key = [Convert]::FromBase64String($this.SecretKey)
        $hash = $hmac.ComputeHash($timeBytes)

        $offset = $hash[$hash.Length - 1] -band 0x0F
        $binary = ($hash[$offset] -band 0x7F) -shl 24 -bor
                  ($hash[$offset + 1] -band 0xFF) -shl 16 -bor
                  ($hash[$offset + 2] -band 0xFF) -shl 8 -bor
                  ($hash[$offset + 3] -band 0xFF)

        $otp = $binary % 1000000
        return $otp.ToString("000000")
    }

    [bool]ValidateTOTP([string]$userCode) {
        $currentCode = $this.GenerateTOTP()
        $previousTimeStep = [Math]::Floor(((Get-Date -UFormat %s) - 30) / 30)
        $timeBytes = [BitConverter]::GetBytes([long]$previousTimeStep)
        [Array]::Reverse($timeBytes)

        $hmac = New-Object System.Security.Cryptography.HMACSHA1
        $hmac.Key = [Convert]::FromBase64String($this.SecretKey)
        $hash = $hmac.ComputeHash($timeBytes)

        $offset = $hash[$hash.Length - 1] -band 0x0F
        $binary = ($hash[$offset] -band 0x7F) -shl 24 -bor
                  ($hash[$offset + 1] -band 0xFF) -shl 16 -bor
                  ($hash[$offset + 2] -band 0xFF) -shl 8 -bor
                  ($hash[$offset + 3] -band 0xFF)

        $previousCode = ($binary % 1000000).ToString("000000")

        return $userCode -eq $currentCode -or $userCode -eq $previousCode
    }
}

# Export functions
function New-SecureCredentialManager {
    return [SecureCredentialManager]::new()
}

function Test-InputValidation {
    param([string]$code)
    return [InputValidator]::IsValidCode($code)
}

function Invoke-SecureExecution {
    param([string]$code, [hashtable]$parameters = @{})
    return [SecureExecutor]::ExecuteInSandbox($code, $parameters)
}

function Get-SelfSignedCertificate {
    param([string]$subjectName)
    return [CertificateManager]::GetOrCreateCertificate($subjectName)
}

function New-OAuthManager {
    param([string]$clientId, [string]$clientSecret, [string]$tokenEndpoint)
    return [OAuthManager]::new($clientId, $clientSecret, $tokenEndpoint)
}

function Initialize-TLS {
    [TLSManager]::ConfigureTLS()
}

function New-SecureLogger {
    param([string]$logPath)
    return [SecureLogger]::new($logPath)
}

function New-MFAManager {
    param([string]$secretKey)
    return [MFAManager]::new($secretKey)
}

Export-ModuleMember -Function * -Variable *