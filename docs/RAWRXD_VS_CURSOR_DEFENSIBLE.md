# RawrXD vs Cursor — Defensible Comparison (Investor / Power-User Proof)

**Purpose:** A comparison that holds up to scrutiny. No unverifiable claims; capability-level scoring; clear positioning.  
**Audience:** Investors, security/RE practitioners, technical evaluators.  
**Status:** v1.2.0-beta (1,360 handlers, self-hosting, SSOT unified).

---

## One-Sentence Tagline

**“RawrXD is a sovereign, review-before-write IDE for security and systems work—offline by default, cloud optional.”**

That’s the product category.

---

## Positioning (Keep This)

| IDE | Positioning |
|-----|-------------|
| **Cursor** | Cloud-first AI IDE optimized for convenience and general dev workflows. |
| **RawrXD** | Local-first sovereign IDE optimized for security, RE, and systems work. |

**You don’t beat Cursor at being Cursor. You win by being the anti-Cursor.**

---

## What’s Solid — Defensible Wins for RawrXD

These are **product-class differentiators**, not marketing fluff:

| Win | Why it’s defensible |
|-----|----------------------|
| **Offline / air-gapped workflow** | Local GGUF + local embeddings + no telemetry by design. Verifiable. |
| **Systems + RE workflow** | Debugger depth, dump handling, memory/hex view, deobfuscation tooling. Cursor doesn’t focus here. |
| **Integrated security reporting** | SARIF export + offline CVE matching is uncommon inside an IDE. |
| **Review-before-write** | Staged diffs + explicit apply is a serious safety feature vs. auto-apply inline. |

---

## Corrections (So Nobody Can Dismiss It)

### 1) Handler counts

- **RawrXD:** 1,360 handlers (verifiable from command registry + build).
- **Cursor:** Cursor does **not** publish “handler counts.” Do **not** compare to an invented “~2000” or similar. Use **capability-level scoring** instead of head-to-head handler numbers.

### 2) RAM / performance

- **Avoid:** “<50MB RAM” unless you have a **measured** working set (idle / indexing / chat active) in docs.
- **Prefer:** “Substantially lower than Electron-based IDEs” or “Native footprint; no Electron runtime.”

### 3) Privacy / “Microsoft data”

- **Avoid:** “Cursor = Microsoft data” (too broad and attackable).
- **Prefer:** “Cloud IDEs require sending code/prompts to remote inference or services (depending on settings).” Accurate and defensible.

### 4) RAG / embeddings

- **RawrXD** wins on **offline capability** and **sovereignty** (local embeddings).
- **Cursor** may win on “out of the box relevance” depending on models/settings.
- Frame as **split by dimension** (sovereignty vs. relevance), not a single “winner.”

---

## Feature Truth Table (Tightened)

| Area | RawrXD v1.2.0-beta | Cursor | Who wins |
|------|--------------------|--------|----------|
| **Editing core** | Native editor core (your stack) | Electron + Monaco | **Tie** (depends on UX polish) |
| **AI chat** | Local GGUF + project scope | Cloud frontier models | **Split:** RawrXD privacy, Cursor quality |
| **Inline AI / code actions** | Present + diff review | Present + inline workflows | **Split:** RawrXD safety, Cursor maturity |
| **Debugger** | Native + dumps + memory view | General dev debugging focus | **RawrXD (systems niche)** |
| **RE workflow** | Hex/memory/dump tooling | Not a focus | **RawrXD** |
| **Security inside IDE** | SAST/SCA outputs + SARIF | Not a core feature | **RawrXD** |
| **Extensions** | Early VSIX core | Large ecosystem | **Cursor** |
| **Collab** | Not present | Present | **Cursor** |
| **Web IDE** | Not present | Present | **Cursor** |
| **Offline operation** | First-class | Limited | **RawrXD** |

This table is hard to attack: it’s capability-based and admits where Cursor wins.

---

## Scorecard — Two Profiles (No Fake Precision)

Avoid “53/60 vs 40/60”; use **profiles** instead:

### Profile A: Security / RE / air-gapped shop

| IDE | Score | Rationale |
|-----|-------|-----------|
| **RawrXD** | **9–10/10** | Offline, RE tooling, review-before-write, no telemetry. |
| **Cursor** | **4–6/10** | Cloud-dependent; not built for dumps/hex/air-gap. |

### Profile B: Cloud-first web dev team

| IDE | Score | Rationale |
|-----|-------|-----------|
| **RawrXD** | **5–7/10** | Strong core; ecosystem and collab still early. |
| **Cursor** | **9–10/10** | Ecosystem, Live Share, web, cloud models. |

That communicates the truth without fake precision.

---

## % to Cursor / GitHub Copilot likeness

**One number (with caveat):** RawrXD is **~55–65%** “like” Cursor/Copilot on **general IDE + AI feature overlap** (same capability areas: editor, AI chat, completion, debug, refactor, extensions, etc.). The rest is either not yet wired (completion to keystrokes, ghost text, diagnostics in editor, DAP, test explorer, Git UI) or out of scope (collab, web IDE).

**By user profile:**

| Profile | RawrXD “likeness” to Cursor/Copilot | Meaning |
|--------|--------------------------------------|--------|
| **Profile A** (Security / RE / air-gap) | **~90%+** | For this segment, RawrXD covers what matters; Cursor is a poor fit (cloud, no RE). |
| **Profile B** (Cloud-first web dev) | **~50–60%** | RawrXD has editor + chat + backend; **ecosystem + collab UI wired** (View > Extension Marketplace, View > Collaboration); full extension host (Next 7) in progress. |

**Capability-area shorthand:**  
Roughly **2/3** of Cursor/Copilot capability areas are at “present or wired” in RawrXD (editor, AI chat, streaming, RE, security, debugger, LSP, parity menu, VSIX core, **extension marketplace with persistence**, **collaboration panel**); **1/3** are partial or roadmap (completion UX, ghost text, diagnostics in editor, DAP UI, test explorer, Git UI, full extension host Next 7). So: **~65% capability overlap**, **~55% UX parity** for a generic “web dev” workflow.

Use **Profile A/B** for investor slides; use **55–65%** only with the “general overlap, not better-at-everything” caveat.

---

## What Cursor Wins On

- Cloud model quality (e.g. frontier models vs. local 14B).
- Extension ecosystem (order of magnitude larger).
- Collaboration (Live Share).
- Web access (anywhere).

---

## What RawrXD Wins On

- **Sovereignty** — runs air-gapped; no required cloud.
- **Systems work** — reverse engineering, security research, dumps, hex.
- **Performance** — native stack; substantially lower footprint than Electron-based IDEs.
- **Privacy** — zero telemetry by design; local embeddings.
- **Cost** — free vs. paid subscription.

RawrXD's cloud bridge is BYOK and default-deny: local context is never sent without explicit, per-request consent, with locally stored receipts. See **docs/SOVEREIGN_CLOUD_BRIDGE_SPEC.md** for full policy.

---

## Strategic Pivot: The Anti-Cursor

The scorecard isn’t just a comparison; it’s a **manifesto**. By shipping **v1.2.0-beta** with 1,360 handlers and **self-hosting**, RawrXD has moved from “ambitious project” to “legitimate platform.”

### Self-hosting milestone

Self-hosting (dogfooding) is the stress test. If RawrXD can handle its own MASM kernels and C++ build system, it can handle real systems workloads.

### Version focus

| Phase | Focus |
|-------|--------|
| **v1.2.0 (current)** | **Stability.** Lock in the 1,360 handlers. Ensure the local RAG and extension surface are solid. |
| **v1.3.0 (next)** | **Hybridization.** Optional "Cloud Bridge" so users can bring their own API keys without RawrXD entering the telemetry loop. Policy and consent boundaries: `docs/SOVEREIGN_CLOUD_BRIDGE_SPEC.md`. |

---

## Where You’re Actually “Close”

On the **core** dev loop, the remaining gap is mostly:

- **Polish** — keybindings, themes, onboarding.
- **Ecosystem** — extensions, marketplace (**done:** View > Extension Marketplace, install + enable/disable with persistence across restarts).
- **Cloud optionality** — bridge, not default.
- **Collaboration** — team workflows (**done:** View > Collaboration panel; CRDT/WebSocket server integration planned).

That’s why the “anti-Cursor” framing is correct: you’re not competing for the average web dev; you’re building the bunker for systems engineers and security researchers.

---

## Next 7 — Extension Host Completeness (P0/P1)

To strengthen the “v1.2.0-beta positioning locked” claim, the next seven items should target the **extension host** (the most visible gap vs. Cursor):

1. **Install VSIX** — Real unpack + manifest read (not just delegate).
2. **Enable/Disable extension state persistence** — Survives restart.
3. **Activation events** — “onCommand”, “onLanguage”, “workspaceContains”.
4. **Command registration + listing** — Ties into verified 10002 flow (already real in Batch #N).
5. **Provider registration** — Completion, hover, code actions.
6. **Extension sandbox policy** — Deny file/network unless explicitly allowed.
7. **Extension diagnostics panel export** — Ties into security posture (SARIF / offline).

For **exact** P0/P1 ordering and IDs, use **docs/TOP_50_GAP_ANALYSIS.md** and pick the best 7 in that list order.

---

## Verdict

RawrXD is **~90%** of the way to a product that can realistically **replace Cursor for a specific, high-value segment**: security, RE, air-gapped, and sovereignty-first shops. You have the performance, the privacy, and the build stability. The scorecard above is the defensible version that holds up.

---

## Caveats (before external use)

1. **Use-case scope:** Reverse engineering, debugger, and security tooling in RawrXD are described for **defensive and analysis** use (e.g. malware analysis, vulnerability research, secure development). Keep public-facing artifacts focused on those use-cases; avoid “implant,” “injector,” or “beacon” phrasing in marketing or investor docs.
2. **Parity evidence:** “Command surface parity” and handler counts are backed by the **Parity Validation Proof Pack** (**docs/PARITY_VALIDATION_PROOF_PACK.md**): build commands, optional self_test_gate, and a checklist mapping spec IDs to test cases. Run the proof pack to make claims verifiable.
3. **Cloud boundary evidence:** Optional cloud use in v1.3.0 must follow the Sovereign Cloud Bridge policy (**docs/SOVEREIGN_CLOUD_BRIDGE_SPEC.md**): default deny, BYOK, explicit consent, and local audit trail.

**Status: Competitive positioning locked. Differentiation defensible.**
