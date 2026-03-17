# Cursor Multi-AI Assistant Extension

A powerful VS Code/Cursor extension that provides access to multiple AI services with intelligent fallback support.

## Features

### Core Functionality
- **Multi-AI Integration**: Connect to Amazon Q, Claude, ChatGPT, Gemini, and Kimi
- **Smart Fallback**: Works even when your Multi-AI Aggregator server is unavailable
- **Code Context**: Right-click on selected code for AI assistance
- **Status Indicator**: Real-time connection status in the status bar

### AI Services
- **Amazon Q**: AWS-specific assistance and guidance
- **Claude**: Advanced reasoning and code analysis
- **ChatGPT**: General programming help and explanations
- **Gemini**: Google's AI capabilities
- **Kimi**: Additional AI perspective and insights

### Commands Available

#### General Commands
- `Ctrl+Shift+I` - Ask any question to your preferred AI
- `Ctrl+Shift+A` - Ask all AIs simultaneously for comparison
- `Ctrl+Shift+Q` - Ask Amazon Q specifically

#### Code-Specific Commands (Right-click on selected code)
- **Explain This Code**: Get detailed explanations of selected code
- **Optimize This Code**: Improve performance and readability
- **Debug This Code**: Find potential bugs and issues
- **Add Comments**: Generate comprehensive code comments
- **Generate Unit Tests**: Create test suites for your code

#### Individual AI Commands
- Ask Amazon Q
- Ask Claude
- Ask ChatGPT
- Ask Gemini
- Ask Kimi

## Installation

### Option 1: Install from VSIX (Recommended)
```bash
# For VS Code
code --install-extension cursor-multi-ai-1.0.0.vsix

# For Cursor (if different)
cursor --install-extension cursor-multi-ai-1.0.0.vsix
```

### Option 2: Manual Installation
1. Open VS Code/Cursor
2. Go to Extensions (Ctrl+Shift+X)
3. Click "..." menu → "Install from VSIX..."
4. Select the `cursor-multi-ai-1.0.0.vsix` file

## Configuration

Open VS Code/Cursor settings and search for "Multi-AI" to configure:

### Settings
- **Server URL**: URL of your Multi-AI Aggregator server (default: http://localhost:3003)
- **Fallback Mode**: Enable built-in responses when server is unavailable (default: true)
- **Preferred AI**: Choose your default AI service (default: amazonq)
- **Timeout**: Request timeout in milliseconds (default: 30000)

### Status Bar Indicators
- `$(robot) Multi-AI ✓` - Server connected and working
- `$(robot) Multi-AI ⚠` - Using fallback mode (server unavailable)
- `$(robot) Multi-AI ✗` - Error state

## Usage Examples

### 1. Ask a General Question
1. Press `Ctrl+Shift+I`
2. Type your question: "How do I implement JWT authentication in Node.js?"
3. Get AI-powered response

### 2. Explain Selected Code
1. Select code in your editor
2. Right-click → "Explain This Code"
3. Get detailed explanation of the code's functionality

### 3. Compare All AIs
1. Press `Ctrl+Shift+A`
2. Ask: "What's the best way to handle async operations in JavaScript?"
3. Get responses from all 5 AI services for comparison

### 4. Optimize Code
1. Select inefficient code
2. Right-click → "Optimize This Code"
3. Get improved version with explanations

## Fallback Mode

When your Multi-AI Aggregator server is unavailable, the extension provides helpful fallback responses that:
- Explain why the server is unavailable
- Provide troubleshooting steps
- Show what the AI would do if connected
- Include the selected code context

## Troubleshooting

### Server Connection Issues
1. Ensure your Multi-AI Aggregator server is running at the configured URL
2. Check firewall settings
3. Verify API keys are properly configured
4. Test server connectivity using the "Test Connection" option

### Extension Not Working
1. Reload VS Code/Cursor window (Ctrl+Shift+P → "Developer: Reload Window")
2. Check the Output panel for "Multi-AI Assistant" logs
3. Verify extension is enabled in Extensions panel

### Performance Issues
1. Increase timeout in settings if requests are timing out
2. Disable fallback mode if you prefer server-only operation
3. Use specific AI commands instead of "Ask All" for faster responses

## Development

### Building from Source
```bash
cd cursor-multi-ai-extension
npm install
npm run compile
vsce package
```

### Project Structure
```
cursor-multi-ai-extension/
├── src/
│   └── extension.ts          # Main extension logic
├── out/
│   └── extension.js          # Compiled JavaScript
├── package.json              # Extension manifest
├── tsconfig.json            # TypeScript configuration
└── cursor-multi-ai-1.0.0.vsix # Packaged extension
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This extension is provided as-is for educational and development purposes.

## Support

For issues and questions:
1. Check the Output panel for error logs
2. Verify your Multi-AI Aggregator server configuration
3. Test connection using the built-in test feature
4. Review the fallback responses for troubleshooting guidance
