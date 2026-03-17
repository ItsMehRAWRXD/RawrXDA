# Phase Implementation Quickstart Guide

## Quick Navigation
- **Phase 1 Status:** ✅ COMPLETE
- **Phase 2 Status:** ✅ COMPLETE  
- **Phase 3 Status:** ⚠️ IN PROGRESS (15 unwired features remain)

---

## Phase 1: Foundation & Feature Registry

### What's Already Done ✅
- [x] EnterpriseLicenseV2 singleton initialized
- [x] Feature manifest (g_FeatureManifest[61]) defined
- [x] Hardware ID computation on startup
- [x] Feature Registry Panel (Win32 native, all features visible)
- [x] Menu items created (Tools > License Creator, Feature Registry)
- [x] Feature status display (locked/unlocked, implemented/planned, wired/not)

### How to Verify Phase 1
```
1. Launch IDE
2. Tools > Feature Registry
3. Observe all 61 features listed with:
   - [OPEN] or [LOCK] status (based on current license)
   - Feature name, tier, implementation status
   - Wiring status (connected to UI or not)
   - Test coverage (all show "untested" currently)
```

### Phase 1 Code Reference
- **Initialization:** `src/enterprise_license.cpp` lines 195-240
- **Manifest:** `src/enterprise_license.cpp` lines 41-125
- **Panel UI:** `src/win32app/feature_registry_panel.cpp`
- **Menu routing:** `src/win32app/Win32IDE_Commands.cpp` (case 3015, 3016)

---

## Phase 2: License Creator with Real Signing

### What's Already Done ✅
- [x] License Creator GUI (Tools > License Creator)
- [x] Tier selection (Community, Professional, Enterprise, Sovereign)
- [x] Signing secret input (user-provided or env var RAWRXD_LICENSE_SECRET)
- [x] Days/expiry configuration
- [x] Output file path selection
- [x] HWID binding toggle
- [x] HMAC-SHA256 key generation (real, cryptographic)
- [x] .rawrlic file creation and validation

### How to Use Phase 2
```
1. Tools > License Creator
2. Select tier (try "Enterprise" for 800B unlock)
3. Enter signing secret (or use RAWRXD_LICENSE_SECRET env var)
4. Set expiry: enter "30" for 30-day license
5. Choose output path (defaults to %APPDATA%\RawrXD\license.rawrlic)
6. Check "Bind to this machine" if desired
7. Click "Create Enterprise License"
8. Receive signed, verified .rawrlic file
9. Click "Install License" to apply
10. Verify 800B now available in model loader
```

### Phase 2 Key Code
- **Dialog:** `src/win32app/Win32IDE_LicenseCreator.cpp` lines 1-804
- **Key creation:** `src/enterprise_license.cpp` CreateKey() method (~line 350)
- **Signing:** HMAC-SHA256 via Windows CryptoAPI
- **Validation:** `src/enterprise_license.cpp` ValidateKey() (~line 300)

### Phase 2 Verification
```powershell
# Create a test license
Tools > License Creator
  → Select "Enterprise"
  → Enter secret "test-secret"
  → Set days "30"
  → Click "Create Enterprise License"
  
# Install it
  → Click "Install License"
  
# Verify 800B is now available
  → Tools > Feature Registry
  → Search for "800B Dual-Engine"
  → Status should show [OPEN] (unlocked)
```

---

## Phase 3: Feature Wiring & Enforcement (TODO)

### What Needs to Be Done ⚠️

**15 Features Need UI Wiring (24.6% of system):**

#### Professional Tier (8 features)
| # | Feature | Current Status | Needs Wiring |
|---|---|---|---|
| 18 | Model Comparison | Implemented | Tools > Compare Models |
| 19 | Batch Processing | Implemented | Tools > Batch Processor |
| 20 | Custom Stop Sequences | Implemented | Settings > Advanced |
| 21 | Grammar-Constrained Gen | Implemented | Settings > Grammar |
| 22 | LoRA Adapter Support | Implemented | File > Load LoRA |
| 23 | Response Caching | Implemented | Tools > Caching |
| 24 | Prompt Library | Implemented | View > Prompt Library |
| 25 | Export/Import Sessions | Implemented | File > Export/Import |

#### Enterprise Tier (7 features)
| # | Feature | Current Status | Needs Wiring |
|---|---|---|---|
| 37 | Model Sharding | Implemented | Advanced > Sharding |
| 38 | Tensor Parallel | Implemented | Advanced > Tensor Parallel |
| 39 | Pipeline Parallel | Implemented | Advanced > Pipeline Parallel |
| 43 | Custom Quant Schemes | Implemented | Tools > Quantization |
| 44 | Multi-GPU Load Balance | Implemented | Advanced > GPU Balance |
| 45 | Dynamic Batch Sizing | Implemented | Advanced > Batch Size |
| 49 | API Key Management | Implemented | Tools > API Keys |

### Implementation Pattern for Phase 3

**Step 1: Add Menu ID** (Win32IDE.h)
```cpp
#define IDM_TOOLS_MODEL_COMPARE           3100  // Professional
#define IDM_TOOLS_BATCH_PROCESSOR         3101
#define IDM_SETTINGS_CUSTOM_STOP_SEQ      3102
#define IDM_SETTINGS_GRAMMAR              3103
#define IDM_FILE_LOAD_LORA                3104
#define IDM_TOOLS_RESPONSE_CACHE          3105
#define IDM_VIEW_PROMPT_LIBRARY           3106
#define IDM_FILE_EXPORT_SESSION           3107
#define IDM_ADVANCED_MODEL_SHARDING       3200  // Enterprise
#define IDM_ADVANCED_TENSOR_PARALLEL      3201
#define IDM_ADVANCED_PIPELINE_PARALLEL    3202
#define IDM_TOOLS_CUSTOM_QUANT            3203
#define IDM_ADVANCED_GPU_LOAD_BALANCE     3204
#define IDM_ADVANCED_BATCH_SIZING         3205
#define IDM_TOOLS_API_KEYS                3206
```

**Step 2: Add Menu Item** (Win32IDE.cpp)
```cpp
// In Tools menu construction
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_MODEL_COMPARE, L"Compare &Models");
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_BATCH_PROCESSOR, L"&Batch Processor");
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RESPONSE_CACHE, L"Response &Caching");
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_CUSTOM_QUANT, L"Custom &Quantization");
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_API_KEYS, L"API &Keys");
```

**Step 3: Add Handler** (Win32IDE_Commands.cpp)
```cpp
case IDM_TOOLS_MODEL_COMPARE: {
    using namespace RawrXD::License;
    if (!EnterpriseLicenseV2::Instance().gate(FeatureID::ModelComparison, "ModelCompare")) {
        MessageBoxW(m_hwnd, L"Model Comparison requires Professional license",
                   L"License Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Launch comparison dialog
    showModelComparisonDialog();
    break;
}

case IDM_TOOLS_BATCH_PROCESSOR: {
    using namespace RawrXD::License;
    if (!EnterpriseLicenseV2::Instance().gate(FeatureID::BatchProcessing, "BatchProcessor")) {
        MessageBoxW(m_hwnd, L"Batch Processing requires Professional license",
                   L"License Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    showBatchProcessorDialog();
    break;
}

// ... similar for other 13 features
```

**Step 4: Verify License Gate at Feature Entry** (Feature source files)
```cpp
// In src/engine/core_generator.cpp (Batch Processing)
bool BatchGenerator::process(...) {
    // Gate check
    if (!RawrXD::License::EnterpriseLicenseV2::Instance()
            .gate(FeatureID::BatchProcessing, "BatchGenerator::process")) {
        return false;  // License check failed
    }
    // ... rest of implementation
}

// Similar pattern for all 15 unwired features
```

### Estimated Effort
- Menu ID setup: 15 min
- Menu item adds: 15 min
- Command handlers: 2-3 hours (8-15 cases)
- Feature entry point gates: 3-4 hours (15 features × 10-15 min each)
- Testing & validation: 2-3 hours
- **Total: 8-10 hours**

### Testing Checklist for Phase 3
- [ ] Professional license: 8 new menu items enabled, others disabled
- [ ] Enterprise license: all 15 menu items enabled
- [ ] Community license: only Community features enabled
- [ ] License change: menu enable/disable updates in real-time
- [ ] Feature Registry Panel: reflects current menu item status
- [ ] Feature gates at entry points: blocked when unlicensed
- [ ] Error messages: clear "requires X license" dialogs

---

## Phase 4+ (Future)

### Planned but Not Implemented (19 features)
- Flash Attention → AVX-512 kernels needed
- Speculative Decoding → Draft model framework
- CUDA/HIP Backends → GPU initialization
- Sovereign tier (8 features) → Gov-grade security

### Test Harness (0% coverage currently)
```cpp
// Create src/tests/license_feature_validation.cpp
#include "enterprise_license.h"

void validateFeature(FeatureID id) {
    auto& lic = EnterpriseLicenseV2::Instance();
    
    // Check manifest entry
    const auto& def = g_FeatureManifest[static_cast<uint32_t>(id)];
    assert(def.id == id);
    assert(def.implemented);  // Must be implemented to test
    
    // Gate should work
    bool gated = lic.gate(id, "validateFeature");
    assert(gated);
    
    // Feature should be in current mask
    bool enabled = lic.isFeatureEnabled(id);
    assert(enabled);
    
    // Mark as tested
    // TODO: Update manifest tested=true when test passes
}
```

---

## Quick Reference: 3 Phases Summary

| Phase | Goal | Status | Time | Dependencies |
|-------|------|--------|------|---|
| **Phase 1** | Features discoverable | ✅ DONE | — | None |
| **Phase 2** | License creation works | ✅ DONE | — | Phase 1 |
| **Phase 3** | All features wired | ⚠️ 8-10 hrs | 8-10h | Phase 1-2 |
| **Phase 4** | Tests pass | ❌ PLAN | 4-6h | Phase 1-3 |
| **Phase 5** | Sovereign features | ❌ PLAN | TBD | Phase 1-4 |

---

## Success Criteria

### Phase 1: Success ✅
- [x] Feature Registry shows all 61 features
- [x] Manifest is compile-time complete
- [x] Hardware ID computes correctly

### Phase 2: Success ✅
- [x] Create Enterprise license in GUI
- [x] License validates (HMAC-SHA256)
- [x] 800B becomes available when enterprise key loaded
- [x] License install/uninstall works

### Phase 3: Success (Target)
- [ ] All 15 unwired features have menu items
- [ ] Menu items respect current license tier
- [ ] Feature entry points check license gates
- [ ] Real-time menu disable when license downgraded
- [ ] Feature Registry Panel shows accurate "Wired" status

---

**Last Updated:** 2026-02-14  
**Author:** RawrXD Engineering Team  
**Questions?** See ENTERPRISE_LICENSE_AUDIT_REPORT.md
