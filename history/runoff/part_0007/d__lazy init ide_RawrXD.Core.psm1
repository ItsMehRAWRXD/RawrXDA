# RawrXD.Core.psm1
# Core agent logic for RawrXD (error handling, agent loop, tool registry, Ollama/AI, security, session, background jobs)

# Emergency Logging Setup
$script:EmergencyLogPath = Join-Path $env:APPDATA "RawrXD"
if (-not (Test-Path $script:EmergencyLogPath)) {
    try { New-Item -ItemType Directory -Path $script:EmergencyLogPath -Force | Out-Null } catch { }
}
$script:StartupLogFile = Join-Path $script:EmergencyLogPath "startup.log"

$script:RawrXDRootPath = if ($env:LAZY_INIT_IDE_ROOT -and (Test-Path $env:LAZY_INIT_IDE_ROOT)) {
    $env:LAZY_INIT_IDE_ROOT
} else {
    $PSScriptRoot
}

# Import-Module (Join-Path $script:RawrXDRootPath "RawrXD.Logging.psm1") -Force
# Import-Module (Join-Path $script:RawrXDRootPath "RawrXD.Config.psm1") -Force

# Load configuration at module import
try {
    $script:RawrXDConfig = Import-RawrXDConfig
} catch {
    $script:RawrXDConfig = @{}
}

function Get-RawrXDConfig {
    return $script:RawrXDConfig
}

# Support environment variable overrides
function Get-ConfigValue {
    param(
        [string]$Key,
        $Default = $null
    )
    $envVar = ${env:$Key}
    if ($envVar) { return $envVar }
    if ($script:RawrXDConfig.PSObject.Properties.Name -contains $Key) {
        return $script:RawrXDConfig.$Key
    }
    return $Default
}

# Runtime reload
function Reload-RawrXDConfig {
    $script:RawrXDConfig = Import-RawrXDConfig
}
function Write-EmergencyLog {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [ValidateSet("DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR", "CRITICAL")]
        [string]$Level = "INFO"
    )

    try {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $logEntry = "[$timestamp] [$Level] $Message"
        if ($script:StartupLogFile) {
            Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }

        if ($Level -in @("ERROR", "CRITICAL")) {
            Write-Host $logEntry -ForegroundColor Red
        }
    }
    catch {
        $displayColor = if ($Level -eq "ERROR") { "Red" } else { "Yellow" }
        Write-Host "[$Level] $Message" -ForegroundColor $displayColor
    }
}
function Write-StartupLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        [ValidateSet("DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR")]
        [string]$Level = "INFO"
    )

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"

    $color = switch ($Level) {
        "DEBUG" { "DarkGray" }
        "INFO" { "Cyan" }
        "SUCCESS" { "Green" }
        "WARNING" { "Yellow" }
        "ERROR" { "Red" }
        default { "White" }
    }

    Write-Host $logEntry -ForegroundColor $color
    if ($script:StartupLogFile) {
        try {
            Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }
        catch { }
    }
}

# Add the C# StealthCrypto class
Add-Type @"
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

public static class StealthCrypto {
    private static readonly byte[] DefaultKey = new byte[] {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };
    
    public static string Encrypt(string data, byte[] key = null) {
        if (string.IsNullOrEmpty(data)) return data;
        key = key != null ? key : DefaultKey;
        
        using (var aes = Aes.Create()) {
            aes.Key = key;
            aes.GenerateIV();
            
            using (var encryptor = aes.CreateEncryptor())
            using (var ms = new MemoryStream())
            using (var cs = new CryptoStream(ms, encryptor, CryptoStreamMode.Write)) {
                var dataBytes = Encoding.UTF8.GetBytes(data);
                cs.Write(dataBytes, 0, dataBytes.Length);
                cs.FlushFinalBlock();
                
                var result = new byte[aes.IV.Length + ms.ToArray().Length];
                Array.Copy(aes.IV, 0, result, 0, aes.IV.Length);
                Array.Copy(ms.ToArray(), 0, result, aes.IV.Length, ms.ToArray().Length);
                
                return Convert.ToBase64String(result);
            }
        }
    }
    
    public static string Decrypt(string encryptedData, byte[] key = null) {
        if (string.IsNullOrEmpty(encryptedData)) return encryptedData;
        key = key != null ? key : DefaultKey;
        
        try {
            var encryptedBytes = Convert.FromBase64String(encryptedData);
            
            using (var aes = Aes.Create()) {
                aes.Key = key;
                
                var iv = new byte[16];
                var encrypted = new byte[encryptedBytes.Length - 16];
                
                Array.Copy(encryptedBytes, 0, iv, 0, 16);
                Array.Copy(encryptedBytes, 16, encrypted, 0, encrypted.Length);
                
                aes.IV = iv;
                
                using (var decryptor = aes.CreateDecryptor())
                using (var ms = new MemoryStream(encrypted))
                using (var cs = new CryptoStream(ms, decryptor, CryptoStreamMode.Read))
                using (var sr = new StreamReader(cs)) {
                    return sr.ReadToEnd();
                }
            }
        }
        catch {
            return encryptedData; // Return original if decryption fails
        }
    }
    
    public static string Hash(string data) {
        using (var sha256 = SHA256.Create()) {
            var hash = sha256.ComputeHash(Encoding.UTF8.GetBytes(data));
            return Convert.ToBase64String(hash);
        }
    }
    
    public static byte[] GenerateKey() {
        using (var rng = RandomNumberGenerator.Create()) {
            var key = new byte[32]; // 256-bit key
            rng.GetBytes(key);
            return key;
        }
    }
}
"@

# ============================================
# SECURITY & STEALTH CORE
# ============================================

$script:SecurityConfig = @{
    EncryptSensitiveData   = $true
    ValidateAllInputs      = $true
    SecureConnections      = $true
    StealthMode            = $false
    AuthenticationRequired = $false
    SessionTimeout         = 3600
    MaxLoginAttempts       = 3
    LogSecurityEvents      = $true
}

function Write-SecurityLog {
    param(
        [string]$EventName,
        [string]$Level = "INFO",
        [string]$Details = ""
    )
    
    $logEntry = @{
        Timestamp   = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Event       = $EventName
        Level       = $Level
        Details     = $Details
        ProcessId   = $PID
        UserContext = [Environment]::UserName
    }
    
    Write-StartupLog "[$Level] Security: $EventName - $Details" "INFO"
}

function Test-InputSafety {
    param([string]$InputText, [string]$Type = "General")
    
    if (-not $script:SecurityConfig.ValidateAllInputs) { return $true }
    
    $dangerousPatterns = @(
        '(?i)(script|javascript|vbscript):',
        '(?i)<[^>]*on\w+\s*=',
        '(?i)(exec|eval|system|cmd|powershell|bash)',
        '[;&|`$(){}[\]\\]',
        '(?i)(select|insert|update|delete|drop|create|alter)\s+',
        '\.\./|\.\.\\',
        '(?i)(http|https|ftp|file)://'
    )
    
    $threatDetected = $false
    foreach ($pattern in $dangerousPatterns) {
        if ($InputText -match $pattern) {
            Write-SecurityLog "Potentially dangerous input detected" "WARNING" "Type: $Type, Pattern: $pattern"
            $threatDetected = $true
        }
    }
    
    # REQUIREMENT: Fully load files instead of saying possible threat (only warn)
    return $true
}

# ============================================
# AGENT TOOL REGISTRY (Requirement D)
# ============================================

$script:agentTools = @{}

function Register-AgentTool {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Description,
        [Parameter(Mandatory=$true)][scriptblock]$Handler,
        [Parameter(Mandatory=$false)][hashtable]$Parameters = @{}
    )

    $script:agentTools[$Name] = @{
        Description = $Description
        Handler     = $Handler
        Parameters  = $Parameters
        Enabled     = $true
    }

    Write-StartupLog "Registered agent tool: $Name" "INFO"
}
# ============================================
# OLLAMA/AI INTEGRATION CORE
# ============================================

function Send-OllamaRequest {
    param(
        [Parameter(Mandatory=$true)][string]$Prompt,
        [string]$Model = "llama3",
        [string]$OllamaHost = "http://localhost:11434",
        [bool]$EnforceJSON = $false
    )
    
    $url = "$OllamaHost/api/generate"
    
    # If EnforceJSON, add JSON format instruction
    $promptWithFormat = if ($EnforceJSON) {
        $Prompt + @"

You must respond with valid JSON in the format: {"tool": "name", "args": {}}
"@
    } else {
        $Prompt
    }
    
    $body = @{ model = $Model; prompt = $promptWithFormat; stream = $false } | ConvertTo-Json
    
    try {
        $response = Invoke-RestMethod -Uri $url -Method POST -Body $body -ContentType "application/json" -ErrorAction Stop
        return $response.response
    } catch {
        Write-StartupLog "Ollama request failed: $($_.Exception.Message)" "ERROR"
        return $null
    }
}

# ============================================
# BACKGROUND JOB/TASK MANAGEMENT (Requirement A-C Compatibility)
# ============================================

$script:BackgroundJobs = @()

function Start-BackgroundTask {
    param(
        [Parameter(Mandatory=$true)][scriptblock]$Task,
        [Parameter(Mandatory=$true)][string]$TaskName,
        [hashtable]$Arguments = @{}
    )
    
    $job = @{
        Name       = $TaskName
        StartTime  = Get-Date
        Task       = $Task
        Arguments  = $Arguments
        Status     = "Running"
        Result     = $null
    }
    
    $script:BackgroundJobs += $job
    Write-StartupLog "Background task started: $TaskName" "INFO"
    
    return $job
}

# ============================================
# OLLAMA CONNECTION VALIDATION
# ============================================

function Test-OllamaConnection {
    param(
        [string]$OllamaHost = "http://localhost:11434"
    )
    
    try {
        $response = Invoke-RestMethod -Uri "$OllamaHost/api/tags" -Method GET -ErrorAction Stop
        Write-StartupLog "✅ Ollama connection successful" "SUCCESS"
        return $true
    } catch {
        Write-StartupLog "❌ Ollama connection failed: $($_.Exception.Message)" "WARNING"
        return $false
    }
}

function Show-OllamaConfigurationDialog {
    [System.Windows.Forms.MessageBox]::Show(
        "Ollama server not found at localhost:11434

Please ensure Ollama is running:
1. Download from ollama.ai
2. Run: ollama serve
3. In another terminal: ollama pull llama3

Would you like to continue without AI features?",
        "Ollama Configuration Required",
        [System.Windows.Forms.MessageBoxButtons]::OKCancel,
        [System.Windows.Forms.MessageBoxIcon]::Information
    )
}

function Verify-AgentToolRegistry {
    <#
    .SYNOPSIS
        Verifies all registered agent tools at startup (Requirement D)
    .DESCRIPTION
        Iterates through all tools and validates that handlers are valid ScriptBlocks.
        Disables invalid tools gracefully rather than crashing.
    #>
    
    Write-StartupLog "🔍 Verifying Agent Tool Registry..." "INFO"
    
    if (-not $script:agentTools -or $script:agentTools.Count -eq 0) {
        Write-StartupLog "⚠️ No agent tools registered" "WARNING"
        return
    }
    
    $validCount = 0
    $disabledCount = 0
    
    foreach ($toolName in $script:agentTools.Keys) {
        $tool = $script:agentTools[$toolName]
        
        # Verify handler is a valid ScriptBlock
        if ($tool.Handler -isnot [scriptblock]) {
            Write-StartupLog "❌ Tool '$toolName' has invalid handler - DISABLING" "WARNING"
            $tool.Enabled = $false
            $disabledCount++
        } else {
            Write-StartupLog "✅ Tool '$toolName' verified and enabled" "SUCCESS"
            $tool.Enabled = $true
            $validCount++
        }
    }
    
    Write-StartupLog "📊 Tool Registry: $validCount valid, $disabledCount disabled" "INFO"
}

function Parse-AgentCommand {
    <#
    .SYNOPSIS
        Parses AI responses into structured commands (Requirement C)
    .DESCRIPTION
        Attempts JSON parsing first, then falls back to regex.
        Always returns a structured object or null.
    #>
    param(
        [Parameter(Mandatory=$true)][string]$AIResponse
    )
    
    # Strategy 1: Try JSON parsing (preferred)
    try {
        # Look for JSON pattern in response
        if ($AIResponse -match '\{[^{}]*"tool"[^{}]*\}') {
            $jsonStr = $matches[0]
            $command = ConvertFrom-Json -InputObject $jsonStr -ErrorAction Stop
            
            if ($command.tool -and $script:agentTools.ContainsKey($command.tool)) {
                Write-StartupLog "✅ Parsed JSON command: $($command.tool)" "SUCCESS"
                return $command
            }
        }
    } catch {
        Write-StartupLog "⚠️ JSON parsing failed, attempting regex fallback" "WARNING"
    }
    
    # Strategy 2: Regex fallback for legacy/incomplete responses
    if ($AIResponse -match '(?i)^/(?<tool>\w+)\s*(?<args>.*)') {
        $toolName = $Matches['tool']
        if ($script:agentTools.ContainsKey($toolName)) {
            $command = @{
                tool = $toolName
                args = $Matches['args'].Trim()
                parsedVia = "regex"
            }
            Write-StartupLog "✅ Parsed regex command: $toolName" "INFO"
            return $command
        }
    }
    
    Write-StartupLog "⚠️ No recognized command pattern in AI response" "WARNING"
    return $null
}

Export-ModuleMember -Function Write-EmergencyLog, Write-StartupLog, Send-OllamaRequest, Start-BackgroundTask, Test-OllamaConnection, Show-OllamaConfigurationDialog, Verify-AgentToolRegistry, Parse-AgentCommand
