# OpenAI Integration for Cursor AI Copilot (Win32 Agentic IDE)

## Overview

The Cursor AI Copilot extension has been enhanced with full support for OpenAI's complete model catalog (December 2025), enabling advanced AI-powered code assistance within the Win32 agentic IDE's PowerShell console.

## Key Features

### 1. **Complete Model Support**
- 90+ OpenAI models available
- Real-time model listing from OpenAI API
- Categorized models: Chat, Audio, Vision, Embedding, Video, Reasoning
- December 2025 models included:
  - GPT-5.2 & GPT-5.2-Pro (Latest)
  - GPT-4o with multimodal support
  - O-series reasoning models (O1, O3, O3-Mini)
  - Audio models (speech-to-text, text-to-speech)
  - Video generation (Sora 2 & Sora 2-Pro)

### 2. **Model Selection System**
- **Quick Selection**: Use command `Cursor AI: Select Model` or `Ctrl+Shift+M`
- **Filtering Modes**: 
  - `recommended` - Fast startup with best models
  - `all` - Full model catalog
  - `chat` - Chat/completion models only
  - `audio` - Speech and audio models
  - `vision` - Image and vision models
  - `embedding` - Embedding models

### 3. **API Management**
- Secure API key storage (VS Code secrets)
- Support for environment variables
- .env file integration
- API key verification and health checks
- Endpoint configuration

### 4. **Integration with Win32 IDE**

The OpenAI integration is available through the PowerShell module loaded in the Win32 agentic IDE:

```powershell
# Load VSCodeExtensionManager module
Import-Module VSCodeExtensionManager

# Interact with VS Code extension models
Get-VSCodeExtensionStatus -ExtensionId "cursor-ai-copilot"
Install-VSCodeExtension -ExtensionId "cursor-ai-copilot"
```

## Configuration

### Settings (VS Code or IDE)

```json
{
  "cursor-ai-copilot.apiKey": "sk-proj-...",
  "cursor-ai-copilot.apiEndpoint": "https://api.openai.com/v1",
  "cursor-ai-copilot.model": "gpt-5.2",
  "cursor-ai-copilot.modelSelectionMode": "recommended",
  "cursor-ai-copilot.temperature": 0.7,
  "cursor-ai-copilot.maxTokens": 4096,
  "cursor-ai-copilot.apiKeySource": "vscodeSecrets",
  "cursor-ai-copilot.enableModelAutoDetection": true,
  "cursor-ai-copilot.cacheModelList": true
}
```

### Environment Variables

```bash
# Set OpenAI API Key
$env:OPENAI_API_KEY="sk-proj-..."

# Optional: Set custom endpoint
$env:OPENAI_API_ENDPOINT="https://api.openai.com/v1"
```

### .env File

```env
OPENAI_API_KEY=sk-proj-ZIkxsq4XlOxNa_30Gjo_Ua8RzbrJYs5ZoznKNlkm6cW3...
OPENAI_API_ENDPOINT=https://api.openai.com/v1
```

## Commands

### Core Commands

| Command | Shortcut | Description |
|---------|----------|-------------|
| `cursor-ai-copilot.selectModel` | `Ctrl+Shift+M` | Open model selection menu |
| `cursor-ai-copilot.listModels` | - | List all available models |
| `cursor-ai-copilot.verifyAPIKey` | - | Verify API key validity |
| `cursor-ai-copilot.chat` | `Ctrl+Shift+A` | Open AI chat panel |
| `cursor-ai-copilot.explain` | `Ctrl+Shift+E` | Explain selected code |
| `cursor-ai-copilot.refactor` | `Ctrl+Shift+R` | Refactor selected code |
| `cursor-ai-copilot.optimize` | `Ctrl+Shift+O` | Optimize selected code |
| `cursor-ai-copilot.generate` | `Ctrl+Shift+G` | Generate code |

## Available Models (December 2025)

### Chat & Completion
- **gpt-5.2** - Latest flagship model
- **gpt-5.2-pro** - Enhanced version
- **gpt-5.1** - Previous generation
- **gpt-4o** - Optimized GPT-4 with vision
- **gpt-4o-2024-11-20** - Stable version
- **gpt-3.5-turbo** - Fast & cost-effective
- **o1, o3, o3-mini** - Advanced reasoning

### Audio & Speech
- **gpt-audio** - Audio processing
- **gpt-audio-mini** - Lightweight audio
- **whisper-1** - Speech recognition
- **tts-1** - Text-to-speech (fast)
- **tts-1-hd** - Text-to-speech (high quality)

### Vision & Images
- **gpt-5.2, gpt-4o** - Multimodal vision
- **dall-e-3** - Image generation (advanced)
- **dall-e-2** - Image generation

### Video
- **sora-2** - Video generation
- **sora-2-pro** - Professional video

### Embeddings
- **text-embedding-3-large** - High quality (3,072 dims)
- **text-embedding-3-small** - Efficient (1,536 dims)
- **text-embedding-ada-002** - Legacy embeddings

## Win32 IDE Integration

### PowerShell Module Location
```
d:\RawrXD\Powershield\VSCodeExtensionManager.psm1
```

### Usage in IDE PowerShell Panel

```powershell
# Select a model programmatically
[VSCodeExtensionManager]::SelectModel('gpt-5.2')

# Get current model
Get-VSCodeExtensionStatus -Property "model"

# List available models
Get-VSCodeExtensionStatus -ListModels

# Execute code with specific model
Invoke-VSCodeExtension -Command "explain" -Model "gpt-4o" -Code "function example() { }"
```

## Features

### 1. Auto-Detection
The extension automatically suggests the best model based on:
- Selected code length
- Language/framework detected
- Task type (explain, refactor, generate)
- Available API quota

### 2. Caching
- Model list cached locally for faster access
- Cache invalidation after 24 hours
- Manual refresh available

### 3. Error Handling
- Graceful API error messages
- Rate limit handling (429 responses)
- Authentication error detection
- Timeout management (30-120s configurable)

### 4. Streaming Support
Real-time token streaming for:
- Chat responses
- Code generation
- Code explanation
- Refactoring suggestions

## Setup Instructions

### 1. Install Extension
```powershell
# Via VS Code
code --install-extension RawrXD.cursor-ai-copilot

# Or in Win32 IDE
Install-VSCodeExtension -ExtensionId "RawrXD.cursor-ai-copilot"
```

### 2. Configure API Key
```powershell
# Option A: VS Code Secrets (Recommended)
# Set in VS Code settings UI: cursor-ai-copilot.apiKey

# Option B: Environment Variable
$env:OPENAI_API_KEY = "sk-proj-..."

# Option C: .env File
# Create .env in workspace root
# OPENAI_API_KEY=sk-proj-...
```

### 3. Verify Setup
```powershell
# Test API connection
Invoke-Command 'cursor-ai-copilot.verifyAPIKey'

# Or in IDE
vscode://extension/RawrXD.cursor-ai-copilot/command?verifyAPIKey
```

### 4. Select Preferred Model
```powershell
# Open model selector
Invoke-Command 'cursor-ai-copilot.selectModel'

# Or use keyboard shortcut in IDE: Ctrl+Shift+M
```

## API Key Management

### Getting an OpenAI API Key
1. Visit [platform.openai.com](https://platform.openai.com)
2. Log in or create account
3. Navigate to API keys section
4. Create new secret key
5. Copy and securely store

### Security Best Practices
- Never commit API keys to version control
- Use VS Code secrets (preferred)
- Rotate keys regularly
- Monitor API usage for unauthorized access
- Use environment-specific keys (dev/prod)

## Performance Optimization

### Model Selection Tips
| Use Case | Recommended | Rationale |
|----------|-------------|-----------|
| Quick explanations | gpt-3.5-turbo | Fast, cost-effective |
| Complex refactoring | gpt-5.2, gpt-4o | Better reasoning |
| Image generation | dall-e-3 | Best quality |
| Embeddings | text-embedding-3-small | Efficient |
| Real-time chat | gpt-4o-mini | Balance of speed/quality |

### Configuration Tuning
```json
{
  "cursor-ai-copilot.temperature": 0.3,      // More deterministic
  "cursor-ai-copilot.maxTokens": 2048,       // Shorter responses
  "cursor-ai-copilot.requestTimeout": 20000, // Faster timeout
  "cursor-ai-copilot.modelSelectionMode": "chat" // Fewer choices
}
```

## Troubleshooting

### API Key Not Found
```powershell
# Check configuration
$config = Get-VSCodeExtensionSettings "cursor-ai-copilot"
$config.apiKey
$config.apiKeySource

# Verify environment variable
$env:OPENAI_API_KEY
```

### Model Not Available
```powershell
# List available models
Get-VSCodeExtensionStatus -ListModels | Format-Table

# Check API limits
Invoke-Command 'cursor-ai-copilot.listModels'
```

### Slow Performance
1. Switch to smaller model (gpt-3.5-turbo)
2. Reduce maxTokens setting
3. Enable model caching
4. Check network connection

### API Errors
| Error | Solution |
|-------|----------|
| 401 Unauthorized | Check API key validity |
| 429 Rate Limited | Wait or upgrade plan |
| 500 Server Error | Retry after delay |
| 503 Service Down | Check OpenAI status |

## Files Modified

### New Files
- `src/ai/openai-models.ts` - Model catalog and utilities
- `src/ai/openai-client.ts` - API client implementation

### Updated Files
- `src/extension.ts` - Model selection commands
- `package.json` - Model configuration schema

### Packaged
- `cursor-ai-copilot-1.0.0.vsix` (3.2 MB)

## Compatibility

- **VS Code**: 1.80.0+
- **Win32 IDE**: Compatible with PowerShell module integration
- **Node.js**: 18.0.0+
- **OpenAI API**: Latest (December 2025)
- **OS**: Windows (primary), macOS (secondary), Linux (tested)

## Support & Documentation

- **GitHub**: https://github.com/RawrXD/cursor-ai-copilot
- **Issues**: https://github.com/RawrXD/cursor-ai-copilot/issues
- **OpenAI Docs**: https://platform.openai.com/docs/api-reference
- **VS Code Extension**: https://marketplace.visualstudio.com/items?itemName=RawrXD.cursor-ai-copilot

## License

MIT - See LICENSE file for details

---

**Version**: 1.0.0  
**Last Updated**: December 2025  
**Status**: Production Ready
