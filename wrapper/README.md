# RawrXD IDE — macOS / Linux Wrapper (Bootable Space)

Run the Windows RawrXD IDE on macOS and Linux via Wine or Docker (no port of the full IDE).

## Smoke test (pwsh)

A feature is **completed and useable only after its smoke test passes**. From repo root:

```pwsh
.\Test-Features-Smoke.ps1
```

Use `-SkipBuild` to only check presence of wrapper files and sources; omit it to build and run binaries. The **Wrapper** feature passes when this folder and required scripts exist.

## Quick start

- **Linux:** `./wrapper/launch-linux.sh` (requires Wine; set `RAWRXD_IDE_EXE` or copy Windows build into space).
- **macOS:** `./wrapper/launch-macos.sh`
- **Docker:** `docker compose -f wrapper/docker-compose.ide-space.yml up -d` (VNC on 5900).
- **Backend only (no Wine):** `./wrapper/run-backend-only.sh` — runs RawrEngine on Linux/macOS.

See `rawrxd-space.env.example` for options.
