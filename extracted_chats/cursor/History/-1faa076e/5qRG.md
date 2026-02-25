# Google Dork Scanner — Usage & Bug Fixes

## Bug fixes (MySQL injection scanner)

**Note:** Bug 1 and Bug 2 fixes live in `RawrXD_GoogleDork_Scanner.cpp` (single file). There are no separate `exploit1.cpp` or `exploit2.cpp` files.

### Bug 1: Infinite loop in data extraction

- **Location:** Extraction loops (Exploit2-style: get info, get DBs, get tables, get columns).
- **Fix:** `extractDataSafe()` in `RawrXD_GoogleDork_Scanner.cpp`:
  - `maxIterations` cap (default 100).
  - Exit on empty response block.
  - Last-chunk detection (no trailing comma from GROUP_CONCAT).
  - Duplicate block detection to stop loops.
- **Config:** `DorkScannerConfig::maxIterations` (default 100).

### Bug 2: Malformed URL construction

- **Location:** Boolean / error payload injection (e.g. `=null`).
- **Fix:**
  - `replaceSuffix(baseUrl, suffix, value)` replaces the parameter suffix with the payload (no literal `"=null"`).
  - Boolean payloads: `null`, `true`, `false`, `0`, `1` (see `BOOLEAN_PAYLOADS`).
  - C API: `DorkScanner_TestBooleanPayloads(baseUrl, outVerdict, outSize)`.

## C++ API

```cpp
#include "security/RawrXD_GoogleDork_Scanner.h"

RawrXD::Security::DorkScannerConfig config = {};
config.threadCount = 4;
config.delayMs = 1500;
config.maxIterations = 100;  // Bug 1 safety
config.enableBoolean = 1;    // Bug 2 payloads

RawrXD::Security::GoogleDorkScanner scanner(config);
scanner.initialize();
auto targets = scanner.scanSingle("inurl:.php?id=");
scanner.exportToJson("dork_results.json");
scanner.exportToCsv("dork_results.csv");
```

## C API (MASM-callable)

```c
DorkScannerConfig config = { 0 };
config.maxIterations = 100;
void* sc = DorkScanner_Create(&config);
DorkScanner_Initialize(sc, NULL);

DorkResult results[100];
int n = DorkScanner_ScanSingle(sc, "inurl:.php?id=", results, 100);
DorkScanner_ExportToJson(sc, "results.json");
DorkScanner_Destroy(sc);
```

## MASM x64

```asm
; Build: ml64 /c Ship\RawrXD_DorkScanner_MASM.asm
; Link with RawrXD_GoogleDork_Scanner.obj

.data
  g_hScanner QWORD 0
  config     DorkScannerConfig <?>
  szDork     DB "inurl:.php?id=", 0
  results    DorkResult 100 DUP(<?>)

.code
  lea rcx, config
  call DorkScanner_Create
  mov g_hScanner, rax
  mov rcx, rax
  xor edx, edx
  call DorkScanner_Initialize

  mov rcx, g_hScanner
  lea rdx, szDork
  lea r8, results
  mov r9d, 100
  call DorkScanner_ScanSingle
  ; eax = count
```

## Built-in dorks

25+ patterns: `inurl:.php?id=`, `inurl:.asp?id=`, `filetype:sql inurl:backup`, `inurl:phpmyadmin`, `intitle:index.of .env`, etc. Use `DorkScanner_GetBuiltinDorkCount` / `DorkScanner_GetBuiltinDork(scanner, index, buf, bufSize)`.

## IDE integration

- Security menu: add "Google Dork Scan" that calls `GoogleDorkScanner::scanSingle()` or `scanFile()` and shows results in the Problems panel or Security dashboard.
- Export: JSON (rawrxd_json) and CSV from `exportToJson()` / `exportToCsv()`.

## Universal Dorker (LDOAGTIAC, XOR, hotpatch)

Extended stack for parameterized queries, XOR obfuscation, URL hotpatching, and result reversal. See [UNIVERSAL_DORKER.md](UNIVERSAL_DORKER.md) for:

- **LDOAGTIAC:** logic/data separation, whitelist validation, LIMIT 1.
- **XorObfuscator / UrlHotpatchEngine / SearchResultReverser** in C++ and MASM.
- **Java examples:** `docs/security/examples/java/` (CartQueryProcessor, XorObfuscator, UrlHotpatcher, UniversalPhpDorker, SearchResultReverser).

## Files

| File | Purpose |
| ------ | -------- |
| `src/security/RawrXD_GoogleDork_Scanner.h` | C/C++ API |
| `src/security/RawrXD_GoogleDork_Scanner.cpp` | Engine, bug-safe extraction, WinHTTP, export |
| `Ship/RawrXD_DorkScanner_MASM.asm` | x64 MASM wrappers |
| `src/security/RawrXD_Universal_Dorker.h` | Universal Dorker C/C++ API (XOR, hotpatch, parameterized query, reverser) |
| `src/security/RawrXD_Universal_Dorker.cpp` | Universal Dorker engine |
| `Ship/RawrXD_Universal_Dorker.asm` | Universal Dorker MASM x64 |

## See also

- [DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md](DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md) — dork anatomy, checkout/e-commerce research methodology, vuln categorization, EVC template, WAF bypass concepts, 0pi100-style framework, Day -2/-1/0. Authorized research only.
