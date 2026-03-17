# Code Signing and Icon Configuration Guide

**MyCopilot IDE - Production Distribution Setup**

This guide covers how to add a custom icon and code signing for production distribution of MyCopilot IDE.

---

## 🎨 Custom Application Icon

### Why Add a Custom Icon?

- **Branding:** Professional appearance in taskbar and start menu
- **Recognition:** Users easily identify your application
- **Trust:** Custom icons convey legitimacy

### Requirements

- **Format:** `.ico` file (Windows icon format)
- **Sizes:** Multiple resolutions for different contexts
  - 16x16 (small icons)
  - 32x32 (standard icons)
  - 48x48 (large icons)
  - 256x256 (high-DPI displays)

### Creating the Icon

#### Option 1: Online Icon Converter

1. Design your logo in any graphics program (PNG, SVG, etc.)
2. Use an online converter:
   - https://icoconvert.com/
   - https://convertio.co/png-ico/
   - https://favicon.io/

3. Upload your image and select multiple sizes
4. Download the generated `.ico` file

#### Option 2: Using ImageMagick

```powershell
# Install ImageMagick
winget install ImageMagick.ImageMagick

# Convert PNG to ICO with multiple sizes
magick convert logo.png -define icon:auto-resize=256,128,96,64,48,32,16 icon.ico
```

#### Option 3: GIMP (Free)

1. Install GIMP from https://www.gimp.org/
2. Open your logo image
3. Scale to 256x256 (Image → Scale Image)
4. Export as `.ico` (File → Export As → Select `.ico` format)
5. In the dialog, enable "Compressed (PNG)"

### Adding Icon to MyCopilot IDE

1. **Save the icon file:**
   ```powershell
   # Place icon in the project directory
   Copy-Item "path\to\your\icon.ico" "D:\MyCopilot-IDE\assets\icon.ico"
   ```

2. **Update package.json:**

   ```json
   {
     "name": "mycopilot-ide",
     "version": "1.0.0",
     "main": "electron-main.js",
     "build": {
       "appId": "com.mycopilot.ide",
       "productName": "MyCopilot IDE",
       "icon": "assets/icon.ico",
       "win": {
         "target": ["nsis", "portable"],
         "icon": "assets/icon.ico"
       },
       "nsis": {
         "oneClick": false,
         "allowToChangeInstallationDirectory": true,
         "createDesktopShortcut": true,
         "createStartMenuShortcut": true
       }
     }
   }
   ```

3. **Rebuild the application:**
   ```powershell
   Set-Location "D:\MyCopilot-IDE"
   npx electron-builder --win --x64
   ```

### Icon Design Best Practices

- **Simple and Clear:** Avoid complex details that don't scale well
- **Recognizable:** Should be identifiable at small sizes
- **Consistent Colors:** Use your brand colors
- **High Contrast:** Ensure visibility on various backgrounds
- **Test at Multiple Sizes:** View at 16x16, 32x32, and 48x48

### Example Icon Design (ASCII Art)

```
┌─────────────────────────────────┐
│                                 │
│    ┌─────┐      ┌─────┐        │
│    │  M  │ ─────│  C  │        │
│    └─────┘      └─────┘        │
│       │            │            │
│       │    ┌───────┘            │
│       └────┤  IDE               │
│            └───────             │
│                                 │
└─────────────────────────────────┘
```

**Suggested Design:** 
- Stylized "MC" or "MCI" letters
- Code brackets `</>` with AI symbol
- Robot/AI icon with code editor
- Terminal window with sparkles

---

## 🔐 Code Signing Configuration

### Why Code Signing?

- **Security:** Prevents tampering and verifies authenticity
- **Trust:** Windows SmartScreen doesn't warn users
- **Professionalism:** Required for enterprise distribution
- **Updates:** Signed apps can auto-update without warnings

### Requirements

1. **Code Signing Certificate** from a trusted Certificate Authority (CA)
2. **Private Key** (kept secure, never shared)
3. **Certificate File** (.pfx or .p12 format)
4. **Password** to protect the private key

### Obtaining a Code Signing Certificate

#### Commercial CAs (Recommended for Production)

**Option 1: DigiCert** (~$400-600/year)
- Website: https://www.digicert.com/signing/code-signing-certificates
- Most trusted by Windows
- 1-3 year validity
- Process: 1-5 business days

**Option 2: Sectigo (formerly Comodo)** (~$200-400/year)
- Website: https://sectigo.com/ssl-certificates-tls/code-signing
- Good reputation
- Faster approval process

**Option 3: GlobalSign** (~$300-500/year)
- Website: https://www.globalsign.com/en/code-signing-certificate
- International support

#### For Development/Testing

**Self-Signed Certificate** (Free, not trusted by Windows)

```powershell
# Create self-signed certificate for testing
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=MyCopilot IDE Development, O=YourCompany, C=US" `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -Provider "Microsoft Enhanced RSA and AES Cryptographic Provider" `
    -KeyExportPolicy Exportable `
    -KeyUsage DigitalSignature `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(2)

# Export to PFX file
$password = ConvertTo-SecureString -String "YourStrongPassword123!" -Force -AsPlainText
Export-PfxCertificate `
    -Cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" `
    -FilePath "D:\MyCopilot-IDE\certs\dev-cert.pfx" `
    -Password $password

Write-Host "Certificate created: $($cert.Thumbprint)"
Write-Host "Exported to: D:\MyCopilot-IDE\certs\dev-cert.pfx"
```

**Note:** Self-signed certificates still trigger SmartScreen warnings. Only use for development.

### Certificate Application Process (Commercial CA)

1. **Prepare Information:**
   - Company name (must match legal registration)
   - DUNS number or equivalent
   - Contact information
   - Business verification documents

2. **Generate CSR (Certificate Signing Request):**
   ```powershell
   # The CA website will guide you through this
   # Usually done via their web portal
   ```

3. **Validation Process:**
   - CA verifies your organization
   - May require phone call, documents, or domain verification
   - Typically takes 1-5 business days

4. **Receive Certificate:**
   - Download the `.pfx` or `.p12` file
   - Store securely with password

### Configuring electron-builder for Code Signing

#### Method 1: Environment Variables (Recommended for CI/CD)

```powershell
# Set environment variables
$env:CSC_LINK = "D:\MyCopilot-IDE\certs\certificate.pfx"
$env:CSC_KEY_PASSWORD = "YourCertificatePassword"

# Build with signing
npx electron-builder --win --x64
```

#### Method 2: package.json Configuration

```json
{
  "build": {
    "win": {
      "target": ["nsis", "portable"],
      "icon": "assets/icon.ico",
      "certificateFile": "certs/certificate.pfx",
      "certificatePassword": "USE_ENVIRONMENT_VARIABLE",
      "signingHashAlgorithms": ["sha256"],
      "rfc3161TimeStampServer": "http://timestamp.digicert.com"
    }
  }
}
```

**Security Note:** Never commit certificate passwords to git!

#### Method 3: Using Azure Key Vault (Enterprise)

For teams, store certificates in Azure Key Vault:

```json
{
  "build": {
    "win": {
      "certificateSubjectName": "Your Company Name",
      "certificateSha1": "THUMBPRINT_HERE",
      "signingHashAlgorithms": ["sha256"],
      "rfc3161TimeStampServer": "http://timestamp.digicert.com",
      "azureKeyVault": {
        "vaultName": "your-vault-name",
        "certificateName": "your-cert-name",
        "clientId": "${AZURE_CLIENT_ID}",
        "clientSecret": "${AZURE_CLIENT_SECRET}",
        "tenantId": "${AZURE_TENANT_ID}"
      }
    }
  }
}
```

### Timestamping (Critical!)

**Why Timestamp?**
- Signature remains valid even after certificate expires
- Required for long-term distribution

**Add to package.json:**
```json
{
  "build": {
    "win": {
      "rfc3161TimeStampServer": "http://timestamp.digicert.com"
    }
  }
}
```

**Alternative Timestamp Servers:**
- DigiCert: `http://timestamp.digicert.com`
- Sectigo: `http://timestamp.sectigo.com`
- GlobalSign: `http://timestamp.globalsign.com`

### Verifying Code Signature

After building, verify the signature:

```powershell
# Check signature
Get-AuthenticodeSignature "D:\MyCopilot-IDE\build-*\MyCopilot-IDE-Setup-1.0.0.exe"

# Expected output:
# Status        : Valid
# SignerCertificate : [Thumbprint: YOUR_THUMBPRINT]
# TimeStamperCertificate : [Thumbprint: TIMESTAMP_THUMBPRINT]

# Detailed verification
sigcheck.exe -v "D:\MyCopilot-IDE\build-*\MyCopilot-IDE-Setup-1.0.0.exe"
```

### Build Script with Signing

Create `Build-Signed.ps1`:

```powershell
#Requires -Version 5.1
param(
    [Parameter(Mandatory)]
    [string]$CertificatePath,
    
    [Parameter(Mandatory)]
    [SecureString]$CertificatePassword
)

$ErrorActionPreference = "Stop"

Write-Host "Building and signing MyCopilot IDE..." -ForegroundColor Cyan

# Set environment variables
$env:CSC_LINK = $CertificatePath
$env:CSC_KEY_PASSWORD = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
    [Runtime.InteropServices.Marshal]::SecureStringToBSTR($CertificatePassword)
)

try {
    # Change to project directory
    Set-Location "D:\MyCopilot-IDE"
    
    # Build with signing
    npx electron-builder --win --x64
    
    Write-Host "`n✓ Build and signing complete!" -ForegroundColor Green
    
    # Verify signature
    Write-Host "`nVerifying signatures..." -ForegroundColor Yellow
    
    $setupExe = Get-ChildItem "build-*\MyCopilot-IDE-Setup-*.exe" | Select-Object -First 1
    $portableExe = Get-ChildItem "build-*\MyCopilot-IDE-Portable-*.exe" | Select-Object -First 1
    
    foreach ($exe in @($setupExe, $portableExe)) {
        if ($exe) {
            $sig = Get-AuthenticodeSignature $exe.FullName
            Write-Host "`n$($exe.Name):" -ForegroundColor Cyan
            Write-Host "  Status: $($sig.Status)" -ForegroundColor $(
                if ($sig.Status -eq 'Valid') { 'Green' } else { 'Red' }
            )
            Write-Host "  Signer: $($sig.SignerCertificate.Subject)"
        }
    }
}
finally {
    # Clear sensitive environment variables
    Remove-Item Env:\CSC_LINK -ErrorAction SilentlyContinue
    Remove-Item Env:\CSC_KEY_PASSWORD -ErrorAction SilentlyContinue
}
```

Usage:
```powershell
$certPassword = Read-Host "Enter certificate password" -AsSecureString
.\Build-Signed.ps1 -CertificatePath ".\certs\certificate.pfx" -CertificatePassword $certPassword
```

### Security Best Practices

1. **Never Commit Certificates to Git:**
   ```gitignore
   # .gitignore
   certs/
   *.pfx
   *.p12
   *.key
   ```

2. **Protect Certificate Files:**
   ```powershell
   # Set file permissions (Windows)
   icacls "D:\MyCopilot-IDE\certs\certificate.pfx" /inheritance:r /grant:r "$env:USERNAME:(R)"
   ```

3. **Use Hardware Security Modules (HSM):**
   - For high-security environments
   - Certificate stored on physical USB token
   - Private key never leaves the device

4. **Rotate Certificates:**
   - Plan for renewal before expiration
   - Test new certificate before old one expires
   - Update build systems and CI/CD pipelines

5. **Audit Certificate Usage:**
   ```powershell
   # Log all signing operations
   Get-AuthenticodeSignature ".\signed-app.exe" | 
       Export-Csv "signing-log.csv" -Append
   ```

### CI/CD Integration

**GitHub Actions Example:**

```yaml
name: Build and Sign

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: windows-latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: '18'
      
      - name: Install dependencies
        run: npm install
      
      - name: Decode certificate
        run: |
          $certBytes = [Convert]::FromBase64String("${{ secrets.CERTIFICATE_BASE64 }}")
          [IO.File]::WriteAllBytes("certificate.pfx", $certBytes)
      
      - name: Build and sign
        env:
          CSC_LINK: certificate.pfx
          CSC_KEY_PASSWORD: ${{ secrets.CERTIFICATE_PASSWORD }}
        run: npx electron-builder --win --x64
      
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: signed-executables
          path: build-*/MyCopilot-IDE-*.exe
```

### Troubleshooting

**Error: "SignTool Error: No certificates were found that met all the given criteria."**

- Certificate not found in specified location
- Certificate password incorrect
- Certificate expired

**Solution:**
```powershell
# Verify certificate
$cert = Get-PfxCertificate -FilePath "certificate.pfx"
Write-Host "Subject: $($cert.Subject)"
Write-Host "Expires: $($cert.NotAfter)"
Write-Host "Valid: $($cert.NotAfter -gt (Get-Date))"
```

**Error: "TimeStamp server unreachable"**

- Timestamp server down or slow
- Network/firewall blocking

**Solution:**
Try alternative timestamp servers in package.json

---

## 📋 Complete Example: Icon + Code Signing

**Final package.json:**

```json
{
  "name": "mycopilot-ide",
  "version": "1.0.0",
  "description": "AI-Powered Development Environment",
  "main": "electron-main.js",
  "scripts": {
    "start": "electron .",
    "build": "electron-builder --win --x64",
    "build-signed": "electron-builder --win --x64"
  },
  "build": {
    "appId": "com.mycopilot.ide",
    "productName": "MyCopilot IDE",
    "copyright": "Copyright © 2025 Your Company",
    "directories": {
      "output": "build-${timestamp}",
      "buildResources": "assets"
    },
    "files": [
      "**/*",
      "!tests",
      "!docs",
      "!.git"
    ],
    "win": {
      "target": [
        {
          "target": "nsis",
          "arch": ["x64"]
        },
        {
          "target": "portable",
          "arch": ["x64"]
        }
      ],
      "icon": "assets/icon.ico",
      "certificateFile": "certs/certificate.pfx",
      "certificatePassword": "${CSC_KEY_PASSWORD}",
      "signingHashAlgorithms": ["sha256"],
      "rfc3161TimeStampServer": "http://timestamp.digicert.com",
      "verifyUpdateCodeSignature": true
    },
    "nsis": {
      "oneClick": false,
      "allowToChangeInstallationDirectory": true,
      "allowElevation": true,
      "createDesktopShortcut": true,
      "createStartMenuShortcut": true,
      "installerIcon": "assets/icon.ico",
      "uninstallerIcon": "assets/icon.ico",
      "installerHeaderIcon": "assets/icon.ico",
      "deleteAppDataOnUninstall": false
    },
    "portable": {
      "artifactName": "${productName}-Portable-${version}.exe"
    }
  },
  "devDependencies": {
    "electron": "^27.3.11",
    "electron-builder": "^24.13.3"
  }
}
```

**Build command:**
```powershell
# Set certificate password
$env:CSC_KEY_PASSWORD = "YourPassword"

# Build with icon and signing
npx electron-builder --win --x64

# Verify
Get-AuthenticodeSignature "build-*\MyCopilot-IDE-Setup-*.exe"
```

---

## ✅ Checklist

Before production release:

- [ ] Custom icon created (256x256, 128x128, 64x64, 48x48, 32x32, 16x16)
- [ ] Icon file saved to `assets/icon.ico`
- [ ] package.json updated with icon path
- [ ] Code signing certificate obtained from trusted CA
- [ ] Certificate stored securely (not in git)
- [ ] package.json updated with signing configuration
- [ ] Timestamp server configured
- [ ] Test build completes successfully
- [ ] Signature verified with Get-AuthenticodeSignature
- [ ] Application runs without SmartScreen warning
- [ ] Icon appears correctly in taskbar, start menu, and file explorer

---

**Ready for production distribution! 🚀**

For questions or issues, see USER-GUIDE.md or BUILD-COMPLETE-REPORT.md.
