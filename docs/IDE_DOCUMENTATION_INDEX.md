# IDE Documentation Index

This folder is opened by **Help → Documentation** (F1) in the RawrXD Win32 IDE when running from the repo. Use this index to find key guides.

---

## Reverse Engineering

| Document | Description |
|----------|-------------|
| [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md) | Main RE guide (desktop copilot, customization). |
| [REVERSE_ENGINEERING_GAME_DEVELOPMENT.md](REVERSE_ENGINEERING_GAME_DEVELOPMENT.md) | Game development, post-release only: module locating, external/internal frameworks, ved/oemASMx64. |
| [REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md](REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md) | Decompilation, extension→source, x64 “subtitles,” code dump, FLIRT/FLOSS, pure x64 MASM/NASM, hot reload. |
| [REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md](REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md) | Source/text digestion, modality, beaconism, M = T + A − NIP SIMD kernel. |
| [REVERSE_ENGINEERING_PORTABLE_X64_LOADER.md](REVERSE_ENGINEERING_PORTABLE_X64_LOADER.md) | Operational requirements, Windows 11 thumb-drive console, streaming loader. |
| [REVERSE_ENGINEERING_BOOT_AND_MINIMAL_KERNEL.md](REVERSE_ENGINEERING_BOOT_AND_MINIMAL_KERNEL.md) | 512-byte boot rule (0x55AA), minimal kernel, MASM boot sector, long-mode trampoline, MBR builder. |

Architecture and suite: `src/reverse_engineering/RE_ARCHITECTURE.md`.

---

## Security

| Document | Description |
|----------|-------------|
| [security/DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md](security/DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md) | Dork anatomy, checkout/e-commerce methodology, vuln categories, EVC template, WAF bypass concepts, 0pi100-style framework, Day -2/-1/0. Authorized research only. |
| [security/DORK_SCANNER_USAGE.md](security/DORK_SCANNER_USAGE.md) | Google Dork Scanner usage and bug fixes. |
| [security/UNIVERSAL_DORKER.md](security/UNIVERSAL_DORKER.md) | Universal Dorker (LDOAGTIAC, XOR, hotpatch, result reversal). |

---

## Other

- **Ship:** Build and Qt-removal index: `Ship/DOCUMENTATION_INDEX.md`.
- **Repo root:** README, UNFINISHED_FEATURES.md, TOP_50_READINESS_GAPS.md.
