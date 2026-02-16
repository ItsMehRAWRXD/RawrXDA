# Security Subsystem — Phased Implementation

Security (Google Dork Scanner + Universal Dorker) is developed in phases. This document tracks status and next steps.

## Phase A — Core engines ✅ DONE

| Item | Status | Location |
| ---- | ------ | -------- |
| Google Dork Scanner (bug-safe SQLi) | Done | `src/security/RawrXD_GoogleDork_Scanner.{h,cpp}`, `Ship/RawrXD_DorkScanner_MASM.asm` |
| Universal Dorker (LDOAGTIAC, XOR, hotpatch, reverser) | Done | `src/security/RawrXD_Universal_Dorker.{h,cpp}`, `Ship/RawrXD_Universal_Dorker.asm` |
| Java reference examples | Done | `docs/security/examples/java/` (CartQueryProcessor, XorObfuscator, UrlHotpatcher, UniversalPhpDorker, SearchResultReverser) |
| Documentation | Done | [DORK_SCANNER_USAGE.md](DORK_SCANNER_USAGE.md), [UNIVERSAL_DORKER.md](UNIVERSAL_DORKER.md) |

**Deliverables:** C/C++ API, MASM x64 wrappers, Java examples, markdown docs. Build: Universal Dorker in `CMakeLists.txt`; Dork Scanner already present.

---

## Phase B — IDE integration (Security menu + dashboard)

| Task | Status | Notes |
| ---- | ------ | ----- |
| Security menu: "Google Dork Scan" | Pending | Call `GoogleDorkScanner::scanSingle()` / `scanFile()`, show in Problems or Security panel |
| Security menu: "Universal Dork Scan" | Pending | Wire `UniversalDorker_IDE_Command_UniversalDorkScan()` to DorkScanner + hotpatch + reverser |
| Security dashboard / panel | Pending | Show scan results, severity, JSON/CSV export entry point |
| Export from IDE (JSON, CSV) | Partial | Scanner has `exportToJson` / `exportToCsv`; UI to trigger and open file |

**Next steps:**

1. In Win32 IDE menu resource and command handler: add Security submenu with "Google Dork Scan" and "Universal Dork Scan".
2. Implement handlers that create `DorkScannerConfig`, run scan, and push results to a Security dashboard or Problems-style list.
3. Optionally add a simple Security dashboard window/tab that lists findings and offers "Export JSON/CSV".

---

## Phase C — HTTP API + REPL (Logic Phase 51) ✅ DONE

| Task | Status | Notes |
| ---- | ------ | ----- |
| GET /api/security/dork/status | Done | `HandleSecurityDorkStatusRequest()` in complete_server.cpp |
| POST /api/security/dork/scan | Done | Body: `{ "dork": "inurl:.php?id=" }` or `{ "file": "path" }`; runs scanner, returns results JSON |
| POST /api/security/dork/universal | Done | Returns universal dorks list + hotpatch markers (optional obfuscate) |
| GET /api/security/dashboard | Done | Aggregate capability summary (builtin counts, endpoints) |
| REPL: /security, /dork status | Done | main.cpp REPL handlers; /security prints dashboard summary, /dork status prints scanner + universal counts |

**Implementation:** Route block in `complete_server.cpp` (Phase 51 block); handler declarations in `complete_server.h`; REPL in `main.cpp`; help text in `main.cpp`.

---

## Phase D — Optional enhancements

| Item | Status |
| ---- | ------ |
| SAST / dependency audit integration with Security dashboard | Optional |
| Rate limiting and safety caps for scan requests | Optional (partially in scanner config) |
| XML export for Universal Dorker results | Optional |
| Configurable whitelist files for ParameterizedQueryEngine | Optional |

---

## Summary

- **Phase A:** Core engines and docs are in place; Security is ready for IDE and API integration.
- **Phase B:** IDE Security menu and dashboard — next concrete step.
- **Phase C:** Expose Security as Logic Phase 51 (HTTP API + REPL) per [LOGIC_PHASES.md](../../LOGIC_PHASES.md).
- **Phase D:** Enhancements as needed.

See also: [LOGIC_PHASES.md](../../LOGIC_PHASES.md) (Phase 51 row), [DORK_SCANNER_USAGE.md](DORK_SCANNER_USAGE.md), [UNIVERSAL_DORKER.md](UNIVERSAL_DORKER.md).
