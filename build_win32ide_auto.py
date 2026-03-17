#!/usr/bin/env python3
"""
Auto-detect toolchain and build Win32IDE + Amphibious ML
"""

import os
import subprocess
import sys
from pathlib import Path
from datetime import datetime

def find_tool(program_name):
    """Try to find a tool in system PATH or common locations"""
    # Try PATH first
    result = subprocess.run(['where', program_name], capture_output=True, text=True)
    if result.returncode == 0:
        return result.stdout.strip().split('\n')[0]
    
    # Try common VS locations
    for vs_version in ['2022', '2019']:
        for edition in ['Community', 'Professional', 'Enterprise', 'BuildTools']:
            path = Path(f"C:\\Program Files\\Microsoft Visual Studio\\{vs_version}\\{edition}\\VC\\Tools\\MSVC")
            if path.exists():
                for msvc_version in sorted(path.iterdir(), reverse=True):
                    tool_path = msvc_version / "bin\\HostX64\\x64" / program_name
                    if tool_path.exists():
                        return str(tool_path)
    
    # Try Program Files (x86) for older toolchains
    path = Path("C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC")
    if path.exists():
        for msvc_version in sorted(path.iterdir(), reverse=True):
            tool_path = msvc_version / "bin\\HostX64\\x64" / program_name
            if tool_path.exists():
                return str(tool_path)
    
    return None

def find_windows_kit():
    """Find Windows SDK"""
    for version in ['10.0.22621.0', '10.0.19041.0', '10.0.18362.0']:
        kit = Path(f"C:\\Program Files (x86)\\Windows Kits\\10")
        if kit.exists():
            return kit
    return None

def main():
    base_dir = Path("D:\\rawrxd")
    
    # Find tools
    cl = find_tool("cl.exe")
    link = find_tool("link.exe")
    ml64 = find_tool("ml64.exe")
    
    if not cl or not link or not ml64:
        print("ERROR: Could not find required tools")
        print(f"  cl.exe: {cl}")
        print(f"  link.exe: {link}")
        print(f"  ml64.exe: {ml64}")
        return 1
    
    print(f"Found toolchain:")
    print(f"  cl.exe: {cl}")
    print(f"  link.exe: {link}")
    print(f"  ml64.exe: {ml64}")
    
    # Create build directory
    build_dir = base_dir / "build_win32ide"
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "obj").mkdir(exist_ok=True)
    (build_dir / "bin").mkdir(exist_ok=True)
    
    # Compile assembly modules
    print("\n" + "="*70)
    print("Compiling assembly modules...")
    print("="*70)
    
    asm_files = [
        "Win32IDE_AmphibiousMLBridge.asm",
        "RawrXD_InlineEdit_Keybinding.asm",
        "gpu_dma_production_final_target9.asm",
    ]
    
    obj_files = []
    for asm_file in asm_files:
        asm_path = base_dir / asm_file
        if not asm_path.exists():
            print(f"SKIP: {asm_file} not found")
            continue
        
        obj_path = build_dir / "obj" / asm_file.replace(".asm", ".obj")
        
        cmd = [ml64, "/c", f"/Fo{obj_path}", str(asm_path)]
        print(f"  Compiling: {asm_file}...")
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"    ✓ {obj_path}")
            obj_files.append(obj_path)
        else:
            print(f"    ✗ Failed")
            if result.stderr:
                print(f"      {result.stderr[:200]}")
    
    # Compile C++ sources
    print("\n" + "="*70)
    print("Compiling C++ sources...")
    print("="*70)
    
    cpp_files = [
        "Win32IDE_AmphibiousIntegration.cpp",
    ]
    
    kit = find_windows_kit()
    includes = []
    if kit:
        includes = [
            f"/I{kit / 'include' / '10.0.22621.0' / 'um'}",
            f"/I{kit / 'include' / '10.0.22621.0' / 'shared'}",
        ]
    
    for cpp_file in cpp_files:
        cpp_path = base_dir / cpp_file
        if not cpp_path.exists():
            print(f"SKIP: {cpp_file} not found")
            continue
        
        obj_path = build_dir / "obj" / cpp_file.replace(".cpp", ".obj")
        
        cmd = [cl, "/c", "/EHsc", "/std:c++17", "/O2", f"/Fo{obj_path}"] + includes + [str(cpp_path)]
        print(f"  Compiling: {cpp_file}...")
        
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=str(base_dir))
        if result.returncode == 0:
            print(f"    ✓ {obj_path}")
            obj_files.append(obj_path)
        else:
            print(f"    ✗ Failed")
            if result.stderr:
                # Show only first error line
                for line in result.stderr.split('\n')[:3]:
                    if line.strip():
                        print(f"      {line}")
    
    # Link
    if obj_files:
        print("\n" + "="*70)
        print("Linking executable...")
        print("="*70)
        
        exe_path = build_dir / "bin" / "Win32IDE_Amphibious.exe"
        
        lib_args = [
            "kernel32.lib", "user32.lib", "gdi32.lib", "comctl32.lib",
            "comdlg32.lib", "ws2_32.lib", "shell32.lib"
        ]
        
        if kit:
            lib_path = kit / "lib" / "10.0.22621.0" / "um" / "x64"
            lib_args = [f"/LIBPATH:{lib_path}"] + lib_args
        
        cmd = [link, f"/OUT:{exe_path}", "/SUBSYSTEM:WINDOWS", "/MACHINE:X64"] + [str(obj) for obj in obj_files] + lib_args
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0 and exe_path.exists():
            size_mb = exe_path.stat().st_size / (1024 * 1024)
            print(f"✓ {exe_path} ({size_mb:.2f} MB)")
            
            print("\n" + "="*70)
            print("BUILD SUCCESSFUL ✓")
            print("="*70)
            print(f"Executable: {exe_path}")
            print(f"Run: {exe_path}")
            
            return 0
        else:
            print("✗ Linking failed")
            if result.stderr:
                print(result.stderr[:500])
            return 1
    else:
        print("No object files to link")
        return 1

if __name__ == "__main__":
    sys.exit(main())
