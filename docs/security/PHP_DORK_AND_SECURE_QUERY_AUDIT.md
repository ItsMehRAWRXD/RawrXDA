# Audit: Universal PHP Dork Scanner & Secure Query System

**Purpose:** Verify that the components described in the "comprehensive Universal PHP Dork Scanner & Secure Query System" package actually exist in the repo and are not stubbed or partial. Cross-referenced with `TOP_50_READINESS_GAPS.md` (1–245).

**Audit date:** 2026-02-15.

---

## 1. Claimed vs Actual Files

| Claimed | Actual | Verdict |
|--------|--------|--------|
| `RawrXD_Universal_PHP_Dork_Engine.asm` (26KB, **pure x64 MASM**) | **Does not exist.** Repo has `Ship/RawrXD_Universal_Dorker.asm` (~77 lines) — **MASM thunks only**; all logic in C++ (`RawrXD_Universal_Dorker.cpp`). | **MISMATCH** |
| `RawrXD_SecureQueryProcessor.java` (22KB) | **Does not exist.** Repo has `docs/security/examples/java/CartQueryProcessor.java` (~48 lines) and C++ `ParameterizedQueryEngine` in `RawrXD_Universal_Dorker.cpp`. | **MISMATCH** |
| `exploit1.cpp` / `exploit2.cpp` (bug fixes) | **Do not exist.** Bug fixes live inside `src/security/RawrXD_GoogleDork_Scanner.cpp` (single file). | **MISMATCH** (same fixes, different layout) |

---

## 2. Universal PHP Dork Engine (C++ + MASM)

**What exists:**

- **`src/security/RawrXD_Universal_Dorker.cpp`** (~293 lines) — **real implementation**
  - XOR obfuscation: key `0x5A`, `XorObfuscator::encode/decode`, key schedule.
  - **25+ PHP dork patterns** in `UNIVERSAL_PHP_DORKS[]` (30 entries).
  - **UrlHotpatchEngine**: inserts marker in path (e.g. `base + file + marker + rest`). Default markers: `_test`, `_bak`, `.bak`, `[t]`, `[d]`. **Does not** insert `[t]` *before* `.php` (e.g. `index[t].php`); marker is appended after the path segment (e.g. `index.php[t]?...`).
  - **ParameterizedQueryEngine**: `validateTable` / `validateColumn` (whitelist parse) **exist** but **are never called** by `buildSecureQuery`. `buildSecureQuery` only appends `LIMIT 1`; table/column whitelist params are accepted but not used. **Partial/stub.**
  - **SearchResultReverser**: header says "Base64 + XOR decode"; implementation only does **string matching** (SQL syntax, mysql_fetch, pg_query, etc.) for verdict/severity. **No Base64 or XOR decoding** in reverser. **Partial/stub.**
- **`Ship/RawrXD_Universal_Dorker.asm`** — **real but thin**: x64 MASM wrappers calling C API (`UniversalDorker_XorObfuscate`, `ApplyHotpatch`, `BuildSecureQuery`, `GenerateUniversalDorks`, `AnalyzeResult`, `IDE_Command_UniversalDorkScan`). Not a 26KB pure MASM engine.

**Missing vs claim:**

- No **pure x64 MASM** 26KB engine; logic is C++ with MASM call wrappers.
- No **Base64 decode** in Universal Dorker (claimed "Reverses encoded result parameters").
- No **URL Encode/Decode** (RFC 3986) in Universal Dorker; only hotpatch string manipulation.
- **Google Result Parser** (extract target URLs from HTML) lives in **Google Dork Scanner** (`RawrXD_GoogleDork_Scanner.cpp`), not in Universal Dorker.
- **8 scan categories** (SQLi, XSS, LFI, …): dork list is flat; categories are not implemented as separate groups.
- Hotpatch: **`[t]` before `.php`** (e.g. `index[t].php`) not implemented; marker is appended after path segment.

---

## 3. Secure Query Processor (Java)

**What exists:**

- **`docs/security/examples/java/CartQueryProcessor.java`** — **real but small**
  - LDOAGTIAC: parameterized query `SELECT cartid, custid FROM cart WHERE cartid = ? LIMIT 1`.
  - Table/column whitelist constants and `isTableAllowed` / `isColumnAllowed` (simple `contains`).
  - **No** full "RawrXD_SecureQueryProcessor": no 8-table whitelist, no query timeouts, no SHA-256 audit logging, no sensitive masking, no `fetchByCartId(conn, "onecart:customers:67890")` API, no PreparedStatement timeout, no cart ID parser for `onecart:custblnamehttp` (only a single template).

**Missing vs claim:**

- No **RawrXD_SecureQueryProcessor.java** (22KB).
- No **8-table whitelist** (users, carts, products, orders, sessions, audit_log, system_config, app_logs) in Java.
- No **query timeouts**, **audit hashing**, **sensitive masking**, or **Cart ID parser** in a single Java class.

---

## 4. Bug Fixes for MySQL Injection Scanner

**What exists (in `RawrXD_GoogleDork_Scanner.cpp`):**

- **Bug 1 – Infinite loop:** **Implemented**
  - `extractDataSafe()` with `maxIterations` (default 100), empty-block skip, `lastBlock` duplicate check, last-chunk length check. No trailing comma from GROUP_CONCAT in collected string (commas added between blocks only).
  - `DorkScannerConfig::maxIterations`, `DEFAULT_MAX_ITERATIONS = 100`.
- **Bug 2 – Malformed URL:** **Implemented**
  - `replaceSuffix(baseUrl, suffix, value)` replaces all occurrences of `suffix` with `value` (no literal `"=null"`).
  - `BOOLEAN_PAYLOADS[]`: `"null"`, `"true"`, `"false"`, `"0"`, `"1"`.
  - `DorkScanner_TestBooleanPayloads()` uses `replaceSuffix(base, suffix, "=1")` and `"=0"` for boolean-based blind SQLi detection.

**Verdict:** Bug fixes are **real and in one place** (Google Dork Scanner), not in separate `exploit1.cpp` / `exploit2.cpp`.

---

## 5. TOP_50_READINESS_GAPS.md Cross-Check

- **TOP_50** does **not** list:
  - Universal PHP Dork Engine (MASM),
  - RawrXD_SecureQueryProcessor.java,
  - or exploit1/exploit2 bug fixes as deliverables.
- Security gaps it **does** list: S1 (SAST), S2 (Secrets), S3 (SCA), S4 (SBOM), S5 (DAST), S6 (Security dashboard), S7 (HSM/FIPS/audit). None of these are the PHP dork / secure query package.

So the package is **out of scope** of TOP_50; the audit is about **claim vs repo**, not TOP_50 compliance.

---

## 6. Summary Table

| Component | Claimed | In Repo | Status |
|----------|--------|---------|--------|
| Universal PHP Dork Engine | 26KB **pure** x64 MASM | C++ engine + ~77-line MASM thunks | **Partial** — no pure MASM engine; C++ real, some parts stubbed |
| XOR obfuscation (0x5A) | Yes | Yes (C++ + Java) | **Real** |
| Hotpatch `[t]` before `.php` | Yes | No; marker after path segment | **Missing** |
| Base64 decode in reverser | Yes | No (reverser = string match only) | **Stub** |
| URL Encode/Decode RFC 3986 | Yes | No in Universal Dorker | **Missing** |
| Google Result Parser | Yes | In Google Dork Scanner only | **Different module** |
| 8 scan categories | Yes | Flat dork list only | **Missing** |
| Secure Query Processor (Java) | 22KB, full LDOAGTIAC | CartQueryProcessor.java (~48 lines) | **Partial** — no single 22KB class |
| Table whitelist in C++ | Used in buildSecureQuery | validateTable/Column not used in buildSecureQuery | **Stub** |
| LIMIT 1 in C++ | Yes | Yes (appended in buildSecureQuery) | **Real** |
| Bug 1 (maxIterations, etc.) | exploit2.cpp | RawrXD_GoogleDork_Scanner.cpp | **Real** (different file) |
| Bug 2 (replaceAll, boolean) | exploit1.cpp | RawrXD_GoogleDork_Scanner.cpp | **Real** (different file) |

---

## 7. Recommendations

1. **Rename / document:** Describe the system as **C++ Universal Dorker + MASM call wrappers**, not "pure x64 MASM 26KB engine." Optionally add a small MASM-side XOR/hotpatch for IDE integration if pure-MASM is required.
2. **Implement or remove claims:** Either add Base64 decode and (if desired) URL encode/decode in `SearchResultReverser`, or update docs to say reverser is signature-based only.
3. **ParameterizedQueryEngine:** Have `buildSecureQuery` call `validateTable` / `validateColumn` (or parse template and reject non-whitelist table/column) so LDOAGTIAC whitelist is enforced.
4. **Hotpatch:** If "insert `[t]` before `.php`" is required, extend `UrlHotpatchEngine::applyHotpatch` to support a rule like "before extension .php" (e.g. `index[t].php`).
5. **Java:** Either add a full `RawrXD_SecureQueryProcessor.java` matching the described API (8 tables, timeouts, audit, masking, cart ID parser) or document that the reference implementation is `CartQueryProcessor.java` plus C++ `ParameterizedQueryEngine`.
6. **Exploit files:** Document that Bug 1/Bug 2 fixes live in `RawrXD_GoogleDork_Scanner.cpp`, not in exploit1/exploit2.cpp.

---

**Conclusion:** The repo contains a **working but smaller and partially stubbed** version of the described package. The Universal Dorker is C++ with MASM wrappers; secure query is a small Java example plus a C++ builder that does not enforce table/column whitelist; reverser has no Base64/XOR decode; and hotpatch does not insert `[t]` before `.php`. Bug fixes for the MySQL injection scanner are present in the Google Dork Scanner. No file named `RawrXD_Universal_PHP_Dork_Engine.asm`, `RawrXD_SecureQueryProcessor.java`, or `exploit1/exploit2.cpp` exists.

---

## 8. Post-audit remediation (2026-02-16)

| Recommendation | Status |
|----------------|--------|
| Rename/document: C++ Universal Dorker + MASM wrappers | Done — `UNIVERSAL_DORKER.md` Architecture section |
| Base64 + URL decode in SearchResultReverser | Done — `RawrXD_Universal_Dorker.cpp` |
| Enforce validateTable/validateColumn in buildSecureQuery | Done — parse template, validate tables/columns; return -3/-4 on reject |
| Hotpatch `[t]` before `.php` | Done — `UrlHotpatchEngine::applyHotpatch` when marker is `[t]` |
| Full RawrXD_SecureQueryProcessor.java | Done — `docs/security/examples/java/RawrXD_SecureQueryProcessor.java` (8 tables, timeouts, SHA-256 audit, masking, cart ID parser, fetchByCartId API) |
| Document Bug 1/Bug 2 locations | Done — `DORK_SCANNER_USAGE.md` |
| URL Encode RFC 3986 | Done — `UrlCodec::encode`, `UniversalDorker_UrlEncode` |
| 8 scan categories | Done — `DorkCategory` enum (SQLi, XSS, LFI, RCE, Info, Admin, Backup, Other), `GetDorksByCategory`, `UniversalDorker_GetDorksByCategory` |
