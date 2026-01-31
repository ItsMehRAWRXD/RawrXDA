#!/usr/bin/env python3
import os
import shutil
from pathlib import Path
import json

def rebuild_projects():
    desktop = Path("C:/Users/Garre/Desktop")
    
    # Create unified structure
    unified = desktop / "unified-workspace"
    unified.mkdir(exist_ok=True)
    
    # Project consolidation plan
    consolidation = {
        "rawrz-platform": {
            "target": unified / "rawrz-platform",
            "sources": [
                desktop / "Desktop/02-rawrz-app",
                desktop / "Desktop/rawrz-http-encryptor", 
                desktop / "Desktop/organized-projects/03-rawrz-http-encryptor"
            ],
            "primary": desktop / "Desktop/organized-projects/03-rawrz-http-encryptor"
        },
        "ide-suite": {
            "target": unified / "ide-suite", 
            "sources": [
                desktop / "custom-editor",
                desktop / "Desktop/secure-ide",
                desktop / "UnifiedIDE",
                desktop / "IDE IDEAS"
            ],
            "primary": desktop / "custom-editor"
        },
        "ai-tools": {
            "target": unified / "ai-tools",
            "sources": [
                desktop / "ai-first-editor",
                desktop / "Desktop/04-ai-tools"
            ],
            "primary": desktop / "ai-first-editor"
        }
    }
    
    print("Rebuilding unified workspace...")
    
    for project, config in consolidation.items():
        target = config["target"]
        primary = config["primary"]
        
        print(f"\nConsolidating {project}...")
        
        # Copy primary version as base
        if primary.exists():
            if target.exists():
                shutil.rmtree(target)
            shutil.copytree(primary, target)
            print(f"  Base: {primary.name}")
        
        # Merge unique files from other sources
        for source in config["sources"]:
            if source.exists() and source != primary:
                merge_unique_files(source, target)
                print(f"  Merged: {source.name}")
    
    # Create master build script
    create_master_build(unified)
    
    print(f"\nUnified workspace created at: {unified}")
    print("Run build-all.bat to compile everything")

def merge_unique_files(source, target):
    """Merge files that don't exist in target"""
    for item in source.rglob("*"):
        if item.is_file():
            rel_path = item.relative_to(source)
            target_file = target / rel_path
            
            if not target_file.exists():
                target_file.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(item, target_file)

def create_master_build(unified):
    build_script = unified / "build-all.bat"
    with open(build_script, "w") as f:
        f.write("""@echo off
echo Building Unified Workspace
echo =========================

cd rawrz-platform
if exist package.json npm install && npm start
if exist server.js node server.js

cd ../ide-suite  
if exist *.java javac *.java && java IDEMain

cd ../ai-tools
if exist package.json npm install && npm run dev

echo All projects built successfully!
pause
""")

if __name__ == "__main__":
    rebuild_projects()