#!/usr/bin/env python3
"""
RawrXD Inline Edit - Week 1 Assembly Compilation & Validation
Builds: Keybinding + Streaming + Validator + Extractor
"""

import subprocess
import os
import sys
import json
from pathlib import Path
from datetime import datetime

BASE_DIR = r"D:\rawrxd"
ML64 = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.42.34433\bin\HostX64\x64\ml64.exe"

# Modules to compile
MODULES = [
    ("RawrXD_InlineEdit_Keybinding.asm", "Hotkey capture + context extraction"),
    ("RawrXD_InlineStream_ml64.asm", "Real-time token streaming from LLM"),
    ("RawrXD_DiffValidator_ml64.asm", "AST-level code validation"),
    ("RawrXD_ContextExtractor_ml64.asm", "Intelligent context windowing"),
]

def log(msg, level="INFO"):
    """Log message with timestamp"""
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"[{ts}] [{level}] {msg}")

def compile_module(asm_file, description):
    """Compile single ASM module via ml64.exe"""
    log(f"Compiling: {description}", "BUILD")
    
    asm_path = os.path.join(BASE_DIR, asm_file)
    obj_path = asm_path.replace(".asm", ".obj")
    
    if not os.path.exists(asm_path):
        log(f"ERROR: {asm_file} not found at {asm_path}", "ERROR")
        return False
    
    # Build ml64 command
    cmd = [
        ML64,
        "/c",                              # Compile only (no link)
        f"/Fo{obj_path}",                 # Output object file
        "/W4",                            # Warning level 4
        "/WX",                            # Treat warnings as errors
        asm_path
    ]
    
    try:
        result = subprocess.run(
            cmd,
            cwd=BASE_DIR,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode != 0:
            log(f"ml64 failed with code {result.returncode}", "ERROR")
            if result.stdout:
                log(f"STDOUT:\n{result.stdout}", "ERROR")
            if result.stderr:
                log(f"STDERR:\n{result.stderr}", "ERROR")
            return False
        
        # Check if object file was created
        if not os.path.exists(obj_path):
            log(f"Object file not created: {obj_path}", "ERROR")
            return False
        
        size = os.path.getsize(obj_path)
        log(f"✓ {asm_file} → {os.path.basename(obj_path)} ({size} bytes)", "SUCCESS")
        return True
        
    except subprocess.TimeoutExpired:
        log(f"Compilation timeout for {asm_file}", "ERROR")
        return False
    except Exception as e:
        log(f"Compilation failed: {e}", "ERROR")
        return False

def validate_objects():
    """Check all object files were created"""
    log("Validating compiled objects...", "VALIDATE")
    
    for asm_file, _ in MODULES:
        obj_file = asm_file.replace(".asm", ".obj")
        obj_path = os.path.join(BASE_DIR, obj_file)
        
        if not os.path.exists(obj_path):
            log(f"Missing object file: {obj_file}", "ERROR")
            return False
        
        size = os.path.getsize(obj_path)
        log(f"  {obj_file}: {size} bytes", "OK")
    
    return True

def generate_report():
    """Generate build report"""
    report = {
        "timestamp": datetime.now().isoformat(),
        "phase": "Week 1 - Inline Edit Assembly Build",
        "modules": []
    }
    
    for asm_file, description in MODULES:
        obj_file = asm_file.replace(".asm", ".obj")
        obj_path = os.path.join(BASE_DIR, obj_file)
        
        module_info = {
            "name": asm_file,
            "description": description,
            "object_file": obj_file,
            "compiled": os.path.exists(obj_path),
        }
        
        if os.path.exists(obj_path):
            module_info["size_bytes"] = os.path.getsize(obj_path)
        
        report["modules"].append(module_info)
    
    # Write report
    report_path = os.path.join(BASE_DIR, "InlineEdit_Week1_BuildReport.json")
    with open(report_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    log(f"Report written to: InlineEdit_Week1_BuildReport.json", "OK")
    return report_path

def main():
    """Main build pipeline"""
    log("="*70)
    log("RawrXD Inline Edit - Week 1 Assembly Build Pipeline")
    log("="*70)
    
    os.chdir(BASE_DIR)
    
    # Phase 1: Compile all modules
    results = []
    for asm_file, description in MODULES:
        success = compile_module(asm_file, description)
        results.append((asm_file, success))
    
    # Phase 2: Validate
    all_compiled = all(success for _, success in results)
    
    if all_compiled:
        log("✓ All modules compiled successfully!", "SUCCESS")
        if validate_objects():
            log("✓ All object files validated!", "SUCCESS")
        else:
            log("✗ Object validation failed", "ERROR")
            return 1
    else:
        log("✗ Some modules failed to compile", "ERROR")
        for asm_file, success in results:
            status = "✓" if success else "✗"
            log(f"  {status} {asm_file}", "SUMMARY")
        return 1
    
    # Phase 3: Generate report
    generate_report()
    
    log("="*70)
    log("Build Pipeline Complete - Ready for Integration Testing")
    log("="*70)
    return 0

if __name__ == "__main__":
    sys.exit(main())
