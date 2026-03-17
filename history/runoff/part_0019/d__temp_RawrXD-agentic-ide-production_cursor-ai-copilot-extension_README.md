# Cursor AI Copilot VS Code Extension

> An advanced AI-powered code assistant integrated with ChatGPT for VS Code, bringing intelligent code generation, analysis, and optimization capabilities to your editor.

## Features

- 🤖 **AI-Powered Code Generation** - Generate code from natural language descriptions
- 🔍 **Code Analysis** - Analyze code for issues and get suggestions
- ♻️ **Code Refactoring** - Refactor code with AI-powered improvements
- ⚡ **Code Optimization** - Optimize code for performance
- 💬 **Interactive Chat** - Chat with AI about your code
- 🎯 **Inline Completion** - Get AI-powered code completion suggestions
- 📊 **Hover Analysis** - Understand code with hover tooltips
- 🔐 **Secure Authentication** - Safe API key storage using VS Code secrets

## Installation

1. Install the extension from the VS Code Marketplace
2. Open the Command Palette (`Ctrl+Shift+P`)
3. Run `Cursor AI: Authenticate with ChatGPT`
4. Enter your OpenAI API key
5. Start using AI features!

## Usage

### Commands

- **Ctrl+Shift+A** - Open AI Chat panel
- **Ctrl+Shift+E** - Explain selected code
- **Ctrl+Shift+R** - Refactor selected code
- Right-click menu options for code operations

### Chat Interface

Open the chat panel with `Ctrl+Shift+A` to:
- Ask questions about your code
- Get explanations
- Discuss programming concepts
- Debug issues

### Code Operations

Select any code and:
- **Explain** - Get a detailed explanation
- **Refactor** - Get refactored version with improvements
- **Optimize** - Get performance-optimized version
- **Generate** - Create new code from description

### Hover Analysis

Hover over any identifier to get quick information about:
- Builtin keywords and functions
- Variable usage context
- API documentation

## Configuration

Open Settings and search for "Cursor AI Copilot":

```json
{
  "cursor-ai-copilot.apiEndpoint": "https://api.openai.com/v1",
  "cursor-ai-copilot.model": "gpt-4",
  "cursor-ai-copilot.temperature": 0.7,
  "cursor-ai-copilot.maxTokens": 2048,
  "cursor-ai-copilot.enableTelemetry": true
}
```

## API Key Setup

1. Get your API key from [OpenAI](https://platform.openai.com/api-keys)
2. Run `Cursor AI: Authenticate with ChatGPT`
3. Paste your key (it will be stored securely)

## Security

- API keys are stored in VS Code's secure storage
- Keys are never logged or transmitted insecurely
- All API calls use HTTPS

## Troubleshooting

### "Not authenticated" error
- Run `Cursor AI: Authenticate with ChatGPT` again
- Verify your API key is valid

### Completions not showing
- Check if API endpoint is correct in settings
- Verify your API key has access to the model

### Slow responses
- Increase `maxTokens` in settings
- Check your internet connection
- Try a simpler prompt

## Requirements

- VS Code 1.80.0 or higher
- Active OpenAI API key

## License

MIT - See LICENSE file

## Support

For issues and feature requests, visit: https://github.com/RawrXD/cursor-ai-copilot

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

**Enjoy coding with AI assistance!** 🚀
