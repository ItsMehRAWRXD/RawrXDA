# IDE Swarm GUI

A graphical user interface for the IDE-Integrated Swarm Controller, providing an intuitive way to interact with all swarm agents.

## Features

### 🖥️ **Graphical Interface**
- Modern Tkinter-based GUI with menu bars and toolbars
- Multi-tabbed editor for working with multiple files
- File explorer panel for navigation
- Output panel for command results and feedback

### 🤖 **Agent Integration**
- **Explorer Agent**: File and folder operations
- **Editor Agent**: Text editing and formatting
- **Git Agent**: Version control operations
- **Chat Agent**: AI assistance and code generation
- **Settings Agent**: Workspace configuration

### 🌐 **God Mode**
- Unified command interface accessible from GUI
- Agent selection dropdown
- Workflow execution
- Real-time feedback

## Launch Options

### GUI Mode
```bash
python ide_swarm_controller.py --gui
```

Or use the launcher:
```bash
launch_gui.bat
```

### Command Line Mode
```bash
# Interactive mode
python ide_swarm_controller.py

# Direct God Mode
python ide_swarm_controller.py --godmode
```

## GUI Components

### Menu Bar
- **File**: New, Open, Save operations
- **Explorer**: Folder operations, file management
- **Editor**: Cut, Copy, Paste, Find, Format
- **Git**: Status, Commit, Push, Pull, History
- **Chat**: AI queries, code explanation, generation
- **Settings**: Workspace configuration
- **Swarm**: Status, agent selection, workflows, God Mode

### Toolbar
Quick access buttons for common operations like New, Open, Save, Cut, Copy, Paste, Git Status, and AI Chat.

### Main Area
- **File Explorer**: Tree view of project files
- **Editor Tabs**: Multi-document editing interface
- **Output Panel**: Command results and system messages

### Status Bar
Shows current operation status and system state.

## Usage Examples

### File Operations
1. Click **File > Open** to load a file
2. Use **File > New** to create a new document
3. **File > Save** to save your work

### Git Integration
1. **Git > Status** to check repository state
2. **Git > Commit** to commit changes
3. **Git > Push** to push to remote

### AI Assistance
1. **Chat > Ask AI** for general questions
2. **Chat > Explain Code** for code explanations
3. **Chat > Generate Code** for code generation

### God Mode
1. **Swarm > God Mode** to enter unified command interface
2. Use commands like `select agent`, `execute <action>`, `workflow <name>`

## Requirements

- Python 3.7+
- Tkinter (usually included with Python)
- GitPython (for Git operations)
- IDE Swarm Controller dependencies

## Installation

1. Ensure Python and required packages are installed
2. Run the GUI launcher or use command line options

## Architecture

The GUI integrates with the existing `IDESwarmController` class, running async operations in background threads to maintain responsiveness. All agent operations are handled through the same backend system used by the command-line interface.