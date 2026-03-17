# RawrXD IDE - Configuration Guide

## Overview
RawrXD IDE now includes a comprehensive startup readiness checker that validates all subsystems before enabling autonomous agent operations. This ensures a reliable, production-ready environment for agentic workflows.

## Configuration Files

### Primary Configuration File
**Location:** `%APPDATA%\RawrXD\RawrXD.ini` (Windows)
- Automatically created on first run with sensible defaults
- Stores all IDE and agent settings
- Hot-reloadable (changes take effect on restart)

### Environment Variables (Optional, Override Config)
```powershell
# Project Root (highest priority)
$env:RAWRXD_PROJECT_ROOT = "E:\"

# Model Cache Directory
$env:RAWRXD_MODEL_CACHE = "D:\OllamaModels"

# Log Level
$env:RAWRXD_LOG_LEVEL = "info"  # debug, info, warning, error
```

## Default Project Root Configuration

### Priority Order
1. **Settings File** (`project/default_root` in RawrXD.ini)
2. **Environment Variable** (`RAWRXD_PROJECT_ROOT`)
3. **Smart Defaults:**
   - `E:\` if exists
   - `D:\RawrXD-production-lazy-init` if exists
   - Current working directory as fallback

### Setting via Code
```cpp
SettingsManager::instance().setDefaultProjectRoot("E:\\MyProject");
```

### Setting via Environment
```powershell
# Persistent (system-wide)
[System.Environment]::SetEnvironmentVariable("RAWRXD_PROJECT_ROOT", "E:\", "User")

# Temporary (current session)
$env:RAWRXD_PROJECT_ROOT = "E:\"
```

## LLM Endpoint Configuration

### Ollama (Local)
**Default:** `http://localhost:11434`

**Config File:**
```ini
[llm]
ollama_endpoint=http://localhost:11434
```

**Required:** Ollama must be running locally on port 11434

### Claude (Cloud)
**Default:** `https://api.anthropic.com`

**Config File:**
```ini
[llm]
claude_endpoint=https://api.anthropic.com
claude_api_key=sk-ant-api03-xxxxx
```

**Required:** Valid Anthropic API key

### OpenAI (Cloud)
**Default:** `https://api.openai.com/v1`

**Config File:**
```ini
[llm]
openai_endpoint=https://api.openai.com/v1
openai_api_key=sk-proj-xxxxx
```

**Required:** Valid OpenAI API key

## GGUF Server Configuration

### Port Configuration
**Default:** `11434` (matches Ollama)

**Config File:**
```ini
[gguf]
server_port=11434
auto_start=true
```

**Note:** If port is in use, the server will report an error but IDE will continue to function.

## Startup Readiness Checks

The IDE performs 8-10 comprehensive checks on startup:

### 1. LLM Endpoints
- **Ollama:** Tests local connectivity
- **Claude:** Validates API key if configured
- **OpenAI:** Validates API key if configured

### 2. GGUF Server
- Tests port availability
- Validates HTTP responsiveness
- Reports latency

### 3. Hotpatch Manager
- Verifies initialization
- Checks memory protection capabilities

### 4. Project Root
- Validates path exists
- Checks read/write permissions
- Reports file/directory count

### 5. Environment Variables
- Lists all RAWRXD_* variables
- Warns if recommended vars missing
- Non-blocking (optional)

### 6. Network Connectivity
- Tests internet access (8.8.8.8:53)
- Warns if limited connectivity
- Non-blocking (local-only mode OK)

### 7. Disk Space
- Requires minimum 10GB free
- Checks project root drive
- Reports available space

### 8. Model Cache
- Validates cache directory
- Creates if missing
- Checks write permissions
- Reports cached model count

## Readiness Dialog Actions

### All Checks Pass ✓
- IDE starts normally
- All agent features enabled
- Project root auto-opened
- Status: "All systems ready!"

### Some Checks Fail ⚠
- **Retry Failed Checks** - Re-runs only failed validations
- **Configure Settings** - Opens configuration help
- **Skip & Continue** - Proceeds with limited functionality
- **Continue Anyway** - Accepts risks and proceeds

### Check Failures Handling
- **Critical failures** (disk space, project root) = feature degradation
- **Non-critical failures** (cloud LLM, network) = warnings only
- Detailed diagnostic log provided for troubleshooting

## Configuration Examples

### Example 1: Local-Only Development
```ini
[project]
default_root=D:\\MyProject

[llm]
ollama_endpoint=http://localhost:11434
# No cloud LLM keys needed

[gguf]
server_port=11434
auto_start=true

[model]
cache_dir=D:\\Models

[agent]
auto_bootstrap_enabled=true
max_concurrent_tasks=3
```

### Example 2: Cloud + Local Hybrid
```ini
[project]
default_root=E:\\

[llm]
ollama_endpoint=http://localhost:11434
claude_endpoint=https://api.anthropic.com
claude_api_key=sk-ant-api03-xxxxx
openai_endpoint=https://api.openai.com/v1
openai_api_key=sk-proj-xxxxx

[gguf]
server_port=11434
auto_start=true

[agent]
auto_bootstrap_enabled=true
max_concurrent_tasks=5
timeout_seconds=600
```

### Example 3: Production Deployment
```ini
[project]
default_root=E:\\Production

[llm]
claude_endpoint=https://api.anthropic.com
claude_api_key=sk-ant-api03-xxxxx

[gguf]
server_port=11434
auto_start=true

[model]
cache_dir=F:\\ModelCache

[agent]
auto_bootstrap_enabled=true
max_concurrent_tasks=10
timeout_seconds=300

[hotpatch]
enabled=true
auto_backup=true

[log]
level=info
file_enabled=true
```

## Troubleshooting

### Check 1: LLM Endpoint Unreachable
**Symptom:** "Ollama endpoint unreachable"
**Fix:**
1. Verify Ollama is running: `curl http://localhost:11434/api/tags`
2. Check firewall settings
3. Try different port in config

### Check 2: GGUF Server Port In Use
**Symptom:** "GGUF Server port 11434 not accessible"
**Fix:**
1. Check if Ollama is already using the port
2. Change port in settings: `gguf/server_port=11435`
3. Or stop conflicting service

### Check 3: Project Root Not Found
**Symptom:** "Project root does not exist: E:\"
**Fix:**
1. Set valid path: `$env:RAWRXD_PROJECT_ROOT = "D:\MyProject"`
2. Or update config file
3. Ensure drive is mounted/accessible

### Check 4: Low Disk Space
**Symptom:** "Low disk space: only 8GB available (need 10GB)"
**Fix:**
1. Free up disk space
2. Change project root to different drive
3. Change model cache location: `model/cache_dir=F:\Cache`

### Check 5: Model Cache Permission Denied
**Symptom:** "Model cache not writable"
**Fix:**
1. Run IDE as administrator (one time)
2. Or change cache to user-writable location:
   ```ini
   [model]
   cache_dir=%USERPROFILE%\AppData\Local\RawrXD\models
   ```

## Performance Tuning

### Fast Startup (Skip Non-Critical Checks)
Set shorter timeouts in settings:
```cpp
StartupReadinessChecker checker;
checker.setTimeout(2000);  // 2 seconds instead of 5
checker.setMaxRetries(1);  // 1 retry instead of 3
```

### Disable Readiness Check (NOT RECOMMENDED)
Comment out in `MainWindow.cpp` constructor:
```cpp
// QTimer::singleShot(1000, this, [this]() {
//     StartupReadinessDialog* readinessDialog = ...
//     ...
// });
```

## Advanced Configuration

### Custom Health Check Extensions
Add custom checks by extending `StartupReadinessChecker`:
```cpp
class CustomChecker : public StartupReadinessChecker {
    HealthCheckResult checkCustomSubsystem() {
        // Your validation logic
    }
};
```

### Metrics Integration
Export readiness metrics to Prometheus:
```cpp
connect(checker, &StartupReadinessChecker::readinessComplete,
        this, [](const AgentReadinessReport& report) {
    PrometheusExporter::gauge("ide_readiness_latency_ms", report.totalLatency);
    PrometheusExporter::gauge("ide_checks_passed", report.checks.size() - report.failures.size());
    PrometheusExporter::gauge("ide_checks_failed", report.failures.size());
});
```

## Security Considerations

### API Key Storage
- API keys stored in settings file (user-accessible location)
- Redacted in logs automatically
- Consider using OS credential store for production:
  ```cpp
  // Use Windows Credential Manager
  QString apiKey = WindowsCredentialStore::read("RawrXD_Claude_Key");
  SettingsManager::instance().setValue("llm/claude_api_key", apiKey);
  ```

### Network Security
- All LLM communication uses HTTPS by default
- Certificate validation enabled
- Proxy support: Set system proxy environment variables

## Logging

### Log Locations
- **IDE Log:** `%APPDATA%\RawrXD\logs\ide.log`
- **Agent Log:** `%APPDATA%\RawrXD\logs\agent.log`
- **Console:** stderr output (when launched from terminal)

### Log Levels
```ini
[log]
level=debug  # debug|info|warning|error
file_enabled=true
console_enabled=true
```

### Diagnostic Output
The readiness dialog provides real-time diagnostic log output:
- Timestamp for each check
- Success/failure indicators (✓/✗)
- Technical details for failures
- Total validation time

## Support

For issues or questions:
1. Check diagnostic log in readiness dialog
2. Review `%APPDATA%\RawrXD\RawrXD.ini` settings
3. Verify environment variables: `Get-ChildItem Env:RAWRXD_*`
4. Check system logs for errors

