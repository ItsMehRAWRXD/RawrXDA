# Bayesian-lite hypotheses (E09)

**Done when:** Repeated error **signatures** update a Beta prior; UI can show top-k “likely fixes” without hiding the raw log.

## Model

- Key: normalized **`errorSignature`** (e.g. hash of first line + exit code + tool name).
- Prior: Beta(α, β) per signature — **success** = fix verified / task completed; **failure** = same signature reappears after attempted fix.
- **Update:** α += 1 on success after fix; β += 1 on recurrence.

## Storage

- Electron: `knowledge_store.js` field **`betaBySignature`**: `{ "sig": { "a": 1, "b": 1 } }`.
- Mean score = `a / (a + b)` for ranking.

## UI (future)

- Agent panel or diagnostics: “Top hypotheses for this signature” with confidence bar.

## Ethics

- Rankings are **assistive**; never auto-apply destructive actions without existing approval gates.
