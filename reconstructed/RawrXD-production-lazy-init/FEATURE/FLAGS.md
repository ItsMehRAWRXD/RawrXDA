# RawrXD IDE - Feature Flags Configuration

**Purpose:** Isolate experimental features from production-stable APIs to preserve the "zero errors" invariant while enabling rapid innovation.

---

## 🎯 Philosophy

**Core Principle:** *Freeze core APIs, shift experimentation behind runtime toggles.*

After achieving production-ready status (v1.0.0), all **new features** and **breaking changes** must:

1. Be **opt-in via feature flags** (default: disabled)
2. Not affect **core API stability** (no signature changes to frozen interfaces)
3. Be **independently testable** (isolated test suites)
4. Include **rollback paths** (graceful degradation if disabled)

---

## 🔒 Frozen Core APIs (v1.0.0+)

These interfaces are **stable** and will not have breaking changes until v2.0.0:

| API | File | Status | Notes |
|-----|------|--------|-------|
| `IDirectoryManager` | `include/RawrXD/interfaces.h` | 🔒 Frozen | Directory operations interface |
| `GitignoreFilter` | `src/qtapp/widgets/project_explorer.h` | 🔒 Frozen | Pattern matching API (8 bugs fixed) |
| `ModelRouterInterface` | `src/model_router_adapter.h` | 🔒 Frozen | Model routing protocol |
| `AgentCoordinator` | `src/agentic/agentic_agent_coordinator.h` | 🔒 Frozen | Agentic execution API |
| `UniversalModelRouter` | `src/universal_model_router.h` | 🔒 Frozen | Load balancing endpoints |
| `LSPClient` | `include/lsp_client.h` | 🔒 Frozen | Language server protocol |

**Guarantee:** No signature changes, no behavior changes (except bug fixes).

---

## 🚩 Feature Flag System

### Implementation

Feature flags are controlled via `src/feature_toggle.cpp`:

```cpp
// Example: Enable experimental hotpatch system
class FeatureFlags {
public:
    static bool isHotpatchEnabled() {
        return getEnvBool("RAWRXD_ENABLE_HOTPATCH", false);
    }
    
    static bool isAdvancedProfilingEnabled() {
        return getEnvBool("RAWRXD_ENABLE_ADVANCED_PROFILING", false);
    }
    
    // Add new flags here...
};
```

### Configuration

Set via **environment variables** or **config file** (`config/features.json`):

```json
{
  "experimental": {
    "hotpatch_system": false,
    "advanced_profiling": false,
    "remote_development": false,
    "collaborative_editing": false,
    "mobile_companion": false
  },
  "beta": {
    "plugin_marketplace": false,
    "time_travel_debugging": false
  }
}
```

---

## 📊 Current Feature Flags

### 🧪 Experimental (High Risk, Opt-In Only)

| Feature | Flag | Default | Stability | Notes |
|---------|------|---------|-----------|-------|
| **MASM Hotpatch** | `RAWRXD_ENABLE_HOTPATCH` | `OFF` | ⚠️ Experimental | Zero-downtime model updates |
| **Advanced Profiling** | `RAWRXD_ENABLE_ADVANCED_PROFILING` | `OFF` | ⚠️ Experimental | Flamegraph + CPU/memory deep dive |
| **Cloud Hybrid Mode** | `RAWRXD_CLOUD_HYBRID` | `OFF` | ⚠️ Experimental | Offload inference to cloud |
| **Autonomous Learning** | `RAWRXD_AGENT_LEARNING` | `OFF` | ⚠️ Experimental | Agents learn from user corrections |
| **Distributed Training** | `RAWRXD_DISTRIBUTED_TRAIN` | `OFF` | ⚠️ Experimental | Multi-node model training |

### 🔬 Beta (Moderate Risk, Early Access)

| Feature | Flag | Default | Stability | Notes |
|---------|------|---------|-----------|-------|
| **Plugin Marketplace** | `RAWRXD_PLUGIN_MARKETPLACE` | `OFF` | 🔬 Beta | Third-party extensions |
| **Time-Travel Debugging** | `RAWRXD_TIME_TRAVEL_DEBUG` | `OFF` | 🔬 Beta | Checkpoint-based replay |
| **Test Generation** | `RAWRXD_TEST_GENERATION` | `OFF` | 🔬 Beta | Auto-generate unit tests |
| **Code Stream Widget** | `RAWRXD_CODE_STREAM` | `OFF` | 🔬 Beta | Live collaboration |

### ✅ Stable (Low Risk, Production-Ready)

| Feature | Flag | Default | Stability | Notes |
|---------|------|---------|-----------|-------|
| **GGUF Streaming** | `RAWRXD_GGUF_STREAMING` | `ON` | ✅ Stable | Memory-mapped model loading |
| **Vulkan Compute** | `RAWRXD_VULKAN_COMPUTE` | `ON` | ✅ Stable | GPU inference backend |
| **Agent Coordinator** | `RAWRXD_AGENT_COORDINATOR` | `ON` | ✅ Stable | Agentic execution engine |
| **Model Router** | `RAWRXD_MODEL_ROUTER` | `ON` | ✅ Stable | Universal routing |
| **LSP Integration** | `RAWRXD_LSP_CLIENT` | `ON` | ✅ Stable | Language servers |

---

## 🎛️ Usage Examples

### Enable Experimental Features

#### Via Environment Variables

```powershell
# Windows PowerShell
$env:RAWRXD_ENABLE_HOTPATCH = "1"
$env:RAWRXD_AGENT_LEARNING = "1"
.\RawrXD-AgenticIDE.exe
```

```bash
# Linux/macOS
export RAWRXD_ENABLE_HOTPATCH=1
export RAWRXD_AGENT_LEARNING=1
./RawrXD-AgenticIDE
```

#### Via Config File

Edit `config/features.json`:

```json
{
  "experimental": {
    "hotpatch_system": true,
    "autonomous_learning": true
  }
}
```

Then restart the IDE.

### Check Flag Status at Runtime

```cpp
#include "feature_toggle.h"

void someFunction() {
    if (FeatureFlags::isHotpatchEnabled()) {
        // Use experimental hotpatch system
        qDebug() << "Hotpatch enabled";
    } else {
        // Use stable fallback
        qDebug() << "Using stable model loader";
    }
}
```

---

## 🧪 Testing Isolated Features

### Run Tests for Specific Flags

```powershell
# Test with hotpatch enabled
$env:RAWRXD_ENABLE_HOTPATCH = "1"
ctest -R hotpatch_tests --output-on-failure

# Test with all flags disabled (production baseline)
$env:RAWRXD_ENABLE_HOTPATCH = "0"
$env:RAWRXD_AGENT_LEARNING = "0"
ctest --output-on-failure
```

### Add Feature-Gated Tests

```cpp
TEST(HotpatchTest, ApplyPatch) {
    if (!FeatureFlags::isHotpatchEnabled()) {
        GTEST_SKIP() << "Hotpatch flag disabled";
    }
    
    // Test hotpatch logic here
    EXPECT_TRUE(applyHotpatch("model.gguf"));
}
```

---

## 📜 Feature Lifecycle

### Experimental → Beta → Stable

1. **Experimental (⚠️)**
   - Default: `OFF`
   - Testing: Internal dogfooding
   - Duration: 1-3 months
   - Exit Criteria: No crashes, basic functionality works

2. **Beta (🔬)**
   - Default: `OFF` (opt-in for early adopters)
   - Testing: Public beta testers
   - Duration: 3-6 months
   - Exit Criteria: User feedback positive, no critical bugs

3. **Stable (✅)**
   - Default: `ON`
   - Testing: Full CI/CD validation
   - Duration: Indefinite (until deprecated)
   - Guarantee: No breaking changes until next major version

### Deprecation Process

```cpp
// Mark as deprecated
[[deprecated("Use newFunction() instead")]]
bool oldFunction() { /* ... */ }

// Provide migration path for 2 versions (v1.1, v1.2)
// Remove in v2.0.0
```

---

## 🚨 Breaking Change Policy

**Major Version Bumps Only:**  
Breaking changes to **frozen APIs** require:

1. **Major version increment** (v1.x → v2.0)
2. **Migration guide** with examples
3. **Deprecation warnings** for 2 minor versions before removal
4. **Automated migration tool** (if feasible)

**Minor Version Bumps:**  
Can add **new optional parameters** with defaults:

```cpp
// v1.0.0
void loadModel(const QString &path);

// v1.1.0 - OK (backward compatible)
void loadModel(const QString &path, ModelOptions opts = {});
```

---

## 🔍 Monitoring Flag Usage

### Telemetry (Opt-In)

If user enables telemetry:

```cpp
if (TelemetryEnabled()) {
    logFeatureFlagUsage("hotpatch_enabled", true);
}
```

Metrics:
- % of users with each flag enabled
- Crash rates per feature flag
- Performance impact per feature

---

## 📝 Adding a New Feature Flag

### Step-by-Step Guide

1. **Add to `FeatureFlags` class:**

   ```cpp
   // src/feature_toggle.cpp
   bool FeatureFlags::isMyNewFeatureEnabled() {
       return getEnvBool("RAWRXD_MY_NEW_FEATURE", false);
   }
   ```

2. **Document in this file:**

   Add to **Experimental** section with:
   - Feature name
   - Flag variable
   - Default value (`OFF`)
   - Stability level (⚠️ Experimental)
   - Notes

3. **Gate code:**

   ```cpp
   if (FeatureFlags::isMyNewFeatureEnabled()) {
       // New code path
   } else {
       // Stable fallback
   }
   ```

4. **Add tests:**

   ```cpp
   TEST(MyFeatureTest, WithFlagEnabled) {
       if (!FeatureFlags::isMyNewFeatureEnabled()) {
           GTEST_SKIP();
       }
       // Test new feature
   }
   ```

5. **Update CI:**

   Add to `.github/workflows/compile-check.yml` if needed.

---

## 🎯 Goals

1. **Preserve "Zero Errors" Invariant** - New features can't break production builds
2. **Enable Rapid Innovation** - Experiment without destabilizing core
3. **User Control** - Power users can opt into bleeding edge
4. **Safe Rollouts** - Gradual feature adoption reduces risk
5. **Clear Communication** - Users know what's stable vs. experimental

---

**Last Updated:** January 22, 2026  
**Version:** 1.0.0  
**Status:** Active
