# Enterprise Features Manifest

Tracking document for RawrXD Enterprise features, license creator, and wiring.
All features are displayed in **Tools > License Creator** and **Help > Enterprise License / Features** (both open the same full dialog).

**Audits:** **ENTERPRISE_LICENSE_CREATOR_AUDIT.md** (full audit: features, phases, wiring, what's missing) | **ENTERPRISE_LICENSE_AUDIT.md** (quick ref) | **ENTERPRISE_FULL_AUDIT.md** (Top 3 Phases detail)

## Enterprise Features (8 total)

| Mask | Feature | Wiring / Gate | UI / Menu |
|------|---------|---------------|-----------|
| 0x01 | 800B Dual-Engine | AgentCommands, streaming_engine_registry, g_800B_Unlocked | Agent > 800B Dual-Engine Status |
| 0x02 | AVX-512 Premium | production_release, StreamingEngineRegistry | License Creator |
| 0x04 | Distributed Swarm | Win32IDE_SwarmPanel | Swarm Panel |
| 0x08 | GPU Quant 4-bit | production_release | License Creator |
| 0x10 | Enterprise Support | Support tier, audit differentiation | License Creator |
| 0x20 | Unlimited Context | enterprise_license.cpp GetMaxContextLength | License Creator |
| 0x40 | Flash Attention | streaming_engine_registry, flash_attention | License Creator |
| 0x80 | Multi-GPU | production_release | License Creator |

## License Creator

- **Menu**: Tools > License Creator... and **Help > Enterprise License / Features...**
- **ID**: IDM_TOOLS_LICENSE_CREATOR (3015), IDM_HELP_ENTERPRISE (7007)
- **Handler**: `showLicenseCreatorDialog()` (Win32IDE_LicenseCreator.cpp)

### Dialog Capabilities

- Shows current edition (Community / Enterprise / Trial)
- Shows all 8 enterprise features with locked/unlocked status and **wiring** (where each is gated)
- **Dev Unlock**: Button visible when `RAWRXD_ENTERPRISE_DEV=1` — brute-forces valid license for development
- **Install License...**: Install a .rawrlic file
- **Copy HWID**: Copy hardware ID to clipboard for license requests
- **Launch KeyGen**: Spawn RawrXD_KeyGen.exe (same dir as IDE exe) for CLI license creation
- KeyGen usage instructions displayed

## RawrXD_KeyGen (CLI License Creator)

- **Path**: `src/tools/RawrXD_KeyGen.cpp`
- **Usage**:
  - `--genkey` — Generate RSA-4096 key pair
  - `--hwid` — Print this machine's HWID
  - `--issue` — Create license (--type, --features, --hwid, --days)
  - `--sign <blob>` — Sign a license blob
  - `--export-pub` — Export public key as MASM .inc

## Other Entry Points

- **Help > Enterprise License / Features (7007)**: Opens full License Creator dialog (same as Tools > License Creator)
- **Startup hint** (Output panel): On sidebar init, `[License] Edition: X | 800B: locked|UNLOCKED | Tools > License Creator for details` — see Win32IDE_Sidebar.cpp
- **Agent > 800B Dual-Engine Status (4225)**: Shows "locked (requires Enterprise license)" or "UNLOCKED (Enterprise)"

## Dev Unlock

Set environment variable `RAWRXD_ENTERPRISE_DEV=1` and restart. Then:

1. Tools > License Creator > Dev Unlock, or
2. Call `Enterprise_DevUnlock()` — brute-forces a valid license hash for this machine
