# RawrZ Security Platform - Project Structure

## Directory Organization

### Core Components
- **`src/`** - Main application source code and engines
- **`ai/`** - AI integration modules and vector databases
- **`RawrZ.NET/`** - .NET desktop applications and security platform
- **`04-NodeJS-Server/`** - Express.js API servers and real-time services

### Development Tools
- **`Eon-ASM/`** - Custom EON programming language implementation
- **`our_own_toolchain/`** - Custom compiler and build system tools
- **`toolchain/`** - Adaptive compiler and cross-platform build utilities
- **`plugins/`** - Language-specific code generation plugins

### AI & Machine Learning
- **`ai/agent/`** - AI agent communication protocols
- **`ai/cognitive/`** - Cognitive processing modules
- **`ai/tke/`** - Token knowledge engine
- **`LLM-From-Scratch/`** - Custom LLM implementation and training

### Security & Encryption
- **`agent/`** - Security agent and compiler integration
- **`security/`** - Security protocols and permission management
- **`10-Temp-Files/`** - Temporary encryption and stealth files

### IDE & Extensions
- **`vscode-extension/`** - VS Code integration
- **`n0mn0m-vscode-extension/`** - Custom IDE extension
- **`.continue/`** - Continue AI assistant configuration
- **`Private Co Pilot/`** - AI-powered development assistant

### Documentation & Testing
- **`05-Documentation/`** - Comprehensive project documentation
- **`06-Test-Files/`** - Test suites and validation files
- **`07-Scripts/`** - Deployment and automation scripts
- **`tests/`** - Automated testing framework

### Generated & Output
- **`09-Generated-Files/`** - Auto-generated code and artifacts
- **`finished/`** - Production-ready deployment packages
- **`uploads/`** - File upload and processing area

## Architectural Patterns

### Microservices Architecture
- **API Gateway**: Express.js routing and middleware
- **Service Discovery**: Docker container orchestration
- **Load Balancing**: Nginx reverse proxy configuration
- **Data Persistence**: SQLite and JSON-based storage

### Plugin System
- **Language Plugins**: Modular compiler support for 50+ languages
- **AI Providers**: Pluggable LLM integration (OpenAI, Anthropic, etc.)
- **Build Systems**: Extensible compilation and deployment pipelines
- **Security Modules**: Configurable encryption and stealth operations

### Event-Driven Design
- **Message Bus**: Inter-service communication
- **Event Sourcing**: Audit logging and state management
- **Real-time Updates**: WebSocket connections for live compilation
- **Async Processing**: Background job queues for heavy operations

## Core Relationships

### Compilation Pipeline
```
Source Code → Language Plugin → Compiler Engine → Security Layer → Output
```

### AI Integration Flow
```
User Input → AI Provider → Code Generation → Validation → Integration
```

### Security Processing
```
File Input → Encryption Engine → Stealth Layer → Obfuscation → Disguised Output
```

### Deployment Chain
```
Development → Testing → Containerization → Cloud Deployment → Monitoring
```

## Key Components

### Engine Manager
Central orchestration of compilation, encryption, and AI services with unified API interface.

### Security Platform
Multi-layered security implementation with encryption, stealth operations, and advanced obfuscation.

### AI Core
Hybrid AI system supporting multiple providers with intelligent code generation and analysis.

### Build System
Cross-platform compilation supporting native, interpreted, and transpiled languages.

### Extension Framework
Modular plugin architecture for IDE integration and custom tool development.