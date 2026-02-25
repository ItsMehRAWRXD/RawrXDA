# BigDaddyG Self-Made Browser 🚀

**A native desktop application built with Electron - no more HTML dependency!**

This is your **self-made browser** - a fully native desktop application that runs the BigDaddyG agentic workspace as a standalone executable. No web browser required, no external dependencies for the end user.

## 🎯 What Makes This a "Self-Made Browser"?

Unlike the previous HTML-based interface, this is now a **native desktop application** built with:

- **Electron**: Chromium-based native app framework
- **Node.js Backend**: Integrated Express + WebSocket server
- **Native APIs**: Direct file system access, OS integration
- **Standalone Executable**: Can be packaged and distributed without installation

## 🏗️ Architecture

```
BigDaddyG Browser (Native App)
├── Electron Main Process (main.js)
│   ├── Express Server (Port 3000)
│   ├── WebSocket Server (Port 8001)
│   └── Native OS APIs
├── Electron Renderer Process
│   ├── 3-Panel Workspace UI
│   ├── Code Editor
│   └── Agent Controls
└── Backend Services
    ├── Agent Orchestration
    ├── File System Operations
    └── WebSocket Communication
```

## 🚀 Quick Start

### Prerequisites
- Node.js 16+
- npm or yarn

### Installation & Run

1. **Clone/Download** this project
2. **Install dependencies:**
   ```bash
   npm install
   ```
3. **Run the browser:**
   ```bash
   # Linux/Mac
   ./run-electron.sh

   # Windows
   run-electron.bat

   # Or directly:
   npm start
   ```

4. **The BigDaddyG Browser opens automatically!**

## 📦 Distribution

Build standalone executables for distribution:

```bash
# Build for current platform
npm run dist

# Build for specific platforms
npm run dist:win    # Windows installer
npm run dist:mac    # macOS .dmg
npm run dist:linux  # Linux AppImage
```

The built executables will be in the `dist/` folder and can be run on any machine without Node.js installation.

## 🎮 Features

### Native Capabilities
- **File System Access**: Direct read/write to local files
- **OS Integration**: Native dialogs, system info, platform detection
- **Process Management**: Run system commands, manage subprocesses
- **Security**: Sandboxed execution with controlled permissions

### Agentic Workspace
- **3-Panel Layout**: Code Editor | Console Output | Agent Dashboard
- **Real-time Agents**: Elder, Fetcher, Browser, Parser agents with live status
- **WebSocket Communication**: Instant updates between frontend and backend
- **Persistent Storage**: Auto-save code, settings, and workspace state

### Development Features
- **Code Editor**: Syntax highlighting, auto-save, load/save functionality
- **Interactive Console**: Execute JavaScript commands with real-time feedback
- **Agent Controls**: Run individual agents with parameter passing
- **Live Logging**: Color-coded console output with timestamps

## 🛠️ Development

### Project Structure
```
bigdaddyg-browser/
├── main.js              # Electron main process
├── preload.js           # Secure API bridge
├── package.json         # App configuration
├── frontend/
│   ├── index-electron.html  # Main UI (Electron version)
│   ├── js/
│   │   ├── ws.js        # WebSocket client
│   │   ├── tabs.js      # Tab management
│   │   └── storage.js   # Persistence
│   └── css/
│       └── style.css    # UI styling
├── backend/             # Integrated backend services
├── assets/              # Icons, images
├── dist/                # Built executables (generated)
└── node_modules/        # Dependencies
```

### Key Files

- **`main.js`**: Electron main process - manages windows, servers, native APIs
- **`preload.js`**: Security bridge between main and renderer processes
- **`frontend/index-electron.html`**: The actual browser UI
- **`package.json`**: App metadata and build configuration

### API Reference

#### Electron APIs (via preload)
```javascript
// File operations
await window.electronAPI.saveFile(path, content)
await window.electronAPI.readFile(path)
await window.electronAPI.listDir(path)

// App information
const appPath = await window.electronAPI.getAppPath()
const platform = window.electronAPI.platform
```

#### Agent Communication
```javascript
// Run code
ws.send(JSON.stringify({command: "run_code", code: "console.log('Hello')"}));

// Run agent
ws.send(JSON.stringify({command: "run_agent", agent: "Fetcher", data: {...}}));

// Receive responses
ws.onmessage = (e) => {
  const data = JSON.parse(e.data);
  console.log(`[${data.agent}] ${data.status}: ${data.message}`);
};
```

## 🔧 Customization

### Adding New Agents
1. Add agent logic in `backend/agents/`
2. Register in WebSocket handler in `main.js`
3. Add UI controls in `frontend/index-electron.html`

### Custom Styling
Edit `frontend/css/style.css` for themes and layouts.

### Native Features
Extend `main.js` to add more Electron capabilities like:
- System tray integration
- Global shortcuts
- Auto-updater
- Native notifications

## 🎯 Why This is Better Than HTML

| Feature | HTML Version | Self-Made Browser |
|---------|-------------|-------------------|
| **Distribution** | Requires web server | Standalone executable |
| **File Access** | Limited by browser sandbox | Full OS file system |
| **Installation** | Web deployment | Native app installation |
| **Performance** | Browser limitations | Native app performance |
| **Security** | Browser restrictions | Configurable permissions |
| **Integration** | Web APIs only | Native OS APIs |
| **Persistence** | localStorage only | Full file system access |
| **Updates** | Manual web refresh | Auto-updater capability |

## 🚦 Status

- ✅ **Native Desktop App**: Electron-based standalone executable
- ✅ **Integrated Backend**: Express + WebSocket servers bundled
- ✅ **Agent System**: Live agent orchestration with real-time updates
- ✅ **File System**: Direct OS file operations
- ✅ **Persistence**: Auto-save/load workspace state
- ✅ **Distribution**: Cross-platform build system

## 🤝 Contributing

This is now a **native application**, not a web app. Contributions should focus on:

- Electron-specific features
- Native OS integrations
- Performance optimizations
- Security hardening
- Cross-platform compatibility

## 📄 License

MIT - Build your own self-made browser!

---

**Remember**: This is your **self-made browser** - a native desktop application that happens to use web technologies internally, but presents as a standalone executable to end users. No more HTML dependency!