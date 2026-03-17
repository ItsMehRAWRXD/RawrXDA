# Hot Patching System - Build Integration Guide

## Overview

This guide walks through integrating the Agent Hot Patching System into your existing RawrXD-ModelLoader build. The system provides real-time hallucination detection and correction for your autonomous agent.

---

## Phase 1: Add Hot Patching Source Files to Build

### Step 1.1: Copy Source Files

Copy these 6 files to `src/agent/`:

```
src/agent/
├── agent_hot_patcher.hpp              (150 lines)
├── agent_hot_patcher.cpp              (420 lines)
├── gguf_proxy_server.hpp              (110 lines)
├── gguf_proxy_server.cpp              (320 lines)
├── ide_agent_bridge_hot_patching_integration.hpp  (250 lines)
└── ide_agent_bridge_hot_patching_integration.cpp  (270 lines)
```

**Status**: ✅ All files are on E:/ drive - copy to src/agent/

### Step 1.2: Verify Files in Source Tree

After copying:
```bash
ls src/agent/agent_hot_patcher.*
ls src/agent/gguf_proxy_server.*
ls src/agent/ide_agent_bridge_hot_patching_integration.*
```

---

## Phase 2: Update CMakeLists.txt

### Step 2.1: Add Hot Patching Source Variables

Find the section where source files are defined (around line 250-300 in CMakeLists.txt) and add:

```cmake
# ============================================================
# Agent Hot Patching System Sources
# ============================================================
set(AGENT_HOT_PATCHING_SOURCES
    src/agent/agent_hot_patcher.cpp
    src/agent/gguf_proxy_server.cpp
    src/agent/ide_agent_bridge_hot_patching_integration.cpp
)

set(AGENT_HOT_PATCHING_HEADERS
    src/agent/agent_hot_patcher.hpp
    src/agent/gguf_proxy_server.hpp
    src/agent/ide_agent_bridge_hot_patching_integration.hpp
)
```

### Step 2.2: Add to RawrXD-QtShell Target

Find the `add_executable(RawrXD-QtShell ...)` call and add the hot patching sources:

**Before:**
```cmake
add_executable(RawrXD-QtShell
    src/main.cpp
    src/ide_main.cpp
    # ... other sources ...
)
```

**After:**
```cmake
add_executable(RawrXD-QtShell
    src/main.cpp
    src/ide_main.cpp
    # ... other sources ...
    ${AGENT_HOT_PATCHING_SOURCES}
)
```

### Step 2.3: Add Qt6::Network to Link Libraries

Find the `target_link_libraries(RawrXD-QtShell ...)` section and add `Qt6::Network`:

**Before:**
```cmake
target_link_libraries(RawrXD-QtShell PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
```

**After:**
```cmake
target_link_libraries(RawrXD-QtShell PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network          # ← ADD THIS for TCP proxy
    Qt6::Sql              # ← ADD THIS for SQLite patterns
)
```

### Step 2.4: Update Include Directories

Verify that the agent directory is included. Find `target_include_directories(RawrXD-QtShell ...)` and ensure `src/agent` is present:

```cmake
target_include_directories(RawrXD-QtShell PRIVATE 
    include 
    ${CMAKE_SOURCE_DIR}/src 
    ${CMAKE_SOURCE_DIR}/src/qtapp 
    ${CMAKE_SOURCE_DIR}/src/llm_adapter 
    ${CMAKE_SOURCE_DIR}/src/agent        # Already included
)
```

### Step 2.5: Complete CMakeLists.txt Section Example

Here's what the relevant section should look like:

```cmake
# ============================================================
# Agent Hot Patching System Sources
# ============================================================
set(AGENT_HOT_PATCHING_SOURCES
    src/agent/agent_hot_patcher.cpp
    src/agent/gguf_proxy_server.cpp
    src/agent/ide_agent_bridge_hot_patching_integration.cpp
)

set(AGENT_HOT_PATCHING_HEADERS
    src/agent/agent_hot_patcher.hpp
    src/agent/gguf_proxy_server.hpp
    src/agent/ide_agent_bridge_hot_patching_integration.hpp
)

# ============================================================
# Main Qt Shell Application
# ============================================================
if(Qt6_FOUND)
    message(STATUS "Building RawrXD-QtShell with Qt6")
    
    add_executable(RawrXD-QtShell
        src/main.cpp
        src/ide_main.cpp
        src/ide_window.cpp
        src/gui.cpp
        src/syntax_engine.cpp
        src/settings.cpp
        # Agent components
        src/agent/ide_agent_bridge.cpp
        src/agent/model_invoker.cpp
        src/agent/action_executor.cpp
        # Hot patching components
        ${AGENT_HOT_PATCHING_SOURCES}
    )
    
    target_link_libraries(RawrXD-QtShell PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network          # TCP proxy
        Qt6::Sql              # SQLite patterns
        Qt6::Concurrent       # Async operations
    )
    
    target_include_directories(RawrXD-QtShell PRIVATE 
        include 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/src/qtapp 
        ${CMAKE_SOURCE_DIR}/src/llm_adapter 
        ${CMAKE_SOURCE_DIR}/src/agent
    )
    
    # ... rest of target configuration ...
endif()
```

---

## Phase 3: Update IDE Initialization Code

### Step 3.1: Update ide_main.cpp

Find the IDE initialization code in `src/ide_main.cpp` and wire hot patching:

**Add include at the top:**
```cpp
#include "src/agent/ide_agent_bridge_hot_patching_integration.hpp"
```

**Find the agent bridge initialization:**
```cpp
// OLD CODE:
auto bridge = std::make_unique<IDEAgentBridge>();
bridge->initialize();

// NEW CODE:
auto bridge = std::make_unique<IDEAgentBridgeWithHotPatching>();
bridge->initializeWithHotPatching();
if (!bridge->startHotPatchingProxy()) {
    qWarning() << "Hot patching proxy failed to start";
}
```

### Step 3.2: Complete Example ide_main.cpp Integration

```cpp
// At the top of file with other includes
#include "src/agent/ide_agent_bridge.hpp"
#include "src/agent/ide_agent_bridge_hot_patching_integration.hpp"

// In your IDE initialization function
void IDEMainWindow::initializeAgent() {
    qDebug() << "[IDE] Initializing agent with hot patching...";
    
    try {
        // Create extended bridge with hot patching
        m_agentBridge = std::make_unique<IDEAgentBridgeWithHotPatching>();
        
        // Initialize including hot patching system
        auto hotPatchBridge = dynamic_cast<IDEAgentBridgeWithHotPatching*>(
            m_agentBridge.get());
        
        if (!hotPatchBridge) {
            qCritical() << "[IDE] Failed to cast to hot patching bridge";
            return;
        }
        
        // Initialize with hot patching
        hotPatchBridge->initializeWithHotPatching();
        qDebug() << "[IDE] Hot patching initialization complete";
        
        // Start proxy server
        if (!hotPatchBridge->startHotPatchingProxy()) {
            qWarning() << "[IDE] Hot patching proxy failed to start";
            // Could fall back to non-patching mode
        } else {
            qDebug() << "[IDE] Hot patching proxy started successfully";
        }
        
        // Enable monitoring (optional)
        setupHotPatchingMonitoring(hotPatchBridge);
        
    } catch (const std::exception& ex) {
        qCritical() << "[IDE] Agent initialization failed:" << ex.what();
    }
}

void IDEMainWindow::setupHotPatchingMonitoring(
    IDEAgentBridgeWithHotPatching* bridge) {
    
    // Periodic statistics monitoring (every 10 seconds)
    QTimer* statsTimer = new QTimer(this);
    connect(statsTimer, &QTimer::timeout, [bridge]() {
        QJsonObject stats = bridge->getHotPatchingStatistics();
        
        int hallucinations = stats["totalHallucinationsDetected"].toInt();
        int corrected = stats["hallucinationsCorrected"].toInt();
        
        qDebug() << "[Hot Patcher] Detections:" << hallucinations
                 << "Corrections:" << corrected;
    });
    
    statsTimer->start(10000);
}
```

### Step 3.3: Update IDE Window Shutdown

In your IDE window destructor or close event, stop the proxy gracefully:

```cpp
IDEMainWindow::~IDEMainWindow() {
    if (m_agentBridge) {
        auto hotPatchBridge = dynamic_cast<IDEAgentBridgeWithHotPatching*>(
            m_agentBridge.get());
        
        if (hotPatchBridge) {
            // Get final statistics before shutdown
            QJsonObject stats = hotPatchBridge->getHotPatchingStatistics();
            qDebug() << "[IDE] Final hot patching stats:" << stats;
            
            // Stop proxy
            hotPatchBridge->stopHotPatchingProxy();
            qDebug() << "[IDE] Hot patching proxy stopped";
        }
    }
}
```

---

## Phase 4: Build System Configuration

### Step 4.1: CMake Build Steps

```bash
# Navigate to project root
cd RawrXD-ModelLoader

# Create/update build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2022_64"

# Build project
cmake --build . --config Release --parallel 8
```

### Step 4.2: Verify Build Success

After building, check for these in build output:

```
✓ [100%] Linking CXX executable bin/RawrXD-QtShell.exe
✓ All Qt6::Network symbols resolved
✓ All hot patching symbols linked
```

### Step 4.3: Troubleshooting Build Errors

**Error: "LNK2019: unresolved external symbol"**

Solution: Verify all source files added:
```cmake
# Check in CMakeLists.txt
set(AGENT_HOT_PATCHING_SOURCES
    src/agent/agent_hot_patcher.cpp
    src/agent/gguf_proxy_server.cpp
    src/agent/ide_agent_bridge_hot_patching_integration.cpp
)
```

**Error: "Qt6::Network not found"**

Solution: Verify Qt6 installation has Network module:
```bash
# List available Qt6 modules
find C:/Qt -name "Qt6Network*"

# Should show Network module files
```

**Error: "Cannot open include file: 'QtNetwork/QTcpServer'"**

Solution: Verify `target_include_directories` includes agent directory

---

## Phase 5: Runtime Integration

### Step 5.1: Verify Proxy Startup

When IDE starts, verify console output:

```
[IDEAgentBridge] Creating extended bridge with hot patching
[IDEAgentBridge] Initializing with hot patching system
[IDEAgentBridge] AgentHotPatcher created
[IDEAgentBridge] AgentHotPatcher initialized
[IDEAgentBridge] GGUFProxyServer created
[IDEAgentBridge] ✓ Proxy server started on port 11435
[IDEAgentBridge] ModelInvoker endpoint redirected to proxy
```

### Step 5.2: Verify Proxy is Listening

Open PowerShell and check:

```powershell
# Check if proxy listening on 11435
Get-NetTCPConnection -LocalPort 11435 -State Listen -ErrorAction SilentlyContinue

# Check if GGUF listening on 11434
Get-NetTCPConnection -LocalPort 11434 -State Listen -ErrorAction SilentlyContinue
```

Expected output:
```
LocalAddress  LocalPort RemoteAddress RemotePort State
127.0.0.1     11435     0.0.0.0       0          Listen
127.0.0.1     11434     0.0.0.0       0          Listen
```

### Step 5.3: Test Agent with Hot Patching

Send a test request through the proxy:

```powershell
$uri = "http://localhost:11435/api/generate"
$body = @{
    model = "mistral"
    prompt = "test prompt"
    stream = $false
} | ConvertTo-Json

Invoke-WebRequest -Uri $uri -Method Post -Body $body -ContentType "application/json"
```

---

## Phase 6: Verification Checklist

### Build Verification
- [ ] CMakeLists.txt updated with hot patching sources
- [ ] Qt6::Network added to link libraries
- [ ] Build completes without errors
- [ ] All symbols resolved
- [ ] Executable created in bin/

### Runtime Verification
- [ ] IDE starts without crashing
- [ ] Console shows hot patching initialization
- [ ] Proxy listens on localhost:11435
- [ ] GGUF backend on localhost:11434
- [ ] ModelInvoker endpoint is 11435

### Functional Verification
- [ ] Agent can generate plans
- [ ] Model outputs are intercepted
- [ ] Hallucinations detected
- [ ] Corrections applied
- [ ] Statistics collection working

---

## Phase 7: Configuration & Tuning

### Step 7.1: Create Configuration File

Create `config/hot_patching_config.json`:

```json
{
  "proxyPort": 11435,
  "ggufEndpoint": "localhost:11434",
  "hotPatchingEnabled": true,
  "connectionPoolSize": 10,
  "connectionTimeout": 30000,
  "debugLogging": false,
  "statisticsInterval": 10000,
  "correctionPatterns": "data/correction_patterns.db",
  "behaviorPatches": "data/behavior_patches.db"
}
```

### Step 7.2: Load Configuration in IDE

```cpp
void IDEMainWindow::loadHotPatchingConfiguration() {
    QFile configFile("config/hot_patching_config.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Hot patching config not found, using defaults";
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    QJsonObject config = doc.object();
    
    auto hotPatchBridge = dynamic_cast<IDEAgentBridgeWithHotPatching*>(
        m_agentBridge.get());
    
    if (hotPatchBridge) {
        bool enabled = config["hotPatchingEnabled"].toBool(true);
        hotPatchBridge->setHotPatchingEnabled(enabled);
        
        qDebug() << "[Hot Patcher] Loaded config:"
                 << "Enabled:" << enabled;
    }
    
    configFile.close();
}
```

### Step 7.3: Enable Debug Logging

For development:

```cpp
// In IDE initialization
auto hotPatchBridge = dynamic_cast<IDEAgentBridgeWithHotPatching*>(
    m_agentBridge.get());

#ifdef DEBUG
    if (hotPatchBridge) {
        hotPatchBridge->getHotPatcher()->setDebugLogging(true);
        qDebug() << "[Hot Patcher] Debug logging enabled";
    }
#endif
```

---

## Phase 8: Testing & Validation

### Step 8.1: Build Integration Test

Create `tests/test_hot_patching_integration.cpp`:

```cpp
#include <gtest/gtest.h>
#include "src/agent/ide_agent_bridge_hot_patching_integration.hpp"

class HotPatchingIntegrationTest : public ::testing::Test {
protected:
    IDEAgentBridgeWithHotPatching bridge;
    
    void SetUp() override {
        bridge.initializeWithHotPatching();
    }
};

TEST_F(HotPatchingIntegrationTest, ProxyStarts) {
    EXPECT_TRUE(bridge.startHotPatchingProxy());
    EXPECT_TRUE(bridge.isHotPatchingActive());
}

TEST_F(HotPatchingIntegrationTest, StatisticsAvailable) {
    EXPECT_TRUE(bridge.startHotPatchingProxy());
    
    QJsonObject stats = bridge.getHotPatchingStatistics();
    EXPECT_TRUE(stats.contains("proxyServerRunning"));
    EXPECT_TRUE(stats["proxyServerRunning"].toBool());
}
```

### Step 8.2: Manual Test Procedure

1. Start IDE
2. Wait for hot patching initialization (2-3 seconds)
3. Verify proxy listening: `netstat -an | grep 11435`
4. Send agent task
5. Monitor console for corrections
6. Check statistics

### Step 8.3: Performance Test

```cpp
QElapsedTimer timer;
timer.start();

// Run agent task
m_agentBridge->executeWithApproval(plan);

qDebug() << "Total execution time:" << timer.elapsed() << "ms";

// Get statistics
auto hotPatchBridge = dynamic_cast<IDEAgentBridgeWithHotPatching*>(
    m_agentBridge.get());
QJsonObject stats = hotPatchBridge->getHotPatchingStatistics();
qDebug() << "Corrections applied:" << stats["hallucinationsCorrected"].toInt();
```

---

## Phase 9: Deployment

### Step 9.1: Build for Release

```bash
cd build
cmake --build . --config Release --parallel 8
```

### Step 9.2: Package Application

Include in deployment:
- `RawrXD-QtShell.exe`
- Hot patching DLLs (Qt6Network.dll, Qt6Sql.dll)
- `config/hot_patching_config.json`
- `logs/` directory

### Step 9.3: User Installation

Users should:
1. Install application
2. Copy hot patching config to config/
3. Start application
4. Verify proxy starts in console

---

## Troubleshooting

### Proxy Not Starting

**Symptom**: Console shows "Failed to start proxy"

**Solutions**:
1. Check port 11435 not in use: `netstat -an | grep 11435`
2. Check firewall allows localhost:11435
3. Check GGUF running on 11434
4. Enable debug logging for more details

### Hallucinations Not Corrected

**Symptom**: Model outputs unchanged

**Solutions**:
1. Verify ModelInvoker endpoint is 11435: `qDebug() << bridge->getModelInvoker()->getEndpoint();`
2. Enable debug logging: `hotPatcher->setDebugLogging(true);`
3. Check correction patterns loaded: `hotPatcher->getCorrectionPatternCount();`
4. Verify proxy is intercepting: monitor network traffic

### Performance Issues

**Symptom**: High latency, slow responses

**Solutions**:
1. Increase connection pool: `proxyServer->setConnectionPoolSize(20);`
2. Profile correction processing time
3. Disable debug logging in production
4. Check system resources (CPU, memory, disk I/O)

---

## Summary

You now have the Agent Hot Patching System fully integrated into your build:

✅ **Build**: CMakeLists.txt updated with hot patching components
✅ **Initialization**: IDE starts proxy on application launch
✅ **Integration**: Transparent to existing agent code
✅ **Monitoring**: Statistics and logging available
✅ **Testing**: Verification procedures documented
✅ **Deployment**: Ready for production

**Next Steps**:
1. Copy source files to src/agent/
2. Update CMakeLists.txt
3. Update ide_main.cpp initialization
4. Build project
5. Test and verify
6. Deploy to production

**Documentation Files Available**:
- QUICK_REFERENCE.md - Quick commands
- COMPLETE_INTEGRATION_GUIDE.md - Detailed walkthrough
- AGENT_HOT_PATCHING_SYSTEM.md - Architecture deep dive
- HOT_PATCHING_DEPLOYMENT_GUIDE.md - Production operations
