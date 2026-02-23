# RawrXD Extension Marketplace System — Complete Implementation

## Overview

The RawrXD IDE now includes a **fully integrated VS Code Extension Marketplace** with:

- ✅ **Automatic installation** of Amazon Q, GitHub Copilot, and priority extensions
- ✅ **Extension toggle UI** with checkboxes for enable/disable
- ✅ **Model selection dropdowns** for AI extensions
- ✅ **Tool activation toggles** for fine-grained control
- ✅ **CLI plugin commands** for headless/automation workflows
- ✅ **Local and online marketplace** support
- ✅ **Full settings persistence** and IDE integration

---

## Architecture

### Components

1. **Extension Marketplace Backend** (`src/marketplace/extension_marketplace.cpp`)
   - VSIX package management
   - Dependency resolution
   - Enterprise policy engine
   - Local/offline cache

2. **Auto-Installer** (`src/marketplace/extension_auto_installer.cpp`)
   - Priority extension auto-install on first run
   - VS Code Marketplace API integration
   - Progress tracking and callbacks
   - Batch installation support

3. **Win32 GUI Toggle Panel** (`src/win32app/Win32IDE_ExtensionToggles.cpp`)
   - Visual toggle interface with checkboxes
   - Model selection dropdowns (GPT-4, Claude, etc.)
   - Tool activation checkboxes
   - Real-time settings persistence

4. **VS Code Marketplace API** (`src/win32app/VSCodeMarketplaceAPI.cpp`)
   - Query marketplace.visualstudio.com
   - Search, browse, and download .vsix files
   - WinHTTP-based (no external dependencies)

5. **VSIX Installer** (`src/win32app/VSIXInstaller.hpp`)
   - Secure VSIX extraction with signature verification
   - Authenticode signature checks
   - Path traversal protection
   - DLL signature enforcement

6. **CLI Plugin Commands** (`src/cli/cli_extension_commands.cpp`)
   - `!plugin` command family for headless workflows
   - Install, list, search, sync operations

---

## Usage

### 1. GUI Mode — Extension Toggle Panel

**Access:** Tools → Extensions → Toggle Panel

**Features:**
- **Checkboxes**: Enable/disable each extension
- **Model Dropdowns**: Select AI model for extensions that support it
  - Amazon Q: `amazon-q-developer-agent`, `amazon-q-code-transformation`
  - GitHub Copilot: `gpt-4-turbo`, `claude-3-opus`, `claude-3-sonnet`
  - Continue: `gpt-4`, `llama-3-70b`, `deepseek-coder-33b`, `custom-local-model`
- **Tool Toggles**: Enable specific tools (Code Completion, Security Scanning, etc.)
- **Buttons**:
  - **Install Amazon Q**: One-click Amazon Q installation
  - **Install GitHub Copilot**: One-click Copilot + Chat installation
  - **Sync Marketplace**: Download entire VS Code marketplace (5000+ extensions)
  - **Select All / Deselect All**: Bulk toggle
  - **Save Settings**: Persist configuration to `%APPDATA%\RawrXD\extension_toggles.json`

### 2. CLI Mode — Plugin Commands

When running `RawrXD.exe --cli`, you can use `!plugin` commands:

```bash
# Show help
!plugin help

# Install specific extension
!plugin install amazonwebservices.amazon-q-vscode
!plugin install GitHub.copilot
!plugin install GitHub.copilot-chat

# Install all AI extensions at once
!plugin install-ai

# Load from local .vsix file
!plugin load amazon-q-vscode-latest.vsix

# List installed extensions
!plugin list

# Search marketplace
!plugin search copilot
!plugin search python

# Sync entire marketplace (offline mode)
!plugin sync
```

### 3. Automatic First-Run Installation

On first run, RawrXD will automatically install priority extensions:
- Amazon Q
- GitHub Copilot + Chat
- C/C++ Tools
- Python

To disable auto-install, delete `%APPDATA%\RawrXD\install_state.json`

---

## Priority Extensions

### AI Extensions (Auto-Installed)

| Extension ID | Display Name | Models | Tools |
|-------------|--------------|--------|-------|
| `amazonwebservices.amazon-q-vscode` | Amazon Q | Q Developer Agent, Code Transform | Completion, Security, Test Gen |
| `GitHub.copilot` | GitHub Copilot | GPT-4, Claude 3 | Inline, Chat, Review |
| `GitHub.copilot-chat` | Copilot Chat | GPT-4, Claude 3 | Chat, Explain, Generate |
| `Continue.continue` | Continue | GPT-4, Llama 3, DeepSeek | Autocomplete, Edit, Custom |

### Language Support

| Extension ID | Display Name |
|-------------|--------------|
| `ms-vscode.cpptools` | C/C++ Tools |
| `ms-python.python` | Python |
| `rust-lang.rust-analyzer` | Rust Analyzer |
| `golang.go` | Go |

### Productivity

| Extension ID | Display Name |
|-------------|--------------|
| `eamodio.gitlens` | GitLens |
| `esbenp.prettier-vscode` | Prettier |
| `dbaeumer.vscode-eslint` | ESLint |

---

## Configuration Files

### Extension Toggles
**Path:** `%APPDATA%\RawrXD\extension_toggles.json`

```json
{
  "extensions": [
    {
      "id": "amazonwebservices.amazon-q-vscode",
      "enabled": true,
      "selectedModel": "amazon-q-developer-agent",
      "enabledTools": ["Code Completion", "Security Scanning"]
    },
    {
      "id": "GitHub.copilot",
      "enabled": true,
      "selectedModel": "gpt-4-turbo",
      "enabledTools": ["Inline Completion", "Chat Interface"]
    }
  ]
}
```

### Install State
**Path:** `%APPDATA%\RawrXD\install_state.json`

Tracks which extensions are installed and whether first-run is complete.

---

## API Integration

### Marketplace Query

```cpp
#include "VSCodeMarketplaceAPI.hpp"

std::vector<VSCodeMarketplace::MarketplaceEntry> results;
if (VSCodeMarketplace::Query("copilot", 20, 1, results)) {
    for (const auto& entry : results) {
        std::cout << entry.id << " — " << entry.displayName << "\n";
    }
}
```

### Install Extension

```cpp
#include "extension_auto_installer.hpp"

using namespace RawrXD::Extensions;

auto callback = [](const InstallProgress& progress) {
    // Handle progress updates
};

AutoInstallResult result = ExtensionAutoInstaller::instance()
    .installExtension("GitHub.copilot", callback);

if (result.success) {
    std::cout << "Installed: " << result.detail << "\n";
}
```

### Install All AI Extensions

```cpp
AutoInstallResult result = installAIExtensions(callback);
std::cout << "Installed: " << result.installedCount << "\n";
```

---

## Security

### VSIX Signature Verification

All .vsix packages undergo:
1. **ZIP structure validation** (magic bytes, size limits)
2. **Authenticode signature verification** via WinVerifyTrust
3. **Manifest presence check**
4. **Post-extraction DLL signature enforcement**
5. **Path traversal protection** (no `../` escapes)

### Dev Mode

To allow unsigned extensions:
```bash
set RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1
```

**Note:** Native DLLs inside unsigned packages will still require individual signing.

---

## Building

### CMake Integration

The marketplace system is automatically included when building RawrXD.

**Required Files:**
- `src/marketplace/extension_marketplace.{cpp,hpp}`
- `src/marketplace/extension_auto_installer.{cpp,hpp}`
- `src/win32app/Win32IDE_ExtensionToggles.cpp`
- `src/win32app/VSCodeMarketplaceAPI.{cpp,hpp}`
- `src/win32app/VSIXInstaller.hpp`
- `src/cli/cli_extension_commands.{cpp,hpp}`

**Dependencies:**
- WinHTTP (`winhttp.lib`)
- WinTrust (`wintrust.lib`)
- nlohmann/json (header-only, already included)

### Build Command

```powershell
cmake --build . --config Release --target RawrXD-Shell
```

---

## Troubleshooting

### Extension Not Found

If `!plugin install` fails with "Extension not found":
- Check extension ID spelling (case-sensitive)
- Verify internet connection
- Try searching first: `!plugin search <name>`

### Signature Verification Failed

If VSIX installation fails with signature error:
- Enable dev mode: `set RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1`
- Most marketplace extensions are unsigned (this is normal)
- Native DLLs will be checked individually

### Amazon Q / Copilot Not Working

After installation:
1. **Restart RawrXD**
2. **Authenticate**:
   - Amazon Q: Requires AWS account
   - GitHub Copilot: Requires GitHub subscription
3. **Check extension toggle**: Ensure enabled in Tools → Extensions

### CLI Plugin Command Not Recognized

- Make sure you're in CLI mode: `RawrXD.exe --cli`
- Commands must start with `!plugin` (with exclamation mark)
- Type `!plugin help` for command list

---

## Roadmap

### Phase 1 (Current)
- ✅ VS Code Marketplace integration
- ✅ Extension toggle UI
- ✅ Auto-install priority extensions
- ✅ CLI plugin commands

### Phase 2 (Future)
- ⏳ Extension hot-reload (no restart required)
- ⏳ Extension settings panel (per-extension configuration)
- ⏳ Extension update checker
- ⏳ Custom extension repository support

### Phase 3 (Future)
- ⏳ Extension development kit
- ⏳ Native RawrXD extension API
- ⏳ Extension marketplace browser UI (grid view)

---

## Support

For issues, feature requests, or questions:
- GitHub Issues: `https://github.com/RawrXD/issues`
- Documentation: `https://docs.rawrxd.dev/extensions`

---

## License

The extension marketplace system is part of RawrXD Shell and follows the same license as the main project.

**Amazon Q** and **GitHub Copilot** are trademarks of their respective owners. RawrXD provides integration but does not bundle or distribute these extensions.
