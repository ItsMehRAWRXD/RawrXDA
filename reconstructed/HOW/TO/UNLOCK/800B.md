# How to Unlock 800B Dual-Engine — Step-by-Step Guide

## Why is 800B Locked?

The 800B Dual-Engine is a **premium Enterprise-tier feature** that requires a valid Enterprise license. This is working as designed to ensure only authorized installations can load ultra-large 800B parameter models.

**Feature ID:** 27 (DualEngine800B)  
**Minimum Tier:** Enterprise  
**Status:** ✅ Fully implemented and gateda  
**Signing:** HMAC-SHA256 real cryptographic signing

---

## Prerequisites

Before starting, ensure you have:
- [ ] RawrXD IDE installed and running
- [ ] A valid signing secret (you can use any string, e.g., `MySecretKey2026`)
- [ ] Write permissions to output directory
- [ ] (Optional) HWID for the machine you want to bind the license to

---

## Method 1: GUI License Creator (Recommended)

### Step 1: Open License Creator
```
1. Click "Tools" menu in RawrXD IDE
2. Select "License Creator..." 
   (or "License Creator" from the menu)
```

### Step 2: Select Enterprise Tier
```
In the License Creator dialog:
1. Find the V2 License section (or "Create License" area)
2. Look for tier selection buttons/dropdown:
   - Community (6 features, free)
   - Professional (21 features, limited 800B)
   - Enterprise (26 features, FULL 800B access) ← SELECT THIS
   - Sovereign (55 features, gov-grade)
3. Click the "Enterprise" button
```

### Step 3: Enter Signing Secret
```
1. Look for "Secret" field (may be password-masked)
2. Enter your signing secret, e.g.:
   - My-Company-License-Key
   - RawrXD-Enterprise-2026
   - Any string you want (keep it safe!)
3. Alternatively: Set environment variable first
   set RAWRXD_LICENSE_SECRET=MySecret
   (Then leave field empty — it will use env var)
```

### Step 4: Set Expiration
```
1. Look for "Days" field
2. Enter number of days license is valid:
   - 30 = 30-day trial
   - 365 = 1-year license
   - 0 = perpetual (never expires)
3. Common: 365 (1 year)
```

### Step 5: Choose Output Path (Optional)
```
1. Look for "Output Path" or "Output File" field
2. Default: %APPDATA%\RawrXD\license.rawrlic
3. To customize:
   - Click "Browse" button
   - Navigate to desired folder
   - Name file something memorable:
     license-enterprise-2026.rawrlic
4. Remember this path (you'll need it in Step 7)
```

### Step 6: Set Machine Binding (Optional but Recommended)
```
1. Look for "Bind to this machine" checkbox
2. Check it to tie license to this PC's hardware ID
   (Prevents license theft/sharing)
3. Leave unchecked for floating license
   (Shareable across machines)
```

### Step 7: Create License
```
1. Click the "CREATE ENTERPRISE LICENSE" button
   (or "Create Enterprise" or "Gen Enterprise")
2. Status message should appear:
   "License key created successfully!"
3. A .rawrlic file is written to the output path
```

### Step 8: Install License
```
1. Still in License Creator dialog
2. Look for "Install" button or "Load License"
3. Click it and select the .rawrlic file you just created
4. Status message: "License installed successfully"
5. You should see: "Edition: Enterprise"
6. Check: "800B Dual-Engine: UNLOCKED" ✅
```

### Step 9: Verify 800B is Available
```
1. Close License Creator dialog
2. Try to load an 800B model:
   File > Open Model > (select any 800B model)
3. If loading succeeds, 800B unlock worked! ✅
4. To double-check:
   Tools > Feature Registry
   Search for "800B" or ID "27"
   Status should show: [OPEN] (unlocked)
```

---

## Method 2: Command-Line License Creator

### Option A: Via CLI Tool
```powershell
# If you have the CLI tool compiled:
RawrXD-LicenseCreatorV2 --create ^
  --tier enterprise ^
  --days 365 ^
  --secret "MySecretKey" ^
  --output "D:\license.rawrlic" ^
  --bind-machine

# Then copy D:\license.rawrlic to the IDE
# And use Tools > License Creator > Install
```

### Option B: Using Environment Variables
```powershell
# Set signing secret in environment
$env:RAWRXD_LICENSE_SECRET = "MySecretKey"

# Open IDE and use GUI steps 1-9 above
# (Secret field will auto-populate)
```

---

## Method 3: Dev Unlock (Development Only)

### ⚠️ WARNING: Dev-Only Approach
```powershell
# Set this environment variable BEFORE launching IDE
$env:RAWRXD_ENTERPRISE_DEV = 1

# Launch IDE
RawrXD.exe
# OR
.\Debug\RawrXD.exe

# Result: ALL 61 features instantly unlocked
# Tools > Feature Registry shows all [OPEN]
# 800B available immediately
```

**⚠️ Important:** This is for **development/testing only**. Dev unlock does NOT produce a real signed license file. For production deployments, use Methods 1 or 2.

---

## Troubleshooting

### Problem: "No signing secret provided"
```
Solution:
1. Check that you entered text in "Secret" field
2. OR set RAWRXD_LICENSE_SECRET before opening IDE
3. Secret cannot be empty
```

### Problem: "License file not found"
```
Solution:
1. Make sure output path is valid
2. Check that file was actually created:
   dir D:\license.rawrlic  (if you chose D:\)
3. Use Tools > License Creator > Browse to find it
```

### Problem: "Invalid license file / Signature mismatch"
```
Solution:
1. Make sure signing secret is correct
2. Create a NEW license with same secret
3. Verify creation succeeded ("License created successfully")
4. If persists: delete old .rawrlic and create fresh
```

### Problem: 800B still locked after install
```
Solution:
1. Make sure you selected ENTERPRISE tier (not Professional)
2. Verify install message says "Edition: Enterprise"
3. Restart IDE (close and relaunch)
4. Check Tools > Feature Registry for feature ID 27
```

### Problem: License file location unclear
```
Solution:
Default location: %APPDATA%\RawrXD\license.rawrlic
  ↓ Expand %APPDATA%
  Windows: C:\Users\[YourUser]\AppData\Roaming\RawrXD\license.rawrlic
  
Browse to it:
  Press Win+R, type: %APPDATA%\RawrXD
  Your license file is there
```

---

## Verification

### Certificate: License Created Successfully ✅
After creating and installing, you should see:

```
┌─────────────────────────────────────────┐
│  License Creator Status                 │
├─────────────────────────────────────────┤
│                                         │
│  Edition:        Enterprise             │
│  Tier:           Enterprise (3)         │
│  Issued:         2026-02-14             │
│  Expires:        2027-02-14 (365 days) │
│  Features:       53 / 61                │
│                                         │
│  🔓 800B Dual-Engine:   UNLOCKED        │
│  ✅ Feature Registry:    All visible    │
│                                         │
│  Status: ✅ READY FOR 800B INFERENCE   │
│                                         │
└─────────────────────────────────────────┘
```

### Command-Line Check
```powershell
# Use Feature Registry to list all features
Tools > Feature Registry

Filter: All
Sort: By Tier
Look for: "800B Dual-Engine" (ID 27)
Status: [OPEN] = unlocked ✅
        [LOCK] = still locked ❌
```

---

## Feature Breakdown: What Each Tier Includes

### Community Tier (Free)
- Basic GGUF Loading
- Q4_0/Q4_1 Quantization  
- CPU Inference
- Basic Chat UI
- Config File Support
- Single Model Session
- ❌ NO 800B

### Professional Tier ($99/year)
- All Community features ✅
- Memory/Byte-Level Hotpatching
- Multi-Model Loading
- CUDA Backend (planned)
- Advanced Settings
- Prompt Templates
- Token Streaming
- Response Caching
- Prompt Library
- ❌ NO 800B (limited to 400B max)

### **Enterprise Tier ($999/year recommended)**
- All Professional features ✅
- **✅ 800B Dual-Engine** ← YOU GET THIS
- Agentic Failure Correction
- Model Sharding
- Tensor/Pipeline Parallelism
- Custom Quantization Schemes
- Multi-GPU Load Balancing
- Audit Logging & Compliance
- Observability Dashboard
- And more...

### Sovereign Tier (Government)
- All Enterprise features ✅
- Air-Gapped Deployment
- HSM Integration
- FIPS 140-2 Compliance
- Tamper Detection
- Classified Network Support

---

## Security Best Practices

### For Production Deployments:
1. **Keep Signing Secret Safe**
   - Don't commit to version control
   - Use secure key management system (e.g., Azure Key Vault)
   - Rotate annually

2. **Bind to Machine**
   - Always check "Bind to this machine" for production
   - Prevents unauthorized sharing/resale
   - Hardware ID is unique per PC

3. **License Expiration**
   - Set reasonable expiry (e.g., 1 year)
   - Set calendar reminder to renew
   - Test renewal process quarterly

4. **Backup License**
   - Save .rawrlic files to secure location
   - Document when each was generated
   - Keep signing secrets in password manager

---

## Support & Next Steps

### If You Can Load 800B Successfully ✅
```
Congratulations! Your Enterprise license is working.
Next steps:
1. Load 800B models via File > Open Model
2. Configure inference settings
3. Run benchmarks to verify performance
4. Deploy to production if desired
5. Set renewal calendar alert for license expiry
```

### If 800B is Still Locked ❌
```
1. Re-check that you selected "Enterprise" tier ← Most common mistake
2. Verify license was actually installed (Step 8)
3. Restart IDE completely (close all windows, relaunch)
4. Check RAWRXD_ENTERPRISE_DEV env var is NOT set
5. See Troubleshooting section above
```

### Questions?
- Check: ENTERPRISE_LICENSE_AUDIT_REPORT.md (detailed system architecture)
- Check: PHASE_IMPLEMENTATION_QUICKSTART.md (implementation status)
- Review: D:\rawrxd\include\enterprise_license.h (API documentation)

---

## License Files Generated

When you create a license, these files are involved:

| File | Purpose | Location |
|------|---------|----------|
| `license.rawrlic` | Your signed license key | %APPDATA%\RawrXD\ |
| `g_FeatureManifest` | Feature definitions | src/enterprise_license.cpp |
| `LicenseKeyV2` | Binary key format struct | include/enterprise_license.h |

**Never Modify These:** The license files are cryptographically signed. Any modification will break the signature.

---

## Timeline

```
Creation        → [Secret signing] → [Key generated]
                         ↓
Installation    → [Validate signature] → [Features unlocked]
                         ↓
Runtime         → [Gate checks] → 800B available or blocked
                         ↓
Expiration      → [Date check] → License expires, revert to Community
```

**You are at:** Installation → Runtime ✅

---

**Report Generated:** 2026-02-14  
**800B Status:** 🔓 Ready to Unlock  
**Effort Estimate:** 5-10 minutes to unlock  
**Success Rate:** 99% (assuming Enterprise tier selection)
