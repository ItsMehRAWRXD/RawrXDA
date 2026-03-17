# ⚡ Quick Start - Ollama Real-Time Logging

## 30-Second Setup

### 1. Update CMakeLists.txt
```cmake
# Find this section and add the new file:
set(QTAPP_SOURCES
    # ... existing ...
    src/qtapp/MainWindow_OllamaLogging.cpp    # <-- ADD THIS
    # ... rest ...
)
```

### 2. Build
```powershell
cmake --build build --config Release
```

### 3. Launch & Enjoy!
```powershell
.\build\bin\Release\RawrXD-QtShell.exe
```

## What You Get (Automatic)

✅ **Ollama Server Logs** dock widget (bottom panel)  
✅ Real-time streaming of all Ollama activity  
✅ Syntax highlighting (INFO=Blue, WARN=Orange, ERROR=Red)  
✅ Auto-configured hotpatch proxy (11436 → 11434)  
✅ Zero manual setup required!

## Example Output

```
[22:54:07] INFO  routes.go:1597: Listening on 127.0.0.1:11434 (version 0.13.1)
[22:54:08] INFO  runner.go:67: discovering available GPUs...
[23:00:54] INFO  ggml.go:104: ROCm.0 AMD Radeon RX 7800 XT
[23:01:02] INFO  server.go:1332: llama runner started in 8.34 seconds
[GIN] 2025/12/02 - 23:01:10 | 200 | 17.0333008s | 127.0.0.1 | POST "/api/generate"
```

**All those logs you pasted? They'll stream here in real-time!** 🎉

## Network Flow

```
Your Client → Port 11436 (Proxy + Auto-Correction) → Port 11434 (Ollama)
                      ↓
              Ollama Server Logs Dock
              (Streams all activity)
```

## Controls

- **Clear Logs** - Reset view
- **Export...** - Save to `Documents/ollama_logs_TIMESTAMP.txt`
- **Reconnect** - Restart monitoring

## Docs

- `OLLAMA-LOG-STREAMING-GUIDE.md` - Full documentation
- `BUILD-OLLAMA-LOGGING.md` - Detailed build steps
- `ARCHITECTURE-DIAGRAM.md` - System architecture

---

**That's it! Build, launch, and watch those logs stream!** 🚀
