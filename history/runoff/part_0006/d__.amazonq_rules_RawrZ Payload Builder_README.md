# RawrZ Payload Builder (Local UI)

Light-themed Electron UI with local-only, safe utilities for file hashing, compression, and a demo text encryption flow. Module checkboxes are placeholders (disabled) for future features.

## Requirements
- Node.js 18+
- Windows, macOS, or Linux

## Install
```powershell
npm install
```

## Run (development)
```powershell
npm run dev
```

This starts Electron and opens the light-themed UI.

## What’s Included (Local-only)
- File hash (SHA-256)
- File compress/decompress (.gz)
- Text encrypt/decrypt demo (AES-256-GCM) using in-memory data
- No network or external calls; strictly local

## Notes
- The sidebar module checkboxes are disabled placeholders by design. Dangerous capabilities are intentionally not implemented in this UI.
- Preload exposes a minimal, safe API. Main process enforces contextIsolation and no remote code.

## Folder Structure
```
RawrZ Payload Builder/
  package.json
  main.js
  preload.js
  src/
    index.html
    renderer.js
    styles.css
```

## Packaging
This scaffold is set up for dev runs. If you want packaging to an .exe, add a packager later (e.g., electron-forge or electron-builder).
