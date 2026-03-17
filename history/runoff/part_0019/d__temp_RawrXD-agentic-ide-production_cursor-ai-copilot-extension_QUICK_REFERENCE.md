# Quick Reference: OpenAI Integration in Cursor AI Copilot

## 🚀 Installation (30 seconds)

```powershell
# Option 1: Install from VSIX file
code --install-extension "d:\temp\RawrXD-agentic-ide-production\cursor-ai-copilot-extension\cursor-ai-copilot-1.0.0.vsix"

# Option 2: Install in Win32 IDE
Install-VSCodeExtension -ExtensionId "cursor-ai-copilot"
```

## 🔑 Setup API Key

### Recommended: VS Code Secrets
1. Open VS Code Settings (Ctrl+,)
2. Search: `cursor-ai-copilot.apiKey`
3. Paste your OpenAI API key

### Alternative: Environment Variable
```powershell
$env:OPENAI_API_KEY = "sk-proj-..."
```

### Alternative: .env File
```
OPENAI_API_KEY=sk-proj-...
OPENAI_API_ENDPOINT=https://api.openai.com/v1
```

## ⌨️ Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Open Chat | `Ctrl+Shift+A` |
| Explain Code | `Ctrl+Shift+E` |
| Refactor Code | `Ctrl+Shift+R` |
| Optimize Code | `Ctrl+Shift+O` |
| Generate Code | `Ctrl+Shift+G` |
| **Select Model** | **`Ctrl+Shift+M`** ⭐ |

## 🎯 Commands

Press `Ctrl+Shift+P` and search:

```
cursor-ai-copilot.selectModel      → Choose model
cursor-ai-copilot.listModels       → Show all models
cursor-ai-copilot.verifyAPIKey     → Test API key
cursor-ai-copilot.chat             → Open chat
cursor-ai-copilot.explain          → Explain code
cursor-ai-copilot.refactor         → Refactor code
cursor-ai-copilot.optimize         → Optimize code
cursor-ai-copilot.generate         → Generate code
```

## 🤖 Available Models

### 🏆 Recommended (Start Here)
- **gpt-5.2** - Newest, best reasoning
- **gpt-4o** - Fast, multimodal
- **gpt-3.5-turbo** - Budget-friendly
- **o1** - Complex problems

### 💬 Chat Models (13 total)
```
gpt-5.2, gpt-5.2-pro, gpt-5.1, gpt-5
gpt-4o, gpt-4o-2024-11-20
gpt-3.5-turbo, gpt-3.5-turbo-1106
o1, o3-mini, o3
davinci-002, babbage-002
```

### 🎨 Image Generation (3 total)
```
dall-e-3       (Best quality)
dall-e-2       (Faster)
gpt-image-1    (Understanding)
```

### 🎙️ Audio (7 total)
```
whisper-1      (Speech to text)
tts-1          (Text to speech - fast)
tts-1-hd       (Text to speech - quality)
gpt-audio      (Audio processing)
gpt-4o-transcribe-diarize
```

### 🎬 Video (2 total)
```
sora-2         (Video generation)
sora-2-pro     (Professional video)
```

### 📊 Embeddings (3 total)
```
text-embedding-3-large   (3,072 dims - high quality)
text-embedding-3-small   (1,536 dims - efficient)
text-embedding-ada-002   (Legacy)
```

## ⚙️ Configuration

### Basic Settings
```json
{
  "cursor-ai-copilot.model": "gpt-4o",
  "cursor-ai-copilot.temperature": 0.7,
  "cursor-ai-copilot.maxTokens": 2048,
  "cursor-ai-copilot.apiKeySource": "vscodeSecrets"
}
```

### Advanced Settings
```json
{
  "cursor-ai-copilot.modelSelectionMode": "recommended",
  "cursor-ai-copilot.enableModelAutoDetection": true,
  "cursor-ai-copilot.cacheModelList": true,
  "cursor-ai-copilot.requestTimeout": 30000
}
```

## 🐛 Troubleshooting

### "API key not found"
```powershell
# Check your API key is set
# VS Code: Settings → cursor-ai-copilot.apiKey
# Or: $env:OPENAI_API_KEY

# Get key from: https://platform.openai.com/account/api-keys
```

### "Model not available"
```powershell
# Verify model exists
Invoke-Command 'cursor-ai-copilot.listModels'

# Check API status: https://status.openai.com
```

### "API rate limited (429)"
```
Wait a moment and try again.
Upgrade your OpenAI plan if needed.
Check usage at: https://platform.openai.com/account/usage
```

### "Request timeout"
Increase timeout in settings:
```json
{
  "cursor-ai-copilot.requestTimeout": 60000
}
```

## 📚 Model Selection Guide

| Need | Best Model | Why |
|------|-----------|-----|
| Quick answers | gpt-3.5-turbo | Fast & cheap |
| Complex logic | gpt-5.2 | Better reasoning |
| Images | dall-e-3 | Highest quality |
| Transcription | whisper-1 | Accurate speech-to-text |
| Embeddings | text-embedding-3-small | Good quality & speed |
| Video | sora-2-pro | Professional results |

## 🔐 Security Best Practices

✅ **DO:**
- Use VS Code Secrets (most secure)
- Use environment variables locally
- Rotate API keys regularly
- Use workspace-specific keys

❌ **DON'T:**
- Commit API keys to git
- Share keys in chat/messaging
- Use production key in development
- Store in plain text files

## 📋 Win32 IDE Integration

### Load in PowerShell Panel
```powershell
# Automatically loaded with RawrXD module
Import-Module VSCodeExtensionManager

# Commands available:
Get-VSCodeExtensionStatus
Search-VSCodeMarketplace
Install-VSCodeExtension
Uninstall-VSCodeExtension
Load-VSCodeExtension
Get-VSCodeExtensionInfo
```

### Set Model in IDE
```powershell
# Set preferred model
$env:OPENAI_MODEL = "gpt-5.2"

# Use in commands
Invoke-Command 'cursor-ai-copilot.selectModel'
```

## 📊 Performance Tips

### For Speed
- Use `gpt-3.5-turbo`
- Lower `maxTokens` to 1024
- Disable hover analysis
- Enable model caching

### For Quality
- Use `gpt-5.2` or `gpt-4o`
- Set `temperature` to 0.5-0.7
- Higher `maxTokens` (2048+)
- Enable auto-detection

### For Budget
- Use `gpt-3.5-turbo`
- Set `temperature` to 0
- Lower `maxTokens`
- Reuse embeddings

## 🆘 Support

| Issue | Resource |
|-------|----------|
| Installation | [DEPLOYMENT_SUMMARY.md](./DEPLOYMENT_SUMMARY.md) |
| Configuration | [OPENAI_INTEGRATION.md](./OPENAI_INTEGRATION.md) |
| API Details | [API_INTEGRATION_DATA.md](./API_INTEGRATION_DATA.md) |
| OpenAI Help | https://help.openai.com |
| API Status | https://status.openai.com |
| GitHub Issues | https://github.com/RawrXD/cursor-ai-copilot/issues |

## 📦 Package Info

- **Version**: 1.0.0
- **Size**: 3.2 MB
- **Models**: 122 available
- **Built**: December 17, 2025
- **Status**: ✅ Production Ready

## 🎯 Quick Tasks

### Explain Code
1. Select code
2. Press `Ctrl+Shift+E`
3. Wait for explanation in chat

### Generate Code
1. Position cursor where you want code
2. Press `Ctrl+Shift+G`
3. Describe what you need
4. Code appears

### Refactor Code
1. Select code to refactor
2. Press `Ctrl+Shift+R`
3. Select suggestion or edit

### Switch Model
1. Press `Ctrl+Shift+M`
2. Pick from list
3. Model changes immediately

### Check Setup
```powershell
# Run these in order:
Invoke-Command 'cursor-ai-copilot.verifyAPIKey'    # Should show ✓
Invoke-Command 'cursor-ai-copilot.listModels'      # Should list 122 models
```

## 📞 Common Questions

**Q: Which model should I use?**  
A: Start with `gpt-4o` for balance, or `gpt-3.5-turbo` for speed.

**Q: Is my API key safe?**  
A: Yes! VS Code Secrets encrypts it. Never commit it to git.

**Q: Can I use a custom API endpoint?**  
A: Yes! Set `cursor-ai-copilot.apiEndpoint` in settings.

**Q: How many models are available?**  
A: 122 models from OpenAI, 24 in the quick selector.

**Q: Will new models be added automatically?**  
A: The extension fetches from OpenAI's live API automatically.

**Q: Does it work offline?**  
A: No, it requires internet to connect to OpenAI's API.

---

**Start using it now:**
1. Install extension
2. Add API key
3. Press `Ctrl+Shift+M` to pick a model
4. Use `Ctrl+Shift+A` to open chat
5. Enjoy! 🎉
