# Secure IDE

A secure, self-contained IDE with built-in AI capabilities that processes everything locally for maximum privacy and security.

## Features

### Core IDE Features
- **Monaco Editor Integration** - Full-featured code editor with syntax highlighting
- **Multi-language Support** - JavaScript, TypeScript, Python, Java, C++, and more
- **File Management** - Complete file system operations with security controls
- **Integrated Terminal** - Built-in terminal with secure execution
- **Real-time Collaboration** - WebSocket-based real-time features

### AI Capabilities
- **Local AI Processing** - All AI operations happen locally, no external APIs
- **Code Completion** - Intelligent code suggestions based on context
- **Code Review** - Automated code analysis and improvement suggestions
- **Chat Assistant** - Interactive AI assistant for coding help
- **Refactoring Support** - AI-powered code refactoring suggestions

### Security Features
- **Sandboxed Execution** - Secure execution environment
- **File Integrity Monitoring** - Continuous monitoring of critical files
- **Access Control** - Granular file and network access controls
- **Audit Logging** - Comprehensive security event logging
- **Resource Monitoring** - Memory and CPU usage tracking

### Extension System
- **Secure Extensions** - Sandboxed extension execution
- **Permission System** - Granular extension permissions
- **Local Extensions** - No external extension downloads
- **API Security** - Secure extension API with access controls

## Installation

### Prerequisites
- Node.js 18+ 
- npm or yarn

### Quick Start

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd secure-ide
   ```

2. **Install dependencies**
   ```bash
   npm install
   ```

3. **Start the development server**
   ```bash
   npm run dev
   ```

4. **Open your browser**
   Navigate to `http://localhost:3000`

### Production Build

1. **Build the application**
   ```bash
   npm run build
   ```

2. **Start the production server**
   ```bash
   npm start
   ```

## Configuration

### Environment Variables

Create a `.env` file in the root directory:

```env
# Server Configuration
PORT=3000
WS_PORT=3001
WORKSPACE=./workspace

# Security Configuration
SECURITY_LEVEL=high
ENABLE_SANDBOX=true
ALLOW_NETWORK_ACCESS=false

# AI Configuration
AI_MODEL_PATH=./models
AI_MAX_TOKENS=4096
AI_TEMPERATURE=0.7

# File System
MAX_FILE_SIZE=10485760
ALLOWED_FILE_TYPES=.js,.ts,.html,.css,.json,.md,.txt
```

### Security Policies

The IDE includes comprehensive security policies:

- **File Access Control** - Restrict file operations to workspace
- **Network Security** - Control network access for AI operations
- **Extension Security** - Sandboxed extension execution
- **Resource Limits** - Memory and CPU usage monitoring

## Usage

### Basic Operations

1. **Open Files** - Click on files in the explorer panel
2. **Create Files** - Use the new file button or right-click context menu
3. **Save Files** - Use Ctrl+S or the save button
4. **Search** - Use the search panel to find files and content

### AI Features

1. **Chat Assistant** - Open the AI panel and start chatting
2. **Code Completion** - Type code and get AI suggestions
3. **Code Review** - Right-click on code and select "Review with AI"
4. **Refactoring** - Select code and use AI refactoring tools

### Terminal

1. **Open Terminal** - Click the terminal toggle button
2. **Run Commands** - Type commands directly in the terminal
3. **Multiple Terminals** - Create new terminal sessions
4. **Clear Terminal** - Use the clear button or Ctrl+L

### Extensions

1. **Install Extensions** - Use the extensions panel
2. **Manage Permissions** - Configure extension permissions
3. **Enable/Disable** - Control extension activation

## Architecture

### Frontend
- **Monaco Editor** - Code editing and syntax highlighting
- **WebSocket Client** - Real-time communication with backend
- **Terminal Integration** - xterm.js for terminal functionality
- **AI Chat Interface** - Interactive AI assistant

### Backend
- **Express Server** - REST API and static file serving
- **WebSocket Server** - Real-time communication
- **Security Manager** - Access control and monitoring
- **Local AI Engine** - AI processing without external APIs

### Security
- **File System Sandbox** - Restricted file access
- **Network Security** - Controlled network access
- **Extension Sandbox** - Isolated extension execution
- **Audit Logging** - Security event tracking

## Development

### Project Structure

```
secure-ide/
├── src/
│   ├── index.html          # Main HTML file
│   ├── styles.css          # Application styles
│   ├── app.js             # Frontend application
│   ├── server/            # Backend server
│   ├── ai/                # AI engine
│   ├── security/          # Security manager
│   └── extensions/        # Extension system
├── package.json           # Dependencies
├── tsconfig.json         # TypeScript config
└── README.md             # This file
```

### Building

```bash
# Development build
npm run dev

# Production build
npm run build

# Type checking
npm run lint

# Format code
npm run format
```

### Testing

```bash
# Run tests
npm test

# Run tests with coverage
npm run test:coverage
```

## Security Considerations

### Local Processing
- All AI operations are processed locally
- No data is sent to external services
- Complete privacy and security

### File System Security
- Restricted access to workspace directory
- File type validation
- Size limits and monitoring

### Network Security
- No external network access by default
- Controlled network operations
- Secure WebSocket communication

### Extension Security
- Sandboxed execution environment
- Permission-based access control
- No external extension downloads

## Troubleshooting

### Common Issues

1. **Port Already in Use**
   ```bash
   # Change ports in .env file
   PORT=3001
   WS_PORT=3002
   ```

2. **File Access Denied**
   - Check file permissions
   - Verify workspace path
   - Review security policies

3. **AI Not Working**
   - Ensure local AI model is loaded
   - Check AI configuration
   - Review error logs

4. **Terminal Not Working**
   - Check terminal permissions
   - Verify shell configuration
   - Review security settings

### Debug Mode

Enable debug mode for detailed logging:

```bash
DEBUG=secure-ide:* npm run dev
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new features
5. Submit a pull request

## License

MIT License - see LICENSE file for details.

## Support

For support and questions:
- Create an issue on GitHub
- Check the documentation
- Review the troubleshooting guide
