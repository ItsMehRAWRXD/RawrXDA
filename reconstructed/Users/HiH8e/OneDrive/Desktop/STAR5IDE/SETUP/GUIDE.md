# Star5IDE - CLI Security Platform Setup & Usage Guide
**Date:** November 21, 2025  
**Project:** Star5IDE (RawrZ Security Platform CLI)  
**Version:** 2.0.0  
**Status:** 100% OPERATIONAL

## 🎯 **Project Overview**

Star5IDE is a **complete standalone security platform** with **110+ security features** providing enterprise-level capabilities through a command-line interface.

### **Key Features:**
- **110+ Total Security Features** across multiple categories
- **No IRC, no network dependencies** - Pure command-line tools
- **25/25 encryption algorithms** working (100% success rate)
- **Enterprise-level capabilities** for security research

## 🚀 **Quick Start Guide**

### **Installation & Setup:**

1. **Navigate to Star5IDE:**
```powershell
cd "D:\Security Research aka GitHub Repos\Star5IDE\ItsMehRAWRXD-Star5IDE-950cfb1"
```

2. **Install Dependencies:**
```powershell
npm install
# or if package.json is minimal:
npm install crc32 dotenv
```

3. **Test Installation:**
```powershell
node rawrz-standalone.js help
```

4. **Check System Info:**
```powershell
node rawrz-standalone.js sysinfo
```

## 📋 **Available Security Features**

### **🔐 Core Crypto (7 commands):**
```powershell
# Encrypt data or files
node rawrz-standalone.js encrypt <algorithm> <input> [extension]

# Decrypt data or files  
node rawrz-standalone.js decrypt <algorithm> <input> [key] [extension]

# Generate hashes
node rawrz-standalone.js hash <input> [algorithm] [save] [extension]

# Generate encryption keys
node rawrz-standalone.js keygen <algorithm> [length] [save] [extension]

# Advanced crypto operations
node rawrz-standalone.js advancedcrypto <input> [operation]

# Digital signatures
node rawrz-standalone.js sign <input> [privatekey]
node rawrz-standalone.js verify <input> <signature> <publickey>
```

### **🔤 Encoding (6 commands):**
```powershell
node rawrz-standalone.js base64encode <input>
node rawrz-standalone.js base64decode <input>
node rawrz-standalone.js hexencode <input>
node rawrz-standalone.js hexdecode <input>
node rawrz-standalone.js urlencode <input>
node rawrz-standalone.js urldecode <input>
```

### **🎲 Random Generation (3 commands):**
```powershell
node rawrz-standalone.js random [length]
node rawrz-standalone.js uuid
node rawrz-standalone.js password [length] [special]
```

### **🔍 Analysis (3 commands):**
```powershell
node rawrz-standalone.js analyze <input>
node rawrz-standalone.js sysinfo
node rawrz-standalone.js processes
```

### **🌐 Network (5 commands):**
```powershell
node rawrz-standalone.js ping <host> [save] [extension]
node rawrz-standalone.js dns <hostname>
node rawrz-standalone.js portscan <host> [startport] [endport]
node rawrz-standalone.js traceroute <host>
node rawrz-standalone.js whois <host>
```

### **📁 File Operations (4 commands):**
```powershell
node rawrz-standalone.js files [path]
node rawrz-standalone.js upload <file>
node rawrz-standalone.js fileops <operation> <path>
```

### **📝 Text Operations:**
```powershell
node rawrz-standalone.js textops <operation> <input>
# Operations: uppercase, lowercase, reverse, wordcount, charcount
```

### **✅ Validation:**
```powershell
node rawrz-standalone.js validate <type> <input>
# Types: email, url, ip, json
```

### **⚙️ Utilities:**
```powershell
node rawrz-standalone.js time
node rawrz-standalone.js math <operation> <numbers>
node rawrz-standalone.js help
```

## 🔧 **Advanced Security Engines**

The platform includes 21 advanced engine modules:

### **Available Engines (src/engines/):**
- **advanced-crypto.js** - Advanced encryption algorithms
- **anti-analysis.js** - Anti-detection capabilities  
- **stealth-engine.js** - Stealth operations
- **malware-analysis.js** - Security analysis tools
- **digital-forensics.js** - Forensic capabilities
- **reverse-engineering.js** - Analysis tools
- **hot-patchers.js** - Runtime patching
- **memory-manager.js** - Memory management
- **polymorphic-engine.js** - Dynamic code generation
- **network-tools.js** - Network utilities

## 💻 **Example Usage Sessions**

### **Encryption Example:**
```powershell
# Generate AES key
node rawrz-standalone.js keygen aes 256

# Encrypt a file
node rawrz-standalone.js encrypt aes-256 "secret.txt"

# Decrypt the file
node rawrz-standalone.js decrypt aes-256 "secret.txt.enc" [key]
```

### **Analysis Example:**
```powershell
# System information
node rawrz-standalone.js sysinfo

# File analysis
node rawrz-standalone.js analyze "suspicious.exe"

# Network scan
node rawrz-standalone.js portscan 192.168.1.1 80 443
```

### **Encoding Example:**
```powershell
# Encode data
node rawrz-standalone.js base64encode "Hello World"

# Generate secure password
node rawrz-standalone.js password 32 true
```

## 🛡️ **Security Research Features**

### **32 Encryption Algorithms:**
- AES (128/192/256)
- Camellia
- ARIA  
- ChaCha20
- And 28 more algorithms

### **Anti-Detection Features:**
- Stealth architecture
- Anti-analysis capabilities
- Polymorphic operations
- Memory protection

### **Forensic Tools:**
- File analysis
- System profiling
- Network investigation
- Digital evidence handling

## 📊 **Platform Status**

### **Operational Status:**
- **Version:** 2.0.0
- **Success Rate:** 100% (25/25 encryption algorithms)
- **Total Features:** 110+
- **Engine Modules:** 21
- **Last Updated:** 2025-09-13

### **Compatibility:**
- **Node.js:** 18+ required
- **OS:** Windows, Linux, macOS
- **Dependencies:** Minimal (crc32, dotenv)

## 🔄 **Build & Development**

### **Scripts Available:**
```powershell
# Start the CLI
npm start

# Help system
npm run help  

# System test
npm test
```

### **Development Mode:**
```powershell
# Run with debugging
node --inspect rawrz-standalone.js help

# Environment variables
$env:NODE_ENV="development"
```

## 📖 **Documentation Files**

- **README.md** - Main documentation
- **Use.txt** - Detailed usage guide
- **CHANGELOG.md** - Version history
- **RELEASE_NOTES.md** - Release information
- **SECURITY.md** - Security guidelines

## 🎯 **Use Cases**

### **Security Research:**
- Encryption algorithm testing
- Network security analysis
- File forensics and analysis
- System security assessment

### **Educational:**
- Learning cryptography
- Understanding security concepts
- Hands-on security training

### **Professional:**
- Penetration testing support
- Digital forensics
- Security auditing
- Incident response

---

**Star5IDE** - Complete Standalone Security Platform for Enterprise-Level Security Research and Analysis.

**Commands to Get Started:**
```powershell
cd "D:\Security Research aka GitHub Repos\Star5IDE\ItsMehRAWRXD-Star5IDE-950cfb1"
node rawrz-standalone.js help
node rawrz-standalone.js sysinfo
```