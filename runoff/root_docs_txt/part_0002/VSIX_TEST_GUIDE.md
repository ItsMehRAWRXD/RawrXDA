# VSIX Loader Test Guide — Amazon Q & GitHub Copilot Extensions

## Overview

This guide covers testing the VSIX loader with Amazon Q and GitHub Copilot extensions in the RawrXD IDE.

---

## Automated Test Script (Agentic)

```powershell
# Run validation and launch IDE
.\scripts\Test-VSIXInstall.ps1 -LaunchIDE

# Download .vsix from VS Code Marketplace and launch IDE
.\scripts\Test-VSIXInstall.ps1 -DownloadCopilot -LaunchIDE
.\scripts\Test-VSIXInstall.ps1 -DownloadAmazonQ -LaunchIDE

# Create minimal test .vsix for smoke test
.\scripts\Test-VSIXInstall.ps1 -CreateTestVsix

# Specify VSIX path to validate
.\scripts\Test-VSIXInstall.ps1 -VsixPath "C:\path\to\github.copilot-1.x.x.vsix" -LaunchIDE
```

- `-DownloadCopilot`: Downloads GitHub Copilot `.vsix` from the VS Code Marketplace (agentic).
- `-DownloadAmazonQ`: Downloads Amazon Q `.vsix` from the VS Code Marketplace (agentic).

---

## Prerequisites

1. **Environment variable** (for unsigned VSIX packages):

   ```powershell
   $env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"
   ```

   Most VS Code extensions (including Amazon Q and GitHub Copilot) are unsigned, so this is usually required.

2. **Extension files** — Obtain `.vsix` packages for:
   - **GitHub Copilot**: Download from [VS Code Marketplace](https://marketplace.visualstudio.com/items?itemName=GitHub.copilot) → "Download Extension" link
   - **Amazon Q**: Download from [VS Code Marketplace](https://marketplace.visualstudio.com/items?itemName=AmazonWebServices.amazon-q-vscode) → "Download Extension" link

   Or use `vsce` CLI:
   ```powershell
   npx @vscode/vsce package --no-dependencies -o amazon-q.vsix
   ```

---

## Installation Steps

### Option 1: Extensions Panel (UI)

1. Launch RawrXD IDE: `build_ide\bin\RawrXD-Win32IDE.exe`
2. Activity Bar → **Exts** (Extensions view)
3. Click **"Install .vsix..."**
4. Browse to your `.vsix` file (e.g. `github.copilot-1.x.x.vsix`)
5. Open → Installation runs; success/failure is shown in a dialog

### Option 2: Terminal Command

1. In the IDE, open the integrated terminal (View > Terminal or Ctrl+`).
2. Run:
   ```
   /install C:\path\to\extension.vsix
   ```
3. Check the Output panel for progress and results.

---

## Install Location

Extensions are installed under:

- **`%APPDATA%\RawrXD\extensions\<extension-name>\`**

Example: `C:\Users\<user>\AppData\Roaming\RawrXD\extensions\github.copilot-1.234.567\`

---

## Amazon Q & GitHub Copilot Notes

### Compatibility

- **RawrXD** uses a native Win32 extension host; VS Code extensions expect the VSCode extension API and Node.js runtime.
- **Amazon Q** and **GitHub Copilot** rely on:
  - VS Code Extension Host (Node.js)
  - VS Code Extension API
  - Network APIs (GitHub, Amazon services)
- RawrXD’s current extension host (QuickJS / custom loader) does **not** fully implement the VS Code extension API. These extensions may:
  - Install and extract correctly
  - Fail to activate or run as intended until the extension host matches VS Code’s API

### Expected Behavior

| Step                  | Result                                                                 |
|-----------------------|------------------------------------------------------------------------|
| Install .vsix         | Package extracts to `%APPDATA%\RawrXD\extensions\<id>\`               |
| Signature check       | May warn; `RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1` allows unsigned packages |
| Activation            | Requires extension host and VS Code API support                        |
| Chat / Copilot UI     | Needs integration with RawrXD’s chat and UI panels                     |

### Validation Checklist

- [ ] `.vsix` installs without errors via "Install .vsix..."
- [ ] Files appear under `%APPDATA%\RawrXD\extensions\`
- [ ] `package.json` and `extension/` (or equivalent) exist
- [ ] `native_manifest.json` is created (converted marker)
- [ ] Output panel shows installation messages
- [ ] Extensions appear in the Extensions list (if wired)
- [ ] Extension activation / chat UI (future work)

---

## Troubleshooting

### "Package too large"

- Max size: 500 MB. Check package size.

### "Not a valid ZIP/VSIX file"

- Ensure the file is a valid ZIP (VSIX is ZIP-based). Re-download if corrupted.

### "Path traversal detected"

- The VSIX contains unsafe paths; installation is blocked for security.

### Native code (DLLs)

- Extensions with `.dll`/`.node` are checked for Authenticode signatures.
- Unsigned native code may be rejected unless `RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1` is set.
- For development, you can set this in the environment before launching the IDE.

---

## Next Steps for Full Agentic Support

1. **Extension Host**: Implement or adapt a VS Code–compatible extension host (e.g. via Node.js or a compatible runtime).
2. **API Surface**: Implement the VS Code Extension API used by Amazon Q and GitHub Copilot.
3. **Chat Panel Integration**: Connect extension chat UIs to RawrXD’s AI Chat and Agent panels.
4. **Authentication**: Integrate GitHub and Amazon Q auth flows for Copilot and Amazon Q services.
