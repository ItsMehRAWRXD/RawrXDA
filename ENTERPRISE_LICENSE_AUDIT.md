# Enterprise License & Features — Audit

**Purpose**: Single source of truth for what is implemented vs missing for RawrXD Enterprise licensing and the 800B Dual-Engine / feature set.

---

## 1. What Is There (Implemented)

### License system (core)

| Component | Location | Status |
|-----------|----------|--------|
| Feature bitmasks (8 features) | `src/core/enterprise_license.h` `LicenseFeature::` | ✅ |
| EnterpriseLicense singleton | `src/core/enterprise_license.cpp` | ✅ |
| C++ stubs (no MASM) | `src/core/enterprise_license_stubs.cpp` | ✅ |
| HWID generation, MurmurHash3 validation | enterprise_license_stubs.cpp | ✅ |
| **Enterprise_DevUnlock()** (RAWRXD_ENTERPRISE_DEV=1) | enterprise_license_stubs.cpp | ✅ |
| Initialize() calls DevUnlock when env set | enterprise_license.cpp | ✅ |
| InstallLicense / InstallLicenseFromFile | enterprise_license.cpp | ✅ |
| g_800B_Unlocked, g_EnterpriseFeatures | stubs + header | ✅ |

### UI and entry points

| Entry point | ID | Handler | Status |
|-------------|-----|---------|--------|
| **Tools > License Creator...** | 3015 | `showLicenseCreatorDialog()` | ✅ Full dialog (Win32IDE_LicenseCreator.cpp) |
| **Help > Enterprise License / Features...** | 7007 | `showLicenseCreatorDialog()` | ✅ Same full dialog |
| **Agent > 800B Dual-Engine Status** | 4225, 4220 | AgentCommands: is800BUnlocked() → message | ✅ |
| Simple popup (fallback) | — | `showEnterpriseLicenseDialog()` | ✅ MessageBox with 8 features + HWID |

### Full License Creator dialog (Win32IDE_LicenseCreator.cpp)

| Capability | Status |
|------------|--------|
| Edition + HWID + all 8 features (locked/unlocked) | ✅ |
| Wiring line per feature (where gated) | ✅ |
| **Dev Unlock** button (when RAWRXD_ENTERPRISE_DEV=1) | ✅ |
| **Refresh** after Dev Unlock / Install (no close) | ✅ |
| **Install License...** (.rawrlic) | ✅ |
| **Copy HWID** to clipboard | ✅ |
| **Launch KeyGen** (RawrXD_KeyGen.exe same dir) | ✅ |

### Feature gates in code

| Feature | Gate location | Check |
|---------|----------------|--------|
| DualEngine800B | streaming_engine_registry.cpp | isFeatureEnabled(DualEngine800B) |
| DualEngine800B | Win32IDE_AgentCommands.cpp, main_win32.cpp, HeadlessIDE.cpp | is800BUnlocked() / g_800B_Unlocked |
| FlashAttention | streaming_engine_registry.cpp | isFeatureEnabled(FlashAttention) |
| DistributedSwarm | Win32IDE_SwarmPanel.cpp | isFeatureEnabled(DistributedSwarm) |
| All 8 | production_release.cpp | registerGate( name, LicenseFeature::*, … ) |
| UnlimitedContext | enterprise_license.cpp | GetMaxContextLength() when feature enabled |

### Scripts and docs

| Item | Status |
|------|--------|
| `scripts/Create-EnterpriseLicense.ps1` (-DevUnlock, -ShowStatus, -CreateForMachine, -PythonIssue) | ✅ |
| ENTERPRISE_FEATURES_MANIFEST.md | ✅ |
| This audit (ENTERPRISE_LICENSE_AUDIT.md) | ✅ |

---

## 2. What Is Missing or Incomplete

| Item | Notes |
|------|--------|
| **RawrXD_KeyGen.exe** | Referenced by License Creator and script; must be built from `src/tools/RawrXD_KeyGen.cpp`. Not all builds include it. |
| **.rawrlic format / RSA sign** | Stubs validate by MurmurHash only; production ASM/MASM path would use RSA-4096. KeyGen and Python generator are the intended producers. |
| **MASM license path** | When RAWR_HAS_MASM=1, ASM symbols replace stubs; KeyGen and RSA flow matter for production. |
| **Telemetry / Airgap license UI** | IDM_TEL_LICENSE_STATUS (11406), IDM_AIRGAP_LICENSE (13046) mentioned in manifest; not audited here. |
| **800B engine registration** | registerDualEngine800B() exists when licensed; actual 800B model load path is separate (Configure Model / Load Model). |

---

## 3. Wiring Table — All 8 Enterprise Features

| Mask | Feature | Display (License Creator) | Menu / UI | Code gate |
|------|---------|---------------------------|-----------|-----------|
| 0x01 | 800B Dual-Engine | [UNLOCKED] / [locked] | Agent > 800B Dual-Engine Status | AgentCommands, streaming_engine_registry, g_800B_Unlocked |
| 0x02 | AVX-512 Premium | ✅ | — | production_release |
| 0x04 | Distributed Swarm | ✅ | Swarm panel | Win32IDE_SwarmPanel |
| 0x08 | GPU Quant 4-bit | ✅ | — | production_release |
| 0x10 | Enterprise Support | ✅ | — | Support tier / audit |
| 0x20 | Unlimited Context | ✅ | — | enterprise_license.cpp GetMaxContextLength |
| 0x40 | Flash Attention | ✅ | — | streaming_engine_registry, flash_attention |
| 0x80 | Multi-GPU | ✅ | — | production_release |

All eight are **displayed** in Tools > License Creator and Help > Enterprise License / Features with locked vs unlocked and (in the full dialog) a short “wiring” line. Where a feature has no dedicated menu, the License Creator is the single place to see status.

---

## 4. Top 3 Phases (Done)

**Phase 1 — Wire and single dialog**

- Tools > License Creator (3015) and Help > Enterprise (7007) both call `showLicenseCreatorDialog()` (full dialog in Win32IDE_LicenseCreator.cpp).
- Removed duplicate `showLicenseCreatorDialog()` from Commands so only the full implementation is used.

**Phase 2 — Refresh without closing**

- After **Dev Unlock** or **Install License...**, the License Creator window refreshes the status and feature list in place instead of closing, so the user sees [UNLOCKED] immediately.

**Phase 3 — Audit, 800B messaging, startup hint**

- This audit document (what’s there vs missing, wiring table).
- 800B status aligned everywhere: both Agent command paths show “800B Dual-Engine: locked (requires Enterprise license)” or “800B Dual-Engine: UNLOCKED (Enterprise)” using `EnterpriseLicense::is800BUnlocked()`.
- **Startup hint**: On sidebar init, Output panel shows `[License] Edition: X | 800B: locked|UNLOCKED | Tools > License Creator for details` (Win32IDE_Sidebar.cpp).

---

## 5. Quick Reference

- **Dev unlock**: Set `RAWRXD_ENTERPRISE_DEV=1`, restart IDE; or use License Creator > Dev Unlock when env is set.
- **See status**: **Tools > License Creator...** or **Help > Enterprise License / Features...** (same dialog).
- **800B message**: **Agent > 800B Dual-Engine Status** — “locked (requires Enterprise license)” until licensed or dev-unlocked.
- **Script**: `.\scripts\Create-EnterpriseLicense.ps1 -DevUnlock` (and -ShowStatus, -CreateForMachine, -PythonIssue).
