# 🚀 RawrXD Universal Access — Quick Start Guide

**30-Second Deployment** — Choose your platform and go!

---

## 📱 Option 1: Web Browser (Any OS)

**Fastest way to get started — No installation required**

```bash
# Start backend + web UI
docker-compose up -d

# Open in browser
http://localhost
```

**Manual Setup (without Docker):**
```bash
# 1. Start RawrEngine backend
cd Ship
python3 rawr_engine.py

# 2. Open web UI
cd ../web_interface
python3 -m http.server 8000

# 3. Access at http://localhost:8000
# Configure connection: Click ⚙️ → Set "http://localhost:23959"
```

---

## 💻 Option 2: Windows Native

**Full IDE with all features**

```powershell
# Just run the IDE (RawrEngine starts automatically)
.\RawrXD-Win32IDE.exe
```

**Web access from Windows:**
```powershell
# Enable external access
$env:RAWRXD_CORS_ORIGINS="*"
$env:RAWRXD_HOST="0.0.0.0"
.\RawrEngine.exe

# Access from any device on your network
http://YOUR_IP:23959
```

---

## 🐧 Option 3: Linux

**Two modes: Full IDE via Wine OR Backend-only**

### Full IDE (Wine):
```bash
# Install Wine first (Ubuntu/Debian example)
sudo apt install wine64

# Launch IDE
./wrapper/launch-linux.sh
```

### Backend Only (Recommended):
```bash
# Native Python backend (faster, lightweight)
./wrapper/launch-linux.sh --backend-only

# Access via web UI: http://localhost:23959
```

---

## 🍎 Option 4: macOS

**Intel and Apple Silicon (M1/M2/M3) supported**

### Full IDE (Wine/CrossOver):
```bash
# Install Wine (Homebrew)
brew install --cask wine-stable
# OR CrossOver (commercial, faster)
brew install --cask crossover

# Launch IDE
./wrapper/launch-macos.sh
```

### Backend Only:
```bash
# Native Python backend
./wrapper/launch-macos.sh --backend-only

# Access via web UI: http://localhost:23959
```

---

## 🐳 Option 5: Docker (Production)

**Complete stack with nginx reverse proxy**

### Backend + Web UI:
```bash
docker-compose up -d rawrxd-backend rawrxd-web

# Access
http://localhost      # Web UI
http://localhost:23959  # API
```

### Full Desktop via noVNC:
```bash
docker-compose --profile desktop up -d

# Access IDE in browser
http://localhost:8080/vnc.html
# Password: rawrxd (or set VNC_PASSWORD env var)
```

---

## ⚙️ Web UI Configuration

**First Time Setup:**

1. Open web UI in browser
2. Click **⚙️ Settings** icon (bottom left)
3. Configure:
   - **RawrEngine URL**: `http://localhost:23959` (or your server IP)
   - **API Key**: Leave empty for local, set for external access
   - **Agent Mode**: Choose Ask (chat) / Plan (generate plan) / Full (auto-execute)
4. Click **Connect**
5. Status dot turns 🟢 green when connected

---

## 🔐 Security (External Access)

**Generate API Key:**
```bash
python Ship/CORSAuthMiddleware.py genkey standard
# Output: rawrxd_standard_abc123xyz...
```

**Enable Authentication:**
```python
# In your RawrEngine startup script
from Ship.CORSAuthMiddleware import UniversalAccessGateway

gateway = UniversalAccessGateway(
    allowed_origins=["https://your-domain.com"],
    api_keys={"rawrxd_standard_abc123xyz"},
    require_auth=True
)
gateway.init_app(app)
```

**Web UI Setup:**
1. Settings → API Key → Paste key
2. Save settings
3. All requests now authenticated

---

## 🧪 Test Your Deployment

**1. Check Backend:**
```bash
curl http://localhost:23959/status
# Should return: {"status":"ok", ...}
```

**2. Check Web UI:**
```bash
# Open browser
http://localhost

# Or test with curl
curl http://localhost/
# Should return HTML
```

**3. Test Chat:**
- Open web UI
- Type: "Hello, RawrXD!"
- Should see streaming response

---

## 🐛 Troubleshooting

### Web UI Not Connecting

**Check 1: Backend Running?**
```bash
curl http://localhost:23959/status
```
If fails → Start backend

**Check 2: CORS Error?**
- F12 → Console → Look for CORS error
- Fix: Add your origin to `allowed_origins`

**Check 3: Firewall?**
```bash
# Linux
sudo ufw allow 23959

# macOS
# System Preferences → Security → Firewall → Allow
```

### Docker Issues

**Port already in use:**
```bash
# Change ports in docker-compose.yml
ports:
  - "8080:80"    # Web UI
  - "24000:23959"  # API
```

**Models not loading:**
```bash
# Check volume mount
docker-compose exec rawrxd-backend ls /opt/rawrxd/models

# Set MODEL_PATH
echo "MODEL_PATH=/path/to/models" > .env
docker-compose up -d
```

### Wine Issues (Linux/macOS)

**Wine not found:**
```bash
# Ubuntu/Debian
sudo apt install wine64

# macOS
brew install --cask wine-stable
```

**Slow startup:**
```bash
# First run takes longer (Wine prefix setup)
# Subsequent runs: 1-2 seconds

# Check progress
tail -f wrapper/rawrxd_wine.log
```

---

## 📊 Performance Tips

### Web UI
- Use Chrome/Edge for best performance
- Enable hardware acceleration in browser
- Clear cache if UI feels sluggish

### Backend
- Use `--backend-only` on Linux/macOS (faster than Wine)
- Mount models as read-only in Docker (faster startup)
- Use SSD for model storage

### Docker
- Allocate more memory: `docker-compose up -d --scale rawrxd-backend=1 --memory 8g`
- Use bind mounts instead of volumes for models
- Enable BuildKit: `DOCKER_BUILDKIT=1 docker-compose build`

---

## 🎯 Common Use Cases

### Scenario: Remote Team Access

```bash
# Server (cloud/VPS)
docker-compose up -d
# Configure nginx with SSL (see UNIVERSAL_ACCESS_DEPLOYMENT.md)

# Team members
# Open: https://your-domain.com
# Settings → API Key → Enter provided key
```

### Scenario: Mobile Development

```bash
# Laptop (backend)
./wrapper/launch-linux.sh --backend-only

# Phone/Tablet (web UI)
# Open: http://YOUR_LAPTOP_IP:23959
# Add to home screen (PWA)
```

### Scenario: Local Dev + Web Testing

```bash
# Terminal 1: Backend
cd Ship
python3 rawr_engine.py

# Terminal 2: Web UI dev server
cd web_interface
python3 -m http.server 8000

# Browser: http://localhost:8000
# Edit index.html → Refresh to see changes
```

---

## 📚 Learn More

- **Full Guide**: `UNIVERSAL_ACCESS_DEPLOYMENT.md`
- **Implementation Details**: `UNIVERSAL_ACCESS_IMPLEMENTATION_SUMMARY.md`
- **Main Docs**: `README.md`

---

## ✅ Checklist

Before deployment, verify:

- [ ] Backend running on port 23959
- [ ] Web UI accessible in browser
- [ ] Model selection dropdown populated
- [ ] Chat streaming works
- [ ] Settings persist after refresh
- [ ] (Production) HTTPS enabled
- [ ] (Production) API keys configured
- [ ] (Production) CORS origins restricted

---

**Need Help?**

1. Check `UNIVERSAL_ACCESS_DEPLOYMENT.md` Section 10 (Troubleshooting)
2. Review logs: `docker-compose logs` or `wrapper/rawrxd_wine.log`
3. Test backend directly: `curl http://localhost:23959/status`
4. Check browser console: F12 → Console tab

---

**🦖 RawrXD — Code Everywhere, Deploy Anywhere**
