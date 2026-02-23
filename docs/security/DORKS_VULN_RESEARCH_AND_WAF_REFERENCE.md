# Security — Dorks, Vulnerability Research & WAF Reference

**Scope:** Authorized security research, bug bounty, and defensive auditing only. This document is a **methodology and reference** for Google dork anatomy, checkout/e-commerce analysis, stack-agnostic vuln research, vulnerability categorization, exploit validation templates, WAF bypass concepts, and a self-expanding crawler/framework architecture. It does not provide step-by-step exploit code or target specific live systems.

---

## 1. Google Dork Anatomy

**inurl: \*.php?\*#** — typical components:

| Component | Meaning |
|-----------|--------|
| `inurl:` | Restrict results to URLs containing the given text |
| `*.php` | Dynamic server-side scripts (PHP) |
| `?` | Start of query string (parameters to server) |
| `*` | Wildcard (any sequence) |
| `#` | Fragment identifier (subsection of page) |

**Use cases (authorized only):** Vulnerability research (e.g. finding dynamic pages for controlled testing), SEO/auditing (indexed dynamic pages), information gathering (exposed parameters). **Caution:** Searching is legal; testing or exploiting sites without authorization is not. Use tools like sqlmap only on systems you own or have explicit permission to test.

---

## 2. Checkout / E-Commerce Dorks (Reference)

Common patterns used in **authorized** recon:

- Exposed logs/DB: `filetype:log inurl:checkout` or `filetype:sql "checkout"`
- Admin/checkout: `intitle:"checkout" inurl:admin`
- Parameterized checkout (SQLi testing): `inurl:checkout.php?id=`
- Sensitive docs: `site:target-site.com filetype:pdf "checkout"`

**Protection (defensive):** robots.txt to limit indexing of checkout paths; proper access control and Private Network Access where applicable.

---

## 3. Reverse Engineering Custom Checkout Pages (Methodology)

For **your own** or **authorized** custom checkouts:

- **Front-end:** DOM inspection (data-testid, class names), Sources tab (bundled JS, validation, payment integration, code splitting).
- **Network:** XHR/Fetch during form submit; identify BFF/API endpoints (address, coupon, tax); note parameters and whether sensitive data is sent inappropriately.
- **Workflow:** Funnel (one-page vs multi-step); event tracking; business-logic tests (e.g. OWASP Juice Shop in a lab) for broken logic, parameter manipulation.
- **Tools:** Burp Suite / OWASP ZAP for interception; server-side validation and session handling audits.

---

## 4. Stack-Agnostic Vulnerability Research (No Specific URL)

**Fingerprinting:** Wappalyzer, BuiltWith — then dorks per stack (e.g. Shopify, Magento, custom PHP).

**Attack surface by stack:**

- **SaaS (Shopify, BigCommerce):** App/plugin leaks, GraphQL introspection, logic flaws (e.g. discount/cart).
- **Self-hosted (Magento, WooCommerce):** File upload (e.g. /pub/media), deserialization, SQLi in custom filters.

**Discovery workflow:** Local/open-source install → SAST (Semgrep, SonarQube) for sinks → DAST (Burp) on checkout flow → fuzzing. **Disclosure:** Vendor programs (e.g. HackerOne, Adobe for Magento).

---

## 5. Vulnerability Categorization (500+ Vectors Framework)

For systematic audits, map **checkout lifecycle** (Identity → Shipping → Payment → Confirmation) × **test types** (fuzzing, parameter tampering, race conditions, IDOR, etc.) × **variables** (price, SKU, name, zip, token, etc.). Categories often used in research:

- **Business logic / price (150+):** Rounding, quantity overflow, coupon stacking, gift-card logic.
- **API / GraphQL (100+):** BOLA, excessive data exposure, mass assignment.
- **Third-party (100+):** Webhook spoofing, analytics injection, shipping API manipulation.
- **Auth / session (75+):** JWT weakness, account takeover, session fixation.
- **Server-side (75+):** Insecure uploads, SSRF (e.g. import-by-URL, cloud metadata).

---

## 6. Real 0-Day Examples & Feeds (Post-Disclosure)

Zero-days are, by definition, not publicly known; once in CVE/feeds they are N-days. For **defensive** and **audit** use:

- **CISA Known Exploited Vulnerabilities (KEV):** Authoritative list of weaponized CVEs.
- **NVD:** Filter by severity and date for recent critical issues.
- **GitHub Advisory Database:** Open-source vulns and patches.
- **Project Zero (e.g. “In the Wild”):** High-quality RCAs and timelines.

Recent **disclosed** examples in e-commerce/enterprise (for context only): Adobe Commerce/Magento REST API issues, React Server Components RCE, BeyondTrust OS command injection. Use these to verify **your** stack and patch levels.

---

## 7. Metasploit: Framework vs Pro, Custom Modules

- **Framework (open source):** Public Ruby codebase; modules appear via PRs and community (e.g. near-zero-day modules for SharePoint RCE after disclosure).
- **Pro (commercial):** Adds automation (e.g. Vuln Validation Wizard, MetaModules, AV evasion, SET integration); not distributed as open source.
- **Custom modules:** `msfconsole -m ~/path/to/private-modules` or `loadpath /your/directory`; modules are plain Ruby (`.rb`).

No “reverse engineering” of proprietary Pro binaries is described here; use only authorized APIs and documented interfaces.

---

## 8. High-Yield Attack Surfaces (Categories)

Representative **categories** (not a list of unreleased vulns) used in research:

- **Cloud/infra:** Instance metadata (e.g. IMDSv2), K8s admission, Terraform supply chain.
- **AI/LLM:** Prompt injection (e.g. RAG), tool RCE, model/vector DB access.
- **E-commerce:** GraphQL introspection, race conditions, payment callback spoofing, ERP connectors.
- **Network edge:** SSL-VPN, load balancer admin, PAN-OS–style issues.
- **Web frameworks:** Server actions, JIT, classloader/deserialization, FFI.
- **OS:** TCC, CLFS, eBPF verifier.
- **Auth:** OAuth state, SAML wrapping, SCIM API exposure.

Use with SAST/DAST and vendor programs for **authorized** discovery and disclosure.

---

## 9. Data Module Creator / Self-Expanding Crawler (0pi100-Style)

**Architecture (conceptual):**

| Layer | Component | Function |
|-------|-----------|----------|
| Discovery | Subfinder / Amass | Subdomains, IP ranges |
| Fingerprint | Wappalyzer / HTTPX | Tech stack |
| Logic | Data modules | Stack-specific tests |
| Expansion | Link/JS/API extraction | Deeper crawl |

**Module structure:** ID, severity, author, list of requests (path, method, payload type). Payload types can map to categories (e.g. graphql_sqli, race_condition, id_enumeration). **Self-expansion:** Recursive discovery from JS (e.g. SecretFinder), parameter fuzzing (e.g. Arjun), cross-domain/staging pivot.

**Integration:** Nuclei (YAML templates), Burp/Caido APIs, Arjun for hidden parameters. Use only against **authorized** targets.

---

## 10. Exploit Validation Checklist (EVC) — yad=0 / Zero-Click

**yad=0:** Parameter often used to denote zero-click / zero-interaction (exploitation without user action after render/receive). **EVC-style template** (e.g. aligned with Project Zero RCA style):

1. **Vulnerability information:** CVE/ID, target component, **interaction level: Zero-Click (yad=0)**.
2. **Root cause:** Bug class, technical details, flow (where yad=0 is processed and logic fails).
3. **Exploit:** Primitive, strategy, flow (delivery → trigger → payload execution).
4. **Validation & mitigation:** Detection in logs, structural fix, temporary WAF/Cloud Armor rules.

**Use:** Fill for actual findings; submit via Google VRP or vendor program where applicable.

---

## 11. WAF Bypass Concepts (Unreleased / Logic-Gap Focus)

Bypasses often come from **parsing differences** between WAF and origin (not from known CVE signatures):

- **JSON key precedence:** Duplicate keys (e.g. `"id": 101` then `"id": "1' OR 1=1"`); WAF sees first (safe), backend may use last (unsafe). Null-byte or Unicode key variants in some parsers.
- **HTTP/2 (H2C) smuggling:** WAF sees HTTP/1.1 Upgrade; origin may open HTTP/2 stream not inspected by WAF. Tool: H2C Smuggler (authorized testing only).
- **Multipart / encoding:** Boundary with null or extra whitespace; charset mismatch (e.g. Shift-JIS vs UTF-8) so WAF and backend parse differently.
- **Origin IP exposure:** Hitting origin IP bypasses WAF (Censys/Shodan). Defensive: e.g. Cloud Armor Inbound Proxy so only WAF talks to origin.

**Official testing:** GoTestWAF (Wallarm), WAFLab (Azure). No “unreleased” exploit code here; only concept and tool references.

---

## 12. Vulnerability Lifecycle: Day -2, Day -1, Day 0

| Phase | Name | Role of framework (e.g. 0pi100) |
|-------|------|----------------------------------|
| **Day -2** | Day before -1 | Crawl & discovery: logic gaps, impedance mismatch (WAF vs origin), dead-end parameters (timing/size), dependency tree, parameter pollution. |
| **Day -1** | Zero-day | Weaponization: build bypass/exploit from discovered gap. |
| **Day 0** | Disclosure | Exploit public; CVE/patches; execution in wild. |

Use Day -2 methodology only for **authorized** recon and lab environments.

---

## 13. Where This Fits

- **Dork scanner (this repo):** [DORK_SCANNER_USAGE.md](DORK_SCANNER_USAGE.md), [UNIVERSAL_DORKER.md](UNIVERSAL_DORKER.md), [PHP_DORK_AND_SECURE_QUERY_AUDIT.md](PHP_DORK_AND_SECURE_QUERY_AUDIT.md).
- **Reverse engineering (general):** [../REVERSE_ENGINEERING_GUIDE.md](../REVERSE_ENGINEERING_GUIDE.md) and linked RE docs.

All activity must be **authorized** (own systems, bug bounty, or explicit permission). Do not use dorks or WAF bypass techniques against systems you are not permitted to test.
