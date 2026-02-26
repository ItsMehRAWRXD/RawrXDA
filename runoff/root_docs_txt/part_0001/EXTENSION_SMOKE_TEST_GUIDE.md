# RawrXD IDE — Extension Smoke Test Guide

## 🎯 Overview

This directory contains comprehensive smoke tests for **Amazon Q** and **GitHub Copilot** VS Code extensions running inside the **RawrXD IDE** environment.

---

## 📦 Extension Files

- **Amazon Q**: `d:\rawrxd\amazon-q-vscode-latest.vsix`
- **GitHub Copilot**: `d:\rawrxd\copilot-latest.vsix`

---

## 🧪 Test Scripts

### 1. **Test-Extensions-Smoke.ps1**
**Comprehensive infrastructure and integration testing**

```powershell
# Run all tests (both GUI and CLI)
.\Test-Extensions-Smoke.ps1

# Test only CLI mode
.\Test-Extensions-Smoke.ps1 -Mode cli

# Test only GUI mode
.\Test-Extensions-Smoke.ps1 -Mode gui

# Verbose output
.\Test-Extensions-Smoke.ps1 -Verbose
```

**Tests:**
- ✓ VSIX file validation and integrity
- ✓ Extension loading in CLI mode
- ✓ Extension loading in GUI mode
- ✓ Plugin directory structure
- ✓ Extension registry persistence
- ✓ Simultaneous extension loading
- ✓ Command execution (`!plugin` commands)

---

### 2. **Test-Extensions-Features.ps1**
**Deep feature validation and API testing**

```powershell
# Test both extensions
.\Test-Extensions-Features.ps1

# Test only Copilot
.\Test-Extensions-Features.ps1 -Extension copilot

# Test only Amazon Q
.\Test-Extensions-Features.ps1 -Extension amazonq

# Interactive mode
.\Test-Extensions-Features.ps1 -Interactive
```

**GitHub Copilot Tests:**
- ✓ Code autocompletion API
- ✓ Chat interface
- ✓ Code explanation
- ✓ GUI integration
- ✓ Performance benchmarks

**Amazon Q Tests:**
- ✓ AWS-aware chat responses
- ✓ Security vulnerability scanning
- ✓ Lambda code generation
- ✓ GUI integration
- ✓ Performance benchmarks

---

### 3. **Run-All-Extension-Tests.ps1**
**Master test runner (executes all tests)**

```powershell
# Run complete test suite
.\Run-All-Extension-Tests.ps1

# Run with detailed output
.\Run-All-Extension-Tests.ps1 -Detailed

# Export results to HTML
.\Run-All-Extension-Tests.ps1 -ExportHTML
```

---

## 🚀 Quick Start

### **Prerequisites**

1. **RawrXD IDE** built and available at:
   - CLI: `d:\rawrxd\RawrXD.exe`
   - GUI: `d:\rawrxd\RawrXD_IDE.exe`

2. **VSIX files** present:
   - `d:\rawrxd\amazon-q-vscode-latest.vsix`
   - `d:\rawrxd\copilot-latest.vsix`

3. **PowerShell 5.1+** (Windows PowerShell or PowerShell Core)

### **Running Tests**

```powershell
# Navigate to RawrXD directory
cd d:\rawrxd

# Run quick smoke test
.\Test-Extensions-Smoke.ps1

# Run feature validation
.\Test-Extensions-Features.ps1

# Run everything
.\Run-All-Extension-Tests.ps1
```

---

## 📊 Test Results

### **Console Output**

Tests display real-time status:
- 🟢 **[PASS]** — Test succeeded
- 🔴 **[FAIL]** — Test failed
- 🟡 **[WARN]** — Warning/partial success
- 🔵 **[INFO]** — Informational

### **Log Files**

Individual test outputs saved to:
```
d:\rawrxd\test_cli_*.txt
d:\rawrxd\test_copilot_*.txt
d:\rawrxd\test_amazonq_*.txt
d:\rawrxd\*_out.txt
d:\rawrxd\*_err.txt
```

### **Summary Reports**

JSON reports generated:
```
d:\rawrxd\extension_smoketest_report_YYYYMMDD_HHMMSS.json
```

---

## 🔧 Manual Testing

### **CLI Mode**

```bash
# Start RawrXD CLI
d:\rawrxd\RawrXD.exe --cli

# Load extensions
!plugin load d:\rawrxd\amazon-q-vscode-latest.vsix
!plugin load d:\rawrxd\copilot-latest.vsix

# List loaded extensions
!plugin list

# Enable an extension
!plugin enable github.copilot

# Test chat
/chat How do I create a REST API?

# Exit
/exit
```

### **GUI Mode**

```bash
# Start RawrXD IDE with extension
d:\rawrxd\RawrXD_IDE.exe --load-extension d:\rawrxd\copilot-latest.vsix

# Or load from menu:
# Menu → Extensions → Install from VSIX → Select .vsix file
```

---

## 🔍 Troubleshooting

### **Extension Not Loading**

1. Check VSIX file integrity:
```powershell
Get-FileHash d:\rawrxd\amazon-q-vscode-latest.vsix
```

2. Verify plugin directory exists:
```powershell
Test-Path d:\rawrxd\plugins
```

3. Check logs:
```powershell
Get-Content d:\rawrxd\rawrxd_ide.log -Tail 50
```

### **Authentication Issues**

**GitHub Copilot:**
```bash
# Set token
$env:GITHUB_TOKEN = "your-token-here"

# Or in IDE:
/settings set github_token your-token-here
```

**Amazon Q:**
```bash
# Configure AWS credentials
$env:AWS_ACCESS_KEY_ID = "your-key"
$env:AWS_SECRET_ACCESS_KEY = "your-secret"

# Or use AWS CLI profile
aws configure
```

### **Performance Issues**

1. Check system resources:
```powershell
Get-Process RawrXD* | Select-Object Name, CPU, WorkingSet
```

2. Clear plugin cache:
```powershell
Remove-Item d:\rawrxd\plugins\* -Recurse -Force
```

3. Rebuild extension registry:
```powershell
Remove-Item d:\rawrxd\plugins\registry.json
```

---

## 📝 Expected Results

### **Successful Test Run**

```
╔═══════════════════════════════════════════════════════════════╗
║              Test Results Summary                              ║
╚═══════════════════════════════════════════════════════════════╝

  Total Tests: 28
  Passed:      26  ✓
  Failed:      0   ✗
  Warnings:    2   ⚠
  Info:        0   ℹ

✓ All critical tests passed!
```

### **Partial Success**

Some features may require authentication:
- Code completion: Requires active API connection
- Chat features: May need tokens/credentials
- Security scanning: May require AWS credentials

---

## 🐛 Known Issues

1. **Extension Registry Persistence**
   - First load may be slower
   - Solution: Registry cached after first run

2. **GUI Threading**
   - GUI tests may timeout on slow systems
   - Solution: Increase timeout in test script

3. **Authentication**
   - Extensions need valid credentials for full features
   - Solution: Set environment variables or use IDE settings

---

## 📚 Additional Resources

- **RawrXD IDE Documentation**: `d:\rawrxd\README.md`
- **Extension API Reference**: `d:\rawrxd\docs\EXTENSION_API.md`
- **Plugin System Guide**: `d:\rawrxd\VSIX_AND_MENU_QUICK_REFERENCE.md`
- **Full Audit Report**: `d:\rawrxd\FULL_PARITY_AUDIT_2026-02-15.md`

---

## 🤝 Support

Issues or questions:
1. Check logs: `d:\rawrxd\rawrxd_ide.log`
2. Review test outputs: `d:\rawrxd\test_*.txt`
3. Consult documentation: `d:\rawrxd\IDE_QUICK_REFERENCE.md`

---

**Last Updated**: February 14, 2026  
**Version**: 7.0 Ultimate Final Implementation
