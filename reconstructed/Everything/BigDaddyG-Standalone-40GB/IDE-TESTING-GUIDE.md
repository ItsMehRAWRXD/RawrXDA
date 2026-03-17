# BigDaddyG IDE - Comprehensive Testing Harness Guide

## 📋 Overview

This testing harness provides **three complementary testing tools** to fully exercise the BigDaddyG IDE API without relying on a working UI. Since the UI has issues (Monaco Editor not loading, chat input missing, model selector absent), all features are tested directly via HTTP/WebSocket APIs.

## 🎯 What Can Be Tested

### ✅ API Endpoints Tested
- **Model Discovery**: `/v1/models` - List all 93+ available models
- **Chat/Inference**: `/v1/chat/completions` - Direct model inference
- **Code Execution**: `/v1/execute` - Run commands directly
- **File Operations**: `/v1/files` - List, read, write files
- **Settings**: `/v1/settings`, `/v1/config` - Get and update settings
- **System Info**: `/v1/system/info`, `/v1/metrics` - Hardware diagnostics
- **Agent Systems**: `/v1/agent/execute`, `/v1/swarm` - Test agentic features
- **Voice**: `/v1/voice/config` - Voice processing setup
- **Code Analysis**: `/v1/analyze` - Security & syntax checking
- **WebSocket**: `ws://localhost:11441` - Real-time communication

### ⚠️ Known UI Issues (Bypassed)
- ❌ Monaco Editor not loading → **Use `/v1/analyze` endpoint instead**
- ❌ Chat input missing → **Use `/v1/chat/completions` directly**
- ❌ Model selector not visible → **Use `/v1/models` then pass model parameter**
- ❌ Bottom pane not resizable → **Not needed for API testing**

## 🚀 Quick Start

### Prerequisites
- Node.js with npm (to run the IDE)
- PowerShell 5.1+ (for PowerShell scripts) OR bash (for shell scripts)
- curl (usually pre-installed on Windows 10+)
- The BigDaddyG IDE running on ports 11441 and 3000

### Start the IDE (in separate terminal)
```powershell
cd "E:\Everything\BigDaddyG-Standalone-40GB\app"
npm start
```

Wait for output showing:
```
✅ Orchestra server process started
✅ Micro-Model-Server process started
✅ Application menu created
```

## 🧪 Testing Tools

### Tool 1: PowerShell Comprehensive Harness

**File**: `IDE-Testing-Harness.ps1`

**What it does**:
- 13 test sections
- 50+ individual tests
- Color-coded output with detailed logging
- JSON export of results
- Concurrent stress testing

**Usage**:
```powershell
# Quick test (connectivity only)
.\IDE-Testing-Harness.ps1 -QuickTest

# Deep test (all features)
.\IDE-Testing-Harness.ps1 -DeepTest

# Fix mode (diagnostic help)
.\IDE-Testing-Harness.ps1 -FixMode

# Test specific feature
.\IDE-Testing-Harness.ps1 -TargetFeature "Chat-Interface"
```

**Output Example**:
```
🔌 TESTING SERVER CONNECTIVITY...
  Orchestra Server (Port 11441):
    ✅ Orchestra server responding on port 11441
    ℹ️  Status: 200, Response size: 93 items

💬 TESTING CHAT/INFERENCE...
  Testing /v1/chat/completions endpoint:
    ✅ Chat endpoint responding
    ✅ Got response with 1 choice(s)
```

**Features**:
- Detailed logging of each request/response
- Success/failure summary by feature
- Performance metrics
- Export results to JSON file

### Tool 2: PowerShell Curl-Based Harness

**File**: `IDE-Testing-Curl.ps1`

**What it does**:
- Lightweight curl wrapper
- Faster execution
- Less dependency on PowerShell features
- Good for rapid testing

**Usage**:
```powershell
# Quick connectivity test
.\IDE-Testing-Curl.ps1 -Quick

# Test models only
.\IDE-Testing-Curl.ps1 -ModelsOnly

# Test chat only
.\IDE-Testing-Curl.ps1 -ChatOnly

# Full suite
.\IDE-Testing-Curl.ps1 -Full

# Stress test (10 concurrent)
.\IDE-Testing-Curl.ps1 -StressTest
```

**Output Example**:
```
✅ Orchestra Server (HTTP 200)
✅ Micro-Model-Server (HTTP 200)
📊 Total models: 93
✅ Chat Completion (HTTP 200)
✅ Passed: 15 | ❌ Failed: 0
```

### Tool 3: Bash Cross-Platform Harness

**File**: `IDE-Testing-Harness.sh`

**What it does**:
- POSIX-compliant shell scripting
- Works on Windows (Git Bash, WSL), macOS, Linux
- Requires: curl, bash
- Optional: jq for JSON parsing

**Installation** (if needed):
```bash
# Windows Git Bash: Already included
# macOS: brew install jq
# Linux: sudo apt-get install jq
```

**Usage**:
```bash
# Make executable (first time only)
chmod +x ./IDE-Testing-Harness.sh

# Quick test
./IDE-Testing-Harness.sh quick

# Full test
./IDE-Testing-Harness.sh full

# Stress test
./IDE-Testing-Harness.sh stress
```

**Output Example**:
```
✅ Orchestra Server (HTTP 200)
✅ Micro-Model-Server (HTTP 200)
📊 Found 93 models
✅ Chat Completion (HTTP 200)
════════════════════════════════════════════
✅ Passed: 15 | ❌ Failed: 0
════════════════════════════════════════════
```

## 📊 Test Coverage Map

```
┌─────────────────────────────────────────────────────────┐
│ SERVER CONNECTIVITY                                     │
│  ├─ Orchestra Server (11441)                            │
│  └─ Micro-Model-Server (3000)                           │
├─────────────────────────────────────────────────────────┤
│ MODEL DISCOVERY                                         │
│  ├─ List all models (/v1/models)                        │
│  ├─ Count and validate structure                        │
│  └─ Sample first 5 models                               │
├─────────────────────────────────────────────────────────┤
│ CHAT/INFERENCE                                          │
│  ├─ Chat completions (/v1/chat/completions)             │
│  ├─ Micro-Model direct chat (/api/chat)                 │
│  ├─ Message handling (system, user, assistant)          │
│  └─ Temperature & token control                         │
├─────────────────────────────────────────────────────────┤
│ CODE EXECUTION                                          │
│  ├─ Command execution (/v1/execute)                     │
│  ├─ Terminal sessions (/v1/terminal)                    │
│  ├─ Language support validation                         │
│  └─ Timeout handling                                    │
├─────────────────────────────────────────────────────────┤
│ FILE SYSTEM                                             │
│  ├─ Directory listing (/v1/files)                       │
│  ├─ File read/write (future)                            │
│  └─ Path validation                                     │
├─────────────────────────────────────────────────────────┤
│ AGENTIC FEATURES                                        │
│  ├─ Agent execution (/v1/agent/execute)                 │
│  ├─ Agent swarm coordination (/v1/swarm)                │
│  ├─ Multi-agent tasks                                   │
│  └─ Task coordination                                   │
├─────────────────────────────────────────────────────────┤
│ VOICE & AUDIO                                           │
│  ├─ Voice config (/v1/voice/config)                     │
│  ├─ Audio format support                                │
│  └─ Codec validation                                    │
├─────────────────────────────────────────────────────────┤
│ CODE ANALYSIS                                           │
│  ├─ Syntax checking (/v1/analyze)                       │
│  ├─ Security analysis                                   │
│  ├─ Performance suggestions                             │
│  └─ Multi-language support                              │
├─────────────────────────────────────────────────────────┤
│ SETTINGS & CONFIG                                       │
│  ├─ Get settings (/v1/settings)                         │
│  ├─ Update config (/v1/config)                          │
│  └─ Persistence validation                              │
├─────────────────────────────────────────────────────────┤
│ PERFORMANCE & DIAGNOSTICS                               │
│  ├─ System info (/v1/system/info)                       │
│  ├─ Performance metrics (/v1/metrics)                   │
│  ├─ Resource usage                                      │
│  └─ Uptime monitoring                                   │
├─────────────────────────────────────────────────────────┤
│ WEBSOCKET TESTING                                       │
│  ├─ Connection to ws://localhost:11441                  │
│  ├─ Connection to ws://localhost:3000                   │
│  ├─ Message delivery                                    │
│  └─ Client tracking                                     │
├─────────────────────────────────────────────────────────┤
│ STRESS & LOAD TESTING                                   │
│  ├─ Concurrent requests (5, 10, 20)                     │
│  ├─ Response time under load                            │
│  └─ Error rate validation                               │
└─────────────────────────────────────────────────────────┘
```

## 🔍 Understanding Test Output

### Success Indicators
```
✅ Test Name (HTTP 200)  → PASS
📊 Count: 93             → Information
ℹ️  Details provided      → Debug info
```

### Failure Indicators
```
❌ Test Name (HTTP 500)  → FAIL
⚠️  Warning about issue   → Non-blocking
🔴 Critical error        → Blocking issue
```

### Summary Report
```
═══════════════════════════════════════════════════════════
✅ PASSED: 47 tests
❌ FAILED: 3 tests
📊 RESULTS BY FEATURE:
  ✅ Server-Orchestra: 2/2 passed
  ✅ Chat-Interface: 3/3 passed
  ⚠️  Voice-AI: 1/2 passed
═══════════════════════════════════════════════════════════
```

## 🐛 Troubleshooting

### "Connection refused" on port 11441
```powershell
# Check if ports are in use
netstat -ano | findstr "11441\|3000"

# Kill existing node processes
Get-Process node | Stop-Process -Force

# Restart the IDE
cd "E:\Everything\BigDaddyG-Standalone-40GB\app"
npm start
```

### Incomplete responses
Some endpoints may require authentication or specific headers. Add them:
```powershell
$headers = @{
    'Authorization' = 'Bearer YOUR_TOKEN'
    'User-Agent' = 'BigDaddyG-IDE-Harness/1.0'
}
```

### WebSocket testing
Install `wscat` for interactive WebSocket testing:
```bash
npm install -g wscat
wscat -c ws://localhost:11441
# Then send messages like: {"command":"test"}
```

## 📈 Performance Baselines

Expected response times (when system is healthy):
```
Model listing              < 100ms
Chat completion            < 500ms (varies by model)
File operations            < 200ms
Code analysis              < 300ms
Settings retrieval         < 50ms
Stress test (10 concurrent) < 2s total
```

## 💡 Advanced Testing Scenarios

### Test with custom model
```powershell
$payload = @{
    model = "llama2:7b"  # Your model name
    messages = @(
        @{ role = "user"; content = "What is 2+2?" }
    )
}

.\IDE-Testing-Curl.ps1 -CustomEndpoint "POST:localhost:11441/v1/chat/completions"
```

### Test with authentication
```bash
curl -H "Authorization: Bearer YOUR_TOKEN" \
     http://localhost:11441/v1/models
```

### Monitor live responses
```bash
# Keep testing while monitoring output
while true; do
    ./IDE-Testing-Harness.sh quick
    sleep 5
done
```

## 📝 Interpreting Results

### All tests passing ✅
IDE is fully functional. UI issues are just rendering problems.

### Chat tests passing, others failing ⚠️
Core inference works but auxiliary features need attention.

### Models not loading ❌
Check Orchestra server:
```powershell
curl http://localhost:11441/v1/models -v
```

### Connectivity failing ❌
Check ports and firewall:
```powershell
Test-NetConnection -ComputerName localhost -Port 11441
Test-NetConnection -ComputerName localhost -Port 3000
```

## 🔧 Next Steps After Testing

Once you confirm tests pass via this harness, you can:

1. **Fix the Monaco Editor**: Check `renderer.js` for loading errors
2. **Fix the Chat Input**: Verify DOM element exists in `index.html`
3. **Fix the Model Selector**: Ensure elements in `model-browser.js` are visible
4. **Improve UI/UX**: With API confirmed working, focus on UI layer

## 📞 Support Commands

```powershell
# View all test results
Get-ChildItem -Filter "IDE-Test-Results*.json" | Sort-Object -Descending | Select-Object -First 1 | Get-Content | ConvertFrom-Json

# Run specific test feature only
.\IDE-Testing-Harness.ps1 -TargetFeature "Chat-Interface"

# Export results to CSV
$results | Export-Csv "test-results.csv" -NoTypeInformation

# Compare with previous run
Compare-Object (Get-Content "previous-results.json" | ConvertFrom-Json) (Get-Content "latest-results.json" | ConvertFrom-Json)
```

## 📦 Files Included

| File | Purpose | Usage |
|------|---------|-------|
| `IDE-Testing-Harness.ps1` | Main PowerShell comprehensive test suite | `.\IDE-Testing-Harness.ps1 -DeepTest` |
| `IDE-Testing-Curl.ps1` | Lightweight PowerShell curl wrapper | `.\IDE-Testing-Curl.ps1 -Full` |
| `IDE-Testing-Harness.sh` | POSIX-compliant bash script | `./IDE-Testing-Harness.sh full` |
| `IDE-TESTING-GUIDE.md` | This documentation | Reference |

---

**Last Updated**: December 28, 2025  
**IDE Version**: 2.0.0  
**Test Harness Version**: 1.0  
**Compatible With**: BigDaddyG IDE 2.0.0+
