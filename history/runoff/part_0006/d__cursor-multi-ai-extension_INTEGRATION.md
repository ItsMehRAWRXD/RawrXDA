# Multi-AI Extension Integration Guide

## System Components

### 1. Multi-AI Aggregator Server (Port 3003)
**Location**: `D:\cursor-multi-ai-extension\server.js`

**Purpose**: Central routing server for multiple AI services

**Endpoints**:
- `GET /health` - Health check
- `GET /services` - List available AI services
- `POST /chat/:service` - Query specific AI service
- `POST /chat/all` - Query all AI services

**Start Server**:
```bash
cd D:\cursor-multi-ai-extension
node server.js
```

Or use batch file:
```bash
START_SERVER.bat
```

### 2. Amazon Q Private IDE Extension
**Location**: `D:\cursor-multi-ai-extension\amazonq-private-ide-extension\`

**Purpose**: Direct AWS Bedrock integration for private IDEs

**Architecture**:
```
extension.js (Entry Point)
├── core/
│   ├── DependencyContainer.js (IoC Container)
│   ├── EventBus.js (Event System)
│   └── BaseService.js (Base Class)
├── services/
│   ├── AmazonQService.js (Application Logic)
│   └── ConversationManager.js (Chat History)
├── domain/
│   ├── Message.js (Entity)
│   └── CodeContext.js (Entity)
├── infrastructure/
│   ├── BedrockClient.js (AWS API)
│   └── CacheManager.js (Caching)
└── presentation/
    └── CommandHandler.js (UI Commands)
```

### 3. VS Code/Cursor Extension
**Location**: `D:\cursor-multi-ai-extension\src\extension.ts`

**Purpose**: Multi-AI integration for VS Code/Cursor

**Connection**: Connects to server at `http://localhost:3003`

## Integration Flow

### Request Flow
```
User Action (IDE)
    ↓
Extension Command
    ↓
CommandHandler (Presentation)
    ↓
AmazonQService (Application)
    ↓
BedrockClient (Infrastructure)
    ↓
AWS Bedrock API
    ↓
Response Processing
    ↓
UI Update
```

### Multi-AI Flow
```
User Action (IDE)
    ↓
Extension Command
    ↓
HTTP Request to localhost:3003
    ↓
Multi-AI Server Router
    ↓
┌─────────┬─────────┬─────────┬─────────┐
│ Amazon Q│ Claude  │ ChatGPT │ Gemini  │
└─────────┴─────────┴─────────┴─────────┘
    ↓
Aggregated Response
    ↓
Extension UI
```

## Configuration

### Environment Variables
```bash
# AWS Credentials (for Amazon Q)
AWS_ACCESS_KEY_ID=your_key
AWS_SECRET_ACCESS_KEY=your_secret
AWS_REGION=us-east-1

# API Keys (for other services)
ANTHROPIC_API_KEY=your_key
OPENAI_API_KEY=your_key
GOOGLE_API_KEY=your_key
KIMI_API_KEY=your_key
```

### Extension Settings
```json
{
  "amazonq.awsRegion": "us-east-1",
  "amazonq.modelId": "anthropic.claude-3-haiku-20240307-v1:0",
  "amazonq.maxTokens": 2000,
  "amazonq.temperature": 0.1,
  "amazonq.timeout": 30000,
  "multiAI.serverUrl": "http://localhost:3003",
  "multiAI.preferredAI": "amazonq"
}
```

## Deployment

### Server Deployment
```bash
# Install dependencies
cd D:\cursor-multi-ai-extension
npm install

# Start server
node server.js

# Or use PM2 for production
npm install -g pm2
pm2 start server.js --name multi-ai-server
pm2 save
```

### Extension Installation

**VS Code/Cursor**:
```bash
cd D:\cursor-multi-ai-extension
npm install
npm run compile
code --install-extension cursor-multi-ai-1.0.0.vsix
```

**Private IDE**:
```bash
cd D:\cursor-multi-ai-extension\amazonq-private-ide-extension
npm install
npm run build
# Copy to IDE extensions folder
```

## Testing

### Test Server
```bash
# Health check
curl http://localhost:3003/health

# List services
curl http://localhost:3003/services

# Test Amazon Q
curl -X POST http://localhost:3003/chat/amazonq \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello"}'
```

### Test Extension
1. Open VS Code/Cursor
2. Press `Ctrl+Shift+Q` to ask Amazon Q
3. Select code and right-click for context menu
4. Use `Ctrl+Shift+I` for general questions

## Monitoring

### Server Logs
```bash
# View logs
tail -f server.log

# Or with PM2
pm2 logs multi-ai-server
```

### Extension Logs
- Open Output panel in IDE
- Select "Amazon Q" channel
- View request/response logs

## Troubleshooting

### Server Not Starting
- Check port 3003 is available
- Verify Node.js version (16+)
- Check dependencies installed

### AWS Connection Failed
- Verify AWS credentials configured
- Check IAM permissions for Bedrock
- Confirm region supports Bedrock

### Extension Not Connecting
- Ensure server is running on port 3003
- Check firewall settings
- Verify extension settings

## Architecture Benefits

### Separation of Concerns
- Presentation: UI/Commands
- Application: Business Logic
- Domain: Entities
- Infrastructure: External APIs

### Scalability
- Stateless design
- Horizontal scaling ready
- Load balancing support

### Maintainability
- Clear boundaries
- Dependency injection
- Event-driven architecture

### Testability
- Unit tests per layer
- Integration tests
- E2E tests
