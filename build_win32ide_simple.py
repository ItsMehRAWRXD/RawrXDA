#!/usr/bin/env python3
"""
Build Win32IDE + Amphibious ML Integration - Simplified
"""

import subprocess
import sys
from pathlib import Path

def run_cmd(cmd, description):
    """Run command and report results"""
    print(f"\n{'='*70}")
    print(f"{description}")
    print(f"{'='*70}")
    print(f"$ {' '.join(str(c) for c in cmd)}")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)
    
    if result.returncode == 0:
        print(f"✓ {description} succeeded")
        return True
    else:
        print(f"✗ {description} failed (code {result.returncode})")
        return False

def find_tool(name):
    """Find tool via where command"""
    result = subprocess.run(["where", name], capture_output=True, text=True)
    if result.returncode == 0:
        return result.stdout.strip().split('\n')[0]
    return None

def main():
    base_dir = Path("D:\\rawrxd")
    build_dir = base_dir / "build_win32ide_simple"
    build_dir.mkdir(parents=True, exist_ok=True)
    
    # Find tools
    cl = find_tool("cl.exe")
    link = find_tool("link.exe")
    
    if not cl or not link:
        print("ERROR: Could not find MSVC toolchain (cl.exe, link.exe)")
        return 1
    
    print(f"Found cl.exe: {cl}")
    print(f"Found link.exe: {link}")
    
    # Compile C++
    cpp_file = base_dir / "Win32IDE_Simple.cpp"
    obj_file = build_dir / "Win32IDE_Simple.obj"
    
    if not cpp_file.exists():
        print(f"ERROR: {cpp_file} not found")
        return 1
    
    # Compile
    cmd = [cl, "/c", "/EHsc", f"/Fo{obj_file}", str(cpp_file)]
    if not run_cmd(cmd, "Compiling Win32IDE_Simple.cpp"):
        return 1
    
    # Link
    exe_file = build_dir / "Win32IDE_Amphibious.exe"
    cmd = [
        link,
        f"/OUT:{exe_file}",
        str(obj_file),
        "/SUBSYSTEM:WINDOWS",
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "comctl32.lib",
        "ws2_32.lib",
        "shell32.lib"
    ]
    
    if not run_cmd(cmd, "Linking executable"):
        return 1
    
    if exe_file.exists():
        size_kb = exe_file.stat().st_size / 1024
        print(f"\n✓ SUCCESS: {exe_file} ({size_kb:.1f} KB)")
        print(f"  Run: {exe_file}")
        return 0
    else:
        print(f"\n✗ Executable not created")
        return 1

if __name__ == "__main__":
    sys.exit(main())
