# LLM Router — Phase 8C Reference

> Task-aware routing layer on top of the Phase 8B Backend Switcher.
> Classifies prompts by intent, selects the optimal backend, and executes
> with auditable fallback.  When disabled, all calls pass through unchanged.

**File:** `src/win32app/Win32IDE_LLMRouter.cpp` (1 040 lines)
**Config:** `router.json` (auto-saved next to `session.json`)
**Depends on:** Phase 8B Backend Switcher (`Win32IDE_BackendSwitcher.cpp`)

---

## 1. Decision Model

Every prompt that enters `routeWithIntelligence()` passes through four stages:

```
prompt
  │
  ▼
┌──────────────────────────┐
│ 1. classifyTask(prompt)  │   Pattern-match → LLMTaskType
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────────────────────┐
│ 2. selectBackendForTask(task, prompt)    │
│    a. Load TaskRoutingPreference         │
│    b. Check preferred backend viability  │
│    c. Failure-adjust (demotion)          │
│    d. Compute confidence score           │
│    e. Return RoutingDecision             │
└──────────┬───────────────────────────────┘
           │
           ▼
┌──────────────────────────────────────────┐
│ 3. Execute: temporarily swap active      │
│    backend → routeInferenceRequest()     │
│    → restore original active backend     │
└──────────┬───────────────────────────────┘
           │  primary failed?
           ▼
┌──────────────────────────────────────────┐
│ 4. handleRoutingFallback()               │
│    Swap to fallback → retry inference    │
│    Log outcome regardless of result      │
└──────────────────────────────────────────┘
```

### Task Classification

`classifyTask()` uses substring pattern matching on the lowercased prompt.
The first matching category wins (no scoring — deterministic and fast).

| LLMTaskType | Example trigger words |
|---|---|
| `CodeGeneration` | "write a", "implement", "generate code", "scaffold" |
| `CodeReview` | "review this", "find bugs", "security review" |
| `CodeEdit` | "refactor", "fix this", "rewrite", "port this" |
| `Planning` | "plan", "step by step", "architecture for" |
| `ToolExecution` | "call", "invoke", "function_call", "tool_use" |
| `Research` | "summarize", "explain", "what is", "compare" |
| `Chat` | "hello", "hi", "thanks", "how are" |
| `General` | *(catch-all when nothing matches)* |

### Capability Scoring

Each backend has a static `BackendCapability` profile:

| Backend | Context | Tool Calls | Fn Calling | JSON Mode | Cost Tier | Quality |
|---|---|---|---|---|---|---|
| LocalGGUF | 4 096 | ✗ | ✗ | ✗ | 0 (free) | 40% |
| Ollama | 8 192 | ✗ | ✗ | ✓ | 0 (free) | 50% |
| OpenAI | 128 000 | ✓ | ✓ | ✓ | 2 | 80% |
| Claude | 200 000 | ✓ | ✓ | ✗ | 3 | 90% |
| Gemini | 1 000 000 | ✓ | ✓ | ✓ | 1 | 70% |

Confidence is computed as:

```
conf = 0.5 + alignment_bonus + 0.2 * qualityScore
```

Alignment bonuses:
- ToolExecution + supportsFunctionCalling → +0.2
- Research + maxContextTokens ≥ 100 000 → +0.2
- Chat + costTier = 0 → +0.1

Clamped to [0, 1].

### Failure Demotion

`getFailureAdjustedBackend()` checks a per-backend `consecutiveFailures`
counter.  When it reaches `maxFailuresBeforeSkip` (default 5):

1. Preferred → demoted to its fallback
2. If fallback is also over the limit → demoted to the globally active backend
3. On any success, the counter resets to 0

This is an **advisory demotion** — it does not disable the backend.

---

## 2. Fallback Semantics

**Rules:**

1. Fallback is **explicit** — always logged with `logWarning()`
2. Fallback is **never silent** — the Output panel shows a message
3. Fallback is **single-hop** — preferred → fallback → done (no cascading)
4. Fallback is **no-retry** — if both fail, the error bubbles up as-is
5. Fallback **preserves** the original active backend — restored after routing

If both primary and fallback fail, the Output panel receives a
`Warning`-severity message:
```
[LLMRouter] BOTH primary ('Claude') and fallback ('OpenAI') failed.
```

### Default Fallback Chains

| Task | Preferred | Fallback |
|---|---|---|
| Chat | LocalGGUF | Ollama |
| CodeGeneration | Claude | OpenAI |
| CodeReview | Claude | OpenAI |
| CodeEdit | LocalGGUF | *(none)* |
| Planning | OpenAI | Claude |
| ToolExecution | OpenAI | Gemini |
| Research | Gemini | Claude |
| General | LocalGGUF | *(none)* |

Each chain is user-configurable via `setTaskPreference()` or `router.json`.

---

## 3. Configuration

### `router.json`

Saved automatically on shutdown, loaded on `initLLMRouter()`.
Lives in the same directory as `session.json`.

```json
{
  "enabled": false,
  "taskPreferences": [
    {
      "task": "Chat",
      "preferred": "LocalGGUF",
      "fallback": "Ollama",
      "allowFallback": true,
      "maxFailuresBeforeSkip": 5
    }
  ],
  "capabilities": [
    {
      "backend": "LocalGGUF",
      "maxContextTokens": 4096,
      "supportsToolCalls": false,
      "supportsStreaming": true,
      "supportsFunctionCalling": false,
      "supportsJsonMode": false,
      "costTier": 0,
      "qualityScore": 0.4,
      "notes": "Native CPU inference, zero latency to network"
    }
  ]
}
```

### Command Palette

| IDM | Command |
|---|---|
| 5048 | Router: Show Status |
| 5049 | Router: Enable |
| 5050 | Router: Disable |
| 5051 | Router: Show Capabilities |
| 5052 | Router: Show Fallback Chains |
| 5053 | Router: Reset Stats |
| 5054 | Router: Test Classification |
| 5055 | Router: Show Last Decision |
| 5056 | Router: Show Stats |
| 5057 | Router: Save Config |

---

## 4. HTTP API

All endpoints live on the Win32IDE local server (default port 11435).

### `GET /api/router/status`

Returns the full router state: enabled flag, task preferences,
consecutive-failure counters, aggregate stats.

```json
{
  "enabled": true,
  "initialized": true,
  "taskPreferences": [ "..." ],
  "consecutiveFailures": { "LocalGGUF": 0, "Ollama": 0 },
  "stats": {
    "totalRouted": 42,
    "totalFallbacksUsed": 3,
    "totalPolicyOverrides": 1
  }
}
```

### `GET /api/router/decision`

Returns the **last** `RoutingDecision` (most recent prompt that went
through the router).

```json
{
  "classifiedTask": "CodeGeneration",
  "selectedBackend": "Claude",
  "fallbackBackend": "OpenAI",
  "confidence": 0.88,
  "reason": "Task 'CodeGeneration' → preferred backend 'Claude' (fallback: 'OpenAI')",
  "policyOverride": false,
  "fallbackUsed": false,
  "decisionEpochMs": 1738000000000,
  "primaryLatencyMs": 1234,
  "fallbackLatencyMs": -1
}
```

### `GET /api/router/capabilities`

Returns the capability profiles for all five backends (array of objects).

### `POST /api/router/route`

**Dry-run** classification + backend selection — does **not** execute
inference.  Useful for testing the classifier.

```json
// Request
{ "prompt": "Write a Python function to sort a list" }

// Response
{
  "classifiedTask": "CodeGeneration",
  "selectedBackend": "Claude",
  "fallbackBackend": "OpenAI",
  "confidence": 0.88,
  "reason": "Task 'CodeGeneration' → preferred backend 'Claude' (fallback: 'OpenAI')",
  "policyOverride": false
}
```

---

## 5. Integration Points

### With Backend Switcher (Phase 8B)

The Router **wraps** `routeInferenceRequest()` — it never replaces it.

```
routeWithIntelligence(prompt)
  ├─ router disabled?  ──→  routeInferenceRequest(prompt)   // passthrough
  └─ router enabled?
      ├─ classifyTask → selectBackendForTask
      ├─ swap activeBackend → call routeInferenceRequest
      ├─ if failed → handleRoutingFallback → routeInferenceRequest
      └─ restore original activeBackend
```

### With HTTP Endpoints

Three server handlers now call `routeWithIntelligence()` instead of
`routeInferenceRequest()`:

- `handleOllamaApiGenerate()` — `/api/generate`
- `handleOpenAIChatCompletions()` — `/v1/chat/completions`
- `handleAskEndpoint()` — `/api/ask`

When the router is disabled, this is a zero-cost passthrough.

### With React IDE

`RouterPanel.tsx` (generated by `react_ide_generator.cpp`) provides three
tabs: Overview, Capabilities, and Test Route.

---

## 6. Design Invariants

1. **Disabled by default** — `m_routerEnabled = false`.  Explicit user
   action required to enable.
2. **No backend mutation** — the Router reads configs; it never writes
   to `m_backendConfigs`.
3. **No auto-disable** — failure demotion is advisory, not permanent.
4. **Single-hop fallback** — preferred → fallback → done.  No cascading.
5. **Always auditable** — every decision is logged and retrievable via
   `/api/router/decision`.
6. **Thread-safe** — `m_routerMutex` guards all shared state.
   `m_backendMutex` guards backend swaps.

---

*Last updated: v7.6.0-stable*
