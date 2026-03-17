# RawrXD Sovereign Guide

## Scope

This guide covers installation, verification, updates, and rollback for the `v14.6.0` Zero Bloat release lane.

## Install

1. Install VC++ runtime (x64) and WebView2 Runtime if not already present.
2. Run `RawrXD-v14.6.0-win64.msi`.
3. Launch `RawrXD` from Start Menu.

## Verify Runtime

1. Open Task Manager and verify process memory target is below `500MB` under normal idle/editor usage.
2. In CLI/command palette, run status commands for:
   - `lsp.status`
   - `telemetry.dashboard`
   - `router.status`
3. Confirm persisted state files are created under:
   - `config\lsp_runtime_state.json`
   - `config\telemetry_runtime_state.json`
   - `config\file_runtime_state.json`

## Signature Verification

Use PowerShell:

```powershell
Get-AuthenticodeSignature .\RawrXD_Gold.exe
Get-AuthenticodeSignature .\RawrXD-Win32IDE.exe
Get-AuthenticodeSignature .\RawrXD-v14.6.0-win64.msi
```

Expected result: `Status = Valid`.

## Update Model

Updater consumes a signed manifest (`update-manifest.json`) containing:

- `version`
- `channel`
- artifact URLs
- SHA-256 hashes
- sizes

Recommended release sequence:

1. Upload signed binaries and MSI.
2. Generate manifest with final URLs.
3. Sign manifest.
4. Publish manifest atomically.

## Rollback

If post-release regression is detected:

1. Unpublish latest manifest.
2. Repoint channel manifest to previous known-good version.
3. Keep artifacts immutable.
4. Issue signed advisory with mitigation and ETA.

## Operational Guardrails

- Never publish unsigned binaries to release channels.
- Never overwrite existing versioned artifacts.
- Keep EV signing isolated to release host/HSM policy.
- Preserve deterministic build inputs for audit replay.
