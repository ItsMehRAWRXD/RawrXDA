# Quick Build Instructions

## 1. Update CMakeLists.txt

Open `CMakeLists.txt` and find the `QTAPP_SOURCES` section. Add the new files:

```cmake
set(QTAPP_SOURCES
    # ... existing files ...
    src/qtapp/MainWindow.cpp
    src/qtapp/MainWindow_AgenticImpl.cpp
    src/qtapp/MainWindow_OllamaLogging.cpp    # <-- ADD THIS LINE
    src/qtapp/MainWindow_AI_Integration.cpp
    # ... rest of files ...
)
```

## 2. Build

```powershell
cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader
cmake --build build --config Release --target RawrXD-QtShell
```

## 3. Launch and Test

```powershell
# Launch IDE
.\build\bin\Release\RawrXD-QtShell.exe

# Expected startup logs in "Ollama Server Logs" dock:
# [HH:MM:SS] INFO: Attempting to attach to Ollama process...
# [HH:MM:SS] INFO: Ollama server detected on port 11434
# [HH:MM:SS] INFO: Hotpatch proxy upstream configured: http://localhost:11434
# [HH:MM:SS] INFO: Clients should connect to: http://localhost:11436

# Test with client:
curl http://localhost:11436/api/tags

# Expected in logs:
# [HH:MM:SS] INFO routes.go: Listening on 127.0.0.1:11434
# [GIN] 2025/12/02 - HH:MM:SS | 200 | ... | GET "/api/tags"
```

## What You Get

### Real-Time Streaming
All Ollama server activity appears instantly in the IDE's **Ollama Server Logs** dock:

```
time=2025-12-02T22:54:07.082-05:00 level=INFO source=routes.go:1544 msg="server config env=..."
time=2025-12-02T22:54:07.095-05:00 level=INFO source=images.go:522 msg="total blobs: 161"
time=2025-12-02T22:54:07.098-05:00 level=INFO source=images.go:529 msg="total unused blobs removed: 0"
time=2025-12-02T22:54:07.103-05:00 level=INFO source=routes.go:1597 msg="Listening on 127.0.0.1:11434 (version 0.13.1)"
time=2025-12-02T22:54:07.103-05:00 level=INFO source=runner.go:67 msg="discovering available GPUs..."
time=2025-12-02T23:00:54.208-05:00 level=INFO source=runner.go:999 msg="Server listening on 127.0.0.1:61574"
[GIN] 2025/12/02 - 23:01:10 | 200 | 17.0333008s | 127.0.0.1 | POST "/api/generate"
```

**With syntax highlighting:**
- 🔵 INFO (blue)
- 🟠 WARN (orange)  
- 🔴 ERROR (red)
- ⚪ DEBUG (gray)

### Zero-Configuration
The hotpatch proxy automatically:
1. Detects Ollama on port 11434
2. Configures upstream URL
3. Starts proxy on port 11436
4. Begins log streaming

**No manual setup required!**

## Troubleshooting

### Logs not appearing?

```powershell
# Check Ollama is running
curl http://localhost:11434/
# Expected: "Ollama is running"

# If not running:
ollama serve

# Then restart IDE or click "Reconnect" in log dock
```

### Proxy not auto-configuring?

Check the LLM Log for initialization messages:
```
[AGENTIC] Hotpatch proxy configured: 11436 → 11434
```

If missing, check that `initializeAgenticSystem()` is called in MainWindow constructor.

## Files Reference

| File | Purpose | Lines |
|------|---------|-------|
| `MainWindow_OllamaLogging.cpp` | Log streaming implementation | 550 |
| `MainWindow.h` | Added monitoring infrastructure | +20 |
| `MainWindow_AgenticImpl.cpp` | Auto-port detection | +30 |
| `OLLAMA-LOG-STREAMING-GUIDE.md` | Full documentation | 800+ |

## Summary

**Before:**
```cpp
// Manual configuration required
m_hotpatchProxy->setUpstreamUrl("http://localhost:11434");
m_hotpatchProxy->start(11436);
// No visibility into Ollama activity
```

**After:**
```cpp
// Automatic everything!
initializeAgenticSystem();
// ✅ Auto-detects Ollama port
// ✅ Configures proxy upstream  
// ✅ Starts log streaming
// ✅ Opens log viewer dock
```

🎉 **Complete observability into your Ollama server!**
