# BigDaddyG - Interactive ML Model Launcher System

## 🎯 Project Overview

**BigDaddyG** is a production-ready, interactive launcher system for deploying large language models (GGUF format) with real-time configuration selection. 

### Key Features

✅ **Interactive Menu System** - Select model and configuration every run  
✅ **7-Step Configuration Wizard** - Flexible parameter selection  
✅ **Auto-Script Generation** - Dynamic Python startup scripts  
✅ **FastAPI Service** - HTTP REST API for model inference  
✅ **Multi-Model Support** - 36GB primary + alternative variants  
✅ **Production Ready** - Complete error handling & logging  

---

## 📁 Project Structure

```
Projects/BigDaddyG/
├─ START-BIGDADDYG.bat                 ← Double-click to launch
├─ bigdaddyg-launcher-interactive.ps1  ← Main launcher script
├─ BIGDADDYG-LAUNCHER-GUIDE.md         ← Complete user guide
├─ BIGDADDYG-LAUNCHER-SETUP.md         ← Setup instructions
├─ BIGDADDYG-LAUNCHER-COMPLETE.md      ← Implementation details
├─ BIGDADDYG-EXECUTIVE-SUMMARY.md      ← Executive overview
├─ BIGDADDYG-QUICK-REFERENCE.txt       ← Quick reference
├─ D-DRIVE-AUDIT-COMPLETE.md           ← Model audit report
├─ INTEGRATION-DECISION.md             ← Integration options
├─ README.md                           ← This file
└─ [Models & Supporting Files]
```

---

## 🚀 Quick Start (3 Steps)

### Step 1: Launch
```
Double-click: START-BIGDADDYG.bat
```

### Step 2: Follow Menu
- Select model (or press Enter for default)
- Configure 7 settings (or use defaults)
- Confirm and launch

### Step 3: Use Service
```
Open: http://localhost:8765
API Docs: http://localhost:8765/docs
```

**Total time:** 5-10 minutes

---

## ⚙️ Configuration Options

When the launcher starts, you'll be asked to configure:

| Setting | Options | Default |
|---------|---------|---------|
| **Model** | 36GB, 18GB, 17GB, 0.5GB | 36GB (bigdaddyg-40gb-model.gguf) |
| **Inference Mode** | CPU, GPU CUDA, GPU ROCm | CPU |
| **Service Port** | Default (8765) or custom | 8765 |
| **Memory** | Auto, 4GB, 8GB, 16GB, 32GB | Auto |
| **CPU Threads** | Auto, half cores, custom | Auto |
| **Logging** | Info, Debug, Trace | Info |
| **API Auth** | None, Token-based | None |

---

## 📊 Performance Expectations

| Mode | Speed | Memory | Best For |
|------|-------|--------|----------|
| **CPU** | 2-5 sec | Low-Medium | Testing, general use |
| **GPU CUDA** | 0.5-1 sec | Medium-High | Production, high-volume |
| **GPU ROCm** | 0.5-1 sec | Medium-High | AMD systems |

---

## 🔗 API Endpoints

### Health Check
```
GET http://localhost:8765/health
```

### Classify/Inference
```
POST http://localhost:8765/api/classify
Content-Type: application/json

{
  "file_bytes": "[base64 encoded]",
  "features": {...},
  "file_path": "/path/to/file"
}
```

### Swagger Documentation
```
http://localhost:8765/docs
```

---

## 📚 Documentation

| Document | Purpose | Time |
|----------|---------|------|
| `BIGDADDYG-QUICK-REFERENCE.txt` | One-page cheat sheet | 5 min |
| `00-START-HERE.md` | Quick overview | 10 min |
| `BIGDADDYG-LAUNCHER-GUIDE.md` | Complete user guide | 20 min |
| `BIGDADDYG-LAUNCHER-SETUP.md` | Setup & architecture | 15 min |
| `BIGDADDYG-EXECUTIVE-SUMMARY.md` | Executive brief | 10 min |
| `D-DRIVE-AUDIT-COMPLETE.md` | Model inventory | Reference |

**Recommended Reading Path:**
1. Start with `BIGDADDYG-QUICK-REFERENCE.txt` (5 min)
2. Read `BIGDADDYG-LAUNCHER-GUIDE.md` (20 min)
3. Reference `BIGDADDYG-LAUNCHER-SETUP.md` as needed

---

## 🔧 Integration with CyberForge

BigDaddyG provides an HTTP service that CyberForge's MLClassificationEngine can call:

```javascript
// From cyberforge-av-engine.js
async classifyFile(fileBuffer, filePath) {
    const response = await fetch('http://localhost:8765/api/classify', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            file_bytes: fileBuffer.toString('base64'),
            file_path: filePath,
            features: {}
        })
    });
    
    const result = await response.json();
    return {
        classification: result.classification, // MALWARE or CLEAN
        confidence: result.confidence,         // 0-1
        threat_score: result.threat_score      // 0-1
    };
}
```

---

## ✅ Project Status

**Phase:** 2 - Complete ✅  
**Status:** Production Ready  
**Last Updated:** November 21, 2025  

### Completed Features
- [x] Interactive launcher with menu system
- [x] 7-parameter configuration wizard
- [x] Auto-script generation
- [x] FastAPI service layer
- [x] Multiple model support
- [x] GPU/CPU mode switching
- [x] Comprehensive logging
- [x] Error handling
- [x] API documentation
- [x] Complete guides & tutorials

### Optional Future Enhancements
- [ ] Model quantization options
- [ ] Real-time performance monitoring
- [ ] Distributed inference
- [ ] Model fine-tuning interface
- [ ] Webhook callbacks for events

---

## 📋 Model Location Reference

**Primary Model:**
```
D:\BigDaddyG-Standalone-40GB\bigdaddyg-40gb-model.gguf (36.2 GB)
```

**Alternative Models:**
```
D:\models\bigdaddyg-18.49GB.gguf
D:\models\bigdaddyg-17.74GB.gguf
D:\models\custom-agentic-coder.gguf (0.5 GB)
```

The launcher automatically scans these locations and presents available models.

---

## 🆘 Troubleshooting

### Model Not Found?
→ Check D:\BigDaddyG-Standalone-40GB\ exists and contains GGUF files

### Service Won't Start?
→ Ensure Python 3.8+ is installed and in system PATH
→ Check logs in: `C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\logs\`

### API Connection Error?
→ Verify service is running: `http://localhost:8765/health`
→ Check firewall isn't blocking port 8765
→ Try different port: when prompted, select custom port

### Memory Issues?
→ Reduce inference mode (CPU instead of GPU)
→ Allocate less memory (8GB instead of auto)
→ Use smaller model variant (18GB instead of 36GB)

**For detailed troubleshooting, see:** `BIGDADDYG-LAUNCHER-GUIDE.md`

---

## 👥 For Team Members

### I want to run the launcher
→ Start with `BIGDADDYG-QUICK-REFERENCE.txt`

### I want to integrate with CyberForge
→ Read `INTEGRATION-DECISION.md` then use code example above

### I want to understand the architecture
→ Read `BIGDADDYG-LAUNCHER-SETUP.md`

### I want technical implementation details
→ Read `BIGDADDYG-LAUNCHER-COMPLETE.md`

---

## 📞 Support

For questions or issues:
1. Check the relevant documentation file above
2. Review troubleshooting section
3. Check logs in `logs/` directory
4. Review phase summaries for context

---

## 📄 License

This project is part of the Mirai-Source-Code collection.  
See root-level LICENSE.md for details.

---

**Last Updated:** November 21, 2025  
**Project Status:** ✅ Complete & Production Ready

