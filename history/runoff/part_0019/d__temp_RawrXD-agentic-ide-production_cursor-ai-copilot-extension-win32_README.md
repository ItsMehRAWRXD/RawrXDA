# RawrXD Agentic IDE - Win32 Integration Guide

## Overview
Production-ready **Cursor AI Copilot** extension for Win32 agentic IDE with gpt-5.2-pro integration.

## Quick Start

### 1. Environment Setup
```powershell
# Set API credentials
$env:OPENAI_API_KEY = "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"
$env:OPENAI_API_BASE = "https://api.openai.com/v1"
$env:OPENAI_MODEL = "gpt-5.2-pro"
```

### 2. Install Dependencies
```bash
npm install
```

### 3. Run Agentic Workflow
```powershell
# Via PowerShell orchestrator
.\orchestration\agentic-orchestrator.ps1 -Objective "Build REST API" -Language "typescript" -Mode "workflow"

# Or directly in Node.js
node -e "
const Agent = require('./orchestration/agent.js');
const agent = new Agent({});
agent.executeAgenticWorkflow('Your objective here', {language: 'typescript'});
"
```

## Architecture

### Directory Structure
```
cursor-ai-copilot-extension-win32/
├── config/
│   └── settings.json              # IDE configuration with API key
├── modules/
│   └── openai-client.js           # Core OpenAI client + model management
├── orchestration/
│   ├── agent.js                   # Agentic workflow orchestrator
│   └── agentic-orchestrator.ps1   # PowerShell CLI launcher
├── manifest.json                  # Extension metadata
└── README.md                       # This file
```

### Module Overview

#### `openai-client.js`
- Direct OpenAI API client with gpt-5.2-pro support
- Model discovery & caching
- Agentic primitives: `plan()`, `generateCode()`, `reflect()`
- Batch processing & streaming
- Error handling + retry logic

**Example:**
```javascript
const OpenAI = require('./modules/openai-client.js');
const client = new OpenAI({ model: 'gpt-5.2-pro' });

const plan = await client.plan('Build a file parser', []);
const code = await client.generateCode(spec, 'typescript');
const review = await client.reflect(code, criteria);
```

#### `agent.js`
- **AgentOrchestrator**: Full Plan/Code/Verify workflow
- Iterative refinement on verification failure
- Logging & telemetry
- Streaming execution for real-time UI
- Batch execution support

**Example:**
```javascript
const Agent = require('./orchestration/agent.js');
const agent = new Agent({ maxIterations: 10 });

const result = await agent.executeAgenticWorkflow(
  'Build a user authentication module',
  { language: 'typescript', contextBlocks: [] }
);

console.log(result.code); // Generated code
console.log(result.verification.score); // Quality score
```

#### `settings.json`
- API endpoint & credentials
- Model priority list (gpt-5.2-pro → gpt-4o-mini fallback)
- Agentic workflow tuning (iterations, timeout, refinement)
- Win32-specific config (credential storage, registry hooks)

## Agentic Workflow

### Phase 1: Planning (Reasoning)
Uses gpt-5.2-pro to break down objective into actionable steps:
```json
{
  "goal": "Build a REST API",
  "steps": [
    {"step": 1, "action": "Define routes", "reasoning": "..."},
    {"step": 2, "action": "Implement handlers", "reasoning": "..."}
  ],
  "confidence": 0.95
}
```

### Phase 2: Code Generation
Iteratively generates code for each plan step:
- Maintains context from previous steps
- Respects language best practices
- Includes error handling & logging

### Phase 3: Verification
Reflects on generated code against criteria:
- Plan adherence
- Error handling
- Code quality
- Production readiness

### Fallback Refinement
If verification fails:
1. Extract improvement suggestions
2. Refine code with gpt-5.2-pro
3. Verify again (up to 1 attempt)

## Configuration

### settings.json
```json
{
  "cursor-ai-copilot": {
    "apiKey": "sk-proj-...",
    "model": "gpt-5.2-pro",
    "maxTokens": 4096,
    "agenticConfig": {
      "maxAgentIterations": 10,
      "agentTimeout": 120000,
      "enableFallback": true
    }
  }
}
```

### Environment Variables
- `OPENAI_API_KEY` - API key (overrides config)
- `OPENAI_API_BASE` - API endpoint
- `OPENAI_MODEL` - Default model

## Usage Patterns

### Single Workflow Execution
```powershell
.\orchestration\agentic-orchestrator.ps1 `
  -Objective "Build a file uploader" `
  -Language "typescript" `
  -Mode "workflow"
```

### Batch Execution
```javascript
const objectives = [
  'Build user auth',
  'Build file upload',
  'Build notifications'
];

const results = await agent.executeBatch(objectives, context);
```

### Streaming Execution (Real-time UI)
```javascript
for await (const update of agent.executeAgenticWorkflowStreaming(objective, context)) {
  console.log(update); // { stage, status, result, ... }
}
```

## Monitoring & Logging

### Workflow Logs
Stored in: `%APPDATA%\RawrXD\Cursor-AI-Copilot\logs\`

Each execution generates:
- `workflow_TIMESTAMP.json` - Full execution result
- Includes plan, code, verification, duration, metrics

### Event Listeners
```javascript
agent.on('info', msg => console.log(msg));
agent.on('debug', msg => console.debug(msg));
agent.on('warn', msg => console.warn(msg));
agent.on('error', msg => console.error(msg));
agent.on('metric', metric => recordMetric(metric)); // Prometheus, etc.
```

## Advanced

### Model Priority & Fallback
When primary model is unavailable:
```
gpt-5.2-pro  (primary, best reasoning)
    ↓
gpt-5.1-codex-max  (code-optimized)
    ↓
gpt-4o  (fast, capable)
    ↓
gpt-4o-mini  (cost-effective)
```

### Custom Context
```javascript
const result = await agent.executeAgenticWorkflow(objective, {
  language: 'typescript',
  contextBlocks: [
    'Project uses Express.js',
    'Database: PostgreSQL',
    'Auth: JWT',
    'Testing: Jest'
  ],
  examples: [
    codeExample1,
    codeExample2
  ]
});
```

### Retry & Error Recovery
Automatic retry on:
- API rate limits (429)
- Server errors (500+)
- Network timeouts
- Model availability issues

### Performance Tuning
```json
{
  "agenticConfig": {
    "maxAgentIterations": 5,        // Fewer = faster, less thorough
    "agentTimeout": 60000,           // 1 min instead of 2 min
    "enableFallback": false          // Skip refinement step
  }
}
```

## Troubleshooting

### API Key Invalid
```powershell
$env:OPENAI_API_KEY = "your-new-key"
```

### Model Not Available
Check available models:
```javascript
const models = await client.getAvailableModels();
console.log(models);
```

### Timeout Issues
Increase `agentTimeout`:
```json
"agentTimeout": 300000  // 5 minutes
```

### Logs Not Generated
Verify log path exists and is writable:
```powershell
Test-Path "$env:APPDATA\RawrXD\Cursor-AI-Copilot\logs"
```

## Production Deployment

1. **Secure API Key**
   - Store in Windows Credential Manager
   - Never commit to version control
   - Rotate periodically

2. **Monitor & Alert**
   - Stream metrics to observability platform
   - Alert on workflow failures
   - Track latency & cost

3. **Rate Limiting**
   - Respect OpenAI rate limits
   - Use batch processing for bulk operations
   - Implement backoff strategy

4. **Caching**
   - Cache model list (1 hour TTL)
   - Cache common completions
   - Reduce redundant API calls

## License
MIT

## Support
GitHub Issues: https://github.com/RawrXD/cursor-ai-copilot/issues
