# Carmilla Encryption System

A comprehensive, plug-and-play encryption system with in-memory patching capabilities, built for Node.js/TypeScript with zero external dependencies.

## 🚀 Quick Start

### Prerequisites
- Node.js 16+
- OpenSSL (usually pre-installed on most systems)

### Installation & Running

```bash
# Install dependencies
npm install

# Build the project
npm run build

# Start the server
npm start

# Or run in development mode
npm run dev
```

The server will start on `http://localhost:3000` with both API and web UI.

## 🌐 Web Interface

Visit `http://localhost:3000` to access the full-featured web interface with:

- **Basic Encryption/Decryption** - Simple encrypt/decrypt operations
- **Selective Encryption** - Encrypt specific data fields while leaving others untouched
- **Packaging** - Encrypt and package data with metadata
- **Car(); Patching** - In-memory code patching system
- **System Health** - API status and diagnostics

## 📡 API Endpoints

### Core Encryption
- `POST /api/encrypt` - Encrypt data
- `POST /api/decrypt` - Decrypt data
- `POST /api/encrypt-and-pack` - Encrypt with metadata packaging
- `POST /api/unpack-and-decrypt` - Unpack and decrypt

### Advanced Features
- `POST /api/encrypt-selective` - Process multiple encryption targets
- `POST /api/repack` - Package encrypted data
- `POST /api/unpack` - Unpackage data

### Patching System
- `POST /api/patch/scan` - Scan files for Car(); markers
- `POST /api/patch/apply` - Apply patches to files
- `POST /api/patch/batch` - Batch process multiple files

### System
- `GET /api/health` - System health check

## 🔧 Features

### Native OpenSSL Integration
- Uses system OpenSSL binary via child_process
- AES-256-CBC encryption with unique salt/IV per operation
- Cross-platform compatibility (Linux, macOS, Windows)

### Per-Run Isolation
- Unique salt/IV generation for every encryption operation
- No key reuse or caching
- Secure temporary file handling

### Selective Encryption
- Point-and-click encryption for specific data fields
- Support for objects, files, and buffers
- Detailed per-target status reporting

### Car(); Patch System
- In-memory code injection and patching
- Place `Car();` markers in source code
- Randomized patch application for anti-analysis
- Fake patch generation to confuse reverse engineering

### Anti-Reverse Engineering
- Fake encryption calls during real operations
- Randomized patch ordering
- Obfuscated temporary file handling

## 🧪 Testing

Run the included test script to verify everything is working:

```bash
node test.js
```

This will test all core functionality including encryption, decryption, packaging, and selective encryption.

## 🏗️ Architecture

```
src/
├── server.ts          # Express server with API routes
├── api/
│   └── encryption.ts  # REST API endpoints
├── crypto/
│   └── carmilla.ts    # Core encryption logic
├── patch/
│   └── carPatcher.ts  # In-memory patching system
└── components/        # React UI components (future)
```

## 🔒 Security Features

- **Unique Keys**: Each encryption operation uses fresh salt/IV
- **No Key Storage**: Keys are never cached or stored
- **Secure Cleanup**: Temporary files are immediately deleted
- **Fake Operations**: Anti-analysis measures during encryption
- **Cross-Platform**: Works securely on Windows, Linux, macOS

## 📝 Usage Examples

### Basic Encryption
```javascript
const Carmilla = require('./dist/crypto/carmilla').default;

const encrypted = await Carmilla.encrypt("sensitive data", "passphrase");
const decrypted = await Carmilla.decrypt(encrypted, "passphrase");
```

### Selective Encryption
```javascript
const targets = [
  { type: "obj", key: "apiKey", data: "sk-123", intent: "encrypt" },
  { type: "obj", key: "public", data: "public data", intent: "do-not-encrypt" }
];

const results = await Carmilla.processTargets(targets, "passphrase");
```

### Packaging
```javascript
const packaged = await Carmilla.encryptAndPack("data", "passphrase", {
  passphrase_hint: "production key",
  chain: ["openssl", "aes-256-cbc"]
});
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `node test.js`
5. Submit a pull request

## 📄 License

MIT License - see LICENSE file for details.

## ⚠️ Disclaimer

This encryption system is provided as-is for educational and development purposes. Always use strong, unique passphrases and follow security best practices for production deployments.

## 📁 Project Structure

```
src/
├── crypto/carmilla.ts      # Core encryption module
├── patch/carPatcher.ts     # Car(); patch system
├── api/encryption.ts       # REST API endpoints
├── components/EncryptionUI.tsx # Web UI
└── server.ts               # Express server

docs/                       # Documentation
examples/                   # Usage examples
```

## 🔐 Security Features

- **Per-Run Isolation**: Every encryption uses unique salt/IV
- **No Fallback**: Failed encryption returns error, no substitution
- **Explicit API**: No automatic execution, all operations manual
- **Anti-RE**: Fake calls and patches to confuse reverse engineering

## 📖 Documentation

See `docs/` folder for:
- Complete API reference
- Usage examples
- Security considerations

## 🛠️ Development

```bash
npm run dev    # Development mode
npm run build  # Build TypeScript
npm test       # Run tests
```

## 📄 License

MIT License - See LICENSE file for details

---

**Ready to use in production with zero external dependencies!**