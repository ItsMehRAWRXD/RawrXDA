# RawrXD Complete IDE - Next-Generation Editor

A fully functional PowerShell-based IDE with real VS Code extension integration, AI capabilities, and comprehensive development tools.

## 🚀 Features

### ✅ Fully Implemented
- **Real VS Code Marketplace Integration** - Install and use actual VSCode extensions
- **Complete GUI Framework** - 3-pane layout with file explorer, editor, and terminal/browser
- **File Operations** - Create, open, save files with full functionality
- **Extension Management** - Browse, search, install extensions from VSCode Marketplace
- **Settings Management** - Persistent configuration with JSON settings
- **Multi-language Support** - Syntax highlighting for various programming languages

### 🔮 Advanced Features
- **AI Integration** - Ollama-powered code completion and chat
- **WebView2 Browser** - Embedded browser for documentation and web apps
- **Git Integration** - Full version control functionality
- **Terminal Integration** - Built-in PowerShell terminal
- **Agent Automation** - Task automation and agentic workflows

## 📦 Installation

1. **Prerequisites**:
   - Windows 10/11
   - PowerShell 5.1 or later
   - Internet connection (for extension marketplace)

2. **Quick Start**:
   ```powershell
   # Run the complete IDE
   .\RawrXD-Complete.ps1
   
   # Or use CLI mode
   .\RawrXD-Complete.ps1 -CliMode -Command vscode-popular
   ```

## 🎯 Usage Examples

### VS Code Extension Integration
```powershell
# Browse popular extensions
.\RawrXD-Complete.ps1 -CliMode -Command vscode-popular

# Search for specific extensions
.\RawrXD-Complete.ps1 -CliMode -Command vscode-search -Prompt "python"

# Install an extension
.\RawrXD-Complete.ps1 -CliMode -Command vscode-install -Prompt "GitHub.copilot"
```

### GUI Mode Features
- **File Menu**: New, Open, Save files
- **Edit Menu**: Cut, Copy, Paste operations
- **View Menu**: Toggle file explorer, terminal, browser
- **Extensions Menu**: Browse marketplace and manage installed extensions

### AI Integration
```powershell
# Test Ollama connection
.\RawrXD-Complete.ps1 -CliMode -Command test-ollama

# Interactive AI chat
.\RawrXD-Complete.ps1 -CliMode -Command chat -Model llama2

# Analyze files with AI
.\RawrXD-Complete.ps1 -CliMode -Command analyze-file -FilePath "script.ps1"
```

## 🛠️ Development

### Project Structure
```
RawrXD-Complete.ps1      # Main IDE executable
settings.json           # Configuration settings
extensions/            # Installed extensions directory
logs/                  # Application logs
Test-RawrXD-Complete.ps1 # Comprehensive test suite
```

### Extension Development
Extensions are stored in the `extensions` directory with metadata in JSON format. Each extension can provide:
- Syntax highlighting rules
- Code completion logic
- Language-specific features
- Integration hooks

### Testing
Run the comprehensive test suite:
```powershell
.\Test-RawrXD-Complete.ps1
```

## 🔧 Configuration

### Settings File (`settings.json`)
```json
{
    "Theme": "Dark",
    "FontSize": 12,
    "AutoSave": true,
    "LineNumbers": true,
    "WordWrap": false,
    "InstalledExtensions": [
        "GitHub.copilot",
        "ms-python.python"
    ]
}
```

## 🌟 Supported Extensions

The IDE supports real VSCode extensions including:
- **GitHub Copilot** - AI code completion
- **Python** - Language support and debugging
- **C#** - .NET development tools
- **JavaScript/TypeScript** - Web development
- **GitLens** - Git history and blame
- **Prettier** - Code formatting
- **ESLint** - JavaScript linting

## 🚨 Troubleshooting

### Common Issues

1. **Extensions not loading**:
   - Check internet connection
   - Verify extension directory permissions
   - Restart the IDE

2. **GUI not starting**:
   - Ensure Windows Forms is available
   - Check PowerShell version (5.1+ required)
   - Run as administrator if needed

3. **AI features not working**:
   - Verify Ollama installation
   - Check model availability
   - Test Ollama connection separately

### Debug Mode
Enable detailed logging by checking the `logs` directory for error information.

## 📚 API Reference

### Core Functions
- `Get-VSCodeMarketplaceExtensions` - Fetch extensions from marketplace
- `Install-VSCodeExtension` - Install and activate extensions
- `Initialize-GUI` - Launch the complete IDE interface
- `Apply-ExtensionFunctionality` - Apply extension-specific features

### Extension API
Extensions can implement:
- Syntax highlighting rules
- Code completion providers
- Language server protocols
- Custom commands and menus

## 🤝 Contributing

This is a complete, production-ready IDE. Contributions welcome for:
- Additional language support
- New extension integrations
- UI/UX improvements
- Performance optimizations

## 📄 License

Open source - feel free to modify and distribute.

---

**RawrXD Complete IDE** - Bringing VSCode-level functionality to PowerShell! 🎉