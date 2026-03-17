# RawrXD CLI - Command Line Interface

**Version:** 2.0  
**Location:** `C:\RawrXD\RawrXD-CLI.ps1`

## Quick Start

```powershell
# Show help
.\RawrXD-CLI.ps1 help

# Generate a PE executable
.\RawrXD-CLI.ps1 generate-pe myapp.exe

# Test the encoder
.\RawrXD-CLI.ps1 test-encoder

# Show toolchain information
.\RawrXD-CLI.ps1 info

# Verify installation
.\RawrXD-CLI.ps1 verify

# List all libraries
.\RawrXD-CLI.ps1 list-libs
```

## From Command Prompt (Batch Wrapper)

```cmd
RawrXD-CLI.bat info
RawrXD-CLI.bat generate-pe output.exe
RawrXD-CLI.bat verify
```

## Available Commands

### `generate-pe [output]`
Generates a PE32+ executable using the RawrXD PE generator.

**Arguments:**
- `output` (optional) - Output filename (default: `output.exe`)

**Examples:**
```powershell
.\RawrXD-CLI.ps1 generate-pe
.\RawrXD-CLI.ps1 generate-pe custom_app.exe
.\RawrXD-CLI.ps1 generate-pe ..\builds\release.exe
```

**Output:**
- Creates a valid PE32+ Windows executable
- Default size: 1024 bytes
- Can be customized by modifying the PE generator source

---

### `test-encoder`
Runs the instruction encoder test suite.

Tests the x64 instruction encoding library functionality including:
- MOV, ADD, SUB, XOR instructions
- CALL, JMP (conditional and unconditional)
- Push/Pop operations
- REX prefix handling
- SIB byte encoding

**Example:**
```powershell
.\RawrXD-CLI.ps1 test-encoder
```

---

### `info`
Displays comprehensive toolchain information.

Shows:
- Installation directory
- All installed executables with sizes
- All static libraries with sizes
- Documentation file count
- Component status

**Example:**
```powershell
.\RawrXD-CLI.ps1 info
```

**Sample Output:**
```
═══════════════════════════════════════════════════════
  RawrXD Production Toolchain CLI v2.0
  Pure x64 Assembly - PE Generation & Encoding
═══════════════════════════════════════════════════════

📍 Installation Directory:
   C:\RawrXD

📦 Executables:
   ✓ instruction_encoder_test.exe (5 KB)
   ✓ pe_generator.exe (4.5 KB)

📚 Static Libraries:
   ✓ rawrxd_encoder.lib (30.1 KB)
   ✓ rawrxd_pe_gen.lib (5.78 KB)
   ...
```

---

### `list-libs`
Lists all available static libraries with detailed information.

Shows for each library:
- Full path
- File size
- Associated header file (if exists)
- Linking instructions

**Example:**
```powershell
.\RawrXD-CLI.ps1 list-libs
```

**Sample Output:**
```
📚 Available RawrXD Libraries:

  📦 rawrxd_encoder.lib
     Size: 30.1 KB
     Path: C:\RawrXD\Libraries\rawrxd_encoder.lib
     Header: Headers\rawrxd_encoder.h

💡 Link against these libraries in your C++ projects:
   link.exe your_code.obj C:\RawrXD\Libraries\rawrxd_encoder.lib /OUT:your_app.exe
```

---

### `verify`
Verifies the integrity of the RawrXD installation.

Checks:
- PE Generator executable
- Required libraries (rawrxd_encoder.lib, rawrxd_pe_gen.lib)
- Header files directory
- Documentation directory

**Example:**
```powershell
.\RawrXD-CLI.ps1 verify
```

**Exit Codes:**
- `0` - Installation verified successfully
- `1` - Installation incomplete or corrupted

---

### `help`
Displays the help message with all available commands.

**Example:**
```powershell
.\RawrXD-CLI.ps1 help
```

---

## Integration Examples

### Generate PE from PowerShell Script

```powershell
# Automated PE generation
$outputPath = "C:\Build\output.exe"
& "C:\RawrXD\RawrXD-CLI.ps1" generate-pe $outputPath

if ($LASTEXITCODE -eq 0) {
    Write-Host "PE generated successfully at $outputPath"
} else {
    Write-Error "PE generation failed"
    exit 1
}
```

### Batch Script Integration

```batch
@echo off
echo Building application...

REM Generate PE
call C:\RawrXD\RawrXD-CLI.bat generate-pe app.exe

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build complete!
```

### C# Process Invocation

```csharp
using System.Diagnostics;

var process = new Process
{
    StartInfo = new ProcessStartInfo
    {
        FileName = "powershell.exe",
        Arguments = @"-NoProfile -ExecutionPolicy Bypass -File C:\RawrXD\RawrXD-CLI.ps1 generate-pe output.exe",
        UseShellExecute = false,
        RedirectStandardOutput = true
    }
};

process.Start();
string output = process.StandardOutput.ReadToEnd();
process.WaitForExit();

if (process.ExitCode == 0)
{
    Console.WriteLine("PE generated successfully");
}
```

---

## Environment Variables

The CLI can be configured with these optional environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `RAWRXD_ROOT` | Installation directory | `C:\RawrXD` |
| `RAWRXD_BIN` | Executables directory | `C:\RawrXD\bin` |
| `RAWRXD_LIB` | Libraries directory | `C:\RawrXD\Libraries` |

**Example:**
```powershell
$env:RAWRXD_ROOT = "D:\CustomLocation\RawrXD"
.\RawrXD-CLI.ps1 info
```

---

## Adding CLI to PATH

For system-wide access, add `C:\RawrXD` to your PATH:

### PowerShell (Current Session)
```powershell
$env:Path += ";C:\RawrXD"
RawrXD-CLI.ps1 info
```

### PowerShell (Permanent)
```powershell
[Environment]::SetEnvironmentVariable(
    "Path",
    [Environment]::GetEnvironmentVariable("Path", "User") + ";C:\RawrXD",
    "User"
)
```

### Command Prompt (Permanent)
```cmd
setx PATH "%PATH%;C:\RawrXD"
```

After adding to PATH, you can run from anywhere:
```powershell
RawrXD-CLI.ps1 generate-pe
```

---

## Troubleshooting

### "Script not found" Error

**Problem:** CLI script is missing  
**Solution:** Run the build script to reinstall:
```powershell
cd D:\RawrXD-Compilers
.\Build-And-Wire.ps1
```

### "PE Generator not found" Error

**Problem:** Executables missing from bin directory  
**Solution:** Verify installation:
```powershell
.\RawrXD-CLI.ps1 verify
```
If verification fails, rebuild with:
```powershell
cd D:\RawrXD-Compilers
.\Build-And-Wire.ps1
```

### "Execution Policy" Error

**Problem:** PowerShell execution policy restrictions  
**Solution:** Run with bypass flag:
```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File C:\RawrXD\RawrXD-CLI.ps1 info
```

Or set execution policy:
```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

---

## Advanced Usage

### Custom PE Generation Workflow

```powershell
# 1. Generate base PE
.\RawrXD-CLI.ps1 generate-pe base.exe

# 2. Verify output
if (Test-Path base.exe) {
    $size = (Get-Item base.exe).Length
    Write-Host "Generated PE: $size bytes"
    
    # 3. Analyze PE headers
    # (Use your own PE analysis tools here)
    
    # 4. Deploy
    Copy-Item base.exe C:\Deploy\myapp.exe
}
```

### Automated Testing

```powershell
# Test suite
$tests = @(
    @{ Name = "Verify"; Command = "verify" },
    @{ Name = "Info"; Command = "info" },
    @{ Name = "Generate"; Command = "generate-pe test.exe" }
)

foreach ($test in $tests) {
    Write-Host "Running test: $($test.Name)..." -ForegroundColor Yellow
    & "C:\RawrXD\RawrXD-CLI.ps1" $test.Command.Split()
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ $($test.Name) passed" -ForegroundColor Green
    } else {
        Write-Host "✗ $($test.Name) failed" -ForegroundColor Red
    }
}
```

---

## Related Documentation

- **Build System:** `D:\RawrXD-Compilers\BUILD_QUICKSTART.md`
- **PE Generator:** `C:\RawrXD\Docs\PE_GENERATOR_QUICK_REF.md`
- **Full Toolchain:** `C:\RawrXD\Docs\PRODUCTION_TOOLCHAIN_DOCS.md`
- **Integration:** `C:\RawrXD\Docs\PRODUCTION_DELIVERY_INDEX.md`

---

## Support & Development

**Repository:** github.com/ItsMehRAWRXD/cloud-hosting  
**Branch:** copilot/courageous-rodent  
**PR:** #5 - MASM x64 Assembly Toolchain

**Last Updated:** January 27, 2026  
**CLI Version:** 2.0
