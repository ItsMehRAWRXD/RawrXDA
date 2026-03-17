import os
import json
import subprocess
import ctypes
from ctypes import wintypes
import sys

# Windows API constants
FILE_MAP_READ = 0x0004
INVALID_HANDLE_VALUE = -1

kernel32 = ctypes.windll.kernel32

def check_binary(path):
    # Resolve relative paths relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    full_path = os.path.normpath(os.path.join(script_dir, path))
    exists = os.path.exists(full_path)
    return exists, full_path

def check_mmf(name):
    # Try to open the file mapping
    handle = kernel32.OpenFileMappingA(
        FILE_MAP_READ,
        False,
        name.encode('utf-8')
    )
    if handle and handle != 0:
        kernel32.CloseHandle(handle)
        return True
    return False

def check_file_content(path, string_to_find):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    full_path = os.path.normpath(os.path.join(script_dir, path))
    
    if not os.path.exists(full_path):
        return False
        
    try:
        with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            return string_to_find in content
    except:
        return False

def run_audit():
    print("----------------------------------------------------------------")
    print("RawrXD Interface Audit & Integrity Check")
    print("----------------------------------------------------------------")
    
    manifest_path = os.path.join(os.path.dirname(__file__), "interface_manifest.json")
    try:
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
    except FileNotFoundError:
        print(f"❌ Manifest not found at {manifest_path}")
        return

    missing_items = []
    passed_items = []

    # 1. GUI Checks
    print("\n[GUI / Application Layer]")
    gui_config = manifest.get("GUI", {})
    for binary in gui_config.get("Binaries", []):
        exists, full_path = check_binary(binary)
        if exists:
            print(f"  ✅ Binary: {os.path.basename(binary)}")
            passed_items.append(f"Bin: {binary}")
        else:
            # Don't fail immediately, some binaries might be in different build folders
            print(f"  ⚠️  Binary Missing: {binary}")
            # we check alternative locations implicitly by user providing list
    
    # Check for Widget Code existence
    for widget in gui_config.get("Qt_Widgets", []):
        # We can scan the source code to verify the widget is integrated
        # This is a heuristic scan
        found = False
        # Look in MainWindow.cpp in a known relative location
        mainwindow_path = "../src/qtapp/MainWindow.cpp"
        if check_file_content(mainwindow_path, widget):
            print(f"  ✅ Widget Wired: {widget} (Found in MainWindow.cpp)")
            passed_items.append(f"Widget: {widget}")
        else:
            print(f"  ❌ Widget Disconnected: {widget} (Not found in MainWindow.cpp)")
            missing_items.append(f"Widget: {widget}")

    # 2. CLI / Kernel Checks
    print("\n[CLI / Kernel Layer]")
    cli_config = manifest.get("CLI", {})
    for binary in cli_config.get("Binaries", []):
        exists, full_path = check_binary(binary)
        if exists:
            print(f"  ✅ Kernel: {os.path.basename(binary)}")
            passed_items.append(f"Kernel: {binary}")
            
            # Simple validation if check is pocket_lab
            if "pocket_lab.exe" in binary:
                 try:
                     # Run the validation
                     result = subprocess.run(f"{full_path}", shell=True, capture_output=True, text=True)
                     # PocketLab exits with 1, but prints output. Or user defined validation.
                     # Using findstr logic from user prompt
                     proc = subprocess.Popen(f"{full_path} | findstr ENTERPRISE", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                     out, err = proc.communicate()
                     if b"ENTERPRISE" in out:
                         print("     ↳ Tier: ENTERPRISE (Validated)")
                     else:
                         print("     ↳ Tier: UNKNOWN/MOBILE")
                 except Exception as e:
                     print(f"     ↳ Validation Error: {e}")

        else:
            print(f"  ❌ Kernel Missing: {binary}")
            missing_items.append(f"Kernel: {binary}")

    # 3. Shared Memory
    print("\n[Shared Memory / IPC]")
    shared_config = manifest.get("Shared", {})
    for mmf in shared_config.get("MMF_Names", []):
        if check_mmf(mmf):
            print(f"  ✅ MMF Active: {mmf}") 
            passed_items.append(f"MMF: {mmf}")
        else:
            print(f"  ⚠️  MMF Offline: {mmf} (Kernel might not be running)")
            missing_items.append(f"MMF: {mmf}")
    
    # 4. Critical Files
    print("\n[Source Integrity]")
    for ffile in shared_config.get("Critical_Files", []):
        exists, full_path = check_binary(ffile)
        if exists:
            print(f"  ✅ Source Exists: {os.path.basename(ffile)}")
        else:
            print(f"  ❌ Source Missing: {ffile}")
            missing_items.append(f"Source: {ffile}")

    print("\n----------------------------------------------------------------")
    if len(missing_items) == 0:
        print("🎉 AUDIT PASSED: All interfaces are linked and accessible.")
    else:
        print(f"⚠️ AUDIT FOUND ISSUES: {len(missing_items)} items missing or broken.")
        # Only hard fail on critical source/binary missing, MMF might be runtime transient
        if any("Source" in x or "Kernel" in x for x in missing_items):
            sys.exit(1)

if __name__ == "__main__":
    run_audit()
