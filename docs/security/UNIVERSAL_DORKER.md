# Universal Dorker — LDOAGTIAC, XOR, Hotpatch, Result Reversal

## Architecture

**C++ Universal Dorker engine with x64 MASM call wrappers.** All logic (XOR obfuscation, hotpatch, secure query building, result reversal, dork generation) lives in C++ (`RawrXD_Universal_Dorker.cpp`). The MASM file (`Ship/RawrXD_Universal_Dorker.asm`) provides thin x64 wrappers that call the C API for IDE integration — not a pure x64 MASM engine.

## Core concept: LDOAGTIAC

**Logic + Data = Logic Data Only Allows Genuine Transactions In All Cases**

- Strict separation of SQL **logic** (hardcoded templates) from **data** (bound parameters only).
- Prevents injection by treating all user input as data; structure is fixed in code.
- Parameterized queries + table/column whitelist validation.

## Components

| Feature | Implementation | Purpose |
| ------- | -------------- | ------- |
| XOR obfuscation | `XorObfuscator` (C++/Java) + MASM | Evasion of signature detection |
| Parameterized queries | `ParameterizedQueryEngine` | SQL injection prevention |
| URL hotpatching | `UrlHotpatchEngine` | Test variant discovery |
| Whitelist validation | Table/column validation | Prevents structural injection |
| LIMIT 1 enforcement | Singular query builder | Prevents plural data leakage |
| Result reversal | `SearchResultReverser` | Base64+XOR decode, RFC 3986 URL decode, signature-based vulnerability detection, severity scoring |
| URL encode/decode | `UrlCodec` | RFC 3986 encode and decode |
| 8 scan categories | `DorkCategory`, `GetDorksByCategory` | SQLi, XSS, LFI, RCE, Info, Admin, Backup, Other |

## C++ / C API

- **Header:** `src/security/RawrXD_Universal_Dorker.h`
- **Implementation:** `src/security/RawrXD_Universal_Dorker.cpp`

Classes: `XorObfuscator`, `ParameterizedQueryEngine`, `UniversalPhpDorker`, `UrlHotpatchEngine`, `UrlCodec`, `SearchResultReverser`.

C API (MASM-callable): `UniversalDorker_XorObfuscate`, `UniversalDorker_ApplyHotpatch`, `UniversalDorker_BuildSecureQuery`, `UniversalDorker_GenerateUniversalDorks`, `UniversalDorker_GetCategoryCount`, `UniversalDorker_GetDorksByCategory`, `UniversalDorker_UrlEncode`, `UniversalDorker_AnalyzeResult`, `UniversalDorker_IDE_Command_UniversalDorkScan`.

## MASM x64

- **File:** `Ship/RawrXD_Universal_Dorker.asm`
- **Build:** `ml64 /c Ship\RawrXD_Universal_Dorker.asm` — link with `RawrXD_Universal_Dorker.obj`

Procedures: `Masm_XorObfuscate`, `Masm_ApplyHotpatch`, `Masm_BuildSecureQuery`, `Masm_GenerateUniversalDorks`, `Masm_AnalyzeResult`, `IDE_Command_UniversalDorkScan`.

## Java examples

Located in `docs/security/examples/java/`:

| File | Purpose |
| ---- | ------- |
| `RawrXD_SecureQueryProcessor.java` | Full LDOAGTIAC: 8-table whitelist, query timeouts, SHA-256 audit, sensitive masking, cart ID parser, `fetchByCartId(conn, "onecart:customers:67890")` |
| `CartQueryProcessor.java` | Minimal reference: secure handling of `cartid=onecart:custblnamehttp` via parameterized queries |
| `XorObfuscator.java` | XOR encode/decode for dork strings |
| `UrlHotpatcher.java` | URL hotpatching (`_test`, `_bak`, `[t]`, `[d]`) |
| `UniversalPhpDorker.java` | 25+ PHP dork patterns, optional obfuscation |
| `SearchResultReverser.java` | Result decoding and vulnerability detection |

## Usage examples

**MASM:**

```asm
invoke Masm_XorObfuscate, addr szDork, addr szEncrypted, 256, 5Ah
invoke Masm_ApplyHotpatch, addr szUrl, addr szMarker, addr szResult, 512
invoke IDE_Command_UniversalDorkScan
```

**C++:**

```cpp
#include "security/RawrXD_Universal_Dorker.h"
auto dorks = RawrXD::Security::UniversalPhpDorker::GenerateUniversalDorks(true);
auto result = RawrXD::Security::SearchResultReverser::AnalyzeResult(url, responseBody);
```

**Java:**

```java
CartQueryProcessor processor = new CartQueryProcessor();
processor.processOneCartParameter(conn, "onecart:custblnamehttp");
```

## IDE integration

All components integrate with RawrXD IDE Security menu. Output can go to the Security Dashboard with severity scoring and JSON/XML/CSV export (see [DORK_SCANNER_USAGE.md](DORK_SCANNER_USAGE.md)).

## See also

- [DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md](DORKS_VULN_RESEARCH_AND_WAF_REFERENCE.md) — dork anatomy, checkout/e-commerce methodology, vuln categories, WAF bypass concepts, EVC template, self-expanding crawler architecture. Authorized research only.
