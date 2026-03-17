# Cursor AI Copilot Extension - Development Guide

## Project Structure

```
cursor-ai-copilot-extension/
├── src/
│   ├── extension.ts              # Extension activation and main entry point
│   ├── ai/
│   │   └── AICopilotProvider.ts  # OpenAI API integration
│   ├── auth/
│   │   └── WebAuthManager.ts     # Authentication and token management
│   ├── ui/
│   │   └── ChatPanel.ts          # Chat interface webview
│   ├── analysis/
│   │   └── CodeAnalyzer.ts       # Code analysis and hover insights
│   ├── completion/
│   │   └── CompletionProvider.ts # AI-powered completions
│   ├── utils/
│   │   └── Logger.ts             # Logging utility
│   └── telemetry/
│       └── Telemetry.ts          # Analytics and telemetry
├── package.json                  # Extension manifest and dependencies
├── tsconfig.json                 # TypeScript configuration
├── .eslintrc.json               # ESLint configuration
├── README.md                     # User documentation
├── CHANGELOG.md                  # Version history
└── setup.bat / setup.sh         # Setup scripts

```

## Getting Started

### Prerequisites

- Node.js 16+
- npm or yarn
- Visual Studio Code 1.80.0+
- TypeScript knowledge

### Installation

1. Clone the repository
   ```bash
   git clone https://github.com/RawrXD/cursor-ai-copilot-extension.git
   cd cursor-ai-copilot-extension
   ```

2. Run setup script
   ```bash
   # Windows
   setup.bat

   # macOS/Linux
   bash setup.sh
   ```

3. Install dependencies
   ```bash
   npm install
   ```

## Development

### Build Commands

```bash
# Build for development
npm run esbuild

# Watch mode (auto-rebuild on changes)
npm run esbuild-watch

# Lint code
npm run lint

# Build for production (minified)
npm run vscode:prepublish
```

### Running in Development

1. Open the extension folder in VS Code
2. Press `F5` to start debugging
3. A new VS Code window will open with the extension loaded
4. Use the Command Palette (`Ctrl+Shift+P`) to test commands

### File Organization

#### `src/extension.ts`
- Main entry point for the extension
- Handles activation and deactivation
- Registers all commands
- Initializes components
- Sets up the status bar

#### `src/ai/AICopilotProvider.ts`
- Wrapper around OpenAI API
- Implements core AI features:
  - `explainCode()` - Code explanation
  - `refactorCode()` - Code refactoring
  - `generateCode()` - Code generation
  - `optimizeCode()` - Performance optimization
  - `chat()` - General conversation
- Handles API timeouts and retries
- Manages API configuration

#### `src/auth/WebAuthManager.ts`
- Manages authentication state
- Stores API keys securely using VS Code secrets
- Provides token retrieval and refresh
- Handles logout

#### `src/ui/ChatPanel.ts`
- Creates and manages the chat webview
- Handles message sending/receiving
- Renders HTML interface for chat
- Manages conversation history

#### `src/analysis/CodeAnalyzer.ts`
- Analyzes code on hover
- Caches analysis results
- Provides builtin keyword definitions
- Uses AI for custom analysis
- Integrated with hover provider

#### `src/completion/CompletionProvider.ts`
- Implements VS Code completion provider
- Throttles AI requests for performance
- Uses AI to generate suggestions
- Integrates with code editor

#### `src/utils/Logger.ts`
- Structured logging with context
- Multiple log levels (debug, info, warn, error)
- Timestamp and context information

#### `src/telemetry/Telemetry.ts`
- Tracks command usage
- Monitors feature usage
- Collects performance metrics
- Event storage and retrieval

## Configuration

### Extension Settings

Users can configure via VS Code Settings:

```json
{
  "cursor-ai-copilot.apiEndpoint": "https://api.openai.com/v1",
  "cursor-ai-copilot.model": "gpt-4",
  "cursor-ai-copilot.temperature": 0.7,
  "cursor-ai-copilot.maxTokens": 2048,
  "cursor-ai-copilot.enableTelemetry": true,
  "cursor-ai-copilot.completionTriggerCharacters": ". "
}
```

### Environment Variables

Create a `.env` file for development:

```
OPENAI_API_KEY=your-api-key-here
VSCODE_API_ENDPOINT=https://api.openai.com/v1
```

## API Integration

### OpenAI Integration

The extension uses OpenAI's GPT API for all AI features.

#### Rate Limiting
- Completions: 500ms throttle between requests
- Cache: 1 hour for analysis results

#### Timeout
- Default: 30 seconds per request
- Configurable via settings

#### Error Handling
- Automatic retry on network errors
- User-friendly error messages
- Logging for debugging

## Security Considerations

### API Key Storage
- Keys stored in VS Code's secure storage
- Never logged or exported
- Encrypted at rest

### API Communication
- HTTPS only
- No keys in URLs
- Authorization headers only

### Input Validation
- Validates API key format
- Sanitizes user inputs
- Prevents prompt injection

## Testing

### Manual Testing

1. **Authentication**
   - Run: `Cursor AI: Authenticate with ChatGPT`
   - Verify key storage
   - Test logout: `Cursor AI: Logout`

2. **Code Explain**
   - Select code
   - Run: `Cursor AI: Explain Code` (Ctrl+Shift+E)
   - Verify explanation in chat

3. **Code Refactor**
   - Select code
   - Run: `Cursor AI: Refactor Code` (Ctrl+Shift+R)
   - Verify refactored code in chat

4. **Code Generate**
   - Run: `Cursor AI: Generate Code`
   - Enter prompt
   - Verify generated code

5. **Chat Interface**
   - Open: `Cursor AI: Open Chat` (Ctrl+Shift+A)
   - Send messages
   - Verify responses

6. **Completions**
   - Type in editor
   - Verify suggestions appear
   - Select suggestion

7. **Hover Analysis**
   - Hover over identifiers
   - Verify analysis appears

## Building for Distribution

### Package the Extension

```bash
# Install vsce globally
npm install -g vsce

# Package
vsce package

# Publish to marketplace
vsce publish
```

### Version Updates

Update version in:
1. `package.json`
2. `CHANGELOG.md`

## Debugging

### Enable Debug Logging

In the Debug Console:
```javascript
// Set debug level
localStorage.setItem('debug', 'cursor-ai-*')
```

### VS Code Developer Tools

Press `Ctrl+Shift+D` to open debugger with breakpoints.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes
4. Lint: `npm run lint`
5. Build: `npm run esbuild`
6. Test thoroughly
7. Submit PR

## Troubleshooting

### Build Errors

**Issue**: `Cannot find module 'vscode'`
- **Solution**: `npm install --save-dev @types/vscode`

**Issue**: `TypeScript compilation errors`
- **Solution**: Check `tsconfig.json` settings

### Runtime Errors

**Issue**: Extension doesn't load
- **Solution**: Check Developer Tools (Help → Toggle Developer Tools)

**Issue**: API calls timeout
- **Solution**: Increase `requestTimeout` in settings

### Performance Issues

**Issue**: Completions are slow
- **Solution**: Check `completionThrottle` setting

**Issue**: Chat responses lag
- **Solution**: Monitor API rate limits

## Resources

- [VS Code Extension API](https://code.visualstudio.com/api)
- [OpenAI API Docs](https://platform.openai.com/docs)
- [TypeScript Docs](https://www.typescriptlang.org/docs)
- [esbuild Docs](https://esbuild.github.io)

## License

MIT - See LICENSE file

## Support

For issues:
1. Check [Troubleshooting](#troubleshooting) section
2. Review [GitHub Issues](https://github.com/RawrXD/cursor-ai-copilot/issues)
3. Create new issue with details

## Authors

- [Your Name] - Initial development

---

Happy coding! 🚀
