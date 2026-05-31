# RawrXD v1.0.0-gold - Deployment Package Structure

## Package Contents

```
RawrXD-v1.0.0-gold-win64/
├── RawrXD-Win32IDE.exe          # Main IDE executable (~14.7MB)
├── verify.exe                    # AST validation test suite
├── README.md                     # Quick start guide
├── LICENSE                       # MIT License
├── RELEASE_NOTES.md              # This release's changelog
│
├── config/
│   ├── default.json              # Default settings
│   ├── keybindings.json          # Keyboard shortcuts
│   └── themes/                   # UI themes
│
├── docs/
│   ├── API_REFERENCE.md          # Extension API docs
│   ├── ARCHITECTURE.md           # System architecture
│   └── TROUBLESHOOTING.md        # Common issues
│
└── redist/
    ├── vc_redist.x64.exe         # Visual C++ Redistributable
    └── vulkan_runtime.exe        # Vulkan Runtime (if needed)
```

## Distribution Archives

| File | Size | Purpose |
|------|------|---------|
| `RawrXD-v1.0.0-gold-win64.zip` | ~15MB | Standard ZIP distribution |
| `RawrXD-v1.0.0-gold-win64.7z` | ~12MB | Compressed 7z distribution |
| `sbom-v1.0.0-gold.json` | ~5KB | Software Bill of Materials |
| `manifest-v1.0.0-gold.json` | ~3KB | Release manifest with hashes |
| `hashes-v1.0.0-gold.txt` | ~2KB | SHA256 hashes of all files |

## Verification

### Checksum Verification

```powershell
# Verify ZIP integrity
Get-FileHash RawrXD-v1.0.0-gold-win64.zip -Algorithm SHA256
# Compare with manifest-v1.0.0-gold.json

# Verify individual files
cd RawrXD-v1.0.0-gold-win64
Get-FileHash RawrXD-Win32IDE.exe -Algorithm SHA256
# Compare with hashes-v1.0.0-gold.txt
```

### Digital Signature

If code-signed:
```powershell
Get-AuthenticodeSignature RawrXD-Win32IDE.exe
```

Expected output:
- Status: Valid
- Issuer: RawrXD Code Signing CA
- Timestamp: Present

## SBOM (Software Bill of Materials)

### Third-Party Components

| Component | Version | License | Usage |
|-----------|---------|---------|-------|
| GGML | b1559 | MIT | Inference backend |
| nlohmann/json | 3.11.2 | MIT | JSON parsing |
| moodycamel::ConcurrentQueue | 1.0.3 | BSD-2-Clause | Lock-free queue |
| QuickJS | 2023-12-23 | MIT | JavaScript engine |

### Build Dependencies

| Tool | Version | Purpose |
|------|---------|---------|
| Visual Studio 2022 | 17.x | Compiler |
| CMake | 3.25+ | Build system |
| Ninja | 1.11+ | Build tool |
| Vulkan SDK | 1.3.275+ | GPU compute |

## Installation Steps

### Standard Installation

1. **Download** `RawrXD-v1.0.0-gold-win64.zip`
2. **Extract** to `C:\Program Files\RawrXD\`
3. **Run** `RawrXD-Win32IDE.exe`
4. **Configure** model path in Settings

### Silent Installation (Enterprise)

```powershell
# Extract
Expand-Archive -Path RawrXD-v1.0.0-gold-win64.zip -DestinationPath "C:\Program Files\RawrXD"

# Create shortcut
$WshShell = New-Object -comObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$env:PUBLIC\Desktop\RawrXD.lnk")
$Shortcut.TargetPath = "C:\Program Files\RawrXD\RawrXD-Win32IDE.exe"
$Shortcut.Save()

# Verify installation
& "C:\Program Files\RawrXD\verify.exe"
```

## Post-Installation Verification

### Quick Test

1. Launch RawrXD
2. Open any code file
3. Type `class MyClass {` and press Enter
4. Verify ghost text suggests completions
5. Check that AST scope is respected (private members not suggested outside class)

### Full Validation

```powershell
# Run all validation tests
& "C:\Program Files\RawrXD\verify.exe"

# Expected output:
# [TEST] Access Modifier Sovereignty... PASS
# [TEST] Template Parameter Deduction... PASS
# [TEST] CRTP Pattern Recognition... PASS
# [TEST] Concept Constraints... PASS
# [TEST] Nested Class Scope Resolution... PASS
# [TEST] Lambda Capture Analysis... PASS
# RESULTS: 6/6 tests passed
```

## Troubleshooting

### "Missing DLL" Error

Install Visual C++ Redistributable:
```
redist\vc_redist.x64.exe /install /quiet /norestart
```

### "No Vulkan Device" Error

1. Update GPU drivers
2. Install Vulkan Runtime:
```
redist\vulkan_runtime.exe
```

### AST Tests Fail

1. Verify file integrity:
```powershell
Get-FileHash verify.exe -Algorithm SHA256
```
2. Re-download package if hash mismatch
3. Check Windows Defender exclusions

## Support

- **Documentation:** https://rawrxd.io/docs
- **Issues:** https://github.com/ItsMehRAWRXD/RawrXD/issues
- **Email:** support@rawrxd.io

---

**Package Generated:** 2026-05-02  
**Validation Status:** PASSED (6/6 tests)  
**Release Status:** PRODUCTION READY
