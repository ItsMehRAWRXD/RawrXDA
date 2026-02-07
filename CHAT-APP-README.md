# RawrXD Chat Application

A lightweight desktop chat application that integrates with RawrXD Model Loader via taskbar/system tray, featuring dual-panel chat interface, file uploads, and 256k context window management.

## Features

### 🎯 Core Chat Interface
- **Dual-Panel Design**: 
  - Top panel displays agent (model) responses with read-only formatting
  - Bottom panel shows user input area for composing prompts
  - Dynamic layout with resizable panels
  
- **Taskbar Integration**:
  - Minimizes to system tray (taskbar)
  - Left-click to show/hide window
  - Right-click context menu for quick access
  - Always-on-top support

### 📎 File Management
- **File Upload System**:
  - Browse and select files via dialog
  - Drag-and-drop file support
  - File preview panel showing uploaded files
  - Multiple file attachments per message
  - File size display and type detection

- **Supported File Types**:
  - Text files (`.txt`, `.md`, `.log`)
  - Code files (`.py`, `.cpp`, `.js`, `.json`)
  - Documents (`.pdf`, `.docx`)
  - Any file type (up to 100MB per file)

### 💾 Context Management (256k Tokens)
- **Sliding Context Window**:
  - Tracks up to 256,000 tokens of conversation history
  - Automatic pruning of oldest messages when capacity exceeded
  - Token estimation based on word count

- **Context Statistics**:
  - Real-time display of token usage
  - Compression ratio (% of max capacity)
  - Message count and pruning history
  - Token breakdown by message type

- **Session Management**:
  - Multiple chat sessions
  - Session history persistence
  - Load/save session state
  - Quick session switching

### 🎨 UI & UX
- **Rich Text Display**:
  - Markdown rendering support
  - Code syntax highlighting
  - Timestamp on all messages
  - Color-coded sender labels

- **Theme Support**:
  - Dark mode (default)
  - Light mode option
  - Customizable colors and fonts
  - Window size persistence

- **Keyboard Shortcuts**:
  - `Ctrl+Enter` - Send message
  - `Ctrl+L` - Clear chat
  - `Ctrl+U` - Upload file
  - `Ctrl+W` - Minimize to tray
  - `Ctrl+Q` - Quit application

### 🔌 Model Connection
- **HTTP API Integration**:
  - Connects to model via HTTP endpoint (default: `http://localhost:11434`)
  - Supports streaming responses
  - Connection status indicator
  - Auto-reconnect on failure

- **Message Formatting**:
  - Automatic prompt formatting for model
  - Context history inclusion
  - File content embedding
  - Response parsing and display

## Architecture

### Key Components

**Win32ChatApp.h / Win32ChatApp.cpp**
- Main application class
- Window creation and management
- UI layout and rendering
- Message handling

**ContextManager.h**
- 256k token context window management
- Message queue with automatic pruning
- Token counting and estimation
- Context statistics and logging

**main_chat.cpp**
- Application entry point
- WinMain function
- Instance creation

## Usage

### Launching the Application

```batch
RawrXD-Chat.exe
```

### Basic Workflow

1. **Start Chat**:
   - Application minimizes to taskbar
   - Click taskbar icon to open window

2. **Send Message**:
   - Type in bottom panel
   - Click "Send" or press Ctrl+Enter
   - Message appears with timestamp

3. **Upload Files**:
   - Click "Upload File" button
   - Select file(s) from dialog
   - Files appear in preview panel
   - Send with message

4. **View Context Stats**:
   - See token usage at bottom
   - Green = ample capacity
   - Yellow = >70% used
   - Red = >90% used

5. **Manage Sessions**:
   - Right-click tray → "New Session"
   - Chat history saved per session
   - Load previous sessions from menu

## Configuration

### Settings File
`%APPDATA%\RawrXD\chat_settings.ini`

```ini
windowWidth=800
windowHeight=600
darkMode=1
fontSize=11
modelEndpoint=http://localhost:11434
autoConnect=1
```

### Chat History
`%APPDATA%\RawrXD\chat_history.json`

Session data and message history stored as JSON for portability.

## Technical Details

### Token Counting
- Default estimate: 1 token ≈ 1.3 words
- 1 word ≈ 4.7 characters
- Code: +20% more tokens than plain text
- Markdown: Similar to code estimate

### Context Pruning
- **Normal Mode**: Remove oldest message when at capacity
- **Compression Mode**: Remove oldest 10% of messages
- **Aggressive Mode**: Remove until 50% capacity (triggered manually)

### Thread Safety
- UI thread: Message display and user input
- Worker thread: Model communication and file uploads
- Thread-safe message queue for IPC

## Performance

- **Memory Usage**: ~50-100MB typical (varies with history)
- **CPU Usage**: <5% idle, <20% during response generation
- **Startup Time**: <1 second
- **Response Latency**: <100ms local, depends on model

## Future Enhancements

- [ ] Voice input/output support
- [ ] Image display in chat
- [ ] Chat search and filtering
- [ ] Message editing and deletion
- [ ] Response regeneration
- [ ] System prompts and templates
- [ ] Multi-model support
- [ ] Plugin/extension system
- [ ] REST API for external tools
- [ ] Web interface option

## Troubleshooting

### Window Won't Show
- Click taskbar icon multiple times
- Check Window menu for "Restore" option

### Model Not Responding
- Verify Ollama is running: `ollama serve`
- Check endpoint setting: `http://localhost:11434`
- See connection status at bottom of window

### Context Too Full
- Context automatically prunes old messages
- Enable "Compression Mode" for aggressive pruning
- Create new session to start fresh

### Files Not Uploading
- Check file permissions
- Verify file path accessibility
- Try with smaller file first

## Development

### Building from Source

```bash
cd RawrXD-ModelLoader
mkdir build
cd build
cmake -B. -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Executable: `build/bin/Release/RawrXD-Chat.exe`

### Dependencies
- Windows 10+ SDK
- Visual C++ runtime
- MinGW/GCC compiler
- Win32 API (commctrl, shell32, winhttp)

## License

Part of RawrXD project - See main LICENSE

## Author

Built with ❤️ for RawrXD Model Loader
