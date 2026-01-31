#!/usr/bin/env python3
"""
RawrZ Universal IDE - Simple Compiler Backup System
Creates ZIP archives of all compilers for emergency recovery
"""

import os
import sys
import zipfile
import json
from datetime import datetime
from pathlib import Path

def create_compiler_backup():
    """Create comprehensive compiler backup"""
    print("🔧 RawrZ Universal IDE - Compiler Backup System")
    print("=" * 60)
    
    # Create backup directory
    backup_dir = Path("compiler_backups")
    backup_dir.mkdir(exist_ok=True)
    
    # Create timestamp
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    
    # Production compilers backup
    print("📦 Creating Production Compilers Backup...")
    prod_backup = backup_dir / f"production_compilers_{timestamp}.zip"
    
    with zipfile.ZipFile(prod_backup, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Add production compiler files
        compiler_files = [
            'real_production_compiler.py',
            'real_cpp_compiler.py', 
            'real_python_compiler.py',
            'real_solidity_compiler.py',
            'gas_assembler.py',
            'complete_n0mn0m_universal_ide.py'
        ]
        
        for file_name in compiler_files:
            file_path = Path(file_name)
            if file_path.exists():
                zipf.write(file_path, f"compilers/{file_name}")
                print(f"  ✅ Added {file_name}")
        
        # Add metadata
        metadata = {
            'backup_type': 'production_compilers',
            'created': datetime.now().isoformat(),
            'version': '1.0.0',
            'description': 'RawrZ Production Compilers Backup',
            'compilers': {
                'c_compiler': 'RealProductionCompiler',
                'cpp_compiler': 'RealCppCompiler', 
                'python_compiler': 'RealPythonCompiler',
                'solidity_compiler': 'RealSolidityCompiler',
                'assembly_compiler': 'gASMCompiler',
                'csharp_compiler': 'EmbeddedCSharpCompiler'
            }
        }
        
        zipf.writestr('metadata.json', json.dumps(metadata, indent=2))
        print(f"  ✅ Added metadata")
    
    print(f"✅ Production compiler backup created: {prod_backup.name}")
    
    # External compilers backup
    print("\n📦 Creating External Compilers Backup...")
    ext_backup = backup_dir / f"external_compilers_{timestamp}.zip"
    
    with zipfile.ZipFile(ext_backup, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Create compiler detection script
        detection_script = '''#!/usr/bin/env python3
"""
Compiler Detection Script
Detects available system compilers
"""

import subprocess
import json

def detect_compiler(name, commands):
    """Detect if a compiler is available"""
    try:
        for cmd in commands:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                return True, result.stdout.strip()
        return False, None
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False, None

def main():
    compilers = {
        'gcc': [['gcc', '--version']],
        'g++': [['g++', '--version']],
        'javac': [['javac', '-version']],
        'dotnet': [['dotnet', '--version']],
        'cargo': [['cargo', '--version']],
        'nasm': [['nasm', '--version']],
        'gradle': [['gradle', '--version']]
    }
    
    results = {}
    for name, commands in compilers.items():
        available, version = detect_compiler(name, commands)
        results[name] = {
            'available': available,
            'version': version
        }
        status = "✅" if available else "❌"
        print(f"{status} {name}: {version if available else 'Not found'}")
    
    with open('compiler_status.json', 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\\n📊 Compiler status saved to compiler_status.json")

if __name__ == "__main__":
    main()
'''
        
        zipf.writestr('detect_compilers.py', detection_script)
        
        # Add compiler configuration
        config = {
            'compilers': {
                'gcc': 'C/C++ Compiler',
                'g++': 'C++ Compiler',
                'javac': 'Java Compiler',
                'dotnet': 'C# Compiler',
                'cargo': 'Rust Compiler',
                'nasm': 'Assembly Compiler',
                'gradle': 'Android Builder'
            },
            'detection_commands': {
                'gcc': ['gcc', '--version'],
                'g++': ['g++', '--version'],
                'javac': ['javac', '-version'],
                'dotnet': ['dotnet', '--version'],
                'cargo': ['cargo', '--version'],
                'nasm': ['nasm', '--version'],
                'gradle': ['gradle', '--version']
            },
            'installation_urls': {
                'gcc': 'https://gcc.gnu.org/install/',
                'g++': 'https://gcc.gnu.org/install/',
                'javac': 'https://openjdk.java.net/install/',
                'dotnet': 'https://dotnet.microsoft.com/download',
                'cargo': 'https://rustup.rs/',
                'nasm': 'https://www.nasm.us/',
                'gradle': 'https://gradle.org/install/'
            }
        }
        
        zipf.writestr('compiler_config.json', json.dumps(config, indent=2))
        
        # Add metadata
        metadata = {
            'backup_type': 'external_compilers',
            'created': datetime.now().isoformat(),
            'version': '1.0.0',
            'description': 'External System Compilers Backup'
        }
        
        zipf.writestr('metadata.json', json.dumps(metadata, indent=2))
        print(f"  ✅ Added external compiler configuration")
    
    print(f"✅ External compiler backup created: {ext_backup.name}")
    
    # .NET Safety Net backup
    print("\n📦 Creating .NET Safety Net Backup...")
    dotnet_backup = backup_dir / f"dotnet_safety_net_{timestamp}.zip"
    
    with zipfile.ZipFile(dotnet_backup, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Create .NET setup script
        dotnet_setup = '''#!/usr/bin/env python3
"""
.NET Safety Net Setup Script
Downloads multiple .NET versions for maximum compatibility
"""

import os
import sys
import subprocess
import zipfile
import requests
from pathlib import Path
import platform

def download_file(url, filename):
    """Download file from URL"""
    try:
        print(f"📥 Downloading {filename}...")
        response = requests.get(url, stream=True)
        response.raise_for_status()
        
        with open(filename, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        
        return True
    except Exception as e:
        print(f"❌ Failed to download {filename}: {e}")
        return False

def setup_dotnet_safety_net():
    """Setup .NET safety net with multiple versions"""
    print("🔷 .NET Safety Net Setup")
    print("=" * 40)
    
    # Create embedded tools directory
    embedded_dir = Path("embedded_tools")
    embedded_dir.mkdir(exist_ok=True)
    
    # Detect platform
    system = platform.system().lower()
    print(f"🖥️  Platform: {system}")
    
    # .NET versions for different platforms
    dotnet_versions = {
        'windows': {
            'sdk_8_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-8.0.0-win-x64.zip',
            'sdk_7_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-7.0.0-win-x64.zip',
            'sdk_6_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-6.0.0-win-x64.zip',
            'runtime_8_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-runtime-8.0.0-win-x64.zip',
            'runtime_7_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-runtime-7.0.0-win-x64.zip',
            'aspnet_8_0': 'https://download.visualstudio.microsoft.com/download/pr/aspnetcore-runtime-8.0.0-win-x64.zip'
        },
        'linux': {
            'sdk_8_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-8.0.0-linux-x64.zip',
            'sdk_7_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-7.0.0-linux-x64.zip',
            'runtime_8_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-runtime-8.0.0-linux-x64.zip'
        },
        'darwin': {
            'sdk_8_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-8.0.0-osx-x64.zip',
            'sdk_7_0': 'https://download.visualstudio.microsoft.com/download/pr/dotnet-sdk-7.0.0-osx-x64.zip'
        }
    }
    
    # Download .NET versions
    dotnet_versions_platform = dotnet_versions.get(system, dotnet_versions['windows'])
    
    for version, url in dotnet_versions_platform.items():
        filename = f"dotnet_{version}.zip"
        if download_file(url, filename):
            with zipfile.ZipFile(filename, 'r') as zipf:
                zipf.extractall(embedded_dir / f"dotnet_{version}")
            os.remove(filename)
            print(f"✅ {version} embedded")
        else:
            print(f"⚠️  {version} download failed - continuing...")
    
    # Create .NET launcher scripts
    create_dotnet_launchers(embedded_dir)
    
    print("\\n🎉 .NET Safety Net Complete!")
    print("✅ Multiple .NET versions available")
    print("✅ Launcher scripts created")
    print("🚨 Safety Net: Multiple versions ensure compatibility!")

def create_dotnet_launchers(embedded_dir):
    """Create .NET launcher scripts"""
    
    # Windows launcher
    windows_launcher = '''@echo off
REM .NET Safety Net Launcher for Windows
set DOTNET_ROOT=embedded_tools\\dotnet_sdk_8_0
set PATH=%DOTNET_ROOT%;%PATH%

REM Try different .NET versions
if exist "embedded_tools\\dotnet_sdk_8_0" (
    set DOTNET_ROOT=embedded_tools\\dotnet_sdk_8_0
    set PATH=%DOTNET_ROOT%;%PATH%
    echo ✅ Using .NET 8.0
) else if exist "embedded_tools\\dotnet_sdk_7_0" (
    set DOTNET_ROOT=embedded_tools\\dotnet_sdk_7_0
    set PATH=%DOTNET_ROOT%;%PATH%
    echo ✅ Using .NET 7.0
) else if exist "embedded_tools\\dotnet_sdk_6_0" (
    set DOTNET_ROOT=embedded_tools\\dotnet_sdk_6_0
    set PATH=%DOTNET_ROOT%;%PATH%
    echo ✅ Using .NET 6.0
)

REM Execute .NET command
%*
'''
    
    with open(embedded_dir / "dotnet_launcher.bat", 'w') as f:
        f.write(windows_launcher)
    
    # Unix launcher
    unix_launcher = '''#!/bin/bash
# .NET Safety Net Launcher
export DOTNET_ROOT="embedded_tools/dotnet_sdk_8_0"
export PATH="$DOTNET_ROOT:$PATH"

# Try different .NET versions
VERSIONS=("dotnet_sdk_8_0" "dotnet_sdk_7_0" "dotnet_sdk_6_0")

for version in "${VERSIONS[@]}"; do
    if [ -d "embedded_tools/$version" ]; then
        export DOTNET_ROOT="embedded_tools/$version"
        export PATH="$DOTNET_ROOT:$PATH"
        echo "✅ Using .NET version: $version"
        break
    fi
done

# Execute .NET command
exec "$@"
'''
    
    with open(embedded_dir / "dotnet_launcher.sh", 'w') as f:
        f.write(unix_launcher)
    os.chmod(embedded_dir / "dotnet_launcher.sh", 0o755)

if __name__ == "__main__":
    setup_dotnet_safety_net()
'''
        
        zipf.writestr('setup_dotnet_safety_net.py', dotnet_setup)
        
        # Add .NET metadata
        dotnet_metadata = {
            'backup_type': 'dotnet_safety_net',
            'created': datetime.now().isoformat(),
            'version': '1.0.0',
            'description': '.NET Safety Net - Multiple Versions',
            'dotnet_versions': {
                'sdk_8_0': '.NET SDK 8.0',
                'sdk_7_0': '.NET SDK 7.0',
                'sdk_6_0': '.NET SDK 6.0',
                'runtime_8_0': '.NET Runtime 8.0',
                'runtime_7_0': '.NET Runtime 7.0',
                'aspnet_8_0': 'ASP.NET Core 8.0'
            }
        }
        
        zipf.writestr('metadata.json', json.dumps(dotnet_metadata, indent=2))
        print(f"  ✅ Added .NET safety net configuration")
    
    print(f"✅ .NET safety net backup created: {dotnet_backup.name}")
    
    # Create master backup
    print("\n📦 Creating Master Backup...")
    master_backup = backup_dir / f"master_compiler_backup_{timestamp}.zip"
    
    with zipfile.ZipFile(master_backup, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Add all individual backups
        zipf.write(prod_backup, f"backups/{prod_backup.name}")
        zipf.write(ext_backup, f"backups/{ext_backup.name}")
        zipf.write(dotnet_backup, f"backups/{dotnet_backup.name}")
        
        # Create emergency recovery script
        recovery_script = '''#!/usr/bin/env python3
"""
Emergency Compiler Recovery Script
Restores compilers from backup archives
"""

import os
import sys
import zipfile
import json
from pathlib import Path
from datetime import datetime

def restore_from_backup(backup_path, restore_dir="restored_compilers"):
    """Restore compilers from backup"""
    print(f"🔄 Restoring from backup: {backup_path}")
    
    restore_path = Path(restore_dir)
    restore_path.mkdir(exist_ok=True)
    
    with zipfile.ZipFile(backup_path, 'r') as zipf:
        # Extract all files
        zipf.extractall(restore_path)
        
        # Read metadata
        if 'metadata.json' in zipf.namelist():
            with zipf.open('metadata.json') as f:
                metadata = json.load(f)
                print(f"📋 Backup Type: {metadata.get('backup_type', 'Unknown')}")
                print(f"📅 Created: {metadata.get('created', 'Unknown')}")
                print(f"📝 Description: {metadata.get('description', 'No description')}")
    
    print(f"✅ Restored to: {restore_path}")
    return restore_path

def list_available_backups(backup_dir="compiler_backups"):
    """List available backup files"""
    backup_path = Path(backup_dir)
    if not backup_path.exists():
        print("❌ No backup directory found")
        return []
    
    backups = []
    for file in backup_path.glob("*.zip"):
        backups.append(file)
    
    return sorted(backups, key=lambda x: x.stat().st_mtime, reverse=True)

def main():
    print("🚨 Emergency Compiler Recovery")
    print("=" * 40)
    
    # List available backups
    backups = list_available_backups()
    if not backups:
        print("❌ No backup files found")
        return
    
    print("📦 Available Backups:")
    for i, backup in enumerate(backups, 1):
        size = backup.stat().st_size / (1024 * 1024)  # MB
        print(f"  {i}. {backup.name} ({size:.1f} MB)")
    
    # Restore latest backup
    if backups:
        latest_backup = backups[0]
        print(f"\\n🔄 Restoring latest backup: {latest_backup.name}")
        restore_from_backup(latest_backup)
        print("✅ Emergency recovery complete!")

if __name__ == "__main__":
    main()
'''
        
        zipf.writestr('emergency_recovery.py', recovery_script)
        
        # Add master metadata
        master_metadata = {
            'backup_type': 'master_comprehensive',
            'created': datetime.now().isoformat(),
            'individual_backups': [prod_backup.name, ext_backup.name, dotnet_backup.name],
            'version': '1.0.0',
            'description': 'Master Compiler Backup - Complete System with .NET Safety Net',
            'recovery_instructions': [
                '1. Extract this ZIP file',
                '2. Run emergency_recovery.py',
                '3. Follow the prompts to restore compilers',
                '4. Restart the IDE'
            ],
            'safety_net_features': [
                'Multiple .NET versions (8.0, 7.0, 6.0)',
                'Production compilers backup',
                'External compiler detection',
                'Emergency recovery scripts',
                'Cross-platform support'
            ]
        }
        
        zipf.writestr('master_metadata.json', json.dumps(master_metadata, indent=2))
        print(f"  ✅ Added master metadata")
    
    print(f"🎉 Master backup created: {master_backup.name}")
    
    # List all backups
    print("\n📦 Available Compiler Backups:")
    print("=" * 40)
    
    backups = list(backup_dir.glob("*.zip"))
    for backup in sorted(backups, key=lambda x: x.stat().st_mtime, reverse=True):
        size = backup.stat().st_size / (1024 * 1024)  # MB
        modified = datetime.fromtimestamp(backup.stat().st_mtime)
        print(f"📁 {backup.name}")
        print(f"   Size: {size:.1f} MB")
        print(f"   Modified: {modified.strftime('%Y-%m-%d %H:%M:%S')}")
        print()
    
    print("✅ Compiler backup system complete!")
    print(f"📁 Backups stored in: {backup_dir}")
    print("🚨 Emergency recovery available via emergency_recovery.py")
    print("🔷 .NET Safety Net: Multiple versions ensure compatibility!")

if __name__ == "__main__":
    create_compiler_backup()
