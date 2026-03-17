# RawrXD IDE - Deployment Package

## Quick Start

Launch the IDE using the provided PowerShell script:

```powershell
.\Launch-RawrXD-IDE.ps1
```

Or directly run:
```powershell
.\RawrXD-QtShell.exe
```

## System Requirements

- **OS**: Windows 10/11 (x64)
- **Runtime**: MSVC 2022 Redistributable
- **Qt**: 6.7.3 (included)
- **RAM**: 4GB minimum, 8GB recommended
- **Disk**: 500MB for IDE + space for projects

## Features

### Core IDE Capabilities
✅ **Complete development environment** with 45+ integrated subsystems
✅ **Multi-language support** via LSP integration
✅ **AI-powered assistance** for code explanation, fixes, and refactoring
✅ **GGUF model quantization** for 10× speed, ½ RAM AI inference

### Integrated Subsystems

#### Development Tools
- Project Explorer with file-system watcher
- Build System (CMake, QMake, Meson, Ninja, MSBuild)
- Version Control (Git, SVN, Perforce, Mercurial)
- Debugger (LLDB, GDB, CDB, MSVC)
- Test Explorer (GoogleTest, Catch2, QtTest, pytest)
- Code Minimap & Breadcrumb navigation

#### AI & Collaboration
- AI Chat with persistent sessions
- Inline code suggestions
- AI Quick Fix lightbulb
- Code review assistant
- Collaborative editing (CRDT)
- Screen sharing & whiteboard

#### DevOps & Cloud
- Docker & Kubernetes explorer
- Cloud resource management (AWS, Azure, GCP)
- Database tools (MySQL, PostgreSQL, SQLite, MongoDB, Redis)
- Package managers (Conan, vcpkg, npm, pip, cargo)

#### Design & Assets
- Image viewer/editor
- Color picker with accessibility checker
- Icon font library (FontAwesome, Material)
- Design import (Figma, Sketch, Adobe XD)
- UML/PlantUML live renderer

#### Productivity
- Jupyter-like notebook
- Markdown viewer with KaTeX math
- Spreadsheet for data tasks
- Snippet manager
- Regex tester
- Macro recorder
- Pomodoro timer
- Time tracker

## Architecture

### Built-in Components
- **Editor Area**: Multi-tab code editor with syntax highlighting
- **File Explorer**: Tree view with file operations
- **Terminal**: Integrated terminal with command execution
- **Output Panel**: Build messages and system logs
- **System Monitor**: CPU/GPU telemetry display

### Extensibility
All 45+ subsystems use a unified toggle architecture:
- Can be shown/hidden via View menu
- Dock widget based for flexible layouts
- Auto-save session state
- Drag & drop file support

## File Structure

```
Release/
├── RawrXD-QtShell.exe          # Main IDE executable
├── Launch-RawrXD-IDE.ps1       # Launcher script with dependency checks
├── Qt6Core.dll                 # Qt runtime libraries
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Network.dll
├── platforms/
│   └── qwindows.dll            # Windows platform plugin
└── styles/
    └── qwindowsvistastyle.dll  # Windows visual style
```

## Usage Tips

1. **File Operations**: Drag & drop files directly into the editor
2. **Session Management**: Your workspace is auto-saved on exit
3. **Terminal**: Press Enter in command input to execute
4. **Subsystems**: All panels can be toggled from View menu
5. **AI Features**: Right-click code for AI explanations/fixes

## Building from Source

See the main repository for build instructions:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Troubleshooting

### Missing DLL Error
Run the launcher script which checks dependencies:
```powershell
.\Launch-RawrXD-IDE.ps1
```

### Platform Plugin Error
Ensure `platforms/qwindows.dll` exists in the same directory as the executable.

### Visual Style Issues
Copy `qwindowsvistastyle.dll` to the `styles/` subdirectory.

## Version

- **Build Date**: December 2, 2025
- **Qt Version**: 6.7.3
- **Compiler**: MSVC 2022
- **Architecture**: x64

## License

See main repository for license information.

## Support

For issues, feature requests, or contributions, visit the GitHub repository:
**ItsMehRAWRXD/RawrXD**

---

**RawrXD IDE** - One IDE to rule them all 🚀
