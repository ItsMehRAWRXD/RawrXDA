#!/usr/bin/env python3
"""
Desktop Project Organization Script
Consolidates duplicate projects and creates clean structure
"""

import os
import shutil
import json
from pathlib import Path
from datetime import datetime

def organize_desktop_projects():
    desktop = Path("C:/Users/Garre/Desktop")
    
    # Create organized structure
    organized = {
        "01-active-projects": {
            "SaaSEncryptionSecurity": "Eng Src/SaaSEncryptionSecurity",
            "Star5IDE": "Eng Src/Star5IDE", 
            "PiEngine": "IDE IDEAS/PiEngine-Extended.java",
            "UnifiedIDE": "UnifiedIDE"
        },
        "02-archived-projects": {
            "rawrz-variants": ["Desktop/rawrz-http-encryptor", "Desktop/RawrZApp", "Desktop/RawrZ-Clean"],
            "ide-experiments": ["ai-first-editor", "custom-editor", "UnifiedAIEditor"],
            "backups": ["Desktop/RawrZApp-Backup*.zip", "Desktop/RawrZ_Backup*.zip"]
        },
        "03-tools-utilities": {
            "security-tools": "Eng Src/*.js",
            "build-scripts": "Desktop/build_*.bat",
            "deployment": "Desktop/deploy-*.ps1"
        },
        "04-documentation": {
            "ai-chats": "AI Chat ETC",
            "readmes": "Desktop/*.md"
        }
    }
    
    # Create directories
    for category in organized.keys():
        (desktop / category).mkdir(exist_ok=True)
    
    print("✅ Desktop organization structure created")
    return organized

def consolidate_rawrz_projects():
    """Consolidate multiple RawrZ project versions"""
    desktop = Path("C:/Users/Garre/Desktop")
    
    rawrz_projects = [
        "Desktop/rawrz-http-encryptor",
        "Desktop/RawrZApp", 
        "Desktop/RawrZ-Clean",
        "Desktop/organized-projects/02-rawrz-app"
    ]
    
    # Find latest version
    latest_project = None
    latest_time = 0
    
    for project in rawrz_projects:
        project_path = desktop / project
        if project_path.exists():
            mtime = project_path.stat().st_mtime
            if mtime > latest_time:
                latest_time = mtime
                latest_project = project_path
    
    if latest_project:
        print(f"📁 Latest RawrZ project: {latest_project.name}")
        return latest_project
    
    return None

def create_cleanup_script():
    """Create batch script for safe cleanup"""
    script = """@echo off
echo Desktop Project Cleanup Script
echo ==============================

REM Create organized structure
mkdir "01-active-projects" 2>nul
mkdir "02-archived-projects" 2>nul  
mkdir "03-tools-utilities" 2>nul
mkdir "04-documentation" 2>nul

REM Move active projects
echo Moving active projects...
if exist "Eng Src\\SaaSEncryptionSecurity" move "Eng Src\\SaaSEncryptionSecurity" "01-active-projects\\"
if exist "Eng Src\\Star5IDE" move "Eng Src\\Star5IDE" "01-active-projects\\"
if exist "UnifiedIDE" move "UnifiedIDE" "01-active-projects\\"

REM Archive old versions
echo Archiving old versions...
if exist "Desktop\\rawrz-http-encryptor" move "Desktop\\rawrz-http-encryptor" "02-archived-projects\\"
if exist "Desktop\\RawrZApp" move "Desktop\\RawrZApp" "02-archived-projects\\"
if exist "Desktop\\RawrZ-Clean" move "Desktop\\RawrZ-Clean" "02-archived-projects\\"

REM Move tools
echo Moving tools and utilities...
move "Eng Src\\*.js" "03-tools-utilities\\" 2>nul
move "Desktop\\build_*.bat" "03-tools-utilities\\" 2>nul
move "Desktop\\deploy-*.ps1" "03-tools-utilities\\" 2>nul

REM Move documentation
echo Moving documentation...
if exist "AI Chat ETC" move "AI Chat ETC" "04-documentation\\"
move "Desktop\\*.md" "04-documentation\\" 2>nul

echo.
echo ✅ Desktop organization complete!
echo.
echo Structure:
echo   01-active-projects/    - Current working projects
echo   02-archived-projects/  - Old versions and backups  
echo   03-tools-utilities/    - Scripts and tools
echo   04-documentation/      - Docs and chat logs
echo.
pause
"""
    
    with open("C:/Users/Garre/Desktop/cleanup-desktop.bat", "w") as f:
        f.write(script)
    
    print("📝 Created cleanup-desktop.bat")

def main():
    print("🗂️  Desktop Project Organization")
    print("=" * 40)
    
    # Analyze current structure
    organize_desktop_projects()
    
    # Find latest RawrZ version
    latest_rawrz = consolidate_rawrz_projects()
    
    # Create cleanup script
    create_cleanup_script()
    
    print("\n📋 Recommendations:")
    print("1. Run cleanup-desktop.bat to organize structure")
    print("2. Keep only latest version of each project")
    print("3. Archive old versions in 02-archived-projects/")
    print("4. Use Git for version control going forward")
    
    if latest_rawrz:
        print(f"5. Latest RawrZ project: {latest_rawrz.name}")
    
    print("\n🎯 Final Structure:")
    print("├── 01-active-projects/")
    print("│   ├── SaaSEncryptionSecurity/")
    print("│   ├── Star5IDE/") 
    print("│   └── UnifiedIDE/")
    print("├── 02-archived-projects/")
    print("├── 03-tools-utilities/")
    print("└── 04-documentation/")

if __name__ == "__main__":
    main()