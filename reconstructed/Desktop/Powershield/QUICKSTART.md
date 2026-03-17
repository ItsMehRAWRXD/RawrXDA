# 🚀 RawrXD 3.0 PRO - Quick Start Guide

## What's New

The production-grade RawrXD IDE now includes:

✅ **Agentic Framework** - Intent recognition, adaptive planning, distributed execution  
✅ **Zero-Trust Security** - Path validation, credential management, process safety  
✅ **Production Observability** - Metrics, audit logging, health checks  
✅ **Command Injection Protection** - All process execution uses safe ArgumentList  
✅ **Cryptographic Security** - PBKDF2 key derivation + AES-256-GCM encryption  

---

## Getting Started

### 1. Start RawrXD with Agentic Engine
```powershell
cd E:\Desktop\Powershield
.\RawrXD.ps1
```

**Startup Messages:**
```
✅ Agentic Engine Ready - Intent Recognition, Adaptive Planning, Tool Execution
✅ Security: Zero-trust architecture with path validation & credential vault
✅ Observability: Metrics, Audit Logging, Health Checks enabled
```

### 2. Check Agentic Status
```powershell
# Get metrics
Get-AgenticMetrics

# Output:
# Total Requests: 42
# Successful: 40
# Failed: 2
# Success Rate: 95.24%
# Average Duration: 250ms
```

### 3. Test Secure Process Execution
```powershell
# The new safe way
Invoke-SafeProcess -ProcessName "powershell.exe" -Arguments @("-Version")

# Output:
# @{
#    Success = $true
#    ExitCode = 0
#    Output = "PowerShell 7.x.x"
# }
```

### 4. Use Agentic Flow
```powershell
# Execute intent with streaming updates
Invoke-AgenticFlow -Prompt "Analyze the current project structure" `
  -OnUpdate {
    param($update)
    Write-Host "Status: $($update.Status)"
    Write-Host "Response: $($update.Response)"
  }
```

### 5. Manage Secure Credentials
```powershell
# Store API key securely (Windows Credential Manager)
$cred = Get-Credential  # "user" for username, paste key as password
Set-SecureAICredential -ServiceName "OllamaAPI" -Credential $cred

# Retrieve later (only accessible to current user)
$apiKey = Get-SecureAICredential -ServiceName "OllamaAPI"
```

---

## Security Features Enabled

### 🔒 Path Validation
```powershell
# Safe - within project directory
[SecurityManager]::ValidatePath(".\project\file.txt", "C:\project")
# Returns: $true

# Blocked - directory traversal attempt
[SecurityManager]::ValidatePath("..\..\windows\system32\config", "C:\project")
# Returns: $false
```

### 🔐 Command Injection Prevention
```powershell
# Old (VULNERABLE):
# $psi.Arguments = "-p `"$userPrompt`""  # If prompt = "; rm -rf /"

# New (SAFE):
$arguments = @("-p", $userPrompt)
Invoke-SafeProcess -ProcessName "llama.cpp" -Arguments $arguments
# Prompt is always treated as literal text, never executed
```

### 🔑 Cryptographic Keys
```powershell
# Generate secure random key
$key = [SecurityManager]::GenerateSecureKey(32)  # 256-bit

# Derive from user password
$kdf = [SecurityManager]::DeriveKeyFromPassword("mypassword")
# $kdf.Key - 256-bit encryption key
# $kdf.Salt - Random salt for storage
# $kdf.Iterations - 100,000 (NIST minimum)

# Encrypt data
$encrypted = [SecurityManager]::EncryptData("sensitive data", $kdf.Key)
# Returns: @{ Encrypted = "...", Nonce = "...", Tag = "..." }

# Decrypt
$plaintext = [SecurityManager]::DecryptData(
  $encrypted.Encrypted, 
  $kdf.Key, 
  $encrypted.Nonce
)
```

### 📊 Production Monitoring
```powershell
# Check system health
Get-SystemHealth

# Output:
# @{
#    Timestamp = 2025-12-05T14:30:00Z
#    OverallStatus = "Healthy"
#    Checks = @(
#        @{ Name = "MemoryUsage"; Status = "Healthy"; Value = "456 MB" }
#        @{ Name = "DiskSpace"; Status = "Healthy"; Value = "85% free" }
#    )
# }

# View audit log (JSON Lines format)
Get-Content "$env:APPDATA\RawrXD\Audit\audit-2025-12-05.jsonl" | ConvertFrom-Json | Select-Object Timestamp, EventType, Message
```

---

## Architecture Overview

```
RawrXD 3.0 PRO
    ├── UI Layer
    │   └── Monaco Editor, AI Chat, File Manager
    │
    ├── Agentic Engine
    │   ├── Intent Recognition (NLP pattern matching)
    │   ├── Adaptive Planning (memory-based learning)
    │   ├── Distributed Execution (parallel tool runs)
    │   └── Result Synthesis
    │
    ├── Tool Registry
    │   ├── File Operations (read, write, list, search)
    │   ├── Code Analysis (syntax checking, structure analysis)
    │   ├── System Execution (PowerShell, Git, external tools)
    │   └── AI Integration (model selection, prompt optimization)
    │
    ├── Security Manager
    │   ├── Path Validation (CWE-22 prevention)
    │   ├── Input Sanitization (injection prevention)
    │   ├── Process Execution (ArgumentList safety)
    │   ├── Cryptography (PBKDF2, AES-256-GCM)
    │   └── Credential Management (Windows Vault)
    │
    ├── Agent Memory
    │   ├── Short-term (recent experiences)
    │   ├── Long-term (consolidated patterns)
    │   └── Vector Database (similarity search)
    │
    └── Production Features
        ├── Metrics Collector (Prometheus format)
        ├── Audit Logger (JSONL with rotation)
        ├── Health Checks (system monitoring)
        ├── Circuit Breaker (resilience)
        └── Rate Limiter (DoS prevention)
```

---

## File Locations

```
E:\Desktop\Powershield\
├── RawrXD.ps1 (Main application - UPDATED)
├── SECURITY_REMEDIATION_REPORT.md (This audit report)
│
└── Modules/
    ├── AgenticEngine.psm1 (Agentic framework)
    ├── SecurityManager.psm1 (Zero-trust security)
    ├── ProductionMonitoring.psm1 (Observability)
    └── AgenticIntegration.psm1 (IDE integration)

Logs & Data:
├── $env:APPDATA\RawrXD\startup.log (Startup events)
├── $env:APPDATA\RawrXD\Audit\ (Audit logs - daily JSONL)
└── $env:APPDATA\RawrXD\Logs\ (Application logs)
```

---

## Common Tasks

### View Agentic Metrics Dashboard
```powershell
$metrics = Get-AgenticMetrics
$metrics | Format-Table @(
    @{ Label = "Total Requests"; Expression = { $_.Total } },
    @{ Label = "Success Rate"; Expression = { "$($_.SuccessRate)%" } },
    @{ Label = "Avg Duration"; Expression = { "$($_.AverageDuration)ms" } }
)
```

### Query Audit Log
```powershell
$auditPath = "$env:APPDATA\RawrXD\Audit\audit-$(Get-Date -Format 'yyyy-MM-dd').jsonl"
Get-Content $auditPath | 
    ConvertFrom-Json | 
    Where-Object EventType -eq "Error" | 
    Select-Object Timestamp, Message, Exception
```

### Run Health Diagnostics
```powershell
$health = Get-SystemHealth
if ($health.OverallStatus -ne "Healthy") {
    Write-Warning "System health: $($health.OverallStatus)"
    $health.Checks | Where-Object Status -ne "Healthy"
}
```

### Test Security Validation
```powershell
# This PASSES (safe)
[SecurityManager]::ValidatePath(".\file.txt", "C:\workspace")

# This FAILS (blocked)
[SecurityManager]::ValidatePath("..\..\windows\system32", "C:\workspace")

# This FAILS (blocked)
[SecurityManager]::ValidateUserInput("test`; rm -rf /")
```

---

## Troubleshooting

### Agentic Engine Not Initializing
```powershell
# Check module paths
$ModulesPath = "E:\Desktop\Powershield\Modules"
Get-ChildItem $ModulesPath -Filter "*.psm1"

# Verify imports
Get-Module -Name AgenticEngine -ErrorAction SilentlyContinue
Get-Module -Name SecurityManager -ErrorAction SilentlyContinue

# Check startup log
Get-Content "$env:APPDATA\RawrXD\startup.log" | Tail -20
```

### Process Execution Failures
```powershell
# Test safe execution
$result = Invoke-SafeProcess -ProcessName "powershell.exe" -Arguments @("-Version")
if (-not $result.Success) {
    Write-Warning "Error: $($result.Error)"
}
```

### Credential Issues
```powershell
# Clear cached credentials
cmdkey /delete:RawrXD_OllamaAPI

# Re-store
$cred = Get-Credential
Set-SecureAICredential -ServiceName "OllamaAPI" -Credential $cred
```

---

## Next Steps

1. **Test the agentic engine** with sample prompts
2. **Review audit logs** for security events
3. **Configure rate limits** based on expected load
4. **Set up log aggregation** to a SIEM (optional)
5. **Schedule security reviews** monthly

---

## Support

**Security Questions:**  
Review: `/SECURITY_REMEDIATION_REPORT.md`

**Architecture Questions:**  
Review: Agentic engine source code and comments

**Bug Reports:**  
Check: `$env:APPDATA\RawrXD\startup.log` for errors

---

**Ready to go!** 🚀  
Start RawrXD and enjoy production-grade agentic AI capabilities with enterprise security.
