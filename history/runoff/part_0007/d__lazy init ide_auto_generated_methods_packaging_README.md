# Packaging & Release

Use `Build_Release.ps1` at repository root to create a zip release and optionally build a Docker image.

Example:

```powershell
# Create zip only
powershell -ExecutionPolicy Bypass -File 'D:/lazy init ide/Build_Release.ps1'

# Create zip and build Docker image (requires Docker installed)
powershell -ExecutionPolicy Bypass -File 'D:/lazy init ide/Build_Release.ps1' -BuildDocker
```

Outputs:
- `D:/lazy init ide/dist/RawrXD_Release_<timestamp>.zip`
- Optional Docker image `rawrxd:release-<timestamp>` if `-BuildDocker` is used and Dockerfile is present.
