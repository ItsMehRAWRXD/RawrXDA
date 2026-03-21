# Getting started — Sovereign Lab synthesis (`.bin` → `compose*`)

This guide explains how to take **raw machine code bytes** and wrap them with the **tri-format lab** helpers in `include/rawrxd/sovereign_emit_formats.hpp`. It is **not** a substitute for `link.exe`, `lld`, or `ld64`; for production expectations and boundaries, read **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** (especially **§6**).

**Runnable samples:** **`examples/01_minimal_nop_synthesis.cpp`** (2-byte payload), **`examples/02_shellcode_payload_wrap.cpp`** (larger NOP sled or file-backed bytes). Build notes in **`examples/README.md`**.

---

## 1. What you get

| Layer | Header | Role |
|--------|--------|------|
| Intent | `rawrxd/sovereign_target_manifest.hpp` | `TargetManifest` — OS, arch, `objectFormat` (informational). |
| Layout | `rawrxd/sovereign_emit_formats.hpp` | `EmitBlueprint`, `emitBlueprintFromManifest`, **`compose*MinimalImage`**. |
| Bytes in | `rawrxd/sovereign_lab_blob_io.hpp` | `readWholeFile`, optional COFF/ELF `.text` extractors. |

The **`compose*`** functions append your **`codeBytes`** and patch headers so sizes and entry semantics stay consistent (ELF `PT_LOAD`, PE `SizeOfImage` / `.text`, Mach-O segment + `LC_MAIN`). **Lab PE/Mach-O images are minimal** (e.g. no full import table in the C++ compose path); Windows IAT smoke lives under **`src/asm/`**, **`tools/pe_emitter.asm`**, and **`cmake/RawrXD_SovereignIAT.cmake`**.

---

## 2. Bring your payload

**Option A — flat file**

```cpp
#include "rawrxd/sovereign_lab_blob_io.hpp"

auto r = rawrxd::sovereign::lab::readWholeFile("payload.bin");
if (!r) { /* r.error */ }
std::vector<std::uint8_t> codeBytes = std::move(r.data);
```

**Option B — literal test buffer**

```cpp
std::vector<std::uint8_t> codeBytes = { 0x90, 0xC3 }; // NOP; RET (x64 example only)
```

**Option C — extract `.text` from an object** (scoped; see header comments)

- `tryExtractCoffSection(bytes, ".text")` — short section names.
- `tryExtractElf64ProgBitsSection(bytes, ".text")` — ELF64 `ET_REL`, x86-64.

---

## 3. Wrap with a manifest (one path)

```cpp
#include "rawrxd/sovereign_emit_formats.hpp"
#include "rawrxd/sovereign_target_manifest.hpp"

namespace sv = rawrxd::sovereign;

sv::TargetManifest m{};
m.os = sv::TargetOs::Linux;
m.arch = sv::TargetArch::X86_64;
m.objectFormat = sv::ObjectFormat::Elf;

std::vector<std::uint8_t> image =
    rawrxd::sovereign::format::composeElf64MinimalImageFromManifest(m, codeBytes);
// Write `image` to disk for lab inspection; running it on real hardware is your responsibility.
```

Switch **`objectFormat`** / **`os`** for PE or Mach-O and call:

- `composePe64MinimalImageFromManifest`
- `composeMacho64MinimalImageFromManifest`

---

## 4. Wrap with an explicit blueprint (alternate path)

Use this when you want to set **`imageBase`**, **alignments**, or **`EmitBlueprint::arch`** (e.g. Mach-O AArch64) without going through the manifest defaults.

```cpp
namespace fmt = rawrxd::sovereign::format;

fmt::EmitBlueprint bp{};
bp.format = fmt::BinaryFormat::Elf64;
bp.arch = fmt::TargetArch::X64;
bp.imageBase = 0x400000;

std::vector<std::uint8_t> image = fmt::composeElf64MinimalImage(bp, codeBytes);
```

---

## 5. Platform realities (short)

| Format | Lab compose | Typical limitation |
|--------|-------------|---------------------|
| **ELF64** | Flat ET_EXEC-style blob | No dynamic loader metadata; use a real linker for libc/PIE workflows. |
| **PE64** | Minimal section layout; entry RVA **0x1000** | No IAT in this compose path; use the **MASM/NASM + link** path for API imports. |
| **Mach-O 64** | `LC_SEGMENT_64` + `LC_MAIN`; code at file offset **0x1000** | **Not codesigned**; real macOS may still refuse unsigned binaries. |

---

## 6. Verify in CI / locally

Build and run (when your CMake tree includes `tests/`):

- `test_sovereign_compose_lab` — contracts + multi-page checks (`testTriFormatEntryContract`).
- `sovereign_header_tests` — magics and patched size fields.

---

## 7. Related documents

| Doc | Purpose |
|-----|---------|
| `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md` | Safe architecture + API table + lab limits. |
| `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` | What “production” means; **§6** — work the repo does **not** take on. |
| `docs/SOVEREIGN_MASTER_TEMPLATE_v224.md` | Windows PE + IAT narrative (honest limits). |

---

## 8. Checklist before you ship anything serious

- [ ] Prefer **clang/MSVC + linker** for real executables.
- [ ] Do not rely on lab compose for **signed** store binaries or **full imports** on PE.
- [ ] Do not add **host scripts** that rewrite IDE `settings.json` or disable platform security — out of scope (**§3 / §6** in the production scope doc).
