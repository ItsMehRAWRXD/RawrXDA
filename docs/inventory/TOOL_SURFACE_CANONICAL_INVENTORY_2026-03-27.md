# Canonical Tool Surface Inventory (2026-03-27)

Purpose: Reconcile tool-count reporting differences by splitting counts into explicit surfaces.

## Verified Counts (Live Source Check)

- Shared core handlers: 36
  - Source: `src/agentic/AgentToolHandlers.cpp`
  - Count method: `ToolCallResult AgentToolHandlers::...`
- Shared registry entries: 36
  - Source: `src/agentic/ToolRegistry.cpp`
  - Count method: `RegisterHandler(...)`
- Public API methods: 18
  - Source: `src/agentic/PublicToolRegistry.cpp`
  - Count method: `ToolResult PublicToolRegistry::...`

## Canonical Definitions

- Core tool: A tool implemented in `AgentToolHandlers` and registered via `ToolRegistry::RegisterHandler`.
- Public wrapper/API method: A `PublicToolRegistry` method that invokes or wraps shared dispatch.
- Surface count: A count for one namespace only. Do not add surfaces together unless deduplicated.

## Why 36 vs 45 Happens

- `36` is the verified shared core pipeline count.
- `45` is a cross-surface/legacy reporting number used in older summaries.
- These are not directly comparable unless normalized by unique canonical tool id.

## Reporting Standard (Effective Immediately)

Always report all three values explicitly:

1. `core_tools_shared`: 36
2. `registry_entries_shared`: 36
3. `public_api_methods`: 18

If a cross-surface total is needed, it must be named `unique_tools_total` and produced from a deduplicated inventory file (not inferred from raw counts).

## Current Status

- Core zero-wrapper architecture: production-ready
- Count mismatch risk: documentation/nomenclature only
- Functional risk: none identified from this mismatch alone

## Verification Commands

```powershell
# Shared handler implementations
(Get-Content d:\rawrxd\src\agentic\AgentToolHandlers.cpp |
  Select-String -Pattern 'ToolCallResult\s+AgentToolHandlers::').Count

# Shared registry entries
(Get-Content d:\rawrxd\src\agentic\ToolRegistry.cpp |
  Select-String -Pattern 'RegisterHandler\(').Count

# Public API methods
(Get-Content d:\rawrxd\src\agentic\PublicToolRegistry.cpp |
  Select-String -Pattern 'ToolResult\s+PublicToolRegistry::').Count
```
