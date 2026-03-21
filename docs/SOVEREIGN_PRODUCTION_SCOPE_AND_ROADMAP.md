# RawrXD “Sovereign” multiformat — **production scope** (honest)

This document defines what **“production”** means for RawrXD’s binary/emission work, what is **already shipped** in-repo, and what **cannot** be promised as a single finished engine.

---

## 1. What “finish everything for Windows / Linux / macOS / Android / iOS” actually requires

Each platform has a **different contract**:

| Platform | Production path | Why a single hand-rolled “universal emitter” is not realistic |
|----------|-----------------|------------------------------------------------------------------|
| **Windows** | MSVC or Clang + `link.exe` / `lld-link`, PE/coff, signing optional | PE + imports + resources + loader edge cases are large; repo already has **documented IAT smoke** |
| **Linux** | Clang/LLVM + `lld`, glibc/musl or static link | ELF **ET_DYN** vs **ET_EXEC**, PIE, interpreters, `DT_*` — full dynamic linking is a **linker** job |
| **macOS** | Clang + `ld64`, **codesign** | Mach-O **load commands**, `LC_MAIN`, code signing / hardened runtime — not “header bytes only” |
| **Android** | **Android Gradle Plugin** + NDK, APK/AAB packaging | Deliverable is usually **APK/AAB**, not a raw ELF you “drop and run” |
| **iOS** | **Xcode**, provisioning, entitlements, App Store / TestFlight | Unsigned arbitrary Mach-O is **not** a supported end-user delivery model |

**Conclusion:** Production = **standard toolchains + CI + signing + store rules**, not one custom “byte scribe” that replaces `link`, `dyld`, and app stores.

---

## 2. What is already **real** in this repository (not marketing)

| Artifact | Status |
|----------|--------|
| PE writer experiment | `tools/pe_emitter.asm` |
| Manual **x64** IAT (NASM) + minimal entry + CMake smoke | `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm`, `RawrXD_Sovereign_MinimalEntry_v224.asm`, `cmake/RawrXD_SovereignIAT.cmake` |
| Opcode helpers (`FF 15`, `E8`) | `include/rawrxd/sovereign_emit_x64.hpp` |
| Tri-format **bootstrap** bytes (experimental) | `include/rawrxd/sovereign_emit_formats.hpp` |
| Neutral target manifest (CMake/tooling) | `include/rawrxd/sovereign_target_manifest.hpp` |
| Safe architecture spec | `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md` |
| Import / “resolver” concepts (policy; not a how-to bypass) | `docs/SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md` |
| Contributor PR checklist (Sovereign / binary) | `docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md` |
| Consolidated Windows truth | `docs/SOVEREIGN_MASTER_TEMPLATE_v224.md` |
| **Tier G — global-use contract** (reusable Sovereign API vs IDE product) | `docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md` |

---

## 2.5 Tier G vs Tier P (global reuse vs IDE product)

- **Tier P (product):** shipping **RawrXD IDE** Windows binaries — **`cl.exe` + `link.exe` + CRT + PDB + CI** (**§7**).
- **Tier G (global):** **Sovereign** headers, examples, tri-format lab emitters, and tools are **intended for reuse** (embedding, education, cross-host builds). See **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`**.  
  Tier G does **not** cancel **§6**/**§7** (no promise of hand-rolled **MSVC bitwise** parity or universal linker-for-all-OSes).

---

## 3. Explicitly **out of scope** (will not be implemented as “production RawrXD”)

- **ARM64 / x64 syscall or SVC tables** for stdlib-free I/O or “direct kernel” apps  
- **Scripts** that overwrite **Cursor** / IDE `settings.json`, disable telemetry, or “seal” the host  
- **Bypass** of code signing, Gatekeeper, AMFI, SmartScreen, Defender, or execution policy  
- Claims of **“binary omnipotence”**, **unlimited size IDE from one emitter**, or **iOS delivery without Apple’s toolchain**
- **MSVC “Rich” header replication** (XOR-masked **`DanS`** blob with `@comp.id` records) as a **maintained RawrXD product emitter** — see **§7**
- **Full `IMAGE_LOAD_CONFIG_DIRECTORY64` synthesis** (CFG / stack cookie / SEH metadata parity with **`link.exe`**) as a **maintained RawrXD product emitter** — see **§7**
- **MSVC-identical section layout, alignment padding, and byte-level “entropy” profile** (matching **`link.exe`** ordering, gaps, and statistical distribution of bytes per section) as a **maintained RawrXD product emitter** — see **§7.3**
- **Full PE debug directory / PDB-grade** (`IMAGE_DEBUG_DIRECTORY`, CodeView records, **`RSDS`**, PDB path/hash parity with **`cl.exe` / `link.exe` / `mspdb`** output) as a **maintained RawrXD product emitter** — see **§7.4**

---

## 4. Phased roadmap (legitimate, verifiable)

**Phase A — Windows (closest to “production” in-tree)**  
- Harden and test the existing **IAT + smoke** path; extend docs; optional extra imports only via normal PE/IAT rules.

**Phase B — Cross-host build metadata**  
- Use `TargetManifest` to drive **CMake** presets (clang, lld, NDK paths) — **metadata only**, no custom kernel bridge.  
  Bounded starter presets: root **`CMakePresets.json`** (`ninja-release`, `linux-ninja-release`).

**Phase C — Analysis / packaging**  
- Optional tools that emit **metadata** (section list, hashes) for inspection — **not** a replacement OS loader.

**Phase D — Mobile**  
- **Android:** NDK sample app or library build in CI (where runners allow).  
- **iOS:** Xcode project + signing **outside** this repo’s “custom emitter” story; document steps only.

**Gap matrix (all platforms, honest):** `docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md` — platform × toolchain × CI × in-repo artifact status; explicitly rejects syscall/“bypass” rows.

---

## 5. Answer to “needs to be fully production non-exampled”

**Production** here means: **reproducible builds, tests, documented limits, and compliance with platform rules** — not a single universal syscall-based emitter for all OSes.

For RawrXD, treat **`docs/SOVEREIGN_MASTER_TEMPLATE_v224.md`** + **`cmake/RawrXD_SovereignIAT.cmake`** as the only **end-to-end** binary path that is **implemented and buildable today**. Everything else in the multiformat headers is **experimental bootstrap** until wired to real linkers and CI.

---

## 6. “Generate all gaps from scratch” / UOB / “Ultimate Sovereign State” — **not accepted work**

Requests that bundle the following are **rejected** for this repository (no implementation, no “gap fill”):

| Narrative | Why |
|-----------|-----|
| **Universal Opcodes-to-Binary (UOB)** replacing normal linkers for all OSes | Infeasible as one in-tree engine; use **clang/lld/MSVC/Xcode/NDK**. |
| **Universal syscall / SVC bridge** (stdlib-free I/O, “kernel direct”) | Disallowed assist pattern; use **documented APIs** and normal CRT/frameworks. |
| **ARM64 opcode table + `svc` dispatch** for Android/iOS/macOS “tonight” | Same; mobile production = **NDK / Xcode**, not hand syscall tables. |
| **Cross-platform “activator”** scripts (Cursor `settings.json`, telemetry, `pkill`, `chattr`, “total bypass”) | Host manipulation — **out of scope**. |
| **Ring-zero / driver / “no metadata”** claims from one emitter | Requires separate driver stacks, WHQL, signing — **not** this project’s sovereign path. |
| **“Reverse engineered to be EXACT ‘Full’ MSVC++ parity”** (hand-rolled bytes matching **`cl.exe`/`link.exe`/Rich/load-config/PDB/layout** as a **maintained** in-repo product) | **Not** accepted — **§7**; **full** MSVC++ surface belongs to the **toolchain + CI**, not a repo emitter marketed as bitwise-perfect reimplementation. |

**Legitimate next steps** remain §4 phases **A–D** and the files listed in §2 — not a from-scratch omnibus emitter.

---

## 7. MSVC / linker-only PE artifacts — **not** RawrXD “product code”

These structures are **normal** in binaries produced by **`link.exe`** (and friends) for Windows. They are **large**, **version-sensitive**, and **not** part of the tri-format **lab** `compose*` contract. RawrXD does **not** treat hand-rolled parity with MSVC as a maintained product goal.

**MSVC++ parity “from scratch” is not this repo’s hand-written emitter.** Treat **“the builder”** as the **real toolchain** — **`cl.exe`** + **`link.exe`** + MSVC **CRT** + **PDB** / **`mspdb`** (and your **CI** that runs them). **§7.1–§7.4** list PE artifacts that **that builder** produces; **tri-format** `compose*` stays **minimal and documented**, not a **byte-for-byte** substitute for **`link.exe`** + **debug info**.

**Marketing / issue titles** that claim something was **“reverse engineered to be EXACT ‘Full’ MSVC++ parity”** are **misaligned** with this document: **exact** parity with a moving MSVC release (Rich, load config, debug/PDB, section layout, …) is **not** a RawrXD maintenance promise — use the **builder** (**§6** row above).

See also: **`docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md`** (scope creep / linker-replacement claims).

### 7.0 Hand-rolled emitter vs. MSVC “big pipeline” (why `link.exe` ≠ a PE byte scribe)

| Layer | MSVC / Windows toolchain | Typical in-repo “Sovereign” lab emitter |
|-------|---------------------------|----------------------------------------|
| **Front-end** | `cl.exe` parses C/C++, lowers to codegen IR, emits **COFF `.obj`**. | Hand-written ASM or tiny C strings; **no** full compiler. |
| **Optimization** | `c2`/LTCG, whole-program and `/OPT:*` behaviors live in the **builder** graph. | Static buffers; **no** LTCG or COMDAT folding. |
| **Link / resolve** | **`link.exe`** merges objects and **`.lib`** archives, resolves symbols, applies relocations, folds COMDAT, incremental link (`.ilk`), synthesizes import thunks, embeds manifests, coordinates **CRT** and security metadata. | Fixed **RVAs** for a **minimal** IDT/IAT (or a small import builder); **no** general symbol graph. |
| **Debug** | **`mspdb`** / PDB pipeline — types, lines, modules — **not** a single `RSDS` string. | Optional **CodeView stub** (e.g. `RSDS` + path) for **forensics / lab** only — **§7.4**. |

**Takeaway:** RawrXD **lab** emitters are for **bootstrap, tests, and transparency**; **production** Windows binaries still come from **`cl.exe` + `link.exe` + CRT + PDB + CI** (this section). To **inspect** what the linker eats from import libraries without replacing it, use **`tools/lib_parser.py`** (COFF **archive** member listing). For a **non-binding** check that a PE has **no base reloc directory** and **no common CRT DLL imports** (heuristic only), use **`tools/sovereign_check_no_deps.py`** — it does **not** certify binaries and does **not** replace normal toolchain validation. For a **research-only** sketch of what a full self-contained IR/linker stack would look like (explicitly **not** a product commitment), see **`docs/SOVEREIGN_TOOLCHAIN_LAB_ARCHITECTURE.md`**.

### 7.1 Rich Header (MSVC linker artifact)

The **Rich** region is an **optional**, **Microsoft-specific** block some PEs carry **between** the DOS stub and the **`PE\0\0`** signature. It is **produced by MSVC `link.exe` / the Microsoft toolchain**, XOR-masked, and commonly described as embedding **compiler/linker component IDs** (informally, “build telemetry”). It is **not** required for the Windows loader to run a typical user-mode **`.exe`** the way **PE headers, sections, and imports** are.

| Topic | Stance |
|-------|--------|
| **Need Rich in a shipping binary?** | Build with **MSVC** (or match whatever your release pipeline uses). Do not treat “Rich parity” as a RawrXD deliverable. |
| **Tri-format lab (`compose*`) / minimal emitters** | **No** commitment to emit or preserve MSVC’s Rich blob or **byte-for-byte** `link.exe` parity. |
| **“Fully product code” for Rich** | Belongs in **Microsoft’s** linker — or your **release CI** invoking it — **not** a hand-rolled reimplementation in this repo as a maintained feature. |

### 7.2 Load Configuration Directory (`IMAGE_LOAD_CONFIG_DIRECTORY64`)

The **load config** directory is referenced from optional header **data directory index 10** (`IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG`). The **`IMAGE_LOAD_CONFIG_DIRECTORY64`** structure (and its **Size** field — the structure is **versioned** and has grown across Windows releases) carries **security and runtime metadata** the loader uses when present, for example:

- **Stack security cookie** (`SecurityCookie`) and related CRT/linker integration  
- **Safe SEH** (SEH table pointer / count where applicable)  
- **Control Flow Guard (CFG)** (`GuardFlags`, guard function table pointers, etc.) when **`/guard:cf`** (or equivalent) is used  

A **minimal** `.exe` **may** omit a valid load-config entry (directory RVA/size zero); many **hardened** MSVC **Release** builds include a **non‑trivial** directory sized and filled by **`link.exe`** and the CRT.

| Topic | Stance |
|-------|--------|
| **Need CFG / full load-config parity?** | **Link** with **MSVC** and the right **flags** (`/guard:cf`, etc.), and test on target OS versions — do not expect a RawrXD lab emitter to duplicate **`link.exe`** + CRT contracts. |
| **Tri-format lab (`compose*`) / minimal emitters** | **No** commitment to emit a **complete, version-correct** `IMAGE_LOAD_CONFIG_DIRECTORY64` matching a given MSVC toolchain. |
| **“Fully product code” for load config** | Belongs in **Microsoft’s** linker, CRT, and your **CI** — **not** a maintained hand-written CFG/cookie/security replica in this repo as a universal emitter feature. |

### 7.3 Section alignment, file alignment, and byte distribution (“entropy”)

**Alignment.** In the PE optional header, **`SectionAlignment`** (virtual) and **`FileAlignment`** (on-disk) govern how **`link.exe`** places **`.text`**, **`.rdata`**, **`.data`**, etc., including **padding** between raw blocks and the **order** of sections. Values such as **`0x1000`** / **`0x200`** are typical defaults; the **exact** layout of a **Release** binary is a **toolchain and flags** artifact (including **`/ALIGN`**, merging, and optimizations).

**“Entropy” (two meanings, one policy):**

1. **Informal / RE:** Analysts sometimes report **per-section Shannon entropy** to spot compression or packing. That is a **measurement** on bytes, not something RawrXD lab emitters try to **match** to another vendor’s build.
2. **Anti-analysis (out of scope):** Features whose **purpose** is to **tune** padding or content to **evade** scanners or **mimic** “benign” entropy profiles are **rejected** — same firewall as **§6** (no bypass narrative).

| Topic | Stance |
|-------|--------|
| **Need MSVC-identical section layout and padding?** | Build and link with **MSVC** / **`link.exe`**; compare hashes in **CI** if required. |
| **Tri-format lab (`compose*`) / minimal emitters** | Use **fixed, documented** alignments for **minimal** images (see **`docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`**). **No** promise of **byte-for-byte** or **entropy-profile** parity with **`link.exe`**. |
| **“Fully product code” for alignment + padding** | **Linker + CRT** — **not** a universal hand-rolled **`link.exe`** clone in this repo. |

### 7.4 Debug directory (`IMAGE_DEBUG_DIRECTORY`) and PDB-grade metadata

The **debug** data directory is optional header **index 6** (`IMAGE_DIRECTORY_ENTRY_DEBUG`). It points at one or more **`IMAGE_DEBUG_DIRECTORY`** records (each **32 bytes** in the usual layout), each with a `Type` (e.g. **`IMAGE_DEBUG_TYPE_CODEVIEW`**), **`SizeOfData`**, and pointers into the mapped image. **CodeView** (`/Zi`, **`/DEBUG`**) typically embeds an **`RSDS`** signature (GUID + age + PDB path) and/or references a separate **`.pdb`** produced by the **compiler + PDB stack** (`mspdb`, `mspdbsrv`, etc. — **not** “a few header bytes”).

| Topic | Stance |
|-------|--------|
| **Need MSVC-identical debug directory + PDB story?** | Build with **`cl.exe`** + **`link.exe`** + **`/Zi` / `/DEBUG`** (or your pipeline’s equivalent), consume **`.pdb`** with normal tools — **not** a RawrXD lab emitter deliverable. |
| **Tri-format lab (`compose*`) / minimal emitters** | **No** commitment to emit **complete** `IMAGE_DEBUG_DIRECTORY` arrays, **CodeView** blobs, or **PDB** contents matching a given MSVC build. |
| **“Fully product code” for debug + PDB** | **Compiler + linker + PDB infrastructure** — **not** a maintained in-repo **PDB synthesizer** or **`link.exe`**-parity debug replica as a universal feature. |
