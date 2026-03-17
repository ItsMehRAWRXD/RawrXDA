# 🚀 Release v0.1.0 - Final Checklist

## ✅ Completed
- [x] All widgets have real implementations (CloudRunner, LSP, InlineChat, PluginManager)
- [x] TelemetryDialog with privacy-first consent
- [x] Dark/Light mode auto-switching
- [x] Smoke test suite (`testbench.ps1`)
- [x] CI/CD pipeline (`.github/workflows/release.yml`)
- [x] Binary builds successfully (376 KB)
- [x] Zero compiler errors
- [x] All signals/slots connected properly

## 🔲 Pre-Release Tasks (5 minutes)

### 1. Code Signing Certificate
**Option A: SSL.com ($99/year)**
```bash
# Purchase at: https://www.ssl.com/certificates/code-signing/
# Download .pfx file
# Convert to base64 for GitHub secrets:
certutil -encode codesign.pfx codesign_base64.txt
```

**Option B: Digicert ($474/year)**
```bash
# Purchase at: https://www.digicert.com/signing/code-signing-certificates
# Same process as SSL.com
```

**Option C: Self-Signed (Free, for testing)**
```powershell
# Generate self-signed cert (users will see SmartScreen warning)
New-SelfSignedCertificate -Type CodeSigning -Subject "CN=RawrXD" `
  -CertStoreLocation "Cert:\CurrentUser\My" `
  -NotAfter (Get-Date).AddYears(3)

# Export to PFX
$cert = Get-ChildItem Cert:\CurrentUser\My | Where Subject -eq "CN=RawrXD"
$pwd = ConvertTo-SecureString -String "YourPassword" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath "codesign.pfx" -Password $pwd

# Convert to base64
[Convert]::ToBase64String([IO.File]::ReadAllBytes("codesign.pfx")) | Out-File codesign_base64.txt
```

### 2. GitHub Secrets Setup
```bash
# Navigate to: https://github.com/ItsMehRAWRXD/RawrXD/settings/secrets/actions
# Click "New repository secret"

# Secret 1: CODESIGN_PFX
# Value: <paste contents of codesign_base64.txt>

# Secret 2: CODESIGN_PWD
# Value: <your pfx password>
```

### 3. Tag and Release
```bash
# Commit any pending changes
git add .
git commit -m "Production release v0.1.0 - All widgets with real implementations"

# Create annotated tag
git tag -a v0.1.0 -m "RawrXD-QtShell v0.1.0

Features:
- Cloud Runner (GitHub Actions integration)
- LSP Client (clangd, pylsp, rust-analyzer, gopls)
- Inline AI Chat (diff preview, ESC/Enter shortcuts)
- Plugin Manager (GitHub marketplace discovery)
- Telemetry consent dialog
- Dark/Light mode auto-switching

Size: 376 KB (uncompressed .exe)
Platforms: Windows 10/11 x64
License: MIT"

# Push tag (triggers CI workflow)
git push origin v0.1.0
```

## 📦 What Happens Next (Automated)

1. **GitHub Actions CI** (5-8 minutes)
   - ✅ Checkout code
   - ✅ Install Qt 6.7.3
   - ✅ Build with MSVC 2022
   - ✅ Run smoke tests
   - ✅ Sign binary (if cert configured)
   - ✅ Create GitHub Release
   - ✅ Upload `RawrXD-QtShell-v0.1.0-win64.zip`

2. **GitHub Release Page**
   - Auto-generated with size in description
   - Download link appears at: https://github.com/ItsMehRAWRXD/RawrXD/releases/tag/v0.1.0

## 🐦 Marketing Copy (Ready to Tweet)

```
🚀 RawrXD-QtShell v0.1.0 – 376 KB native Qt IDE

✨ Features:
• ⚡ Cloud builds on GitHub Actions
• 🔧 LSP client (C++, Python, Rust, Go)
• 💬 AI inline chat with diff preview
• 🧩 Plugin marketplace

📦 Download: https://github.com/ItsMehRAWRXD/RawrXD/releases/tag/v0.1.0

#cpp #Qt #IDE #opensource
```

## 📊 Post-Release Monitoring

### GitHub Insights (24 hours)
- Downloads: Check releases page
- Stars: Target 100 in first week
- Issues: Triage within 24h

### User Feedback Channels
- GitHub Issues: https://github.com/ItsMehRAWRXD/RawrXD/issues
- Reddit: r/cpp, r/programming
- Hacker News: https://news.ycombinator.com/submit

## 🔧 Troubleshooting

### Issue: Smoke tests fail with exit code -1073741515
**Cause:** Qt DLLs not in PATH
**Fix:** 
```powershell
# Option 1: Add Qt to PATH temporarily
$env:PATH = "C:\Qt\6.7.3\msvc2022_64\bin;$env:PATH"
.\testbench.ps1

# Option 2: Deploy Qt DLLs alongside exe (for release)
windeployqt.exe build\bin-msvc\Release\RawrXD-QtShell.exe
```

### Issue: Binary exceeds 200 KB budget
**Status:** Expected (376 KB with 4 new widgets)
**Fix (future):** 
- Strip debug symbols: `/DEBUG:NONE`
- Enable LTO: `/GL /LTCG`
- UPX compression: `upx --best RawrXD-QtShell.exe` (can reduce to ~120 KB)

### Issue: SmartScreen blocks unsigned binary
**Status:** Expected without paid cert
**Fix:** Users must click "More info" → "Run anyway"
**Long-term:** Purchase code signing cert ($99/year)

## 📝 Version History

### v0.1.0 (2025-12-01)
- Initial release
- All major widgets implemented
- 376 KB binary size
- Windows 10/11 x64 support

### v0.2.0 (Planned)
- Real GGML kernels (Q4_0/Q8_0)
- 10× inference speed
- KV-cache optimization
- Target: 400 KB binary

### v1.0.0 (Planned)
- Linux/macOS ports
- Stable plugin API
- 50+ community plugins
- Sub-500 KB binary

---

**Ready to ship?** Run: `git tag v0.1.0 && git push origin v0.1.0`
