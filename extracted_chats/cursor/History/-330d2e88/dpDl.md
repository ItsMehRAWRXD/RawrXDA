# Puppeteer Agentic Browser System

A comprehensive multi-agent system for web automation, bulk text processing, and intelligent file organization. Built with Puppeteer and enhanced with BigDaddyG orchestrator patterns.

## Features

### 🤖 Multi-Agent Architecture
- **Browser Agent**: Web automation with Puppeteer
- **Text Processor**: Handle 10,000+ lines efficiently
- **Bulk Handler**: Process massive copy-paste operations
- **Download Manager**: Automated file downloads
- **File Organizer**: Intelligent content categorization
- **Security Scanner**: Detect secrets and sensitive data
- **Performance Monitor**: System optimization
- **Orchestrator**: Coordinate all agents

### 🚀 Key Capabilities
- **Web Automation**: Navigate, click, type, download
- **Bulk Text Processing**: Handle massive text operations
- **Web Subscription Integration**: Automated subscription workflows
- **Intelligent Organization**: Auto-categorize by type, date, content
- **Security Scanning**: Detect API keys, passwords, tokens
- **Performance Analysis**: Measure and optimize operations
- **Live Status Board**: Real-time agent monitoring

## Quick Start

### 1. Installation
```bash
cd D:\puppeteer-agent
npm install
```

### 2. VS Code Snippets Setup
1. Open VS Code
2. Press `Ctrl+Shift+P` → "Preferences: Configure User Snippets"
3. Choose language (or global.code-snippets)
4. Copy contents from `vscode-snippets.json`
5. Save the file

### 3. Usage

#### Method 1: VS Code Snippets
- Type `bigg` + Tab → BigDaddyG Orchestrator Panel
- Type `puppet` + Tab → Puppeteer Agentic Browser
- Type `bulk` + Tab → Bulk Text Processor
- Press `Ctrl+F5` to run

#### Method 2: Command Line
```bash
# Launch interactive menu
node launcher.js

# Run specific components
node src/main.js          # Web automation
node src/run-bulk.js       # Bulk text processing
node src/BigDaddyG-Launch-Panel.js  # Orchestrator panel
```

## Configuration

### Web Subscription Setup
Edit `src/config.js` to add your subscription services:

```javascript
const subscriptionConfigs = {
    yourService: {
        name: "Your Service",
        url: "https://your-service.com/login",
        login: {
            usernameSelector: 'input[name="username"]',
            passwordSelector: 'input[name="password"]',
            submitSelector: 'button[type="submit"]',
            username: process.env.YOUR_USERNAME || '',
            password: process.env.YOUR_PASSWORD || ''
        },
        downloads: [
            {
                name: "Report",
                selector: 'a[href*="report"]',
                options: { timeout: 30000 }
            }
        ]
    }
};
```

### Environment Variables
Create `.env` file:
```
YOUR_USERNAME=your_username
YOUR_PASSWORD=your_password
ANOTHER_USERNAME=another_username
ANOTHER_PASSWORD=another_password
```

## Usage Examples

### 1. Web Automation
```javascript
const task = {
    action: 'navigate',
    url: 'https://example.com',
    selector: 'body',
    options: { timeout: 30000 }
};

const result = await orchestrator.browserTask(task);
```

### 2. Bulk Text Processing
```javascript
// Process 10,000+ lines of text
const text = "your massive text content...";
const files = await textProcessor.processBulkText(text, {
    filename: 'bulk-content.txt'
});
```

### 3. Web Subscription Automation
```javascript
const config = subscriptionConfigs.yourService;
const result = await browser.executeWebSubscription(config);
```

### 4. Security Scanning
```javascript
const scanResult = await orchestrator.securityTask({
    action: 'scan_text',
    content: yourContent,
    options: {}
});
```

## File Organization

The system automatically organizes downloads and processed content:

```
downloads/
├── by-type/
│   ├── images/
│   ├── documents/
│   ├── videos/
│   ├── audio/
│   ├── archives/
│   └── applications/
├── by-date/
│   ├── 2024-01-15/
│   └── 2024-01-16/
└── organized/
    ├── code/
    ├── data/
    ├── logs/
    └── text/
```

## Agent Status Board

The live status board shows real-time agent activity:

```
🧠 Enhanced BigDaddyG Orchestrator – Agent Status
┌─────────────┬─────────────────┬────────┬─────┐
│ id          │ name           │ status │ ttl │
├─────────────┼─────────────────┼────────┼─────┤
│ browser     │ Browser Agent  │ idle   │ 120 │
│ text        │ Text Processor │ idle   │ 60  │
│ bulk        │ Bulk Handler   │ idle   │ 60  │
│ download    │ Download Mgr   │ idle   │ 60  │
│ organize    │ File Organizer │ idle   │ 60  │
│ security    │ Security Scan │ idle   │ 60  │
│ performance │ Performance    │ idle   │ 60  │
│ orchestrator│ Orchestrator   │ idle   │ 60  │
└─────────────┴─────────────────┴────────┴─────┘
```

## Advanced Features

### 1. Chunked Processing
Handles massive text by processing in 10,000-line chunks to prevent memory issues.

### 2. Content Categorization
Automatically detects and categorizes:
- **Code**: Functions, classes, imports
- **Data**: JSON, CSV, key-value pairs
- **Logs**: Timestamps, log levels, errors
- **Text**: General text content

### 3. Security Features
- Scans for API keys, passwords, tokens
- Sanitizes sensitive content
- Validates URLs and domains

### 4. Performance Monitoring
- Measures processing time
- Calculates throughput
- Optimizes content
- Compresses data

## Troubleshooting

### Common Issues

1. **Puppeteer fails to launch**
   ```bash
   # Install missing dependencies
   npm install puppeteer-extra puppeteer-extra-plugin-stealth
   ```

2. **Memory issues with large text**
   - The system automatically chunks large text
   - Adjust `chunkSize` in `TextProcessor` if needed

3. **Download permissions**
   - Ensure download directory is writable
   - Check browser download settings

### Debug Mode
```bash
# Enable debug logging
DEBUG=puppeteer-agent:* node launcher.js
```

## Contributing

1. Fork the repository
2. Create feature branch
3. Make changes
4. Test thoroughly
5. Submit pull request

## License

MIT License - see LICENSE file for details.

## Support

For issues and questions:
- Check the troubleshooting section
- Review configuration examples
- Test with sample data first

---

**BigDaddyG Orchestrator** - Multi-agent system for web automation and bulk processing.
