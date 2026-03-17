# RawrZ - Security Platform Dashboard & Components

## 🎯 Project Overview

**RawrZ** is a comprehensive security platform providing centralized monitoring, control, and management for security operations.

### Key Features
✅ **Interactive Dashboard** - Web-based control center  
✅ **Real-time Monitoring** - System and threat surveillance  
✅ **Payload Management** - Create and deploy payloads  
✅ **Bot Management** - Control agent networks  
✅ **Component Integration** - Modular architecture  

---

## 📁 Project Structure

```
Projects/RawrZ/
├─ README.md                             ← This file
├─ BRxC.html                             ← Main dashboard
├─ BRxC-Recovery.html                    ← Backup dashboard
├─ RawrBrowser.ps1                       ← Browser launcher
├─ RawrCompress-GUI.ps1                  ← Compression utility
├─ RawrZ-Payload-Builder-GUI.ps1         ← Payload builder
├─ RAWRZ-COMPONENTS-ANALYSIS.md          ← Component breakdown
├─ quick-setup-rawrz-http.bat            ← Quick HTTP setup
└─ quick-build-rawrzdesktop.bat          ← Desktop build
```

---

## 🚀 Quick Start

### Launch Dashboard
```powershell
# Option 1: Open in default browser
RawrBrowser.ps1

# Option 2: Direct open
Open BRxC.html in web browser
```

### Access Web Interface
```
http://localhost:3001  (Default RawrZ port)
```

### Build Desktop Version
```batch
quick-build-rawrzdesktop.bat
```

---

## 🏗️ Architecture

### Main Components

**Dashboard** (Frontend)
- `BRxC.html` - Complete control interface
- Auto-configuration for all services
- Real-time status monitoring

**Tools** (Backend Support)
- `RawrBrowser.ps1` - Launcher utility
- `RawrCompress-GUI.ps1` - Compression tools
- `RawrZ-Payload-Builder-GUI.ps1` - Payload management

**Build Systems**
- `quick-setup-rawrz-http.bat` - HTTP server setup
- `quick-build-rawrzdesktop.bat` - Desktop deployment

---

## 📊 Dashboard Features

### Main Sections

**1. Authentication & Health**
- Auth token management
- System health checks
- Engine status monitoring

**2. Panel Navigation**
- Unified control panel
- Encryption tools
- CLI interface
- Health dashboard
- CVE analysis
- Bot manager
- Certificate panel

**3. Hash Operations**
- SHA-256, MD5, SHA-1
- SHA-512, SHA-384
- Custom hashing

**4. Encryption**
- AES-256-GCM
- AES-256-CBC
- ChaCha20-Poly1305
- ARIA-256-GCM

**5. Engine Management**
- CVE Analysis Engine
- HTTP Bot Manager
- Payload Manager
- Plugin Architecture

**6. Output Display**
- Real-time results
- Status indicators
- Error handling

---

## 🔌 Integration Points

### With BigDaddyG
```javascript
// Use ML classifier for threat detection
fetch('http://localhost:8765/api/classify', {
    method: 'POST',
    body: JSON.stringify({file_bytes, file_path})
})
```

### With CyberForge
```javascript
// Integrate AV scanning
fetch('http://localhost:8765/av/scan', {
    method: 'POST',
    body: JSON.stringify({file_path})
})
```

### With Beast System
```javascript
// Control swarm operations
fetch('http://localhost:3000/api/swarm/command', {
    method: 'POST',
    body: JSON.stringify({command, targets})
})
```

---

## 🎨 Customization

### Theme Configuration
Edit HTML `<style>` section for:
- Colors (default: Blue & Purple theme)
- Fonts and sizing
- Layout adjustments

### API Endpoints
Configure in dashboard initialization:
```javascript
window.API_CONFIG = {
    ollama: 'http://localhost:11434',
    bigdaddyg: 'http://localhost:3000',
    rawrz: 'http://localhost:3001',
    genesisos: 'http://localhost:3002'
};
```

### Port Configuration
Edit quick-setup script:
```batch
set PORT=3001
# Change to your preferred port
```

---

## 📈 Performance

**Dashboard Load Time:** <2 seconds  
**API Response Time:** <500ms (typical)  
**Concurrent Connections:** 100+  

---

## 🔧 Configuration

### HTTP Server Setup
```batch
quick-setup-rawrz-http.bat
```
Creates local HTTP server for dashboard access

### Desktop Build
```batch
quick-build-rawrzdesktop.bat
```
Creates standalone desktop executable

### Custom Configuration
Edit `quick-setup-rawrz-http.bat`:
```batch
REM Configuration options
set HOST=localhost
set PORT=3001
set ENABLE_AUTH=false
set LOG_LEVEL=info
```

---

## 🧪 Testing

### Verify Dashboard
1. Open `BRxC.html` in browser
2. Click "Health Check"
3. Verify all green status indicators
4. Test sample operations

### Test Engine Connections
Use dashboard buttons to:
- Check CVE database
- List available bots
- View payloads
- Verify encryption

---

## 🆘 Troubleshooting

### Dashboard Won't Load
→ Check file path and browser console
→ Ensure JavaScript is enabled
→ Try `BRxC-Recovery.html` backup

### API Connection Error
→ Check service is running on configured port
→ Verify firewall allows connections
→ Check `http://localhost:PORT/health`

### Engine Connection Failed
→ Ensure all services started
→ Check port numbers match configuration
→ Review logs for detailed errors

### Authentication Issues
→ Generate new token in Auth section
→ Clear browser cookies
→ Restart all services

---

## 📚 Documentation

| File | Purpose |
|------|---------|
| `RAWRZ-COMPONENTS-ANALYSIS.md` | Detailed component breakdown |
| `quick-setup-rawrz-http.bat` | HTTP setup instructions |
| This README | Project overview |

---

## ✅ Project Status

**Phase:** 2 - Core Dashboard Complete  
**Status:** Functional  
**Last Updated:** November 21, 2025  

### Completed
- [x] Main dashboard interface
- [x] Engine management system
- [x] Encryption tools
- [x] Bot manager
- [x] Payload system

### Future Enhancements
- [ ] Advanced analytics
- [ ] Custom plugin support
- [ ] Distributed dashboard nodes
- [ ] Machine learning automation
- [ ] Cloud integration

---

## 👥 For Team Members

### I want to open the dashboard
→ Run: `RawrBrowser.ps1`
→ Or open: `BRxC.html` directly

### I want to understand components
→ Read: `RAWRZ-COMPONENTS-ANALYSIS.md`

### I want to integrate other systems
→ Check the Integration Points section above

### I want to customize
→ Edit HTML/CSS in `BRxC.html`
→ Modify config in quick-setup script

---

## 🔐 Security Notes

- Use authentication for production
- Change default ports
- Implement SSL/TLS for network
- Restrict access by IP
- Review logs regularly

---

## 📞 Support

1. Check troubleshooting section
2. Review component analysis document
3. Verify all services running
4. Check dashboard logs

---

## 📄 License

Part of Mirai-Source-Code collection.  
See root-level LICENSE.md for details.

---

**Project Status:** ✅ Dashboard Functional  
**Last Updated:** November 21, 2025

