# AI Foundation Editor

A VS Code extension with multi-AI provider support for chat, code review, and suggestions.

## Features

- **Multi-AI Support**: OpenAI, Anthropic Claude, and Echo providers
- **Unified Editor**: Chat, review, and suggest with any AI provider
- **CLI Interface**: Command-line tool for AI interactions
- **Secure API Key Management**: Store keys securely in VS Code

## Setup

1. Install dependencies:
   ```bash
   npm install
   ```

2. Compile TypeScript:
   ```bash
   npm run compile
   ```

3. Set API keys using Command Palette:
   - `Ctrl+Shift+P` → "Set AI API Key"
   - Choose provider and enter your key

## Usage

### VS Code Extension
- `Ctrl+Shift+P` → "Open Unified Editor" - Multi-AI chat interface
- `Ctrl+Shift+P` → "Open AI Notepad" - Simple notepad with AI

### CLI Tool
```bash
npx ts-node src/cli.ts

# Commands:
/key openai sk-your-key-here
/key anthropic your-anthropic-key
/switch openai
/complete "function fibonacci(n) {"
/providers
/exit
```

## Providers

- **Echo**: Simple echo for testing
- **OpenAI**: GPT-3.5-turbo chat completions
- **Anthropic**: Claude-3-sonnet for advanced reasoning

## Development

```bash
npm run watch    # Watch mode compilation
npm run compile  # One-time compilation
```