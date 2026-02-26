# Copilot Streaming Integration for Win32IDE

Real-time AI completions and chat integrated directly into your C++ Win32 IDE.

## What's Included

### 1. **CopilotStreaming.h/cpp**
- Native C++ streaming client using WinHTTP
- Connects to `copilot-proxy` service (port 3200)
- SSE (Server-Sent Events) parsing
- Thread-safe token callbacks
- Auto-reconnect and error handling

### 2. **Win32IDE_CopilotIntegration.cpp**
- Hooks into existing IDE Copilot UI controls
- `HandleCopilotSend()` - Send button handler
- `HandleCopilotClear()` - Clear chat history
- `AppendCopilotMessage()` - Format and display messages
- `HandleCopilotStreamUpdate()` - Real-time token streaming display

### 3. **IDE Button Handlers**
Already wired to your existing controls:
- `IDC_COPILOT_SEND_BTN` (1204)
- `IDC_COPILOT_CLEAR_BTN` (1205)
- `IDC_COPILOT_CHAT_INPUT` (1202)
- `IDC_COPILOT_CHAT_OUTPUT` (1203)

## How It Works

```
User types in IDE Chat Input
         ↓
HandleCopilotSend() captures text + editor context
         ↓
CopilotStreaming sends POST to localhost:3200/stream
         ↓
Proxy forwards to Ollama (bigdaddyg-fast:latest)
         ↓
Ollama streams tokens back via NDJSON
         ↓
CopilotStreaming parses SSE events
         ↓
OnToken() callback fires for each token
         ↓
PostMessage(WM_USER+100) updates UI thread-safely
         ↓
HandleCopilotStreamUpdate() appends to RichEdit
         ↓
User sees real-time streaming response
```

## Build Integration

Add to your `CMakeLists.txt`:

```cmake
# Copilot streaming integration
set(IDE_COPILOT_SOURCES
    src/win32app/CopilotStreaming.cpp
    src/win32app/Win32IDE_CopilotIntegration.cpp
)

add_executable(RawrXD-Win32IDE
    ${IDE_SOURCES}
    ${IDE_COPILOT_SOURCES}  # Add this
)

target_link_libraries(RawrXD-Win32IDE
    winhttp  # Required for CopilotStreaming
)
```

## Button Wiring (if not already done)

In your `WM_COMMAND` handler (`Win32IDE.cpp` or `Win32IDE_VSCodeUI.cpp`):

```cpp
case WM_COMMAND:
    switch (LOWORD(wParam)) {
        case IDC_COPILOT_SEND_BTN:
            HandleCopilotSend();
            break;
        case IDC_COPILOT_CLEAR_BTN:
            HandleCopilotClear();
            break;
    }
    break;

case WM_USER + 100:  // Streaming token update
    HandleCopilotStreamUpdate();
    break;
```

## Usage in IDE

1. **Start the proxy** (in a terminal):
   ```powershell
   cd services\copilot-proxy
   node server.js
   ```

2. **Start Ollama** (if not already running):
   ```powershell
   ollama serve
   ```

3. **Launch your IDE**:
   ```powershell
   .\build\RawrXD-Win32IDE.exe
   ```

4. **Use Copilot Chat**:
   - Click the Copilot icon in the Activity Bar (or secondary sidebar)
   - Type a question in the chat input
   - Click Send or press Ctrl+Enter
   - Watch tokens stream in real-time
   - The IDE automatically includes your current editor content as context

## Features

### ✅ Real-Time Streaming
- Tokens appear character-by-character as Ollama generates them
- No waiting for complete response
- Natural chat-like experience

### ✅ Context Awareness
- Automatically includes current editor content in prompts
- Ask questions like "Explain this code" or "Find bugs in this function"
- Model sees your full file for better answers

### ✅ Model Selection
Default: `bigdaddyg-fast:latest`
Change in `CopilotStreaming.cpp`:
```cpp
m_model("your-model:latest"),
```

### ✅ Multi-Provider Support
Works with any backend the proxy supports:
- Ollama (default)
- OpenAI
- Anthropic Claude
- Google Gemini
- LM Studio
- text-generation-webui

Change provider:
```cpp
m_provider("openai"),  // or anthropic, gemini, etc.
m_upstream("https://api.openai.com/v1/chat/completions"),
```

## Customization

### Change Proxy URL
```cpp
m_proxyUrl("http://your-server:3200/stream"),
```

### Change Max Tokens
In `CopilotStreaming.cpp`, `StreamRequest()`:
```cpp
{"max_tokens", 1024}  // default is 512
```

### Add Custom System Prompt
Modify context building in `HandleCopilotSend()`:
```cpp
context = "You are an expert C++ developer.\n\nCurrent code:\n" + std::string(m_editorContent);
```

## Troubleshooting

### No response / timeout
- Check proxy is running: `curl http://localhost:3200/health`
- Check Ollama is running: `curl http://localhost:11434/api/tags`
- Check model exists: `ollama list`

### Garbled output
- Ensure RichEdit control (`m_copilotChatOutput`) is created with `ES_MULTILINE | WS_VSCROLL`
- Check text encoding (should be UTF-8)

### Slow streaming
- Use a smaller/faster model (e.g., `llama3.1:8b-instruct`)
- Reduce `max_tokens`
- Check CPU/GPU load

## Production Deployment

1. **Bundle the proxy** with your IDE installer
2. **Auto-start proxy** when IDE launches (spawn `node server.js` as child process)
3. **Add OAuth** if using GitHub Copilot API (use `copilot-auth` service)
4. **Secure the connection** (HTTPS + authentication for remote deployments)

## Files Modified

- `include/CopilotStreaming.h` (new)
- `src/win32app/CopilotStreaming.cpp` (new)
- `src/win32app/Win32IDE_CopilotIntegration.cpp` (new)
- `src/win32app/Win32IDE.h` (method declarations added)

## Next Steps

- Wire `WM_COMMAND` handlers for Send/Clear buttons
- Add to CMakeLists.txt
- Rebuild IDE
- Test with your existing `bigdaddyg-fast:latest` model
