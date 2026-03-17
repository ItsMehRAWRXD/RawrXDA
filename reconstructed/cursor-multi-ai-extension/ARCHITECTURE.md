# Multi-AI Extension - Technical Architecture

## System Architecture

### High-Level Architecture
```
┌──────────────────────────────────────────────────────────────┐
│                    IDE Extension Layer                        │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐ │
│  │ Cursor/VS Code │  │  Private IDE   │  │   DevMarket    │ │
│  └────────┬───────┘  └────────┬───────┘  └────────┬───────┘ │
└───────────┼──────────────────┼──────────────────┼───────────┘
            │                  │                  │
            └──────────────────┼──────────────────┘
                               │
┌──────────────────────────────┼──────────────────────────────┐
│                    Application Layer                          │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │           Multi-AI Aggregator Server (Port 3003)        │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │ │
│  │  │   Routing    │  │    Caching   │  │  Monitoring  │  │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  │ │
│  └─────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
┌───────────▼──────┐  ┌────────▼────────┐  ┌────▼──────────┐
│  AWS Bedrock     │  │  Anthropic API  │  │  OpenAI API   │
│  (Amazon Q)      │  │  (Claude)       │  │  (ChatGPT)    │
└──────────────────┘  └─────────────────┘  └───────────────┘
            │                  │                  │
┌───────────▼──────┐  ┌────────▼────────┐
│  Google Gemini   │  │  Kimi AI        │
└──────────────────┘  └─────────────────┘
```

## Layered Architecture Pattern

### 1. Presentation Layer
**Responsibility**: User interaction and command handling

**Components**:
- CommandHandler: Registers IDE commands
- WebviewProvider: Chat interface UI
- StatusBarManager: Status indicators
- NotificationService: User feedback

**Technologies**: VS Code API, HTML/CSS/JavaScript

### 2. Application Layer
**Responsibility**: Business logic orchestration

**Components**:
- AmazonQService: AI interaction orchestration
- ConversationManager: Chat history management
- CodeAnalysisService: Code understanding
- ConfigurationService: Settings management

**Patterns**: Service Layer, Facade Pattern

### 3. Domain Layer
**Responsibility**: Core business entities and logic

**Entities**:
- Message: Chat message representation
- CodeContext: Code analysis context
- AIResponse: Structured AI responses
- ConversationHistory: Conversation state

**Patterns**: Domain Model, Value Objects

### 4. Infrastructure Layer
**Responsibility**: External service integration

**Components**:
- BedrockClient: AWS Bedrock API wrapper
- CredentialProvider: AWS authentication
- CacheManager: Response caching
- TelemetryService: Usage analytics

**Patterns**: Repository Pattern, Adapter Pattern

## Design Patterns Implementation

### Dependency Injection
```javascript
class DependencyContainer {
    constructor(config) {
        this.services = new Map();
        this.register('bedrockClient', new BedrockClient(config));
        this.register('amazonQService', new AmazonQService({
            bedrockClient: this.get('bedrockClient'),
            conversationManager: this.get('conversationManager')
        }));
    }
}
```

### Strategy Pattern
```javascript
class ModelStrategy {
    selectModel(context) {
        if (context.isAWSRelated) return 'claude-3-haiku';
        if (context.needsCreativity) return 'claude-3-opus';
        return 'claude-3-sonnet';
    }
}
```

### Observer Pattern (Event Bus)
```javascript
class EventBus extends EventEmitter {
    static getInstance() {
        if (!EventBus.instance) {
            EventBus.instance = new EventBus();
        }
        return EventBus.instance;
    }
}

// Usage
eventBus.on('bedrock:response', (data) => {
    console.log('Tokens used:', data.tokens);
});
```

### Repository Pattern
```javascript
class ConversationRepository {
    async save(conversation) {
        return await this.storage.set(conversation.id, conversation);
    }
    
    async findById(id) {
        return await this.storage.get(id);
    }
}
```

## Data Flow Architecture

### Request Flow
```
User Action
    ↓
Command Handler (Presentation)
    ↓
Service Layer (Application)
    ↓
Domain Logic (Domain)
    ↓
API Client (Infrastructure)
    ↓
AWS Bedrock / External API
```

### Response Flow
```
API Response
    ↓
Response Parser (Infrastructure)
    ↓
Domain Entity Creation (Domain)
    ↓
Service Processing (Application)
    ↓
Event Emission (Observer)
    ↓
UI Update (Presentation)
```

## Error Handling Strategy

### Error Hierarchy
```
BaseError
├── NetworkError
│   ├── ConnectionError
│   └── TimeoutError
├── AuthenticationError
│   ├── CredentialsError
│   └── PermissionError
├── ThrottlingError
└── ValidationError
```

### Retry Logic with Exponential Backoff
```javascript
async executeWithRetry(fn, retries = 3, delay = 1000) {
    for (let i = 0; i < retries; i++) {
        try {
            return await fn();
        } catch (error) {
            if (i === retries - 1) throw error;
            await this.sleep(delay * Math.pow(2, i));
        }
    }
}
```

### Circuit Breaker Pattern
```javascript
class CircuitBreaker {
    constructor(threshold = 5, timeout = 60000) {
        this.failureCount = 0;
        this.threshold = threshold;
        this.timeout = timeout;
        this.state = 'CLOSED';
    }

    async execute(fn) {
        if (this.state === 'OPEN') {
            throw new Error('Circuit breaker is OPEN');
        }
        
        try {
            const result = await fn();
            this.onSuccess();
            return result;
        } catch (error) {
            this.onFailure();
            throw error;
        }
    }
}
```

## Security Architecture

### Credential Management
- AWS credential chain (environment → profile → IAM role)
- No hardcoded credentials
- Secure token storage using OS keychain
- Automatic credential rotation

### Data Privacy
- No PII in logs
- Encrypted conversation storage
- Configurable data retention
- GDPR compliance

### API Security
- Rate limiting per service
- Request throttling
- API key rotation
- TLS/SSL for all connections

## Performance Optimization

### Caching Strategy
```javascript
class CacheManager {
    constructor() {
        this.cache = new Map();
        this.ttl = {
            response: 3600,    // 1 hour
            context: 300,      // 5 minutes
            config: Infinity   // In-memory only
        };
    }
}
```

### Connection Pooling
```javascript
class ConnectionPool {
    constructor(maxConnections = 10) {
        this.pool = [];
        this.maxConnections = maxConnections;
    }

    async acquire() {
        if (this.pool.length > 0) {
            return this.pool.pop();
        }
        return this.createConnection();
    }
}
```

### Request Batching
```javascript
class RequestBatcher {
    constructor(batchSize = 10, delay = 100) {
        this.queue = [];
        this.batchSize = batchSize;
        this.delay = delay;
    }

    async add(request) {
        this.queue.push(request);
        if (this.queue.length >= this.batchSize) {
            await this.flush();
        }
    }
}
```

## Scalability Considerations

### Horizontal Scaling
- Stateless service design
- Shared cache layer (Redis)
- Load balancing support
- Distributed tracing

### Vertical Scaling
- Async/await patterns
- Stream processing for large responses
- Memory-efficient data structures
- Resource pooling

## Monitoring & Observability

### Metrics Collection
```javascript
class MetricsCollector {
    trackRequest(service, duration, success) {
        this.metrics.push({
            service,
            duration,
            success,
            timestamp: Date.now()
        });
    }

    getMetrics() {
        return {
            requestLatency: this.calculateP95(),
            errorRate: this.calculateErrorRate(),
            tokenUsage: this.calculateTokenUsage(),
            cacheHitRatio: this.calculateCacheHitRatio()
        };
    }
}
```

### Structured Logging
```javascript
class Logger {
    log(level, message, meta = {}) {
        console.log(JSON.stringify({
            timestamp: new Date().toISOString(),
            level,
            message,
            correlationId: meta.correlationId,
            service: meta.service,
            ...meta
        }));
    }
}
```

### Distributed Tracing
```javascript
class TraceContext {
    constructor() {
        this.traceId = this.generateTraceId();
        this.spans = [];
    }

    startSpan(name) {
        const span = {
            name,
            startTime: Date.now(),
            traceId: this.traceId
        };
        this.spans.push(span);
        return span;
    }
}
```

## Testing Strategy

### Unit Tests
- Service layer tests
- Domain logic tests
- Utility function tests

### Integration Tests
- API client tests
- Database integration tests
- Cache integration tests

### End-to-End Tests
- Command execution tests
- UI interaction tests
- Full workflow tests

## Deployment Architecture

### Extension Packaging
```
extension.vsix
├── package.json
├── src/
│   ├── core/
│   ├── services/
│   ├── domain/
│   ├── infrastructure/
│   └── presentation/
└── assets/
```

### Server Deployment
```
Multi-AI Server
├── Docker Container
├── Environment Variables
├── Health Checks
└── Auto-restart on Failure
```

## Technology Stack

### Frontend
- VS Code Extension API
- HTML/CSS/JavaScript
- WebView API

### Backend
- Node.js 16+
- Express.js
- AWS SDK v3

### Infrastructure
- AWS Bedrock
- Redis (optional)
- SQLite (local storage)

### DevOps
- Docker
- GitHub Actions
- npm/yarn
