# Adaptive Policy Engine — Phase 7

> RawrXD-Win32IDE v7.3.0-stable

## Overview

The Policy Engine is **"iptables for agent behavior"** — a rule-based system that
evaluates every agent event against a set of policies and returns merged actions.
Policies are created manually, imported, or **suggested automatically** by
heuristic analysis of historical agent events.

**Files:**

| File | Lines | Role |
|------|-------|------|
| `agent_policy.h` | 324 | All structs + `PolicyEngine` class declaration |
| `agent_policy.cpp` | 1 293 | Full implementation: CRUD, evaluation, heuristics, suggestions, persistence |

---

## 1  Data Model

### 1.1  `PolicyTrigger`

Defines **when** a policy fires.  All fields are optional — empty/zero means "match all".

| Field | Type | Purpose |
|-------|------|---------|
| `eventType` | `string` | Agent event type (e.g. `"agent_complete"`, `"swarm_start"`) |
| `failureReason` | `string` | Substring match against error message |
| `taskPattern` | `string` | Substring match against event description |
| `toolName` | `string` | Exact tool name match |
| `failureRateAbove` | `float` | Trigger only when failure rate exceeds threshold (requires heuristic data) |
| `minOccurrences` | `int` | Minimum events before trigger evaluates (prevents premature firing) |

**Serialization:** `toJSON()` / `fromJSON(string)` — self-contained, no external parser.

### 1.2  `PolicyAction`

Defines **what** a policy does.  All fields have "no-op" defaults (`-1`, `false`, `0`, empty).

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `maxRetries` | `int` | -1 | Override retry cap |
| `retryDelayMs` | `int` | -1 | Override delay between retries |
| `preferChainOverSwarm` | `bool` | false | Switch from parallel swarm to sequential chain |
| `reduceParallelism` | `int` | 0 | Cap concurrent agent count |
| `timeoutOverrideMs` | `int` | -1 | Override per-operation timeout |
| `confidenceThreshold` | `float` | -1.0 | Minimum confidence to accept output |
| `addValidationStep` | `bool` | false | Inject validation sub-agent before accepting output |
| `validationPrompt` | `string` | "" | Prompt for the validation sub-agent |
| `customAction` | `string` | "" | Freeform action identifier for extensibility |

### 1.3  `AgentPolicy`

A complete policy — the unit of configuration.

| Field | Type | Purpose |
|-------|------|---------|
| `id` | `string` | Unique ID (auto-generated: `pol-{hex_timestamp}-{counter}`) |
| `name` | `string` | Human-readable name |
| `description` | `string` | Detailed description |
| `version` | `int` | Auto-incremented on update |
| `trigger` | `PolicyTrigger` | When this policy fires |
| `action` | `PolicyAction` | What this policy does |
| `enabled` | `bool` | Soft toggle without deletion |
| `requiresUserApproval` | `bool` | If true, user must confirm before action applies |
| `priority` | `int` | Lower = higher priority (used for merge ordering) |
| `createdAt` | `int64_t` | Epoch ms — creation time |
| `modifiedAt` | `int64_t` | Epoch ms — last modification |
| `createdBy` | `string` | `"user"`, `"system"`, or `"import"` |
| `appliedCount` | `int` | How many times this policy matched during evaluation |

### 1.4  `PolicySuggestion`

A system-generated policy proposal awaiting user decision.

| Field | Type | Purpose |
|-------|------|---------|
| `id` | `string` | Unique suggestion ID |
| `proposedPolicy` | `AgentPolicy` | The full policy being proposed |
| `rationale` | `string` | Human-readable explanation of why this is suggested |
| `estimatedImprovement` | `float` | Predicted improvement fraction (0.0–1.0) |
| `supportingEvents` | `int` | Number of historical events supporting this suggestion |
| `affectedEventTypes` | `vector<string>` | Event types this policy would affect |
| `affectedAgentIds` | `vector<string>` | Agent IDs this policy would affect |
| `state` | `State` | `Pending` / `Accepted` / `Rejected` / `Expired` |
| `generatedAt` | `int64_t` | When the suggestion was generated |
| `decidedAt` | `int64_t` | When the user accepted/rejected |

**Lifecycle:** `Pending` → user calls `acceptSuggestion(id)` → `Accepted` (policy activated) or `rejectSuggestion(id)` → `Rejected`.

### 1.5  `PolicyHeuristic`

Computed statistics derived from Agent History events.

| Field | Type | Purpose |
|-------|------|---------|
| `key` | `string` | Heuristic key (event type or `"tool:<name>"`) |
| `totalEvents` | `int` | Total events for this key |
| `successCount` | `int` | Events with `success == true` |
| `failCount` | `int` | Events with `success == false` |
| `successRate` | `float` | `successCount / totalEvents` |
| `avgDurationMs` | `float` | Mean duration across all events |
| `p95DurationMs` | `float` | 95th percentile duration |
| `topFailureReasons` | `vector<string>` | Top 5 failure reasons, sorted by count |
| `failureReasonCounts` | `map<string,int>` | Full reason→count breakdown |

---

## 2  PolicyEngine Class

### 2.1  Construction

```cpp
PolicyEngine(const std::string& storageDir);
~PolicyEngine();
```

- `storageDir` → `%APPDATA%\RawrXD` (auto-creates via `std::filesystem::create_directories`)
- Constructor calls `load()` to restore persisted policies + suggestions
- Destructor calls `save()` to flush state

### 2.2  CRUD Operations

| Method | Signature | Behavior |
|--------|-----------|----------|
| `addPolicy` | `string addPolicy(const AgentPolicy&)` | Assigns ID if empty, sets timestamps, returns ID |
| `updatePolicy` | `bool updatePolicy(string id, const AgentPolicy&)` | Preserves ID, increments version |
| `removePolicy` | `bool removePolicy(string id)` | `std::remove_if` + erase |
| `setEnabled` | `bool setEnabled(string id, bool)` | Soft toggle |
| `getPolicy` | `const AgentPolicy* getPolicy(string id)` | Returns pointer into vector (nullptr if not found) |
| `getAllPolicies` | `vector<AgentPolicy> getAllPolicies(bool enabledOnly)` | Copy semantics |
| `policyCount` | `size_t policyCount()` | Thread-safe count |

All operations are **mutex-guarded** (`m_mutex`).

### 2.3  Evaluation

```cpp
PolicyEvalResult evaluate(const string& eventType,
                           const string& description,
                           const string& toolName,
                           const string& errorMessage);
```

**Flow:**
1. Increment `m_evalCount` (atomic)
2. For each **enabled** policy, call `matchesTrigger`:
   - Check `eventType` (exact match or empty = wildcard)
   - Check `failureReason` (substring match against `errorMessage`)
   - Check `taskPattern` (substring match against `description`)
   - Check `toolName` (exact match)
   - Check `failureRateAbove` (requires heuristic lookup — cross-mutex)
   - Check `minOccurrences` (requires heuristic lookup)
3. Sort matched policies by `priority` (ascending = higher priority first)
4. `mergePolicyActions` — first non-default value wins per field
5. Return `PolicyEvalResult`:

| Field | Type | Purpose |
|-------|------|---------|
| `hasMatch` | `bool` | Any policy matched |
| `matchedPolicies` | `vector<const AgentPolicy*>` | All matching policies (sorted) |
| `mergedAction` | `PolicyAction` | Combined action from all matches |
| `needsUserApproval` | `bool` | True if any matched policy requires approval |
| `summary` | `string` | Human-readable (e.g. "3 policies matched: X (pri=10), Y (pri=20), Z (pri=30)") |

### 2.4  Action Merging

`mergePolicyActions` iterates matched policies (priority-sorted) and applies **first-write-wins**:

```
For each field in PolicyAction:
    If the field has a non-default value AND the merged field is still at default:
        merged.field = policy.field
```

This means the **highest-priority policy** (lowest `priority` number) wins for each field.

---

## 3  Heuristic Computation

### 3.1  `computeHeuristics`

Requires a `m_historyRecorder` (Agent History — Phase 5) to be set.

**Algorithm:**
1. Read all events from Agent History
2. Group by event type → `byType`
3. Group `tool_invoke` / `tool_result` events by tool name → `byTool`
4. For each group:
   - Count success/fail
   - Compute success rate
   - Collect durations → compute avg and P95 (sorted array, index at 95%)
   - Collect failure reasons → sort by count, keep top 5
5. Atomic swap into `m_heuristics` (guarded by `m_heuristicsMutex`)

### 3.2  Query

| Method | Returns |
|--------|---------|
| `getHeuristic(key)` | `const PolicyHeuristic*` (nullptr if not found) |
| `getAllHeuristics()` | `vector<PolicyHeuristic>` (copy) |
| `heuristicsSummaryJSON()` | `{"heuristics":[...], "count": N}` |

---

## 4  Suggestion Generation

### 4.1  `generateSuggestions`

Called manually or on-demand.  First calls `computeHeuristics()` to refresh data.

**Four suggestion algorithms**, each with guard conditions:

#### Algorithm 1: High Failure Rate → Auto-Retry

| Condition | Value |
|-----------|-------|
| Trigger | `successRate < 0.7` AND `totalEvents >= 5` AND `failCount > 0` |
| Guard | No existing policy/pending suggestion covers `eventType` with `maxRetries >= 0` |
| Action | `maxRetries = 2`, `retryDelayMs = 1000` |
| Priority | 40 |
| Est. Improvement | `min(0.3, (1 - successRate) × 0.5)` |

#### Algorithm 2: Slow Operations → Timeout Extension

| Condition | Value |
|-----------|-------|
| Trigger | `p95DurationMs > 30000` AND `totalEvents >= 5` |
| Guard | No existing policy covers `eventType` with `timeoutOverrideMs >= 0` |
| Action | `timeoutOverrideMs = p95DurationMs × 1.5` |
| Priority | 60 |
| Est. Improvement | 0.1 |

#### Algorithm 3: Swarm Failures → Chain Alternative

| Condition | Value |
|-----------|-------|
| Trigger | Event key = `"swarm_complete"` AND `successRate < 0.8` AND `totalEvents >= 3` |
| Guard | No existing policy has `preferChainOverSwarm = true` |
| Action | `preferChainOverSwarm = true` |
| Priority | 45 |
| Est. Improvement | 0.15 |

#### Algorithm 4: Agent Validation Step

| Condition | Value |
|-----------|-------|
| Trigger | Event key = `"agent_complete"` AND `0.5 < successRate < 0.85` AND `totalEvents >= 10` |
| Guard | No existing policy covers `eventType` with `addValidationStep = true` |
| Action | `addValidationStep = true`, `validationPrompt = "Validate the following output…"` |
| Priority | 55 |
| Est. Improvement | 0.2 |

### 4.2  Suggestion Lifecycle

```
generateSuggestions()  →  Pending
     │
     ├── acceptSuggestion(id)  →  Accepted  →  policy auto-added to m_policies
     │
     └── rejectSuggestion(id)  →  Rejected  →  no further action
```

All suggestions (including decided ones) are retained for audit.

---

## 5  Persistence

### 5.1  Storage Files

| File | Location | Format |
|------|----------|--------|
| `policies.json` | `{storageDir}/policies.json` | JSON array of `AgentPolicy.toJSON()` |
| `suggestions.json` | `{storageDir}/suggestions.json` | JSON array of `PolicySuggestion.toJSON()` |

### 5.2  Save

`PolicyEngine::save()` — called by destructor and can be called explicitly:

1. Lock `m_mutex`
2. Write `policies.json` — JSON array, one `toJSON()` per policy
3. Write `suggestions.json` — JSON array, one `toJSON()` per suggestion

### 5.3  Load

`PolicyEngine::load()` — called by constructor:

1. Read `policies.json` → hand-parse JSON array → `AgentPolicy::fromJSON` per object
2. Read `suggestions.json` → hand-parse JSON array → `PolicySuggestion::fromJSON` per object
3. JSON parsing uses brace-depth tracking (no external parser)

### 5.4  Self-Contained JSON Serialization

All serialization is **hand-written** — no external JSON library:

- `escapeJsonStr` / `unescapeJsonStr` — handle `"`, `\`, `\n`, `\r`, `\t`
- `extractField(json, key)` — string field extraction
- `extractInt(json, key, default)` — integer extraction
- `extractFloat(json, key, default)` — float extraction
- `extractBool(json, key, default)` — boolean extraction
- `extractInt64(json, key, default)` — 64-bit integer extraction
- Nested object extraction uses brace-depth scanning

---

## 6  Export / Import

### 6.1  Export

```cpp
string exportPolicies() const;
bool   exportToFile(const string& filePath) const;
```

- Exports **enabled policies only** as a JSON object:

```json
{"version":1, "exportedAt":1737654321000, "policies":[...]}
```

### 6.2  Import

```cpp
int importPolicies(const string& json);
int importFromFile(const string& filePath);
```

- Parses `"policies":[...]` array from JSON
- Assigns **new IDs** to all imported policies (collision avoidance)
- Sets `createdBy = "import"`
- Resets `appliedCount = 0`
- Returns count of imported policies

---

## 7  Threading Model

| Resource | Guard |
|----------|-------|
| `m_policies`, `m_suggestions` | `m_mutex` (`std::mutex`) |
| `m_heuristics` | `m_heuristicsMutex` (`mutable std::mutex`) |
| `m_evalCount`, `m_matchCount` | `std::atomic<int>` |

**Cross-mutex access:** `matchesTrigger` may acquire `m_heuristicsMutex` while `m_mutex` is held (for `failureRateAbove` checks).  Lock ordering: `m_mutex` → `m_heuristicsMutex` (never reversed).

---

## 8  Statistics

```cpp
string getStatsSummary() const;
```

Returns JSON:

```json
{
  "policies": {"total": 5, "enabled": 3, "disabled": 2, "totalApplied": 47},
  "suggestions": {"total": 8, "pending": 2, "accepted": 4, "rejected": 2},
  "evaluations": 312,
  "matches": 89
}
```

---

## 9  UUID Generation

```cpp
string generateUUID() const;
```

Format: `pol-{hex_timestamp_low32}-{atomic_counter}`

- Uses `steady_clock` for monotonic timestamp
- Atomic counter prevents collisions within same millisecond
- Sufficient for local single-process use

---

## 10  Feedback Loop

The Policy Engine completes the system's closed feedback loop:

```
User Action
    │
    ▼
Agent Execution  ──events──▶  Agent History (Phase 5)
    │                              │
    ▼                              ▼
Failure Intelligence (Phase 6)     PolicyEngine.computeHeuristics()
    │                              │
    │   FailureDetected events     ▼
    └──────────────────────▶  PolicyEngine.generateSuggestions()
                                   │
                                   ▼
                             User accepts/rejects
                                   │
                                   ▼
                             Active Policies
                                   │
                                   ▼
                             PolicyEngine.evaluate()  ──▶  mergedAction
                                   │
                                   ▼
                             Applied to next Agent Execution
```

---

*Phase 7 is frozen in v7.3.0-stable.  No new features.  Bug fixes only.*
