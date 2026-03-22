# Enterprise compliance bundle export (E15)

**Done when:** Operator can export a **single JSON** (or zip) for support: **HWID**, **tier**, **last validation**, **cache state**, **app version**.

## Electron shell (MVP)

- **IPC:** `enterprise:export-compliance-bundle` → writes `compliance-bundle-<ISO>.json` under `userData/exports/`.
- **Preload:** `exportComplianceBundle()` for a future **Settings → Enterprise** button.

## Win32 target

- **`Win32IDE_AirgappedEnterprise.cpp`** — “Export bundle…” saves same schema to user-chosen path.

## Bundle schema (version 1)

```json
{
  "version": 1,
  "generatedAt": "ISO-8601",
  "app": { "name": "BigDaddyG IDE", "version": "from package.json" },
  "workspaceFp": "optional 16-hex",
  "license": {
    "tier": "unknown|trial|enterprise",
    "lastValidationAt": null,
    "cacheState": "unknown",
    "hwid": "placeholder-until-win32-bridge"
  }
}
```

> **Note:** HWID + real tier require Win32/native license bridge; Electron MVP fills honest placeholders.
