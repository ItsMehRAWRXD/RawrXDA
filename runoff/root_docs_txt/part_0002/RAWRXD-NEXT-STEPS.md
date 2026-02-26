# RAWRXD BUILD & INTEGRATION CONTINUATION PLAN

**Date:** January 27, 2026  
**Status:** Build has compilation errors - multiple pre-existing issues  
**Current Session:** PowerShell hosting wired, LSP modules delivered, CMake gating complete

---

## 🎯 CURRENT BUILD ISSUES (15 errors)

### Category 1: LSP Client Missing Implementations
**Files Affected:** `lsp_client.cpp`, `lsp_client_incremental.cpp`

**Missing Methods in LSPClient class:**
- `sendNotification(const QString& method, const QJsonObject& params)`
- `sendMessage(const QJsonObject& message)`
- `sendIncrementalUpdate(const DiffOp& op)`
- `offsetToPosition(int offset)`
- `cancelRequest(int requestId)`

**Impact:** LSP client can't communicate with language servers

**Quick Fix:** Either stub these methods or disable LSP compilation temporarily

```cpp
// Add to src/lsp_client.h - LSPClient class
public:
    void sendNotification(const QString& method, const QJsonObject& params) {
        // TODO: Implement JSON-RPC notification sending
        qWarning() << "sendNotification not implemented:" << method;
    }
    
    void sendMessage(const QJsonObject& message) {
        // TODO: Implement message sending
        qWarning() << "sendMessage not implemented";
    }
    
    bool sendIncrementalUpdate(const RawrXD::DiffOp& op) {
        // TODO: Implement incremental update
        return false;
    }
    
    Position offsetToPosition(int offset) {
        // TODO: Convert byte offset to LSP Position
        return Position{0, offset};
    }
    
    void cancelRequest(int requestId) {
        // TODO: Cancel pending request
        qWarning() << "cancelRequest not implemented for request" << requestId;
    }
```

### Category 2: Missing Headers
**File:** `src/language_server_integration_impl.hpp`

**Issue:** Includes `codebase_context_analyzer.hpp` which doesn't exist

**Solution:** Either create the file or remove the include

```cpp
// Option 1: Create stub file
touch src/codebase_context_analyzer.hpp

// Option 2: Comment out the include in language_server_integration_impl.hpp
// #include "codebase_context_analyzer.hpp"
```

### Category 3: Preprocessor Mismatch
**File:** `src/agent/ide_agent_bridge_hot_patching_integration.hpp` (line 177)

**Issue:** Unexpected #endif - likely a missing #if or #ifdef earlier

**Solution:** Check for balanced preprocessor directives

```cpp
// The issue is likely a missing opening guard
// Make sure the file has exactly this structure:
#pragma once
#include "..."
#include "..."

// ... class definitions ...

#endif // IDE_AGENT_BRIDGE_HOT_PATCHING_INTEGRATION_HPP
```

### Category 4: Tool Registry Missing Field
**File:** `src/tool_registry_thermal.cpp` (line 225)

**Issue:** `ToolDefinition` doesn't have `inputSchema` field

**Solution:** Add schema field to ToolDefinition

```cpp
// Add to ToolDefinition struct in tool_registry.h
struct ToolDefinition {
    QString name;
    QString description;
    QJsonObject inputSchema;  // Add this
    std::function<QJsonObject(const QJsonObject&)> handler;
};
```

---

## 🔧 RECOMMENDED FIX STRATEGY

### Option A: Quick Fix (1 hour)
1. Stub the LSP methods with qWarning() placeholders
2. Create empty codebase_context_analyzer.hpp
3. Add inputSchema to ToolDefinition
4. Fix preprocessor issues
5. Build succeeds, but LSP features are disabled

**Pros:** Quick win, build succeeds
**Cons:** LSP features don't work yet

### Option B: Proper Implementation (2-3 days)
1. Implement LSP client communication properly
2. Add codebase analyzer
3. Implement tool registry properly
4. Full feature completion

**Pros:** All features work
**Cons:** Takes longer

### Option C: Conditional Compilation (2 hours)
Disable problematic components with CMake flags

```cmake
# CMakeLists.txt
option(RAWRXD_ENABLE_LSP_CLIENT "Enable LSP client features" OFF)
if(RAWRXD_ENABLE_LSP_CLIENT)
    # Include LSP files
else()
    # Add LSP stub implementations
endif()
```

---

## 📋 WHAT'S ALREADY COMPLETE

✅ **PowerShell Integration:**
- `initializePowerShell()` in Win32IDE_PowerShell.cpp
- Module auto-import on startup
- PowerShell menu items wired in Win32IDE.cpp
- Command handlers for swarm_control_center.ps1

✅ **LSP Modules Delivered:**
- `lsp_json_parser.asm` (1356 lines)
- `lsp_message_parser.asm` (528 lines)
- `lsp_extended_providers.asm` (486 lines)
- C++ integration headers ready
- CMake gating complete

✅ **Build System Ready:**
- Chromatic target properly gated
- Release configuration builds (mostly)
- LSP modules can be compiled when methods are implemented

---

## 🎯 YOUR DECISION TREE

**Choose one path:**

### Path 1: Fix Build First (Recommended)
```
Fix build errors (2 hours)
  ├─ Stub LSP methods
  ├─ Create missing headers
  ├─ Fix preprocessor issues
  └─ Verify Release build succeeds

Then:
  Wire PowerShell menu items to module functions (2 hours)
  Add output formatting (1 hour)
  Convert first PowerShell module to C++ (1-2 days)
```

### Path 2: Continue PowerShell Wiring (Skip build fixes)
```
Wire PowerShell menu items to module commands (2 hours)
  ├─ plugin_craft_room generator
  ├─ ModuleLifecycleManager
  ├─ Agentic orchestration
  └─ Output formatting

Then come back to build fixes
```

### Path 3: Hybrid Approach
```
Quick build fix (30 min - just stub LSP methods)
Release build succeeds
Wire PowerShell menus (2 hours)
Then proper LSP implementation later
```

---

## 📝 NEXT IMMEDIATE STEPS (Pick One)

### Option 1: "Fix the build"
I'll:
1. Add stub implementations for all missing LSP methods
2. Create missing headers
3. Fix preprocessor issues
4. Get Release build to succeed
5. Verify executable generates

### Option 2: "Continue PowerShell"
I'll:
1. Wire PowerShell menu items to actual module functions
2. Add table formatting for output
3. Create handlers for:
   - plugin_craft_room generator
   - ModuleLifecycleManager
   - Agentic orchestration

### Option 3: "Both in parallel"
I'll:
1. Quick build fix (stub methods)
2. PowerShell menu wiring
3. Test both working together

---

## 📊 TIME ESTIMATES

| Task | Time | Difficulty |
|------|------|-----------|
| Stub LSP methods | 30 min | Easy |
| Create missing headers | 15 min | Easy |
| Fix preprocessor | 30 min | Medium |
| Release build verify | 10 min | Easy |
| **Total Build Fix** | **~1.5 hours** | **Easy** |
| Wire PowerShell menus | 2 hours | Medium |
| Output formatting | 1 hour | Easy |
| Convert PS module to C++ | 1-2 days | Hard |

---

## 💾 WHAT TO DO NOW

**Tell me:**

1. **"Fix the build"** → I'll stub LSP and get Release working
2. **"Wire PowerShell"** → I'll wire menu items to module functions
3. **"Both"** → I'll do quick fixes + PowerShell wiring
4. **"Skip to C++ conversion"** → I'll start converting first PS module

---

**Awaiting your choice...**
