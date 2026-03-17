# Failure Intelligence — Phase 6

> RawrXD-Win32IDE v7.3.0-stable

## Overview

Failure Intelligence is the **history-aware, classification-driven retry system** that
replaces blind retry loops with precision recovery.  Every failed agent response is
classified into one of **19 granular failure reasons**, matched to one of **9 typed
retry strategies**, recorded to disk, and used to proactively warn the user when the
same prompt fails again.

**Files:**

| File | Lines | Role |
|------|-------|------|
| `Win32IDE_FailureIntelligence.cpp` | 1 217 | Full implementation |
| `Win32IDE.h` | Enums + structs + declarations | `FailureReason`, `RetryStrategyType`, `RetryStrategy`, `FailureIntelligenceRecord`, `FailureReasonStats` |
| `Win32IDE_FailureDetector.cpp` | Integration glue | `executeWithFailureDetection` delegates to intelligence pipeline |
| `Win32IDE_Commands.cpp` | Commands 5023–5026 | Toggle, panel, stats, execute |
| `Win32IDE_Core.cpp` | `initFailureIntelligence()` | Called from `deferredHeavyInit` |

---

## 1  Failure Classification

### 1.1  `classifyFailureReason`

The entry point.  Dispatches on `AgentFailureType` to five sub-classifiers:

```
Refusal         → classifyRefusalReason(response)
Hallucination   → classifyHallucinationReason(response)
FormatViolation → classifyFormatReason(response, prompt)
InfiniteLoop    → classifyLoopReason(response)
QualityDegrad.  → classifyQualityReason(response)
EmptyResponse   → FailureReason::EmptyOutput  (direct map)
```

### 1.2  The 19 `FailureReason` Values

| # | Enum | Category | Detection |
|---|------|----------|-----------|
| 0 | `Unknown` | — | Fallback |
| 1 | `PolicyRefusal` | Refusal | Keyword scan: "violates my guidelines", "safety policy", … (9 patterns) |
| 2 | `TaskRefusal` | Refusal | Model says "no" without policy language |
| 3 | `CapabilityRefusal` | Refusal | "unable to", "beyond my capabilities", … (7 patterns) |
| 4 | `FabricatedAPI` | Hallucination | "import quantum\_", "#include \<turbo\_", "pip install magic\_", … (6 patterns) |
| 5 | `FabricatedFact` | Hallucination | Default hallucination — no fabricated-API match |
| 6 | `SelfContradiction` | Hallucination | Yes/no density both > 3 in < 500-char response |
| 7 | `WrongFormat` | Format | Prompt asks for JSON but response has no `{` or `[` |
| 8 | `MissingStructure` | Format | Default format failure |
| 9 | `ExcessiveVerbosity` | Format | Prompt asks "only code" but response has > 200 alpha, < 5 code chars |
| 10 | `TokenRepetition` | Loop | 50-char chunk repetition scan |
| 11 | `BlockRepetition` | Loop | Line-level frequency: max repeating line > ⅓ of total lines |
| 12 | `LowDensity` | Quality | Unique-word ratio < 0.15 |
| 13 | `FillerDominant` | Quality | > 70 % of unique words are filler words (40-word stoplist) |
| 14 | `TooShort` | Quality | Response < 50 chars or < 10 words |
| 15 | `EmptyOutput` | Direct | Response is empty string |
| 16 | `Timeout` | Direct | Inference timed out |
| 17 | `ToolError` | Direct | Tool invocation returned error |
| 18 | `ContextOverflow` | Direct | Prompt exceeds context window |

---

## 2  Retry Strategies

### 2.1  The 9 `RetryStrategyType` Values

| # | Enum | Mechanism |
|---|------|-----------|
| 0 | `None` | No retry — give up |
| 1 | `Rephrase` | Prepend/append prompt text to steer model behavior |
| 2 | `AddContext` | Inject grounding constraints (facts, API verification) |
| 3 | `ForceFormat` | Inject strict format requirements ("MUST be valid JSON") |
| 4 | `ReduceScope` | Truncate prompt to 1 000 chars + add brevity instruction |
| 5 | `AdjustTemperature` | Override `InferenceConfig.temperature` (restored after call) |
| 6 | `SplitTask` | Extract first sentence/paragraph only |
| 7 | `RetryVerbatim` | Re-send exact same prompt (transient failure recovery) |
| 8 | `ToolRetry` | Retry the failed tool call only (tool layer handles it) |

### 2.2  Reason → Strategy Mapping

| FailureReason | Strategy | maxAttempts | Temp Override | Prompt Modification |
|---------------|----------|-------------|---------------|---------------------|
| PolicyRefusal | Rephrase | 2 | — | "legitimate software development task…" prefix |
| TaskRefusal | Rephrase | 1 | — | "focus on the technical aspects only" prefix |
| CapabilityRefusal | ReduceScope | 1 | — | "break this into smaller parts" prefix |
| FabricatedAPI | AddContext | 2 | — | "only use real, verified APIs" prefix |
| FabricatedFact | AddContext | 1 | — | "only state facts you are confident about" prefix |
| SelfContradiction | AdjustTemp | 1 | 0.3 | "provide a single, consistent answer" prefix |
| WrongFormat | ForceFormat | 2 | — | "MUST be valid JSON" prefix |
| MissingStructure | ForceFormat | 1 | — | "include ALL required sections" prefix |
| ExcessiveVerbosity | ForceFormat | 1 | — | "Output ONLY code. No explanations." prefix |
| TokenRepetition | AdjustTemp | 1 | 0.5 | "under 200 words. Do not repeat." prefix |
| BlockRepetition | ReduceScope | 1 | — | "single paragraph. Stop after answering once." prefix |
| LowDensity | Rephrase | 1 | — | "specific examples and concrete details" prefix |
| FillerDominant | Rephrase | 1 | — | "every sentence should convey new information" prefix |
| TooShort | Rephrase | 2 | — | "comprehensive and detailed response" prefix |
| EmptyOutput | RetryVerbatim | 2 | — | None (transient) |
| Timeout | ReduceScope | 1 | — | "under 150 words" suffix |
| ToolError | ToolRetry | 2 | — | None (tool layer) |
| ContextOverflow | ReduceScope | 1 | — | Truncation only |

### 2.3  `applyRetryStrategy` Dispatch

Strategy types are applied in `applyRetryStrategy`:

- **Rephrase / AddContext / ForceFormat:** Prepend `promptPrefix`, append `promptSuffix`
- **ReduceScope:** Truncate to 1 000 chars first, then prepend/append
- **AdjustTemperature:** Prompt mods applied, temperature handled by caller
- **SplitTask:** Extract first sentence or first `\n`-delimited line (< 500 chars)
- **RetryVerbatim / ToolRetry:** No prompt modification

---

## 3  Execution Model: `executeWithFailureIntelligence`

```
 ┌────────────────────────────┐
 │   Compute DJB2 prompt hash │
 └────────────┬───────────────┘
              ▼
 ┌────────────────────────────┐
 │ Check history for matching │◄── getMatchingFailures(hash)
 │ previous failures          │
 └────────────┬───────────────┘
              ▼  (if matches → log WARNING with suggested strategy)
 ┌────────────────────────────┐
 │   attempt = 0              │
 └────────────┬───────────────┘
              ▼
 ┌────────────────────────────────────────┐
 │ For attempt 0..m_intelligenceMaxRetries│
 │                                        │
 │  1. If retry: apply temperature override│
 │  2. AgenticBridge::ExecuteAgentCommand  │
 │  3. Restore temperature if modified    │
 │  4. detectFailures(response, prompt)   │
 │                                        │
 │  ┌── No failures ──┐                  │
 │  │  Return success  │                  │
 │  │  Record recovery │                  │
 │  └──────────────────┘                  │
 │                                        │
 │  ┌── Failures ──────────────────┐      │
 │  │ classifyFailureReason(type)  │      │
 │  │ getRetryStrategyForReason    │      │
 │  │ recordFailureIntelligence    │      │
 │  │ Update m_failureReasonStats  │      │
 │  │                              │      │
 │  │ If last attempt → return     │      │
 │  │ If no strategy  → return     │      │
 │  │                              │      │
 │  │ currentPrompt = applyRetry…  │      │
 │  └──────────────────────────────┘      │
 └────────────────────────────────────────┘
```

**Bounded by:**
- `m_intelligenceMaxRetries` (default 2) — hard cap on retry count
- `strategy.maxAttempts` — per-reason cap (may be lower)
- Both checked before every retry

---

## 4  Prompt Hashing (DJB2)

```cpp
hash = 5381
for each char c in normalize(prompt):
    hash = hash * 33 + tolower(c)
return sprintf("%08lx", hash)
```

**Normalization:** trim leading/trailing whitespace, lowercase.
**Empty prompts:** return `"empty"`.

Used for:
- Matching current prompt against historical failures
- Stored in every `FailureIntelligenceRecord`
- Enables history-aware warnings without storing full prompts

---

## 5  Persistence

### 5.1  JSONL File

**Path:** `%APPDATA%\RawrXD\failure_intelligence.jsonl`

Each line is a self-contained JSON object:

```json
{"ts":1737654321000,"hash":"a7b3c2d1","snippet":"Write a function that…","type":0,"reason":1,"strategy":1,"attempt":0,"retryOk":false,"detail":"Policy Refusal: I cannot…","session":"ses-abc123"}
```

| Field | Type | Description |
|-------|------|-------------|
| `ts` | uint64 | Epoch milliseconds |
| `hash` | string | DJB2 prompt hash (8 hex chars) |
| `snippet` | string | First 256 chars of prompt (JSON-escaped) |
| `type` | int | `AgentFailureType` enum value |
| `reason` | int | `FailureReason` enum value |
| `strategy` | int | `RetryStrategyType` enum value |
| `attempt` | int | Which attempt this record represents |
| `retryOk` | bool | Whether this attempt's retry succeeded |
| `detail` | string | Human-readable failure detail (JSON-escaped) |
| `session` | string | Session ID for cross-session tracking |

### 5.2  Write Strategy

- **Append on record:** `recordFailureIntelligence` → append to in-memory vector → `flushFailureIntelligence`
- **Flush:** Overwrites entire file (small file, < 500 records)
- **Pruning:** `MAX_FAILURE_INTELLIGENCE_RECORDS` cap — oldest records erased first
- **Directory:** `CreateDirectoryA` on `%APPDATA%\RawrXD` (auto-create)

### 5.3  Load Strategy

`loadFailureIntelligenceHistory` (called from `initFailureIntelligence`):
1. Read `failure_intelligence.jsonl` line by line
2. Parse each line with `nlohmann::json::parse`
3. Reconstruct `FailureIntelligenceRecord` structs
4. Rebuild `m_failureReasonStats` from loaded history (occurrences, retries, successes)

---

## 6  History-Aware Suggestion UI

### 6.1  Proactive Warnings

When `executeWithFailureIntelligence` is called:
1. Compute prompt hash
2. `getMatchingFailures(prompt)` — returns all records with same hash
3. If matches exist → log `[FailureIntelligence] WARNING:` with last failure reason + suggested strategy
4. Proceed with execution (strategy applied automatically on failure)

### 6.2  `showFailureSuggestionDialog`

Win32 `MessageBoxA` (modal, `MB_YESNOCANCEL | MB_ICONWARNING`):
- Lists all previous failure reasons with counts
- Shows history (N failures, M recoveries)
- Recommends best strategy based on most recent match
- User can: **Yes** (approve retry) / **No** (proceed without) / **Cancel**

### 6.3  Failure Intelligence Panel

`showFailureIntelligencePanel` creates a top-level `WS_OVERLAPPEDWINDOW` with:

**ListView (7 columns, 280px tall):**

| # | Time | Failure | Reason | Strategy | Retry? | Session |
|---|------|---------|--------|----------|--------|---------|

- Populated newest-first, capped at 200 rows
- Full-row select, grid lines, double-buffered
- Time formatted as `YYYY-MM-DD HH:MM:SS` from epoch ms

**Detail Pane (EDIT, read-only, 140px tall):**
- Shows prompt snippet, failure detail, and hash for selected record

---

## 7  Statistics

### 7.1  `getFailureIntelligenceStatsString`

Outputs to the Output pane:

```
=== Failure Intelligence Statistics ===
Intelligence Enabled:   Yes
Max Retries:            2
Total Records:          47
---
Per-Reason Breakdown:
  Policy Refusal: 12 occurrence(s), 8 recovered
  Fabricated API: 5 occurrence(s), 3 recovered
  Wrong Format: 9 occurrence(s), 7 recovered
  ...
---
Unique Failed Prompts:  23

Strategy Effectiveness:
  Rephrase: 15 attempts, 10 successes (66%)
  ForceFormat: 9 attempts, 7 successes (77%)
  AddContext: 5 attempts, 3 successes (60%)
  ...
```

### 7.2  Per-Reason Stats (`FailureReasonStats`)

Tracked per `FailureReason` in `m_failureReasonStats[int]`:
- `occurrences` — total times this reason was classified
- `retriesAttempted` — retry attempts triggered
- `retriesSucceeded` — retries that produced a clean response

### 7.3  Strategy Effectiveness

Computed from full history at display time:
- Group by `RetryStrategyType`
- Count attempts vs. successes
- Display as percentage

---

## 8  Command Palette Integration

| ID | Command | Action |
|----|---------|--------|
| 5023 | `Toggle Failure Intelligence` | Flip `m_failureIntelligenceEnabled` |
| 5024 | `Show Failure Intelligence Panel` | Open/focus the ListView panel |
| 5025 | `Show Failure Intelligence Stats` | Dump stats to Output pane |
| 5026 | `Execute with Failure Intelligence` | Run current editor content through `executeWithFailureIntelligence` |

All wired in `handleToolsCommand` and registered in `buildCommandRegistry`.

---

## 9  Integration with Phase 5 (Agent History)

Every failure detection and correction emits `AgentEventType` events:

| Event | When | Metadata |
|-------|------|----------|
| `FailureDetected` | Each classified failure | `record.toMetadataJSON()` — reason, strategy, attempt, hash |
| `FailureCorrected` | Successful retry | Same metadata with `retryOk: true` |

These events flow into the Agent History ring buffer (Phase 5), enabling:
- Timeline replay of failure→correction sequences
- History search by failure reason
- Cross-session failure pattern analysis

---

## 10  Integration with Phase 7 (Adaptive Policy)

The PolicyEngine (Phase 7) reads `FailureDetected` / `FailureCorrected` events from
Agent History to compute heuristics.  When failure rates exceed thresholds, the
PolicyEngine generates `PolicySuggestion` entries that can override retry behavior,
add validation steps, or switch execution topology (chain vs. swarm).

This creates a closed feedback loop:

```
Failure Intelligence  ──records──▶  Agent History  ──feeds──▶  PolicyEngine
       ▲                                                            │
       └─────────── policy overrides (retry, timeout, validation) ──┘
```

---

*Phase 6 is frozen in v7.3.0-stable.  No new features.  Bug fixes only.*
