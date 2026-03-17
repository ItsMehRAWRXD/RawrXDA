# Native IDE

A portable, self-contained C/C++ development environment that runs without installation.

## Features

### 🚀 **Zero Installation Required**

- Runs directly from USB stick or extracted folder
- No registry modifications or system dependencies
- Completely portable across Windows machines

### 🔧 **Complete Development Environment**

- Bundled GCC and Clang compilers with static libraries
- Built-in text editor with syntax highlighting
- Project management and file browser
- Integrated build system with Makefile support
- Output window with real-time compilation feedback

### 🎯 **Professional IDE Experience**

- Native Win32 application with Direct2D rendering
- Familiar Visual Studio-style interface
- Keyboard shortcuts and menu system
- Plugin architecture for extensibility

### 📦 **Startup Options**

When launching the IDE, choose from:

1. **Open Project** - Load existing .vcxproj, .sln, or Makefile projects
2. **Clone Repository** - Clone Git repositories with built-in Git support  
3. **Open Folder** - Open any folder as a workspace
4. **Create New Project** - Use built-in templates (Console App, etc.)
5. **Continue Empty** - Start with blank workspace

## Building from Source

### Prerequisites

- Windows 10 or later
- MinGW-w64 GCC compiler
- CMake 3.25 or later
- Windows SDK (for Direct2D/DirectWrite)

### Build Steps

1. **Clone Repository**

   ```bash
   git clone <repository-url>
   cd NativeIDE
   ```

2. **Build with CMake**

   ```bash
   # Using provided build script
   build.bat
   
   # Or manually
   mkdir build && cd build
   cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

3. **Run the IDE**

   ```bash
   cd build/bin
   ./NativeIDE.exe
   ```

## Project Structure

```text
NativeIDE/
├── src/                    # Source code
│   ├── main.cpp           # Application entry point
│   ├── ide_application.*  # Main application class
│   ├── main_window.*      # Main window implementation
│   ├── startup_dialog.*   # Startup experience
│   └── compiler_integration.* # Compiler toolchain
├── include/               # Header files
├── resources/            # Windows resources (icons, dialogs)
├── templates/            # Project templates
│   └── console-app/     # C++ console application template
├── plugins/              # Plugin examples
├── toolchain/           # Bundled compilers (deploy-time)
└── tests/               # Unit tests
```

## Usage

### Creating a New Project

1. Launch NativeIDE.exe
2. Select "Create New Project"
3. Choose template (Console Application)
4. Select destination folder
5. Start coding!

### Opening Existing Code

1. Select "Open Folder" from startup dialog
2. Browse to your source code directory
3. Files appear in project tree on the left
4. Double-click files to open in editor

### Building and Running

- **F7** - Compile current file
- **F5** - Build entire project (runs Make)
- **Ctrl+F5** - Run executable
- View compilation output in bottom panel

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New File |
| Ctrl+O | Open File |
| Ctrl+S | Save File |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| F7 | Compile |
| F5 | Build |
| Ctrl+F5 | Run |

## Architecture

### Core Components

- **IDE Application**: Main application controller
- **Main Window**: Win32 window with Direct2D rendering
- **Editor Core**: High-performance text editing engine
- **Compiler Integration**: GCC/Clang toolchain management
- **Plugin Manager**: Dynamic plugin loading system
- **Project Manager**: File tree and project operations

### Plugin System

The IDE supports plugins through DLL loading:

```cpp
// Plugin interface
struct IPlugin {
    virtual const char* GetName() const = 0;
    virtual bool Initialize(IDEApplication* app) = 0;
    virtual void OnFileOpened(const std::wstring& filename) {}
    // ... other hooks
};

// Plugin export
extern "C" __declspec(dllexport) IPlugin* CreatePlugin() {
    return new MyPlugin();
}
```

## Deployment

For portable deployment:

1. Build the IDE with static linking
2. Copy bundled compiler toolchain to `toolchain/` folder
3. Include project templates in `templates/` folder
4. Distribute as single folder - no installer needed

### Toolchain Structure

```text
toolchain/
├── gcc/           # MinGW-w64 GCC
├── clang/         # Clang/LLVM  
├── make/          # GNU Make
├── gdb/           # GNU Debugger
└── libs/          # Static libraries
```

## Contributing

This is a code challenge implementation demonstrating:

- Native Win32 application development
- Direct2D graphics and text rendering
- Plugin architecture design
- Compiler toolchain integration
- Portable application deployment

## License

This implementation is provided as a coding demonstration and follows the systems programming principles outlined in the `classified.md` specification.