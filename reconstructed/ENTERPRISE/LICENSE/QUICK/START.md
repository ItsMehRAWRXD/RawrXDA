# RawrXD Enterprise License — Quick Start Guide
**5-Minute Setup for License Creation & Testing**

---

## 🚀 Quick Start (Copy-Paste Ready)

### Step 1: Build License Creator (1 minute)
```powershell
cd d:\rawrxd\tools\license_creator
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

**Output**: `build\Release\RawrXD_LicenseCreator.exe`

---

### Step 2: Generate Master Keypair (30 seconds)
```powershell
cd build\Release
.\RawrXD_LicenseCreator.exe --generate-keypair --output master
```

**Creates**:
- `master_public.blob` — Embed in ASM (safe to share)
- `master_private.blob` — **KEEP SECRET** (signing key)

---

### Step 3: Create Test Licenses (1 minute)
```powershell
# Enterprise license (all features, perpetual, any machine)
.\RawrXD_LicenseCreator.exe --create-license --tier enterprise --output .\enterprise.rawrlic

# Trial license (30 days, limited features)
.\RawrXD_LicenseCreator.exe --create-license --tier trial --expiry-days 30 --output .\trial.rawrlic

# Pro license (perpetual, mid-tier features)
.\RawrXD_LicenseCreator.exe --create-license --tier pro --output .\pro.rawrlic
```

**Creates**: `.rawrlic` files ready to install

---

### Step 4: Test in RawrXD-Shell (2 minutes)

#### Option A: Dev Unlock (No License Needed)
```powershell
# Set dev mode
$env:RAWRXD_ENTERPRISE_DEV = "1"

# Launch RawrXD
cd d:\rawrxd\build\Release
.\RawrXD-Shell.exe
```

**Expected Output**:
```
[Enterprise] DEV UNLOCK ACTIVE (all features enabled)
╔════════════════════════════════════════════════════════════════════════╗
║                    RawrXD Enterprise License System                    ║
╠════════════════════════════════════════════════════════════════════════╣
║ Edition:       Enterprise                                              ║
║ Features:      800B Dual-Engine, AVX-512 Premium, Flash-Attention...  ║
║ Max Model:     800 GB                                                  ║
║ Max Context:   200000 tokens                                           ║
║ 800B Dual-Engine: UNLOCKED ✓                                          ║
╚════════════════════════════════════════════════════════════════════════╝
```

#### Option B: Install License File
```powershell
# Launch RawrXD (community mode)
.\RawrXD-Shell.exe

# In RawrXD console:
> import-license d:\rawrxd\tools\license_creator\build\Release\enterprise.rawrlic

# Shows:
# ✓ License installed successfully!
# Edition: Enterprise
# Features: 800B Dual-Engine, AVX-512 Premium, ...
```

---

## 🎯 Key Commands Cheat Sheet

### License Creator CLI
```powershell
# Generate keypair
RawrXD_LicenseCreator.exe --generate-keypair [--output <prefix>]

# Create license
RawrXD_LicenseCreator.exe --create-license \
    --tier <community|trial|pro|enterprise|oem> \
    --output <file.rawrlic> \
    [--expiry-days <N>] \
    [--hwid <hash>] \
    [--seats <N>]

# Verify license
RawrXD_LicenseCreator.exe --verify <file.rawrlic>

# Show machine HWID
RawrXD_LicenseCreator.exe --show-hwid

# Embed public key (for production build)
RawrXD_LicenseCreator.exe --embed-public-key \
    --public-key <file> \
    --output <include.inc>
```

### Environment Variable
```powershell
# Windows
set RAWRXD_ENTERPRISE_DEV=1

# Linux/Mac
export RAWRXD_ENTERPRISE_DEV=1
```

### RawrXD-Shell Commands
```
> import-license <path>      — Install .rawrlic file
> license-status             — Show current license
> list-engines               — Show available engines
> load-model <path>          — Load model (checks tier limits)
```

---

## 🔥 Feature Unlock Matrix (Quick Reference)

| Tier | Model Limit | Context | Features Mask | 800B Engine |
|------|-------------|---------|---------------|-------------|
| **Community** | 70 GB | 32K | 0x00 | 🔒 Locked |
| **Trial** | 180 GB | 128K | 0x4A | 🔒 Locked |
| **Pro** | 400 GB | 128K | 0x4A | 🔒 Locked |
| **Enterprise** | 800 GB | 200K | 0xFF | ✅ Unlocked |
| **OEM** | 800 GB | 200K | 0xFF | ✅ Unlocked |
| **Dev Mode** | ∞ | 200K | 0xFF | ✅ Unlocked |

### Feature Bits
- `0x01` — 800B Dual-Engine
- `0x02` — AVX-512 Premium
- `0x04` — Distributed Swarm
- `0x08` — GPU 4-bit Quantization
- `0x10` — Enterprise Support
- `0x20` — Unlimited Context (200K)
- `0x40` — Flash-Attention v2
- `0x80` — Multi-GPU Parallel

**Pro/Trial Mask** = `0x4A` (bits 1,3,6: AVX512 + GPUQuant + FlashAttention)  
**Enterprise Mask** = `0xFF` (all 8 bits)

---

## 🛠️ Troubleshooting

### Problem: "License verification failed"
**Solution**: 
1. Check if keypair matches (public key embedded in ASM must match signing key)
2. Verify .rawrlic file is not corrupted
3. Run `--verify` to see detailed error

### Problem: "Dev unlock not working"
**Solution**:
1. Check env var: `echo %RAWRXD_ENTERPRISE_DEV%` (should show "1")
2. Restart terminal after setting env var
3. Check ASM build includes `Enterprise_DevUnlock` function

### Problem: "800B Dual-Engine still locked with Enterprise license"
**Solution**:
1. Verify license tier is `Enterprise` or `OEM` (not Pro/Trial)
2. Check feature mask is `0xFF` (not `0x4A`)
3. Run `license-status` to confirm

### Problem: "Cannot load model — exceeds tier limit"
**Solution**:
- Community: Max 70B models (70 GB)
- Pro: Max 400B models (400 GB)
- Enterprise: Max 800B models (800 GB)
- Use `--tier enterprise` when creating license

---

## 📂 Important File Locations

### Build Outputs
```
d:\rawrxd\tools\license_creator\build\Release\
  └─ RawrXD_LicenseCreator.exe       — CLI tool
  └─ master_public.blob              — Public key
  └─ master_private.blob             — **KEEP SECRET**
  └─ *.rawrlic                       — License files
```

### Source Files
```
d:\rawrxd\src\asm\
  └─ RawrXD_EnterpriseLicense.asm    — License validation (ASM)
  └─ RawrXD_License_Shield.asm       — Anti-tamper shield

d:\rawrxd\src\core\
  └─ enterprise_license.{h,cpp}      — C++ bridge
  └─ enterprise_license_panel.cpp    — UI/console display

d:\rawrxd\tools\license_creator\
  └─ main.cpp                        — CLI tool
  └─ license_generator.{hpp,cpp}     — License API
```

### Documentation
```
d:\ENTERPRISE_LICENSE_AUDIT.md                      — Full audit report
d:\ENTERPRISE_FEATURES_DISPLAY_GUIDE.md             — Integration guide
d:\ENTERPRISE_LICENSE_IMPLEMENTATION_COMPLETE.md    — Summary
d:\ENTERPRISE_LICENSE_QUICK_START.md                — This file
```

---

## 🎓 Common Workflows

### Workflow 1: Local Development (No License)
```powershell
$env:RAWRXD_ENTERPRISE_DEV = "1"
.\RawrXD-Shell.exe
# All features unlocked automatically
```

### Workflow 2: Create License for Customer
```powershell
# Get customer HWID (if binding to specific machine)
$hwid = .\RawrXD_LicenseCreator.exe --show-hwid

# Create license
.\RawrXD_LicenseCreator.exe --create-license \
    --tier enterprise \
    --customer-name "Acme Corp" \
    --customer-email "admin@acme.com" \
    --hwid $hwid \
    --output acme_enterprise.rawrlic

# Verify before delivery
.\RawrXD_LicenseCreator.exe --verify acme_enterprise.rawrlic
```

### Workflow 3: Embed Production Key (One-Time Setup)
```powershell
# Generate production keypair
.\RawrXD_LicenseCreator.exe --generate-keypair --output production

# Generate ASM include
.\RawrXD_LicenseCreator.exe --embed-public-key \
    --public-key production_public.blob \
    --output ..\..\..\..\src\asm\embedded_public_key.inc

# Update RawrXD_EnterpriseLicense.asm:
# Replace RSA_PUBLIC_KEY_BLOB with contents of embedded_public_key.inc

# Rebuild RawrXD-Shell.exe
cd ..\..\..\..\build
cmake --build . --config Release
```

---

## ✅ Quick Verification Tests

### Test 1: Dev Unlock
```powershell
$env:RAWRXD_ENTERPRISE_DEV = "1"
.\RawrXD-Shell.exe
# Should show: "DEV UNLOCK ACTIVE"
```

### Test 2: License Install
```powershell
.\RawrXD-Shell.exe
> import-license .\enterprise.rawrlic
# Should show: "Edition: Enterprise"
```

### Test 3: 800B Engine Registration
```powershell
.\RawrXD-Shell.exe
> list-engines
# Should show: Llama 3.3 800B (Enterprise), Qwen 800B (Enterprise), ...
```

### Test 4: Feature Check
```powershell
.\RawrXD-Shell.exe
> license-status
# Should show all 8 features with checkmarks
```

---

## 📞 Need Help?

**Documentation**:
- Full audit: `ENTERPRISE_LICENSE_AUDIT.md`
- Feature guide: `ENTERPRISE_FEATURES_DISPLAY_GUIDE.md`
- Implementation summary: `ENTERPRISE_LICENSE_IMPLEMENTATION_COMPLETE.md`

**CLI Help**:
```powershell
.\RawrXD_LicenseCreator.exe --help
```

**Common Issues**: See "Troubleshooting" section above

---

**TIP**: Bookmark this file — it's all you need for day-to-day license operations!

---

**Last Updated**: February 14, 2026  
**Version**: 1.0  
**Status**: Production Ready ✅
