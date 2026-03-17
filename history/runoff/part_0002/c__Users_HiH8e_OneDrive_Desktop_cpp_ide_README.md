# C++ IDE Project

A simple text editor with tab support written in C++.

## Features

- ✅ **Text Box**: Full text editing capabilities
- ✅ **Tab System**: Create new tabs with `[+]` functionality
- ✅ **File Operations**: Save and load files
- ✅ **Tab Management**: Switch between tabs without overwriting
- ✅ **Console Interface**: Command-line based interaction
- ✅ **Memory Safe**: Uses modern C++ with smart pointers

## Building

### Requirements
- C++17 compatible compiler (GCC, Clang, or MSVC)
- Standard library support

### Compile Commands

**Windows (MSVC):**
```cmd
cl /EHsc /std:c++17 main.cpp ide.cpp /Feide.exe
```

**Windows (GCC/MinGW):**
```cmd
g++ -std=c++17 -O2 -o ide.exe main.cpp ide.cpp
```

**Linux/Mac (GCC/Clang):**
```bash
g++ -std=c++17 -O2 -o ide main.cpp ide.cpp
```

## Usage

### Starting the IDE
```
./ide.exe
```

### Commands

| Command | Description | Example |
|---------|-------------|---------|
| `new` | Create new tab | `new` |
| `close` | Close current tab | `close` |
| `save <file>` | Save current tab | `save mycode.cpp` |
| `load <file>` | Load file into tab | `load example.txt` |
| `tabs` | Show all tabs | `tabs` |
| `switch <index>` | Switch to tab | `switch 1` |
| `edit <text>` | Add text to tab | `edit Hello World!` |
| `content` | Show tab content | `content` |
| `clear` | Clear current tab | `clear` |
| `help` | Show help | `help` |
| `exit` | Quit IDE | `exit` |

### Tab System Features

1. **Multiple Tabs**: Create unlimited tabs
2. **Non-Destructive**: Switching tabs preserves content
3. **Visual Indicators**: Shows active tab and unsaved changes (*)
4. **Unique Names**: Auto-generates unique tab names

### Example Session

```
=== C++ IDE Starting ===
Initializing main window...
[TabManager] Created tab: welcome.txt (index: 0)
Initialization complete!

=== IDE Console Mode ===
Type commands or text. Use 'help' for commands, 'exit' to quit.

> new
[TabManager] Created tab: untitled1.txt (index: 1)

--- Tabs ---
[0] welcome.txt
[1] untitled1.txt [ACTIVE]
--- End Tabs ---

> edit #include <iostream>
Added to tab: untitled1.txt

> edit int main() { return 0; }
Added to tab: untitled1.txt

> save test.cpp
Saved to: test.cpp

> switch 0
[TabManager] Switched to tab: welcome.txt

--- Tabs ---
[0] welcome.txt [ACTIVE]
[1] test.cpp
--- End Tabs ---

> exit
```

## Architecture

### Core Components

1. **Tab Class**: Manages individual file content
   - File content storage
   - Modification tracking
   - File path association

2. **TextEditor Class**: Handles text operations
   - Text insertion/deletion
   - Cursor management
   - Change callbacks

3. **TabManager Class**: Manages multiple tabs
   - Tab creation/deletion
   - Tab switching
   - File I/O operations

4. **MainWindow Class**: Main application controller
   - User interface coordination
   - Command processing
   - Event handling

### Design Patterns Used

- **RAII**: Automatic resource management
- **Smart Pointers**: Memory safety with `std::unique_ptr`
- **Observer Pattern**: Text change callbacks
- **Command Pattern**: Command processing system

## Future Enhancements

### GUI Version
- Port to Qt/GTK for graphical interface
- Visual tab bar with close buttons
- Syntax highlighting
- Line numbers and text wrapping

### Advanced Features
- Undo/Redo system
- Find/Replace functionality
- Project management
- Build system integration
- Syntax highlighting for multiple languages

### Performance
- Lazy loading for large files
- Efficient text buffer management
- Background save operations

## Code Structure

```
cpp_ide/
├── ide.hpp          # Header with class declarations
├── ide.cpp          # Main implementation
├── main.cpp         # Application entry point
└── README.md        # This file
```

## License

MIT License - Feel free to modify and use in your projects!

## Converting from IDEre2.html

This C++ version provides the core functionality of the HTML IDE:

1. **Text Editing**: Equivalent to the Monaco editor
2. **Tab System**: Implements the tab management from the web version
3. **File Operations**: Save/load functionality
4. **Extensible**: Easy to add new features

The console interface allows testing all features while providing a foundation for a future GUI implementation.