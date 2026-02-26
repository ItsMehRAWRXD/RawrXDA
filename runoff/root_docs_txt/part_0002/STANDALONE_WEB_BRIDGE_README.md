# RawrXD Standalone Web Bridge - Qt-Free Browser TCP Bypass

This is a **reverse-engineered, Qt-free version** of the RawrXD web bridge server that completely bypasses browser TCP limitations without any Qt dependencies.

## 🎯 Problem Solved

**Browser Security Limitation**: HTML/JavaScript cannot open TCP listening sockets or accept inbound connections.

**Qt-Free Solution**: Pure C++ HTTP/WebSocket server that serves HTML interfaces and provides IPC through standard web protocols.

## 🏗 Architecture Comparison

### Qt Version (Original)
```
HTML/JS ←──Qt WebChannel──→ Qt Application ←──TCP──→ GGUF Server
     ↑                           ↑
Qt WebEngine required      Qt framework required
```

### Standalone Version (Qt-Free)
```
HTML/JS ←──HTTP/WebSocket──→ Standalone Server ←──TCP──→ GGUF Server
     ↑                              ↑
Any modern browser           Pure C++ (no Qt)
```

## 📁 Files Created

### Core Implementation
- `src/standalone_web_bridge.hpp/cpp` - Main standalone server implementation
- `src/standalone_main.cpp` - Console application entry point
- `standalone_interface.html` - Browser-compatible HTML interface
- `CMakeLists-Standalone.txt` - Build configuration (Qt-free)

### Key Components
- **StandaloneWebAPI** - Qt-free API that mirrors WebBridgeAPI functionality
- **SimpleHttpServer** - Lightweight HTTP server (no external deps)
- **SimpleWebSocketServer** - WebSocket server for real-time communication
- **StandaloneWebBridgeServer** - Main coordinator class

## 🚀 Building & Running

### Prerequisites
- C++17 compatible compiler
- CMake 3.16+
- No Qt dependencies required!

### Build Steps

```bash
# Create build directory
mkdir build-standalone
cd build-standalone

# Configure with standalone CMake file
cmake -C ../CMakeLists-Standalone.txt ..

# Build
cmake --build . --config Release

# Run
./rawrxd-standalone [http_port] [ws_port] [gguf_port] [gguf_endpoint] [web_root]
```

### Default Configuration
- **HTTP Port**: 8080 (web interface)
- **WebSocket Port**: 8081 (real-time communication)
- **GGUF Port**: 11434 (model server)
- **Web Root**: Current directory

### Example Usage
```bash
# Basic run with defaults
./rawrxd-standalone

# Custom configuration
./rawrxd-standalone 3000 3001 11434 "http://localhost:11434" ./web
```

## 🌐 Web Interface

Once running, open `http://localhost:8080` in any modern browser:

### Features
- ✅ **Real-time server monitoring** - Live status updates
- ✅ **Model communication** - Send prompts via HTTP/WebSocket
- ✅ **Statistics display** - Server performance metrics
- ✅ **Error handling** - Comprehensive error reporting
- ✅ **Keyboard shortcuts** - Ctrl+Enter to send
- ✅ **Responsive design** - Works on desktop and mobile

### API Endpoints

#### HTTP API
```
GET  /api/status     - Server status
GET  /api/stats      - Server statistics
POST /api/sendToModel - Send prompt to model
```

#### WebSocket API (JSON-RPC 2.0)
```json
{
  "jsonrpc": "2.0",
  "method": "sendToModel",
  "params": {
    "prompt": "Hello world",
    "options": {"temperature": 0.7}
  },
  "id": 1
}
```

## 🔧 How It Works

### 1. HTTP Server
- Serves static HTML/CSS/JS files
- Handles REST API calls
- CORS enabled for browser access

### 2. WebSocket Server
- Real-time bidirectional communication
- JSON-RPC 2.0 protocol
- Automatic reconnection handling

### 3. TCP Proxy Bridge
- Forwards requests to GGUF model server
- Maintains connection pooling
- Handles protocol translation

### 4. Browser Interface
- Pure HTML/JavaScript (no Qt WebEngine)
- Uses standard `fetch()` and `WebSocket` APIs
- Works in any modern browser

## 🔒 Security & Benefits

- **Zero external exposure** - All servers bind to localhost
- **No Qt dependencies** - Pure C++ standard library + sockets
- **Browser compatibility** - Works with any modern browser
- **Cross-platform** - Windows, Linux, macOS support
- **Lightweight** - Minimal dependencies and footprint

## 📊 Performance Comparison

| Feature | Qt Version | Standalone Version |
|---------|------------|-------------------|
| Dependencies | Qt6 WebEngine (~100MB) | None (pure C++) |
| Browser Support | Qt WebEngine only | Any modern browser |
| Communication | Qt WebChannel IPC | HTTP + WebSocket |
| Build Size | Large (Qt framework) | Small (standalone binary) |
| Deployment | Qt runtime required | Single executable |
| Platform Support | Qt-supported platforms | Any C++17 platform |

## 🎯 Use Cases

### When to Use Qt Version
- Integrated Qt application
- Need Qt WebEngine features
- Existing Qt codebase
- Desktop app with embedded browser

### When to Use Standalone Version
- **Web deployment** - Serve over network
- **No Qt dependencies** - Lightweight deployment
- **Cross-platform** - Any OS with C++ compiler
- **Microservices** - Container-friendly
- **Headless operation** - No GUI required

## 🔄 Migration from Qt Version

### Code Changes
```cpp
// Qt version
WebBridgeServer bridge(port, hotPatcher, this);
bridge.loadHtmlFile("interface.html");

// Standalone version
StandaloneWebBridgeServer server(httpPort, wsPort, hotPatcher);
server.serveStaticFiles("./web");
server.start();
```

### HTML Changes
```javascript
// Qt version (WebChannel)
rawrxdAPI.sendToModel(prompt, options);

// Standalone version (WebSocket)
ws.send(JSON.stringify({
  jsonrpc: "2.0",
  method: "sendToModel",
  params: {prompt, options},
  id: requestId++
}));
```

## 🚦 Next Steps

1. **Build and test** the standalone version
2. **Customize HTML interface** for your needs
3. **Add authentication** if required
4. **Implement SSL/TLS** for secure connections
5. **Add load balancing** for multiple model servers
6. **Containerize** with Docker for easy deployment

## 🐛 Troubleshooting

### Server Won't Start
- Check if ports 8080/8081 are available
- Ensure GGUF server is running on port 11434
- Check firewall settings

### Browser Can't Connect
- Verify server is running (`netstat -an | grep 8080`)
- Check CORS headers in browser dev tools
- Try different browser or incognito mode

### WebSocket Issues
- Check WebSocket port 8081 is accessible
- Verify WebSocket handshake in browser dev tools
- Check for proxy/firewall blocking WebSocket

## 📝 API Reference

### StandaloneWebAPI Methods
- `sendToModel(prompt, options)` - Send prompt to model
- `getServerStatus()` - Get server status
- `getStatistics()` - Get server statistics
- `isServerReady()` - Check if server is ready

### Server Configuration
- `serveStaticFiles(path)` - Set web root directory
- `addRoute(path, handler)` - Add custom HTTP route
- `start()` / `stop()` - Control server lifecycle

---

**This standalone version provides the same browser TCP bypass functionality as the Qt version, but without any Qt dependencies - making it perfect for web deployment, microservices, or any scenario where you want to avoid Qt's overhead!** 🎉</content>
<parameter name="filePath">d:\rawrxd\STANDALONE_WEB_BRIDGE_README.md