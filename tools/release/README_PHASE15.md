# Phase 15 Production Hardening

This workflow produces signed release artifacts, a WiX MSI, and an update manifest for `v14.6.0`.

## Prerequisites

- VS2022 build environment (for `ninja` builds)
- WiX v4 CLI (`wix`) in `PATH` for MSI packaging
- `signtool.exe` in `PATH` for EV code-signing
- EV certificate available in Windows cert store (thumbprint required)

## Quick Start

```powershell
pwsh -File tools/release/Invoke-Phase15Hardening.ps1 `
  -Version 14.6.0 `
  -Channel stable `
  -BuildDir build_replacement_check_ssot `
  -BaseUrl https://releases.rawrxd.dev/v14.6.0 `
  -CertThumbprint "<YOUR_CERT_THUMBPRINT>"
```

## Outputs

- `release_out/v14.6.0/staging/RawrXD-Win32IDE.exe`
- `release_out/v14.6.0/staging/RawrXD_Gold.exe`
- `release_out/v14.6.0/RawrXD-v14.6.0-win64.msi` (if WiX available)
- `release_out/v14.6.0/update-manifest.json`

## Notes

- Use `-SkipSign` for dry runs.
- Use `-SkipMsi` if WiX is not installed.
- Manifest entries include SHA-256 for updater integrity checks.
