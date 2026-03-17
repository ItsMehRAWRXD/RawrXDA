# Agentic Policy & State — Schema and Store

**Date:** 2026-03-06  
**Scope:** Persistence for `rawrxd_agentic_get_policy` / `rawrxd_agentic_set_policy` and `rawrxd_agentic_get_state` / `rawrxd_agentic_set_state` in `src/ui/agentic_bridge.cpp`.

---

## 1. Schema

### 1.1 Policy block

- **Logical size:** up to 4096 bytes (caller-facing buffer).
- **On-disk format:** length-prefixed blob:
  - `length` (4 bytes, little-endian uint32_t): number of payload bytes (≤ 4096).
  - `payload`: exactly `length` bytes.
- **Semantics:** Opaque to the store. Router/kernel may use it for agent policy flags, limits, or config.

### 1.2 State block

- **Logical size:** up to 8192 bytes (caller-facing buffer).
- **On-disk format:** length-prefixed blob:
  - `length` (4 bytes, little-endian uint32_t): number of payload bytes (≤ 8192).
  - `payload`: exactly `length` bytes.
- **Semantics:** Opaque to the store. Orchestrator may use it for state snapshots, session data, or checkpoints.

### 1.3 Validation

- If `length` read from file is 0 or > max (4096 / 8192), treat as “no valid data” and return zeroed buffer.
- On write, store `min(provided_size, max)` so we never write more than the schema allows.

---

## 2. Store

### 2.1 Location

- **Base directory:** `PathResolver::getConfigPath()`  
  - Windows: `%APPDATA%\RawrXD`  
  - Fallback: `~/.config/RawrXD`
- **Files:**
  - Policy: `{config}/agentic_policy.bin`
  - State: `{config}/agentic_state.bin`

### 2.2 Behaviour

- **Get:** Read file; if missing or invalid, return zeroed buffer of the appropriate size (4096 / 8192).
- **Set:** Ensure config directory exists; write length (4 bytes) then payload; truncate file so no trailing garbage.
- **Thread safety:** One mutex for policy I/O, one for state I/O (or a single mutex for both). All get/set operations are serialized per blob.

### 2.3 Implementation

- Implemented inside `src/ui/agentic_bridge.cpp`.
- Uses `PathResolver::getConfigPath()`, `PathResolver::ensurePathExists()`, and `<fstream>` for read/write.
- No SQLite dependency; file-based only.

---

## 3. C API (unchanged)

```c
void* rawrxd_agentic_get_policy(void);   // Returns pointer to 4096-byte buffer (static); load from store or zeroed.
void  rawrxd_agentic_set_policy(void*);  // Persist up to 4096 bytes from pointer to agentic_policy.bin.

void* rawrxd_agentic_get_state(void);    // Returns pointer to 8192-byte buffer (static); load from store or zeroed.
void  rawrxd_agentic_set_state(void*);   // Persist up to 8192 bytes from pointer to agentic_state.bin.
```

Callers must not assume the returned pointer remains valid after a subsequent call to the same getter (same thread or another); the implementation may reuse a single static buffer.
