# RawrXD Chat Application - Quick Start Guide

## What is RawrXD Chat?

A minimal, focused desktop chat application that:
- ✅ Sits in your taskbar (system tray)
- ✅ Lets you chat with AI models (Ollama-compatible)
- ✅ Uploads and processes files
- ✅ Manages 256k tokens of conversation history
- ✅ Saves your chats automatically

---

## Installation

### Prerequisites
- Windows 10 or later
- Ollama running locally (download from https://ollama.ai)
- At least one model installed in Ollama

### Setup

1. **Start Ollama**
   ```
   ollama serve
   ```
   (Runs on localhost:11434)

2. **Run RawrXD Chat**
   ```
   RawrXD-Chat.exe
   ```

3. **First Launch**
   - Window opens with chat interface
   - System tray icon appears in taskbar
   - Ready to chat!

---

## Basic Usage

### Sending a Message

1. **Type in bottom panel**
   - "What is machine learning?"
   - "Explain this code..."
   - Anything you want to ask

2. **Send the message**
   - Click the "Send" button, OR
   - Press `Ctrl+Enter`

3. **View response**
   - Model response appears in top panel
   - Timestamp shows when sent
   - Context stats update at bottom

### Uploading Files

1. **Click "Upload File"** button
2. **Choose file** from dialog
   - Any file type supported
   - Up to 100MB per file
3. **File appears** in preview panel
4. **Type message** about the file
5. **Send together**

### Managing the Window

- **Minimize to tray**: Click minimize button
- **Show/hide**: Click taskbar icon
- **Quick menu**: Right-click taskbar icon
  - Show/Hide
  - New Session
  - Exit

---

## Features Explained

### 🗨️ Chat Panels

```
┌─────────────────────────────────┐
│  AGENT (Model Responses)         │  ← Read-only, shows AI responses
│  "That's a great question..."    │
├─────────────────────────────────┤
│  USER INPUT (You)                │  ← Type your messages here
│  "What is..."                    │
└─────────────────────────────────┘
```

- **Top panel**: Shows all responses from the model
- **Bottom panel**: Where you type your questions
- **Both show**: Timestamps and sender name

### 📎 Files

```
┌─────────────────────────────────┐
│  UPLOADED FILES                  │
│  • document.pdf (2.3MB)          │
│  • script.py (45KB)              │
└─────────────────────────────────┘
```

- Upload files with the button
- Files shown in preview panel
- Automatically included when sending
- Remove by clicking the X

### 📊 Context Stats

```
Tokens: 45,230 / 256,000 (18%)
```

- Shows how much of context you're using
- **Green** (0-50%): Lots of room
- **Yellow** (50-80%): Getting full
- **Red** (80-100%): Approaching limit
- Oldest messages auto-removed when full

### ⚙️ Settings

Located in: `C:\Users\[You]\AppData\Roaming\RawrXD\chat_settings.ini`

```ini
windowWidth=800
windowHeight=600
darkMode=1
fontSize=11
modelEndpoint=http://localhost:11434
```

Change and restart app to apply.

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Enter` | Send message |
| `Ctrl+L` | Clear all chat |
| `Ctrl+U` | Upload file |
| `Ctrl+W` | Minimize to tray |
| `Ctrl+Q` | Quit application |

---

## Common Tasks

### Start a New Chat

1. Right-click taskbar icon
2. Click "New Session"
3. Previous chat saved automatically
4. Clean chat to start fresh

### Check Model Connection

- Look at top-right of window
- Green dot = connected
- Red dot = not connected
- Check Ollama is running if disconnected

### Increase Context

- Default: 256,000 tokens
- Can't increase (hard limit)
- Create new session when full

### Save Chat

- Chats save automatically
- Stored in `C:\Users\[You]\AppData\Roaming\RawrXD\`
- Safe to close anytime

### Load Previous Chat

*Coming soon*: Session menu to load old chats

---

## Troubleshooting

### "Model not responding"

**Problem**: Chat doesn't show responses

**Solution**:
1. Check Ollama is running: `ollama serve`
2. Verify model exists: `ollama list`
3. Pull model if needed: `ollama pull llama2`
4. Restart chat app

### "Files not uploading"

**Problem**: Upload button doesn't work

**Solution**:
1. Check file exists
2. Try smaller file first
3. Ensure file not locked by another app
4. Restart chat app

### "Chat disappeared"

**Problem**: Window vanished

**Solution**:
1. Click taskbar icon to restore
2. Right-click tray icon → "Show"
3. Check task manager for the app

### "Too slow"

**Problem**: Responses are slow

**Solution**:
- Could be model (not chat)
- Try faster model: `ollama pull orca-mini` (faster than llama2)
- Reduce context size (create new session)
- Check system resources

---

## Tips & Tricks

### For Code Questions
```
Upload a file with:
1. Your source code
2. Error message
3. Ask what's wrong

Model has full context!
```

### For Long Conversations
```
Monitor token counter at bottom
When approaching 100%, create new session
Recent 256k tokens kept in memory
```

### For Better Responses
```
1. Be specific in prompts
2. Provide examples if possible
3. Break complex questions into parts
4. Upload related files for context
```

### Efficient File Handling
```
Upload once per session
Reference in multiple questions
Don't re-upload same file
Files stored in memory, not disk
```

---

## What's Next?

### Coming Soon
- Message editing
- Chat search
- Export to text/PDF
- Multiple models
- Plugins support

### Ideas You Can Submit
- Voice chat
- Image upload
- Real-time collaboration
- Web interface
- Mobile app

---

## Getting Help

### Issues?

1. **Check status**:
   - Ollama running?
   - Model installed?
   - Network working?

2. **Restart**:
   - Close app
   - Restart Ollama
   - Relaunch chat

3. **Reset**:
   - Close app
   - Delete `%APPDATA%\RawrXD\chat_settings.ini`
   - Restart app

### Want to Report a Bug?

Create issue at: https://github.com/ItsMehRAWRXD/RawrXD

Include:
- What happened
- Steps to reproduce
- Screenshot if possible
- Ollama version

---

## System Requirements

| Component | Requirement |
|-----------|------------|
| OS | Windows 10 / 11 / Server 2019+ |
| RAM | 4GB minimum (8GB+ recommended) |
| Storage | 1GB for app + models |
| Network | Localhost only (no internet needed) |

---

## Privacy & Data

- ✅ No data sent to cloud
- ✅ Chats stored locally only
- ✅ Can delete anytime
- ✅ Completely offline operation
- ✅ Open source (you can audit code)

---

## FAQ

**Q: Does this cost anything?**
A: No, it's completely free and open source.

**Q: Can I use it without Ollama?**
A: Not yet, but we're working on integrations.

**Q: What models can I use?**
A: Any model available on Ollama (llama2, mistral, neural-chat, etc.)

**Q: Can I run multiple instances?**
A: Not recommended, but yes. Each gets its own config.

**Q: Is my data safe?**
A: Yes, everything stays on your computer. No cloud required.

**Q: Can I use other AI providers?**
A: Not currently, but planned for future versions.

**Q: How much context is 256k?**
A: Roughly 200 average-length messages (~1000 words each).

**Q: Will adding more RAM help?**
A: Not for the chat app itself (uses <100MB), but helpful for models.

---

## Quick Reference

```
Launch:         RawrXD-Chat.exe
Config:         %APPDATA%\RawrXD\chat_settings.ini
History:        %APPDATA%\RawrXD\chat_history.json
Model API:      http://localhost:11434
Max Context:    256,000 tokens
Max File Size:  100MB per file
Max Files/Msg:  10 files
```

---

## Support Resources

- **Documentation**: CHAT-APP-README.md
- **Technical Details**: CHAT-APP-IMPLEMENTATION.md
- **Ollama Docs**: https://ollama.ai
- **GitHub**: https://github.com/ItsMehRAWRXD/RawrXD

---

**Happy Chatting! 🚀**

Questions? Check the docs or report an issue on GitHub.
