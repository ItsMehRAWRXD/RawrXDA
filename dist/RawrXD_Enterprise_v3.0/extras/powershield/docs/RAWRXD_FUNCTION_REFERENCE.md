# 📖 RawrXD Function Reference Guide

**File**: `RawrXD.ps1`  
**Total Lines**: 27,877  
**Total Functions**: 150+  
**Last Updated**: November 25, 2025

---

## 🗂️ Function Categories & Index

### **Core Systems (Lines 100-1,000)**
Emergency logging, error handling, and system initialization

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 118 | `Ensure-LoadMetrics` | Validates thread-safe LoadMetrics structure | None |
| 217 | `Write-EmergencyLog` | Color-coded emergency log output | `$Message`, `$Level` (INFO/WARNING/ERROR/SUCCESS/DEBUG) |
| 405 | `Write-ErrorToFile` | Structured error logging to file | `$ErrorMessage`, `$ErrorCategory`, `$LineNumber`, `$StackTrace`, `$PositionMessage` |
| 594 | `Initialize-LogConfiguration` | Setup log paths and configuration | None |
| 648 | `Get-LogFilePath` | Retrieve current log file path | `$LogType` |
| 683 | `Write-StartupLog` | Startup phase logging | `$Message`, `$Level` |
| 770 | `Register-ErrorHandler` | Register error event handlers | `$ErrorMessage`, `$ErrorCategory`, `$Severity`, `$SourceFunction`, `$AdditionalData` |

---

### **Security & Authentication (Lines 2,000-4,000)**
User authentication, credential validation, and security configuration

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 2199 | `Write-SecurityLog` | Audit security events | `$Message`, `$Level`, `$Details` |
| 2233 | `Protect-SensitiveString` | Encrypt sensitive data | `$Data` |
| 2251 | `Unprotect-SensitiveString` | Decrypt sensitive data | `$EncryptedData` |
| 2269 | `Test-InputSafety` | Validate input for injection patterns | `$InputText`, `$Type` |
| 2295 | `Enable-StealthMode` | Disable logging/debugging features | None |
| 2331 | `Test-SessionSecurity` | Validate current session | None |
| **3574** | **`Test-AuthenticationCredentials`** | ⭐ **NEW: Brute force protection with 5-attempt lockout** | `$Username`, `$Password` |
| 3728 | `Show-AuthenticationDialog` | Display login dialog | None |
| 3927 | `Show-SecuritySettings` | Display security configuration UI | None |
| 4357 | `Get-SecureAPIKey` | Retrieve stored API key | None |
| 4391 | `Set-SecureAPIKey` | Store API key securely | `$ApiKey` |

---

### **Ollama Integration (Lines 4,700-5,500)**
Ollama server management and API interaction

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 4760 | `Connect-OllamaServer` | Establish Ollama connection | `$Host`, `$Port`, `$Username`, `$Password` |
| 4870 | `Authenticate-OllamaUser` | Authenticate with Ollama server | `$Username`, `$Password` |
| 4916 | `Switch-OllamaServer` | Switch between Ollama servers | `$ServerName` |
| 4960 | `Test-OllamaServerConnection` | Test Ollama connectivity | None |
| 4984 | `Get-OllamaServerModels` | List available AI models | None |
| 5073 | `Start-OllamaHealthMonitoring` | Monitor Ollama server health | `$IntervalSeconds` |
| 5114 | `Show-OllamaServerManager` | Display Ollama management UI | None |
| 8474 | `Start-OllamaServer` | Start Ollama service | None |
| 8571 | `Stop-OllamaServer` | Stop Ollama service | None |
| 8606 | `Test-OllamaConnection` | Quick Ollama connectivity test | None |
| 8629 | `Get-OllamaStatus` | Get Ollama service status | None |

---

### **Chat System (Lines 9,000-10,000)**
Multi-tab chat interface and conversation management

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 9080 | `Get-ActiveChatTab` | Get current chat conversation | None |
| 9087 | `Save-ChatHistory` | Persist chat to disk | `$ChatTab`, `$FilePath` |
| 9104 | `Get-ChatHistory` | Load previous conversations | `$FilePath` |
| 9133 | `Clear-ChatHistory` | Clear all chat messages | None |
| 9146 | `Export-ChatHistory` | Export chat to file | `$Format` (JSON/TXT/HTML) |
| 9170 | `Import-ChatHistory` | Import chat from file | `$FilePath` |
| **10722** | **`Send-OllamaRequest`** | ⭐ **NEW: Async job-based requests (non-blocking)** | `$Prompt`, `$Model` |
| 10099 | `Send-Chat` | Process and send chat message | `$Message` |

---

### **Settings & Configuration (Lines 2,900-3,200)**
Settings persistence and UI configuration

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 2998 | `Load-Settings` | Load settings from JSON file | `$SettingsPath` |
| 3098 | `Apply-WindowSettings` | Apply settings to UI controls | None |
| 3178 | `Update-Insights` | Update performance insights display | None |

---

### **File Operations (Lines 4,500-4,700)**
File I/O, caching, and chunked reading

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 4540 | `Get-CachedResponse` | Retrieve cached API responses | `$Key`, `$MaxAgeSeconds` |
| 4575 | `Set-CachedResponse` | Store API response in cache | `$Key`, `$Value`, `$TTLSeconds` |
| 4593 | `Read-FileChunked` | Read large files in chunks | `$FilePath`, `$ChunkSizeKB` |
| 4631 | `Write-StructuredErrorLog` | Write structured error logs | `$ErrorMessage`, `$ErrorCategory`, `$Severity`, `$SourceFunction`, `$AdditionalData` |

---

### **API & Web Integration (Lines 4,100-4,500)**
WebView2, browser, and HTTP client

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 4143 | `Test-DotNetVersionForWebView2` | Check .NET version for WebView2 | None |
| 4182 | `Ensure-WebView2ContextMenuShim` | Setup WebView2 context menus | None |
| 4445 | `Read-SecureInput` | Read secure password input | `$Prompt`, `$AsSecureString` |
| 4475 | `Test-InputValidation` | Validate input by type | `$Input`, `$Type` |
| 11360 | `Open-Browser` | Open URL in WebView2/IE | `$URL` |

---

### **Git Integration (Lines 11,600-11,700)**
Git command execution and status display

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 11649 | `Get-GitStatus` | Get formatted git status | None |
| 11674 | `Invoke-GitCommand` | Execute git commands | `$Command`, `$Arguments` |

---

### **Developer Console (Lines 8,300-8,400)**
Dev console output and logging

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 8389 | `Write-DevConsole` | Output to developer console | `$Message`, `$Level` |

---

### **Dependencies & Build (Lines 6,700-7,200)**
Dependency tracking and build system integration

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 6769 | `Track-Dependency` | Track project dependency | `$DependencyName`, `$Version` |
| 6846 | `Analyze-DependencyHealth` | Analyze dependency health | None |
| 6901 | `Start-DependencySecurityScan` | Scan for security vulnerabilities | None |
| 6947 | `Register-BuildSystem` | Register build system | `$SystemName` |
| 6975 | `Detect-ProjectDependencies` | Detect project dependencies | `$ProjectPath` |
| 7077 | `Resolve-DependencyConflicts` | Resolve conflicting versions | None |
| 7129 | `Export-DependencyReport` | Export dependency report | `$OutputPath` |

---

### **Task Scheduling & Agents (Lines 7,300-7,900)**
Agent task automation and execution

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 7303 | `Schedule-AgentTask` | Schedule background task | `$TaskName`, `$Schedule`, `$Command` |
| 7385 | `Test-ResourceAvailability` | Check resource availability | None |
| 7432 | `Start-ScheduledTaskExecution` | Execute scheduled task | `$TaskName` |
| 7500 | `Monitor-TaskExecution` | Monitor task progress | `$TaskName` |
| 7579 | `Complete-TaskExecution` | Mark task as complete | `$TaskName` |
| 7642 | `Retry-FailedTask` | Retry failed task with backoff | `$TaskName`, `$MaxRetries` |
| 7681 | `Fail-TaskExecution` | Mark task as failed | `$TaskName`, `$ErrorMessage` |
| 7715 | `Get-TaskStatusReport` | Get task execution report | None |
| 7756 | `Export-TaskReport` | Export task report to file | `$OutputPath` |
| 7785 | `Invoke-TaskMaintenance` | Cleanup old tasks | None |

---

### **UI Components (Lines 1,300-2,000)**
Windows Forms controls and event handling

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 1335 | `Initialize-WindowsForms` | Initialize Windows Forms assemblies | None |
| 1100 | `Show-FindDialog` | Display Find/Search dialog | None |
| 1182 | `Show-ReplaceDialog` | Display Find & Replace dialog | None |
| 889 | `Show-ErrorNotification` | Show error notification dialog | `$ErrorMessage` |
| 1049 | `Show-ErrorReportDialog` | Show error report window | None |
| 8794 | `Update-UndoRedoMenuState` | Update undo/redo menu items | None |

---

### **CLI Commands (Lines 21,900-26,700)**
Command-line interface handlers

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 22050 | `Invoke-CliTestOllama` | CLI: Test Ollama | None |
| 22100 | `Invoke-CliListModels` | CLI: List models | None |
| 22120 | `Invoke-CliChat` | CLI: Interactive chat | `$Model` |
| 22173 | `Invoke-CliAnalyzeFile` | CLI: Analyze file | `$FilePath`, `$Model` |
| 22280 | `Invoke-CliGitStatus` | CLI: Show git status | None |
| 22300 | `Invoke-CliCreateAgent` | CLI: Create agent task | `$AgentName`, `$Prompt` |
| 22350 | `Invoke-CliListAgents` | CLI: List agents | None |
| 22380 | `Invoke-CliMarketplaceSync` | CLI: Sync marketplace | None |
| 22600 | `Invoke-CliDiagnose` | CLI: Run diagnostics | None |
| 22950 | `Invoke-CliTestEditorSettings` | CLI: Test editor settings | None |
| 23050 | `Invoke-CliTestFileOperations` | CLI: Test file ops | None |
| 23150 | `Invoke-CliTestAllFeatures` | CLI: Test all features | None |
| 23250 | `Invoke-CliGetSettings` | CLI: Get settings | `$SettingName` |
| 23300 | `Invoke-CliSetSetting` | CLI: Set setting | `$SettingName`, `$SettingValue` |

---

### **Performance & Monitoring (Lines 3,200-3,600)**
Performance metrics and insights

| Line | Function | Purpose | Parameters |
|------|----------|---------|-----------|
| 3238 | `Send-InsightEmailNotification` | Email performance insights | `$To`, `$Subject` |
| 3292 | `Analyze-RealTimeInsights` | Analyze performance in real-time | None |
| 3354 | `Update-PerformanceMetrics` | Update performance display | None |
| 3405 | `Check-InsightThresholds` | Check performance thresholds | None |
| 3463 | `Analyze-UserBehavior` | Analyze user interaction patterns | None |
| 3498 | `Send-AlertNotification` | Send alert notification | `$AlertType`, `$Message` |
| 3526 | `Show-DesktopNotification` | Display desktop notification | `$Title`, `$Message` |
| 3590 | `Cleanup-OldInsights` | Archive old performance data | None |
| 3603 | `Export-InsightsReport` | Export performance report | `$OutputPath` |
| 3643 | `Invoke-SecureCleanup` | Secure data cleanup on exit | `$Force` |

---

## 🎯 Quick Reference by Use Case

### **For Authentication Issues**
- `Test-AuthenticationCredentials` (3574) - **NEW: With brute force protection**
- `Show-AuthenticationDialog` (3728)
- `Get-SecureAPIKey` (4357)
- `Set-SecureAPIKey` (4391)

### **For Ollama/AI Issues**
- `Test-OllamaConnection` (8606)
- `Get-OllamaServerModels` (4984)
- `Send-OllamaRequest` (10722) - **NEW: With async processing**
- `Start-OllamaServer` (8474)
- `Get-OllamaStatus` (8629)

### **For Chat Issues**
- `Send-Chat` (10099)
- `Get-ChatHistory` (9104)
- `Save-ChatHistory` (9087)
- `Clear-ChatHistory` (9133)
- `Export-ChatHistory` (9146)

### **For Settings/Configuration**
- `Load-Settings` (2998)
- `Apply-WindowSettings` (3098)
- `Invoke-CliGetSettings` (23250)
- `Invoke-CliSetSetting` (23300)

### **For Security**
- `Protect-SensitiveString` (2233)
- `Unprotect-SensitiveString` (2251)
- `Test-InputSafety` (2269)
- `Write-SecurityLog` (2199)
- `Invoke-SecureCleanup` (3643)

### **For Performance Debugging**
- `Update-PerformanceMetrics` (3354)
- `Analyze-RealTimeInsights` (3292)
- `Check-InsightThresholds` (3405)
- `Export-InsightsReport` (3603)

### **For CLI Usage**
- `Invoke-CliTestOllama` (22050)
- `Invoke-CliChat` (22120)
- `Invoke-CliAnalyzeFile` (22173)
- `Invoke-CliDiagnose` (22600)

---

## 🔧 Function Patterns & Conventions

### **Error Handling Pattern**
```powershell
try {
    # Main logic
} catch {
    Write-SecurityLog "Error: $_" "ERROR"
    Register-ErrorHandler -ErrorMessage $_.Exception.Message `
        -ErrorCategory "CATEGORY" -Severity "HIGH" `
        -SourceFunction "FunctionName"
}
```

### **Security Pattern**
```powershell
# Validate input
if (-not (Test-InputSafety -Input $userInput -Type "Prompt")) {
    Write-SecurityLog "Dangerous input blocked" "WARNING"
    return
}

# Log activity
Write-SecurityLog "Action performed" "INFO"
```

### **Async Pattern** (NEW)
```powershell
# Non-blocking REST call
$job = Start-Job -ScriptBlock { Invoke-RestMethod ... }
# Wait with progress
while ($job.State -eq 'Running') { 
    # Show spinner...
}
$result = Receive-Job -Job $job
```

### **CLI Command Pattern**
```powershell
function Invoke-CliXXXCommand {
    param([string]$Param1)
    
    try {
        Write-Host "Processing..." -ForegroundColor Yellow
        # Main logic
        Write-Host "✓ Complete" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "✗ Error: $_" -ForegroundColor Red
        return $false
    }
}
```

---

## 📊 Statistics

| Metric | Count |
|--------|-------|
| Total Functions | 150+ |
| Core System Functions | 7 |
| Security Functions | 12 |
| Ollama Functions | 12 |
| Chat Functions | 7 |
| File Operations | 4 |
| CLI Commands | 14+ |
| Task Management | 9 |
| Performance Monitoring | 10 |
| UI Functions | 5 |

---

## 🚀 Recently Enhanced Functions

### **Send-OllamaRequest** (Line 10722) - Async Processing
**Enhancement**: Non-blocking REST calls using `Start-Job`
- Prevents UI freezes during long-running AI generation
- Shows spinner progress indicator
- Automatic job cleanup and timeout handling
- **Impact**: Eliminates 60+ second UI hangs

### **Test-AuthenticationCredentials** (Line 3574) - Rate Limiting
**Enhancement**: Brute force protection with account lockout
- 5 failed attempts trigger 5-minute account lock
- Exponential backoff support
- Comprehensive attempt tracking
- **Impact**: Prevents dictionary attacks and brute force exploits

---

## 🔗 Related Documents

- **RAWRXD_COMPREHENSIVE_AUDIT_REPORT.md** - Full audit findings and recommendations
- **SECURITY-SETTINGS-FIX-REPORT.md** - Security configuration documentation
- **ASM-CORE-README.md** - Native assembly integration details

---

**Last Updated**: November 25, 2025  
**Maintained By**: GitHub Copilot AI  
**Status**: ✅ Current & Comprehensive
