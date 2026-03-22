# Project knowledge store (E08)

**Done when:** Per-project state survives IDE restart: sessions, artifacts, doc refs.

## Electron MVP

- **Implementation:** `bigdaddyg-ide/electron/knowledge_store.js` — JSON blob under `%APPDATA%/…/knowledge/<workspaceFp>.json`.
- **Workspace key:** SHA-256 fingerprint of `projectRoot` (16 hex chars), same family as approval audit.

## Schema (version 1)

```json
{
  "version": 1,
  "sessions": [{ "id": "", "startedAt": "", "summary": "" }],
  "artifacts": [{ "path": "", "kind": "", "note": "" }],
  "docRefs": [{ "url": "", "title": "" }]
}
```

## Win32 target

- SQLite under `%LOCALAPPDATA%\RawrXD\knowledge\<workspace>.db` — align with existing SQLite patterns in repo.

## Related

- **E09:** `docs/BAYESIAN_LITE_HYPOTHESES.md` — ranks hypotheses using the same store.
