# RawrZ Security Platform - Technology Stack

## Programming Languages

### Primary Languages
- **JavaScript/Node.js** - Main backend services and API development
- **C#/.NET** - Desktop applications and security platform
- **C/C++** - Low-level system components and performance-critical modules
- **Python** - AI/ML integration, automation scripts, and data processing
- **Assembly (x86/x64)** - Direct hardware interaction and optimization

### Supported Languages (50+)
- **Compiled**: C, C++, Rust, Go, Java, C#, Kotlin, Scala
- **Interpreted**: Python, JavaScript, Ruby, PHP, Lua, R
- **Functional**: Haskell, OCaml, F#, Elixir, Erlang
- **Systems**: Zig, Odin, V, Carbon, Jai
- **Web**: TypeScript, Dart, WebAssembly
- **Custom**: EON (proprietary language)

## Backend Technologies

### Runtime Environments
- **Node.js 18+** - Primary backend runtime
- **.NET 8.0** - Desktop and enterprise applications
- **Python 3.11+** - AI/ML and automation
- **Docker** - Containerization and deployment

### Web Framework
- **Express.js 4.18+** - RESTful API services
- **CORS middleware** - Cross-origin resource sharing
- **Multer** - File upload handling
- **Body parsing** - JSON/URL-encoded data processing

### Database & Storage
- **SQLite** - Embedded database for local storage
- **JSON files** - Configuration and lightweight data
- **File system** - Direct file operations and caching
- **In-memory storage** - High-performance temporary data

## AI & Machine Learning

### LLM Providers
- **Amazon Q Developer** - AWS Bedrock integration
- **OpenAI GPT** - GPT-4/GPT-5 models
- **Anthropic Claude** - Claude Sonnet models
- **Google Gemini** - Gemini Pro integration
- **Ollama** - Local LLM deployment
- **Moonshot (Kimi)** - Chinese LLM provider

### AI Frameworks
- **Continue.dev** - AI code assistant integration
- **Custom vector database** - Semantic code search
- **Transformer models** - Natural language processing
- **Tokenization** - Custom tokenizer implementation

## Security & Encryption

### Cryptographic Libraries
- **Node.js Crypto** - Built-in cryptographic functions
- **AES-256-GCM** - Advanced encryption standard
- **Camellia-256-CBC** - Secondary encryption layer
- **ChaCha20-Poly1305** - Stream cipher encryption
- **Argon2** - Password hashing and key derivation

### Security Tools
- **NASM** - Netwide Assembler for x86/x64
- **GCC** - GNU Compiler Collection
- **UPX** - Ultimate Packer for eXecutables
- **Custom obfuscation** - Proprietary stealth techniques

## Development Tools

### Build Systems
- **CMake** - Cross-platform build automation
- **MSBuild** - Microsoft build platform
- **Make** - Traditional Unix build tool
- **Custom toolchain** - Proprietary build system

### Compilers & Interpreters
- **GCC/Clang** - C/C++ compilation
- **NASM/YASM** - Assembly compilation
- **Roslyn** - C# compilation services
- **LLVM** - Compiler infrastructure
- **Custom EON compiler** - Proprietary language support

## Infrastructure & Deployment

### Containerization
- **Docker** - Application containerization
- **Docker Compose** - Multi-container orchestration
- **Kubernetes** - Container orchestration (optional)
- **Nginx** - Reverse proxy and load balancing

### Cloud Platforms
- **DigitalOcean** - Primary cloud deployment
- **AWS** - Enterprise cloud services
- **Local deployment** - On-premises installation
- **Hybrid cloud** - Multi-cloud strategy

### Monitoring & Logging
- **Prometheus** - Metrics collection
- **Loki** - Log aggregation
- **Custom telemetry** - Application-specific monitoring
- **Health checks** - Service availability monitoring

## Development Commands

### Installation & Setup
```bash
npm install                    # Install Node.js dependencies
docker-compose up -d          # Start containerized services
./setup.sh                    # Initialize development environment
```

### Build & Compilation
```bash
npm start                     # Start main server
node server.js               # Direct server execution
./build.sh                   # Build all components
make all                     # Traditional make build
```

### Testing & Validation
```bash
npm test                     # Run test suite
./run_all_tests.sh          # Comprehensive testing
docker exec -it test        # Container-based testing
```

### Deployment
```bash
./deploy-production.sh      # Production deployment
docker build -t rawrz .    # Container image build
kubectl apply -f k8s/      # Kubernetes deployment
```

## Version Requirements

### Minimum Versions
- **Node.js**: 18.0+
- **.NET**: 8.0+
- **Python**: 3.11+
- **Docker**: 20.0+
- **GCC**: 9.0+
- **NASM**: 2.15+

### Recommended Versions
- **Node.js**: 20.x LTS
- **.NET**: 8.0 LTS
- **Python**: 3.12+
- **Docker**: 24.0+
- **Ubuntu**: 22.04 LTS
- **Windows**: 11 Pro