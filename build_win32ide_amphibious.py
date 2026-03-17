#!/usr/bin/env python3
"""
RawrXD Win32IDE + Amphibious ML System - Pure Win32 Build Pipeline
Builds: Win32IDE front-end + ML bridge + Amphibious ML backend
No Qt dependency - Zero stubs - Production ready
"""

import os
import subprocess
import sys
import json
from pathlib import Path
from datetime import datetime

# Build configuration
BASE_DIR = Path(r"D:\rawrxd")
BUILD_DIR = BASE_DIR / "build_win32ide"
SRC_DIR = BASE_DIR / "src"
ASM_DIR = BASE_DIR

# Toolchain
MSVC_CL = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\HostX64\x64\cl.exe"
MSVC_LINK = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\HostX64\x64\link.exe"
ML64 = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.42.34433\bin\HostX64\x64\ml64.exe"
WINDOWS_KIT = Path(r"C:\Program Files (x86)\Windows Kits\10")

# Source files
ASSEMBLY_MODULES = [
    ("Win32IDE_AmphibiousMLBridge.asm", "Win32IDE ↔ Amphibious ML bridge"),
    ("RawrXD_InlineEdit_Keybinding.asm", "Ctrl+K hotkey capture"),
    ("RawrXD_InlineStream_ml64.asm", "Token streaming to GUI"),
    ("RawrXD_DiffValidator_ml64.asm", "Code validation"),
    ("RawrXD_ContextExtractor_ml64.asm", "Context extraction"),
    ("gpu_dma_production_final_target9.asm", "GPU DMA kernel"),
]

CPP_SOURCES = [
    ("Win32IDE_AmphibiousIntegration.cpp", "Main Win32IDE application"),
    ("RawrXD_AmphibiousHost.cpp", "Amphibious host integration"),
]

def log(message, level="INFO"):
    """Log message with timestamp"""
    ts = datetime.now().strftime("%H:%M:%S")
    prefix = f"[{ts}]"
    if level == "ERROR":
        print(f"{prefix} ❌ {message}", file=sys.stderr)
    elif level == "SUCCESS":
        print(f"{prefix} ✓ {message}")
    elif level == "BUILD":
        print(f"{prefix} 🔨 {message}")
    else:
        print(f"{prefix} ℹ {message}")

def create_build_dir():
    """Create build directory structure"""
    log("Creating build directory structure...")
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    (BUILD_DIR / "obj").mkdir(exist_ok=True)
    (BUILD_DIR / "bin").mkdir(exist_ok=True)

def compile_assembly_module(asm_file, description):
    """Compile single assembly module"""
    log(f"Compiling: {description}", "BUILD")
    
    asm_path = ASM_DIR / asm_file
    obj_path = BUILD_DIR / "obj" / asm_file.replace(".asm", ".obj")
    
    if not asm_path.exists():
        log(f"Source not found: {asm_path}", "ERROR")
        return False
    
    cmd = [
        str(ML64),
        "/c",
        f"/Fo{obj_path}",
        "/W4",
        str(asm_path)
    ]
    
    try:
        result = subprocess.run(cmd, cwd=str(ASM_DIR), capture_output=True, text=True, timeout=30)
        
        if result.returncode != 0:
            log(f"ml64.exe failed: {result.stderr}", "ERROR")
            return False
        
        log(f"  ✓ {asm_file}", "SUCCESS")
        return obj_path
    except Exception as e:
        log(f"Compilation exception: {e}", "ERROR")
        return False

def compile_cpp_sources():
    """Compile all C++ sources to objects"""
    log("Compiling C++ sources...", "BUILD")
    
    cpp_objs = []
    
    for cpp_file, description in CPP_SOURCES:
        log(f"Compiling: {description}", "BUILD")
        
        cpp_path = BASE_DIR / cpp_file
        obj_path = BUILD_DIR / "obj" / cpp_file.replace(".cpp", ".obj")
        
        if not cpp_path.exists():
            log(f"Source not found: {cpp_path}", "ERROR")
            continue
        
        includes = [
            str(WINDOWS_KIT / "include" / "10.0.22621.0" / "um"),
            str(WINDOWS_KIT / "include" / "10.0.22621.0" / "shared"),
        ]
        
        cmd = [
            str(MSVC_CL),
            "/c",
            "/EHsc",
            "/std:c++17",
            "/O2",
            "/W4",
            f"/Fo{obj_path}",
        ]
        
        # Add include paths
        for inc in includes:
            cmd.append(f"/I{inc}")
        
        cmd.append(str(cpp_path))
        
        try:
            result = subprocess.run(cmd, cwd=str(BASE_DIR), capture_output=True, text=True, timeout=60)
            
            if result.returncode != 0:
                log(f"cl.exe failed:\n{result.stderr}", "ERROR")
                continue
            
            log(f"  ✓ {cpp_file}", "SUCCESS")
            cpp_objs.append(obj_path)
        except Exception as e:
            log(f"Compilation exception: {e}", "ERROR")
    
    return cpp_objs

def link_executable(asm_objs, cpp_objs):
    """Link all objects into final executable"""
    log("Linking executable...", "BUILD")
    
    exe_path = BUILD_DIR / "bin" / "Win32IDE_Amphibious.exe"
    
    lib_paths = [
        str(WINDOWS_KIT / "lib" / "10.0.22621.0" / "um" / "x64"),
    ]
    
    libs = [
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "comctl32.lib",
        "comdlg32.lib",
        "ws2_32.lib",
        "shell32.lib",
    ]
    
    cmd = [str(MSVC_LINK)]
    
    # Add all object files
    for obj in asm_objs:
        cmd.append(str(obj))
    for obj in cpp_objs:
        cmd.append(str(obj))
    
    # Output
    cmd.append(f"/OUT:{exe_path}")
    
    # Library paths
    for lib_path in lib_paths:
        cmd.append(f"/LIBPATH:{lib_path}")
    
    # Libraries
    cmd.extend(libs)
    
    # Options
    cmd.append("/SUBSYSTEM:WINDOWS")
    cmd.append("/MACHINE:X64")
    
    try:
        result = subprocess.run(cmd, cwd=str(BASE_DIR), capture_output=True, text=True, timeout=60)
        
        if result.returncode != 0:
            log(f"link.exe failed:\n{result.stderr}", "ERROR")
            return False
        
        if exe_path.exists():
            size_mb = exe_path.stat().st_size / (1024 * 1024)
            log(f"✓ Executable created: {exe_path} ({size_mb:.2f} MB)", "SUCCESS")
            return exe_path
        else:
            log(f"Executable not created", "ERROR")
            return False
    except Exception as e:
        log(f"Linking exception: {e}", "ERROR")
        return False

def generate_build_report(exe_path, asm_objs, cpp_objs):
    """Generate build report JSON"""
    report = {
        "timestamp": datetime.now().isoformat(),
        "project": "Win32IDE + Amphibious ML System",
        "configuration": "Release - Pure Win32 (No Qt)",
        "status": "success" if exe_path else "failed",
        "executable": str(exe_path) if exe_path else None,
        "assembly_modules": len(asm_objs),
        "cpp_sources": len(cpp_objs),
        "features": [
            "Real-time token streaming to editor",
            "Ctrl+K hotkey for inline edits",
            "Code validation (AST-level)",
            "Context extraction with language detection",
            "GPU DMA kernel execution",
            "Structured JSON telemetry",
            "Dual-mode (CLI/GUI) support",
        ],
        "dependencies": {
            "windows_api": "Win32 HWND, HWND edit controls, message pumps",
            "networking": "Winsock2 for llama.cpp streaming",
            "ml_inference": "RawrXD Amphibious ML system (local llama.cpp)",
            "assembly": "x64 MASM (ml64.exe)",
        },
        "build_artifacts": {
            "assembly_objects": [str(obj) for obj in asm_objs],
            "cpp_objects": [str(obj) for obj in cpp_objs],
            "final_executable": str(exe_path) if exe_path else None,
        }
    }
    
    report_path = BUILD_DIR / "build_report.json"
    with open(report_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    log(f"Build report: {report_path}", "SUCCESS")
    return report_path

def main():
    """Main build orchestrator"""
    log("=" * 70)
    log("RawrXD Win32IDE + Amphibious ML System - Pure Win32 Build Pipeline")
    log("=" * 70)
    log("")
    
    # Setup
    create_build_dir()
    
    # Phase 1: Compile assembly modules
    log("=" * 70)
    log("PHASE 1: Assembly Modules", "BUILD")
    log("=" * 70)
    
    asm_objs = []
    for asm_file, description in ASSEMBLY_MODULES:
        obj_path = compile_assembly_module(asm_file, description)
        if obj_path:
            asm_objs.append(obj_path)
        else:
            log(f"Failed to compile {asm_file}", "ERROR")
    
    log(f"Compiled {len(asm_objs)}/{len(ASSEMBLY_MODULES)} assembly modules", "SUCCESS")
    log("")
    
    # Phase 2: Compile C++ sources
    log("=" * 70)
    log("PHASE 2: C++ Sources", "BUILD")
    log("=" * 70)
    
    cpp_objs = compile_cpp_sources()
    log(f"Compiled {len(cpp_objs)}/{len(CPP_SOURCES)} C++ sources", "SUCCESS")
    log("")
    
    # Phase 3: Link executable
    if asm_objs and cpp_objs:
        log("=" * 70)
        log("PHASE 3: Linking", "BUILD")
        log("=" * 70)
        
        exe_path = link_executable(asm_objs, cpp_objs)
        
        if exe_path:
            log("")
            log("=" * 70)
            log("BUILD COMPLETE ✓", "SUCCESS")
            log("=" * 70)
            log(f"Executable: {exe_path}")
            log(f"Run: {exe_path}")
            log("")
            
            generate_build_report(exe_path, asm_objs, cpp_objs)
            return 0
    
    log("Build failed", "ERROR")
    return 1

if __name__ == "__main__":
    sys.exit(main())
