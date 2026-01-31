# RawrXD Phase 2 Implementation Plan

**Status**: In Progress  
**Estimated Duration**: 11 hours total  
**Date Started**: November 25, 2025

---

## Overview

Phase 2 focuses on hardening security (API key encryption) and improving code maintainability (CLI modularization).

### Tasks

| # | Task | Duration | Priority | Status |
|---|------|----------|----------|--------|
| **1** | Implement API Key Encryption Module | 3 hrs | 🔴 HIGH | 🟡 IN PROGRESS |
| **2** | Migrate Existing Key Storage | 1 hr | 🔴 HIGH | ⏳ PENDING |
| **3** | Modularize CLI Command Handlers | 8 hrs | 🟡 MEDIUM | ⏳ PENDING |
| **4** | Test All Modules | 2 hrs | 🔴 HIGH | ⏳ PENDING |
| **5** | Update Documentation | 1 hr | 🟡 MEDIUM | ⏳ PENDING |

**Total**: 15 hours (with testing & documentation)

---

## Task 1: API Key Encryption Module (3 hrs)

### Objective
Replace plain text API key storage with SecureString + DPAPI encryption.

### Current Implementation (Lines 148-183)
```powershell
# BEFORE: Plain text storage in environment variables
function Get-SecureAPIKey {
    $envKeyCandidates = @('RAWRXD_API_KEY','OLLAMA_API_KEY','RAWRAI_API_KEY') | 
        ForEach-Object { (Get-Item -Path ("env:" + $_) -ErrorAction SilentlyContinue).Value }
    # Returns unencrypted string
}
```

### New Architecture

#### File: `secure-api-key-module.ps1`
- **Purpose**: Centralized encryption/decryption for API keys
- **Location**: `Powershield/modules/secure-api-key-module.ps1`
- **Functions**:
  - `New-SecureAPIKeyStore` - Initialize encrypted storage
  - `Get-SecureAPIKey` - Retrieve decrypted key
  - `Set-SecureAPIKey` - Store encrypted key (DPAPI)
  - `Revoke-SecureAPIKey` - Clear credentials
  - `Test-SecureAPIKeyIntegrity` - Verify encryption

#### Encryption Strategy
```
Plain Text API Key
        ↓
SecureString (PowerShell managed)
        ↓
DPAPI Encryption (Windows Data Protection API)
        ↓
Base64 Encoded Storage in config file
        ↓
Registry Fallback (if file unavailable)
```

#### Storage Locations (Priority Order)
1. **Encrypted File**: `Powershield/config/.apikeys.enc` (preferred)
2. **Windows Registry**: `HKCU:\Software\RawrXD\APIKeys` (fallback)
3. **Environment Variables**: Plain text (legacy, marked deprecated)

### Implementation Details

```powershell
# Function 1: Initialize Encryption Store
function New-SecureAPIKeyStore {
    param(
        [string]$StorePath = "$PSScriptRoot/config/.apikeys.enc"
    )
    # - Create encryption key if not exists
    # - Initialize storage location (file or registry)
    # - Set ACLs to restrict access (current user only)
    # - Return store handle for operations
}

# Function 2: Store Encrypted Key
function Set-SecureAPIKey {
    param(
        [string]$KeyName,
        [SecureString]$SecureKey,
        [string]$StorePath = "$PSScriptRoot/config/.apikeys.enc"
    )
    # - Convert SecureString to encrypted bytes
    # - Use DPAPI with CurrentUser scope + machine-specific data
    # - Base64 encode for storage
    # - Write to file with restricted permissions (Mode 600)
    # - Fallback to registry if file write fails
}

# Function 3: Retrieve Decrypted Key
function Get-SecureAPIKey {
    param(
        [string]$KeyName,
        [string]$StorePath = "$PSScriptRoot/config/.apikeys.enc"
    )
    # - Check encrypted file first
    # - Fall back to registry if not found
    # - Fall back to env vars if not in registry (legacy)
    # - Decrypt using DPAPI
    # - Return as [SecureString] or plain [string] depending on caller
    # - Cache in memory (clear on exit)
}

# Function 4: Revoke Stored Key
function Revoke-SecureAPIKey {
    param(
        [string]$KeyName
    )
    # - Securely delete from file
    # - Remove from registry
    # - Clear memory caches
    # - Log revocation event
}

# Function 5: Verify Integrity
function Test-SecureAPIKeyIntegrity {
    # - Verify encryption key exists and accessible
    # - Test decrypt/encrypt round-trip
    # - Check file/registry permissions (user-only access)
    # - Return $true if all checks pass
}
```

### Migration Path
```
Step 1: Load new module alongside old Get-SecureAPIKey
Step 2: Create migration function to import env vars → encrypted storage
Step 3: Keep old function for backward compatibility (with deprecation warning)
Step 4: Users can call New-SecureAPIKeyStore to migrate
Step 5: Eventually remove env var fallback in Phase 3
```

---

## Task 2: Migrate Existing Key Storage (1 hr)

### Migration Function
```powershell
function Invoke-SecureKeyMigration {
    param(
        [switch]$Force  # Skip confirmation
    )
    # 1. Detect existing keys in environment/registry
    # 2. Display current keys (masked)
    # 3. Ask for confirmation to encrypt
    # 4. For each key:
    #    - Read plain text
    #    - Encrypt and store
    #    - Verify round-trip
    #    - Clear plain text source
    # 5. Log migration audit trail
}
```

### Implementation
- Add to `RawrXD.ps1` lines ~200-250
- Call on first launch (detect new installation)
- Provide `-Force` flag for automation

---

## Task 3: Modularize CLI Command Handlers (8 hrs)

### Objective
Split massive 579-line switch statement (Lines 27751-28330) into separate module files.

### Current Structure (Monolithic)
```
RawrXD.ps1 (28,316 lines total)
├── Lines 27751-28330: CLI switch statement (579 lines)
│   ├── test-ollama
│   ├── list-models
│   ├── chat
│   ├── analyze-file
│   ├── git-status
│   ├── marketplace-search
│   ├── marketplace-install
│   ├── vscode-popular
│   ├── vscode-search
│   └── ... 15+ more commands
└── [All imported handlers: Invoke-CliTestOllama, Invoke-CliChat, etc.]
```

### Target Structure (Modularized)
```
Powershield/
├── RawrXD.ps1 (core, ~27,500 lines, CLI handler removed)
└── modules/cli-handlers/
    ├── cli-handlers-loader.ps1 (dynamic import system)
    ├── ollama/
    │   ├── test-ollama.ps1 (Invoke-CliTestOllama)
    │   └── list-models.ps1 (Invoke-CliListModels)
    ├── chat/
    │   ├── chat.ps1 (Invoke-CliChat)
    │   └── analyze-file.ps1 (Invoke-CliAnalyzeFile)
    ├── git/
    │   └── git-status.ps1 (Invoke-CliGitStatus)
    ├── agents/
    │   ├── create-agent.ps1 (Invoke-CliCreateAgent)
    │   └── list-agents.ps1 (Invoke-CliListAgents)
    ├── marketplace/
    │   ├── marketplace-sync.ps1
    │   ├── marketplace-search.ps1
    │   └── marketplace-install.ps1
    ├── vscode/
    │   ├── vscode-popular.ps1
    │   ├── vscode-search.ps1
    │   ├── vscode-install.ps1
    │   └── vscode-categories.ps1
    └── testing/
        ├── test-*.ps1 (various test commands)
        └── diagnose.ps1
```

### Module Loader System

**File**: `modules/cli-handlers/cli-handlers-loader.ps1`

```powershell
function Initialize-CliHandlers {
    param(
        [string]$HandlerPath = "$PSScriptRoot/cli-handlers"
    )
    
    # Dynamically discover and load all handler modules
    # Pattern: Recursive search for *.ps1 files
    # Each file must export a function matching "Invoke-Cli*"
    
    # Returns: hashtable of @{CommandName = FunctionReference}
    #   - "test-ollama" → Invoke-CliTestOllama
    #   - "chat" → Invoke-CliChat
    #   - etc.
}

function Invoke-CliCommand {
    param(
        [string]$Command,
        [hashtable]$HandlerMap,
        [hashtable]$Parameters
    )
    
    # Lookup handler in map
    # Invoke with parameters
    # Return exit code
}
```

### CLI Handler Contract

Each handler module must follow this interface:

```powershell
# Example: modules/cli-handlers/ollama/test-ollama.ps1

<#
.SYNOPSIS
    CLI handler for test-ollama command

.PARAMETER [none]
    (Inherit from parent script scope)
#>

function Invoke-CliTestOllama {
    param()
    
    try {
        # Handler logic here
        # Write-Host output
        # Return $true (success) or $false (failure)
        return $true
    }
    catch {
        Write-Host "Error: $_" -ForegroundColor Red
        return $false
    }
}

# Export function for loader
Export-ModuleMember -Function Invoke-CliTestOllama
```

### Modularization Benefits
- ✅ Reduces main script complexity (28k → ~27.5k lines)
- ✅ Easier maintenance (each handler is 50-100 lines, self-contained)
- ✅ Better testing (can test handlers independently)
- ✅ Faster startup (lazy-load handlers on demand)
- ✅ Clearer code organization (related commands grouped by category)

---

## Task 4: Testing (2 hrs)

### Test Plan

#### API Encryption Tests
```powershell
# Test 1: Encryption/Decryption Round-trip
$key = ConvertTo-SecureString "test-key-12345" -AsPlainText -Force
Set-SecureAPIKey -KeyName "OLLAMA" -SecureKey $key
$retrieved = Get-SecureAPIKey -KeyName "OLLAMA"
Assert $retrieved -eq "test-key-12345"

# Test 2: File Storage Persistence
Set-SecureAPIKey -KeyName "OLLAMA" -SecureKey $key
Restart-PowerShell
$retrieved = Get-SecureAPIKey -KeyName "OLLAMA"
Assert $retrieved -eq "test-key-12345"

# Test 3: Permission Checking
Test-Path ".apikeys.enc" -Access Read  # Verify mode 600

# Test 4: Fallback Chains
# - File unavailable → registry
# - Registry unavailable → env var
# - Env var unavailable → null
```

#### CLI Modularization Tests
```powershell
# Test 1: Handler Discovery
$handlers = Initialize-CliHandlers
Assert $handlers.Count -gt 15

# Test 2: Handler Invocation
$result = Invoke-CliCommand -Command "test-ollama" -HandlerMap $handlers
Assert $result -eq $true

# Test 3: Error Handling
# Invalid command → Exit code 1

# Test 4: Parameter Passing
$result = Invoke-CliCommand -Command "chat" -HandlerMap $handlers `
    -Parameters @{Model = "llama2"}
```

---

## Task 5: Documentation Updates (1 hr)

### Files to Update
1. `README.md` - Add security section
2. `RAWRXD_DEVELOPER_QUICKSTART.md` - Add API key migration steps
3. `SECURITY.md` - New file, describe encryption implementation
4. `CLI_HANDLERS_GUIDE.md` - New file, guide for adding new handlers

---

## Implementation Sequence

```
Day 1 (4 hours):
  ✓ Create secure-api-key-module.ps1 (2 hrs)
  ✓ Migrate existing storage (1 hr)
  ✓ Integration test (1 hr)

Day 2 (4 hours):
  ✓ Create cli-handlers directory structure (1 hr)
  ✓ Implement cli-handlers-loader.ps1 (1 hr)
  ✓ Migrate first 5 handlers (2 hrs)

Day 3 (4 hours):
  ✓ Migrate remaining 10+ handlers (2 hrs)
  ✓ Update main switch statement (1 hr)
  ✓ Integration testing (1 hr)

Day 4 (3 hours):
  ✓ Full end-to-end testing (1.5 hrs)
  ✓ Documentation updates (1.5 hrs)

Total: 15 hours
```

---

## Success Criteria

- ✅ All API keys stored encrypted on disk
- ✅ Backward compatible with env vars (deprecated)
- ✅ Migration tool available and tested
- ✅ 15+ CLI handlers modularized
- ✅ CLI still works identically from user perspective
- ✅ Code complexity reduced (single files <150 lines)
- ✅ All tests passing
- ✅ Documentation complete

---

## Rollback Plan

If issues occur:
1. Revert modularization → Use monolithic switch (easy, no data loss)
2. Revert encryption → Use env vars (easy, keys already there)
3. Combined rollback: Git checkout previous commit

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| CLI handlers missing function | Low | High | Comprehensive testing before merge |
| Encryption key loss | Very Low | Critical | Backup keys to registry automatically |
| Performance regression | Low | Medium | Lazy-load handlers, cache results |
| User confusion (env vars deprecated) | Medium | Low | Clear migration guide + warning messages |

---

## Notes

- **DPAPI Scope**: Use CurrentUser + machine-specific data
  - Secure: Key only works on this user account on this machine
  - Portable: Can't copy encrypted file to another machine/user
  
- **Fallback Strategy**: File → Registry → Env Vars → Null
  - Ensures maximum compatibility
  - Clear priority for security levels

- **Handler Loading**: Lazy-load on first use
  - Faster startup time
  - Lower memory footprint
  - Handlers cached for subsequent invocations

---

## Success Metrics (Post-Deployment)

- API keys never appear in plain text in logs ✅
- Zero security vulnerabilities in key storage ✅
- CLI startup time < 500ms ✅
- Handler functions independently testable ✅
- Developer onboarding time for new CLI commands: 15 min ✅

---

**Next**: Start with Task 1 - Create secure-api-key-module.ps1
