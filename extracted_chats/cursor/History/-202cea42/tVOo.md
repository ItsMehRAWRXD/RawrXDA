# Enterprise Features — Audit Report

Full audit of what is **present** vs **missing** for the 8 enterprise features, license creator, and wiring. Includes **Top 3 Phases** for tracking.

---

## 1. What Is There (Present)

### 1.1 License system (core)

| Component | Location | Status |
|-----------|----------|--------|
| Feature bits (0x01–0x80) | `src/core/enterprise_license.h` | Present |
| Singleton + HasFeature / HasFeatureMask / GetMaxContextLength | `src/core/enterprise_license.cpp` | Present |
| ASM stubs (g_800B_Unlocked, Enterprise_Unlock800BDualEngine, etc.) | `src/core/enterprise_license_stubs.cpp` | Present |
| 800B unlock → g_800B_Unlocked | `enterprise_license.cpp` Initialize() | Present |
| Unlimited Context → 200K tokens | `enterprise_license.cpp` GetMaxContextLength() | Present |

### 1.2 Runtime wiring (feature checks)

| Feature | Where checked | Type |
|---------|----------------|------|
| **0x01 800B Dual-Engine** | `Win32IDE_AgentCommands.cpp` (IDM_AI_800B_STATUS 4225); `streaming_engine_registry.cpp` (register 800B); `main_win32.cpp` / `HeadlessIDE.cpp` (g_800B_Unlocked) | **Wired** |
| **0x02 AVX-512 Premium** | `production_release.cpp` (gate only); no direct runtime use of AVX512Premium in engine registry | Display / gate only |
| **0x04 Distributed Swarm** | `Win32IDE_SwarmPanel.cpp` — `isFeatureEnabled(DistributedSwarm)` | **Wired** |
| **0x08 GPU Quant 4-bit** | `production_release.cpp` (gate only) | Display / gate only |
| **0x10 Enterprise Support** | No runtime check; support tier / audit differentiation | Display only |
| **0x20 Unlimited Context** | `enterprise_license.cpp` GetMaxContextLength() | **Wired** |
| **0x40 Flash Attention** | `flash_attention.cpp` Initialize() — HasFeature(FlashAttention); `streaming_engine_registry.cpp` registerFlashAttention | **Wired** |
| **0x80 Multi-GPU** | `production_release.cpp` (gate only) | Display / gate only |

### 1.3 UI and display

| Item | Location | Status |
|------|----------|--------|
| **Help > Enterprise License / Features...** | IDM_HELP_ENTERPRISE **7007** → `handleHelpCommand` → `showEnterpriseLicenseDialog()` | Present |
| **Tools > License Creator...** | IDM_TOOLS_LICENSE_CREATOR **3015** → `showLicenseCreatorDialog()` | Present |
| Full License Creator window (Install, Copy HWID, Launch KeyGen) | `Win32IDE_LicenseCreator.cpp` — all 8 features + wiring strings | Present (if built) |
| Fallback (MessageBox) | `Win32IDE_Commands.cpp` `showEnterpriseLicenseDialog()` — lists all 8 + edition, HWID, mask | Present |
| **Agent > 800B Dual-Engine Status** | IDM_AI_800B_STATUS **4225** — "locked (requires Enterprise license)" / "UNLOCKED (Enterprise)" | Present |
| Command palette | 7007 "Help: Enterprise License / Features" in registry | Present |

### 1.4 License creator assets

| Asset | Location | Status |
|-------|----------|--------|
| Manifest | `ENTERPRISE_FEATURES_MANIFEST.md` (repo root) | Present |
| PowerShell script | `scripts/Create-EnterpriseLicense.ps1` | Present |
| KeyGen (CLI) | `src/tools/RawrXD_KeyGen.cpp` (--genkey, --hwid, --issue, --sign) | Present |
| Production gates | `production_release.cpp` — 7 gates registered (all except EnterpriseSupport) | Present |

---

## 2. What Is Missing or Incomplete

### 2.1 Gaps

| Gap | Detail | Recommendation |
|-----|--------|----------------|
| **ProductionReleaseEngine never refreshed** | `refreshLicense()` exists but is **never called**; `m_currentLicenseFlags` stays 0, so `isFeatureAllowed()` / `enforceGate()` always deny. | Call `ProductionReleaseEngine::instance().refreshLicense()` after `EnterpriseLicense::Instance().Initialize()` (e.g. in `main_win32.cpp` or IDE startup). |
| **AVX512Premium not wired in engine** | Gate is registered; no code path uses `isFeatureEnabled(AVX512Premium)` to enable AVX-512 premium kernels. | Either wire in streaming engine / inference path or keep as display-only and document. |
| **GPUQuant4Bit / MultiGPU** | Gates only; no runtime use of these bits in GPU or multi-GPU code. | Wire when GPU/multi-GPU stacks use license, or keep display-only. |
| **EnterpriseSupport (0x10)** | Not registered as a gate; no runtime check. | Keep as display/support tier only unless support portal or audit needs a check. |
| **Airgapped text license** | Conversation referenced `rawrxd.rawrlic` text format and airgapped panel; not re-audited here. | Confirm airgapped panel and text license path in wwa if needed. |

### 2.2 Help menu ID vs About

- Help menu uses **IDM_HELP_ENTERPRISE 7007** for Enterprise License/Features and **IDM_HELP_ABOUT 4001** for About.
- `routeCommand` sends **7000–7999** to `handleHelpCommand`, so 7007 correctly opens Enterprise. About at 4001 may route to Terminal range (4000–4099) per summary; worth confirming in `routeCommand` (e.g. 4001 vs 7004) so About goes to Help.

### 2.3 License Creator build

- Two implementations of **showLicenseCreatorDialog()**:  
  - **Win32IDE_LicenseCreator.cpp**: full window (Install License, Copy HWID, Launch KeyGen, all 8 features + wiring).  
  - **Win32IDE_Commands.cpp**: fallback that calls `showEnterpriseLicenseDialog()` (MessageBox).  
- If both are linked, duplicate symbol. Typically only one is in the build; the other is excluded or stubbed. Manifest should state which build (e.g. “full License Creator window” vs “MessageBox only”) is the default.

---

## 3. Wiring Summary Table

| Mask | Feature | Displayed (UI) | Runtime check | Where |
|------|---------|----------------|---------------|--------|
| 0x01 | 800B Dual-Engine | Yes (Agent, License Creator, Help) | Yes | AgentCommands, streaming_engine_registry, g_800B_Unlocked |
| 0x02 | AVX-512 Premium | Yes | Gate only | production_release |
| 0x04 | Distributed Swarm | Yes | Yes | Win32IDE_SwarmPanel |
| 0x08 | GPU Quant 4-bit | Yes | Gate only | production_release |
| 0x10 | Enterprise Support | Yes | No | Display only |
| 0x20 | Unlimited Context | Yes | Yes | enterprise_license.cpp GetMaxContextLength |
| 0x40 | Flash Attention | Yes | Yes | flash_attention.cpp, streaming_engine_registry |
| 0x80 | Multi-GPU | Yes | Gate only | production_release |

---

## 4. Top 3 Phases (Enterprise)

Use these as the **top 3 phases** for enterprise/license work so you can keep up with what’s done and what’s next.

### Phase 1: License creator + display (DONE in wwa)

- [x] Single source of truth for 8 features: `ENTERPRISE_FEATURES_MANIFEST.md`
- [x] License creator script: `scripts/Create-EnterpriseLicense.ps1` (-DevUnlock, -ShowStatus, -CreateForMachine, -PythonIssue)
- [x] Help > Enterprise License / Features (7007) — all 8 features shown (MessageBox or dialog)
- [x] Tools > License Creator (3015) — same dashboard / full window when built
- [x] Agent > 800B Dual-Engine Status (4225) — locked / UNLOCKED message
- [x] All 8 features listed with [UNLOCKED] / [locked] in UI

### Phase 2: Audit and document (DONE with this doc)

- [x] Audit: what’s wired vs display-only (this document)
- [x] Wiring table for all 8 features
- [x] Gaps: refreshLicense never called; AVX512/GPUQuant/MultiGPU/EnterpriseSupport not wired or gate-only
- [ ] Optional: fix Help vs Terminal ID routing for About (4001 vs 7004) if needed
- [ ] Optional: document which build (full License Creator vs MessageBox) is default in manifest

### Phase 3: Wire and sync (TODO)

- [ ] Call `ProductionReleaseEngine::instance().refreshLicense()` after license init so gates use current feature mask
- [ ] Optionally wire AVX512Premium in engine registration or inference path (or explicitly mark “display only” in manifest)
- [ ] Optionally wire GPUQuant4Bit / MultiGPU where GPU/multi-GPU code is used (or keep display-only)
- [ ] Ensure binary (.rawrlic) and text (rawrxd.rawrlic for airgapped) flows are documented in manifest and script help
- [ ] Add “Enterprise Features” or “License” to command palette search keywords if desired

---

## 5. Quick reference

- **Manifest**: `ENTERPRISE_FEATURES_MANIFEST.md`
- **License script**: `scripts/Create-EnterpriseLicense.ps1`
- **KeyGen**: `src/tools/RawrXD_KeyGen.cpp`
- **Core license**: `src/core/enterprise_license.h` / `.cpp` / `_stubs.cpp`
- **Help entry**: 7007 → `showEnterpriseLicenseDialog()` (or License Creator window)
- **Tools entry**: 3015 → `showLicenseCreatorDialog()`
- **800B status**: 4225 in `Win32IDE_AgentCommands.cpp`
- **Swarm gate**: `Win32IDE_SwarmPanel.cpp` — DistributedSwarm
- **Gates (no refresh)**: `production_release.cpp` — call `refreshLicense()` at startup to sync
