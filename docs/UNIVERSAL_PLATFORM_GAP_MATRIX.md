# Universal platform gap matrix (production-honest)

This document answers “what is missing for Windows / Linux / macOS / Android / iOS?” **without** promising a single in-tree engine that replaces normal linkers, loaders, or store/signing rules.

| Canonical doc | Role |
|---------------|------|
| `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` | Production vs lab; **§6** rejected narratives; **§7** MSVC “builder” artifacts (Rich, load config, alignment, debug/PDB) |
| `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md` | Tri-format **lab** `compose*` scope; IR → object → **link** → sign |
| `docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md` | PR boundaries (no syscall bridge / bypass / universal linker replacement claims) |

**Status legend**

| Symbol | Meaning |
|--------|---------|
| ✅ | In-repo, documented, usable for its stated scope |
| ⚠️ | Partial / experimental / needs normal toolchain to finish |
| ❌ | Not present as an end-to-end product path in this repo |
| 🚫 | **Not a gap** — explicitly **rejected** (do not file as “TODO to implement”) |

---

## 1. What will not appear as “finished production” (🚫 rows)

These are **policy rejections**, not backlog items. They do not get rows in “gap to close” tables except as **🚫**.

| Ask | Verdict |
|-----|---------|
| One **UOB** / opcode stream that emits final PE + ELF + Mach-O for all OSes **without** a linker | 🚫 Real platforms need **linkers, loaders, signing, packaging** |
| **Direct syscall / `svc` tables** for stdlib-free or “kernel direct” I/O | 🚫 Use **documented APIs**; see **§6** in production scope doc |
| **PEB / export walk** as **shipped product** resolution (vs documented IAT + `link.exe`) | 🚫 See resolver strategy + covenant |
| **Bypass** signing, Gatekeeper, AMFI, SmartScreen, Defender, execution policy | 🚫 |
| Scripts that **overwrite** IDE `settings.json`, “seal” host, disable telemetry as a **feature** | 🚫 |
| **MSVC byte-for-byte** parity (Rich, full load config, debug/PDB, layout) as a **maintained** hand-rolled emitter | 🚫 Use **`cl` + `link.exe` + CI** — **§7** |
| **iOS / Android “tonight”** without NDK / Xcode / Gradle / store flows | 🚫 |
| Polymorphic emitters whose **purpose** is **syscall / `svc`** I/O | 🚫 |

---

## 2. Platform × production contract (what “done” means)

| Platform | Deliverable shape | Signing / policy gate | RawrXD in-repo reality |
|----------|-------------------|------------------------|-------------------------|
| **Windows** | PE32+ `.exe` / `.dll`, COFF objects | Optional Authenticode; SmartScreen | ✅ Strongest path: IAT reference `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm`, minimal entry `RawrXD_Sovereign_MinimalEntry_v224.asm`, `cmake/RawrXD_SovereignIAT.cmake`, `tools/pe_emitter.asm`; tri-format lab `include/rawrxd/sovereign_emit_formats.hpp` |
| **Linux** | ELF64 `ET_EXEC` / `ET_DYN`, `PT_LOAD`, dynamic linking as needed | Distro / glibc / musl policy | ⚠️ Lab compose + headers in `sovereign_emit_formats.hpp`; **full** dynamic link = **clang + lld** (not in-tree omnibus) |
| **macOS** | Mach-O 64, dyld, load commands | **codesign** (adhoc or Apple); hardened runtime | ⚠️ Lab compose + spec; shipping = **clang + ld64** + signing step |
| **Android** | **APK/AAB** + JNI/NDK `.so` | Play signing, API levels, manifest | ❌ No checked-in end-to-end NDK/APK product pipeline |
| **iOS** | Signed **.app** / **IPA** | **Xcode** + provisioning + entitlements | ❌ Documentation-only; unsigned drop is not a shipping model |

---

## 3. Toolchain × platform (recommended production stack)

| Platform | Compiler / assembler | Linker | Notes |
|----------|----------------------|--------|--------|
| **Windows** | MSVC `cl`, Clang for Windows; NASM/MASM for asm | `link.exe`, `lld-link` | RawrXD IDE build: `scripts/build.ps1` (CMake + MSVC) |
| **Linux** | Clang / GCC | `lld`, `bfd` `ld` | Preset: root `CMakePresets.json` → `linux-ninja-release` (host must be Linux or WSL) |
| **macOS** | Apple Clang | `ld64` | Not exercised in current default CI |
| **Android** | NDK Clang | NDK linker + Gradle packaging | **Gap** for in-repo automation |
| **iOS** | Xcode Clang | Xcode linker + codesign | **Gap** for in-repo automation |

---

## 4. CI × automation (current vs gap)

**Workflow:** `.github/workflows/ci.yml` (single job, `windows-latest`).

| Step / concern | Runs in CI? | Notes |
|----------------|-------------|--------|
| `tools/validate_command_registry.py --strict` + baseline | ✅ | SSOT for menu/command IDs |
| NASM install | ✅ | For asm targets as configured by CMake |
| Qt via `aqtinstall` | ✅ | **If** CMake graph still expects Qt for some targets — align with “Win32-first” docs; trim when project is fully Qt-free |
| `scripts/build.ps1` Release x64 | ✅ | Main IDE build path |
| `services/` Python + HexMag smoke | ✅ | HTTP `/health`, `/ask`, `/agent` smoke |
| **Linux** CMake build | ❌ | Optional future: `ubuntu-latest` + `cmake --preset linux-ninja-release` |
| **macOS** build + adhoc sign | ❌ | Optional future: `macos-latest` |
| **Android NDK** | ❌ | Phase D candidate |
| **Semgrep / “3 rulesets”** (mentioned in archived CI docs) | ❌ | Not in current `ci.yml`; do not assume it runs |

---

## 5. In-repo artifact matrix (by subsystem)

| Artifact / path | Windows | Linux | macOS | Android | iOS |
|-----------------|---------|-------|-------|---------|-----|
| `include/rawrxd/sovereign_emit_formats.hpp` (`compose*`) | ✅ lab PE | ✅ lab ELF | ✅ lab Mach-O | — | — |
| `include/rawrxd/sovereign_lab_blob_io.hpp` | ✅ | ✅ | ✅ | — | — |
| `include/rawrxd/sovereign_target_manifest.hpp` | ✅ metadata | ✅ | ✅ | ⚠️ fields exist; NDK wiring **gap** | ⚠️ same |
| `cmake/RawrXD_SovereignIAT.cmake` + PE64 IAT asm | ✅ | — | — | — | — |
| `toolchain/from_scratch/phase2_linker/` (`pe_writer.c`, `rawrxd_link`) | ✅ research PE linker | — | — | — | — |
| `tests/test_sovereign_compose_lab.cpp` | ✅ | ✅ | ✅ | — | — |
| Win32 IDE (`src/win32app/`) | ✅ primary | — | — | — | — |

**Phase 2 linker note:** `toolchain/from_scratch/phase2_linker` is a **bounded from-scratch PE linker experiment** (COFF → PE). It is **not** a substitute for declaring “production parity” with MSVC **`link.exe`**; keep it in the **research / toolchain** column, not the IDE product promise. See `toolchain/from_scratch/phase2_linker/README.md`.

---

## 6. Gap matrix (actionable IDs — engineering only)

Rows are **gaps to close with normal toolchains and CI**, not kernel bridges.

| ID | Area | Windows | Linux | macOS | Android | iOS |
|----|------|---------|-------|-------|---------|-----|
| **G1** | **Primary shipping IDE** | ✅ CMake + MSVC (`scripts/build.ps1`, CI) | N/A (Win32 product) | N/A | N/A | N/A |
| **G2** | **Sovereign lab / compose** | ✅ tests + headers | ⚠️ needs **clang+lld** job to prove | ⚠️ needs **ld64** + sign | ❌ NDK sample | ❌ Xcode doc-only |
| **G3** | **CI diversity** | ✅ `windows-latest` | ❌ no `ubuntu` job | ❌ no `macos` job | ❌ | ❌ |
| **G4** | **`TargetManifest` → CMake** | ⚠️ partial | ⚠️ preset exists; wire more options | ⚠️ | ❌ | ❌ |
| **G5** | **`CMakePresets.json`** | ✅ `ninja-release` | ✅ `linux-ninja-release` | ⚠️ add when needed | ❌ | ❌ |
| **G6** | **“Universal syscall map” / bypass loaders** | 🚫 | 🚫 | 🚫 | 🚫 | 🚫 |

---

## 7. Ordered next steps (verifiable)

1. **Windows (Phase A):** Keep IAT + smoke + command registry as the **documented** Windows binary story (`docs/SOVEREIGN_MASTER_TEMPLATE_v224.md`, `docs/PE64_IAT_FABRICATOR_v224.md`).
2. **Metadata (Phase B):** Extend `TargetManifest` → CMake variables **without** adding syscall bridges (`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` §4).
3. **Linux CI (optional):** `ubuntu-latest` job: `cmake --preset linux-ninja-release` + build a **small** target — proves **toolchain**, not “sovereign bypass.”
4. **macOS CI (optional):** `macos-latest` + `codesign -s -` on a tiny artifact.
5. **Android (Phase D):** NDK + minimal Gradle under `docs/` + optional workflow.
6. **iOS (Phase D):** Xcode steps in docs only; no raw Mach-O shipping claim.

---

## 8. Traceability

| Topic | Document |
|-------|----------|
| Production vs fantasy | `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` §1, §3, §6, §7 |
| Tri-format lab | `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`, `docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md` |
| Windows IAT | `docs/PE64_IAT_FABRICATOR_v224.md`, `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm` |
| Import resolver policy | `docs/SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md` |
| Command SSOT / CI gate | `docs/COMMAND_LEGACY_ID_AUDIT.md`, `tools/validate_command_registry.py` |
| Phase 2 PE linker (research) | `toolchain/from_scratch/phase2_linker/README.md`, `pe_writer.c` |

---

**Last updated:** 2026-03-20 — full matrix: toolchain + CI + in-repo artifacts + 🚫 rows + phase2_linker scope + `CMakePresets.json` traceability.
