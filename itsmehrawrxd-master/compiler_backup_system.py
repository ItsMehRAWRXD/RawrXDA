#!/usr/bin/env python3
"""
RawrZ Universal IDE - Compiler Backup System
Creates ZIP archives of all compilers for emergency recovery
"""

import os
import sys
import zipfile
import shutil
import json
import subprocess
from datetime import datetime
from pathlib import Path

class CompilerBackupSystem:
    def __init__(self):
        self.ide_root = Path(__file__).parent
        self.backup_dir = self.ide_root / "compiler_backups"
        self.backup_dir.mkdir(exist_ok=True)
        
        # Compiler categories
        self.production_compilers = {
            'c_compiler': 'RealProductionCompiler',
            'cpp_compiler': 'RealCppCompiler', 
            'python_compiler': 'RealPythonCompiler',
            'solidity_compiler': 'RealSolidityCompiler',
            'assembly_compiler': 'gASMCompiler',
            'csharp_compiler': 'EmbeddedCSharpCompiler'
        }
        
        self.external_compilers = {
            'gcc': 'C/C++ Compiler',
            'g++': 'C++ Compiler',
            'javac': 'Java Compiler',
            'dotnet': 'C# Compiler',
            'cargo': 'Rust Compiler',
            'nasm': 'Assembly Compiler',
            'gradle': 'Android Builder'
        }
        
        self.embedded_tools = {
            'dotnet_sdk': 'Embedded .NET SDK',
            'dotnet_runtime': 'Embedded .NET Runtime',
            'dotnet_aspnet': 'Embedded ASP.NET Core',
            'gradle_wrapper': 'Embedded Gradle',
            'android_sdk': 'Embedded Android SDK',
            'java_jdk': 'Embedded Java JDK'
        }

    def create_production_compiler_backup(self):
        """Create backup of production compilers"""
        print("🔧 Creating Production Compiler Backup...")
        
        backup_name = f"production_compilers_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
        backup_path = self.backup_dir / backup_name
        
        with zipfile.ZipFile(backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
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
                file_path = self.ide_root / file_name
                if file_path.exists():
                    zipf.write(file_path, f"compilers/{file_name}")
                    print(f"  ✅ Added {file_name}")
            
            # Add compiler metadata
            metadata = {
                'backup_type': 'production_compilers',
                'created': datetime.now().isoformat(),
                'compilers': self.production_compilers,
                'version': '1.0.0',
                'description': 'RawrZ Production Compilers Backup'
            }
            
            zipf.writestr('metadata.json', json.dumps(metadata, indent=2))
            print(f"  ✅ Added metadata")
        
        print(f"✅ Production compiler backup created: {backup_name}")
        return backup_path

    def create_external_compiler_backup(self):
        """Create backup of external system compilers"""
        print("🔧 Creating External Compiler Backup...")
        
        backup_name = f"external_compilers_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
        backup_path = self.backup_dir / backup_name
        
        with zipfile.ZipFile(backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Create compiler detection scripts
            detection_script = self.create_compiler_detection_script()
            zipf.writestr('detect_compilers.py', detection_script)
            
            # Create installation scripts for each platform
            for platform in ['windows', 'linux', 'macos']:
                install_script = self.create_installation_script(platform)
                zipf.writestr(f'install_{platform}.sh', install_script)
            
            # Add compiler configuration
            config = {
                'compilers': self.external_compilers,
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
                'compilers': self.external_compilers,
                'version': '1.0.0',
                'description': 'External System Compilers Backup'
            }
            
            zipf.writestr('metadata.json', json.dumps(metadata, indent=2))
            print(f"  ✅ Added external compiler configuration")
        
        print(f"✅ External compiler backup created: {backup_name}")
        return backup_path

    def create_embedded_tools_backup(self):
        """Create backup of embedded tools"""
        print("🔧 Creating Embedded Tools Backup...")
        
        backup_name = f"embedded_tools_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
        backup_path = self.backup_dir / backup_name
        
        with zipfile.ZipFile(backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Add embedded tools directory if it exists
            embedded_dir = self.ide_root / "embedded_tools"
            if embedded_dir.exists():
                for root, dirs, files in os.walk(embedded_dir):
                    for file in files:
                        file_path = Path(root) / file
                        arc_path = file_path.relative_to(self.ide_root)
                        zipf.write(file_path, arc_path)
                        print(f"  ✅ Added {arc_path}")
            
            # Create embedded tools setup script
            setup_script = self.create_embedded_tools_setup_script()
            zipf.writestr('setup_embedded_tools.py', setup_script)
            
            # Add metadata
            metadata = {
                'backup_type': 'embedded_tools',
                'created': datetime.now().isoformat(),
                'tools': self.embedded_tools,
                'version': '1.0.0',
                'description': 'Embedded Development Tools Backup'
            }
            
            zipf.writestr('metadata.json', json.dumps(metadata, indent=2))
            print(f"  ✅ Added embedded tools configuration")
        
        print(f"✅ Embedded tools backup created: {backup_name}")
        return backup_path

    def create_compiler_detection_script(self):
        """Create script to detect available compilers"""
        return '''#!/usr/bin/env python3
"""
Compiler Detection Script
Detects available system compilers
"""

import subprocess
import sys
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

    def create_installation_script(self, platform):
        """Create installation script for specific platform"""
        scripts = {
            'windows': '''@echo off
echo Installing compilers on Windows...

REM Install Chocolatey if not present
if not exist "C:\\ProgramData\\chocolatey\\bin\\choco.exe" (
    powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
)

REM Install compilers
choco install -y gcc
choco install -y openjdk
choco install -y dotnet
choco install -y rust
choco install -y nasm
choco install -y gradle

echo ✅ All compilers installed!
''',
            'linux': '''#!/bin/bash
echo "Installing compilers on Linux..."

# Update package manager
sudo apt update

# Install compilers
sudo apt install -y gcc g++
sudo apt install -y openjdk-11-jdk
sudo apt install -y dotnet-sdk-6.0
sudo apt install -y nasm
sudo apt install -y gradle

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

echo "✅ All compilers installed!"
''',
            'macos': '''#!/bin/bash
echo "Installing compilers on macOS..."

# Install Homebrew if not present
if ! command -v brew &> /dev/null; then
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install compilers
brew install gcc
brew install openjdk
brew install dotnet
brew install rust
brew install nasm
brew install gradle

echo "✅ All compilers installed!"
'''
        }
        return scripts.get(platform, '')

    def create_embedded_tools_setup_script(self):
        """Create script to setup embedded tools"""
        return '''#!/usr/bin/env python3
"""
Embedded Tools Setup Script - Safety Net Edition
Downloads and configures embedded development tools including complete .NET stack
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

def setup_embedded_tools():
    """Setup embedded development tools with complete .NET safety net"""
    print("🔧 Setting up embedded tools - Safety Net Edition...")
    print("=" * 60)
    
    # Create embedded tools directory
    embedded_dir = Path("embedded_tools")
    embedded_dir.mkdir(exist_ok=True)
    
    # Detect platform
    system = platform.system().lower()
    arch = platform.machine().lower()
    
    print(f"🖥️  Platform: {system} {arch}")
    
    # .NET Safety Net - Multiple versions for maximum compatibility
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
    
    # Download .NET Safety Net
    print("🔷 .NET Safety Net - Multiple Versions")
    print("-" * 40)
    
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
    
    # Create .NET launcher script
    dotnet_launcher = '''#!/bin/bash
# .NET Safety Net Launcher
export DOTNET_ROOT="embedded_tools/dotnet_sdk_8_0"
export PATH="$DOTNET_ROOT:$PATH"

# Try different .NET versions in order of preference
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
        f.write(dotnet_launcher)
    os.chmod(embedded_dir / "dotnet_launcher.sh", 0o755)
    
    # Create .NET Windows launcher
    dotnet_launcher_bat = '''@echo off
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
        f.write(dotnet_launcher_bat)
    
    # Download Gradle
    print("\\n📦 Gradle Build System")
    print("-" * 40)
    gradle_url = "https://services.gradle.org/distributions/gradle-8.3-bin.zip"
    if download_file(gradle_url, "gradle.zip"):
        with zipfile.ZipFile("gradle.zip", 'r') as zipf:
            zipf.extractall(embedded_dir / "gradle")
        os.remove("gradle.zip")
        print("✅ Gradle embedded")
    
    # Download Android SDK
    print("\\n📱 Android Development Kit")
    print("-" * 40)
    android_url = "https://dl.google.com/android/repository/commandlinetools-win-9477386_latest.zip"
    if download_file(android_url, "android_sdk.zip"):
        with zipfile.ZipFile("android_sdk.zip", 'r') as zipf:
            zipf.extractall(embedded_dir / "android_sdk")
        os.remove("android_sdk.zip")
        print("✅ Android SDK embedded")
    
    # Download Java JDK
    print("\\n☕ Java Development Kit")
    print("-" * 40)
    java_url = "https://download.java.net/java/GA/jdk11/9/GPL/openjdk-11.0.2_windows-x64_bin.zip"
    if download_file(java_url, "java_jdk.zip"):
        with zipfile.ZipFile("java_jdk.zip", 'r') as zipf:
            zipf.extractall(embedded_dir / "java")
        os.remove("java_jdk.zip")
        print("✅ Java JDK embedded")
    
    # Create comprehensive launcher script
    launcher_script = '''#!/usr/bin/env python3
"""
RawrZ IDE - Embedded Tools Launcher
Safety net for all development tools
"""

import os
import sys
import subprocess
from pathlib import Path

def launch_dotnet(*args):
    """Launch .NET with safety net"""
    embedded_dir = Path("embedded_tools")
    
    # Try different .NET versions
    versions = ["dotnet_sdk_8_0", "dotnet_sdk_7_0", "dotnet_sdk_6_0"]
    
    for version in versions:
        dotnet_path = embedded_dir / version
        if dotnet_path.exists():
            dotnet_exe = dotnet_path / "dotnet.exe" if os.name == 'nt' else dotnet_path / "dotnet"
            if dotnet_exe.exists():
                print(f"🚀 Launching .NET from {version}")
                cmd = [str(dotnet_exe)] + list(args)
                subprocess.run(cmd)
                return True
    
    print("❌ No .NET version found in embedded tools")
    return False

def launch_gradle(*args):
    """Launch Gradle with safety net"""
    embedded_dir = Path("embedded_tools")
    gradle_path = embedded_dir / "gradle" / "gradle-8.3" / "bin"
    
    if gradle_path.exists():
        gradle_exe = gradle_path / "gradle.bat" if os.name == 'nt' else gradle_path / "gradle"
        if gradle_exe.exists():
            print("🚀 Launching Gradle")
            cmd = [str(gradle_exe)] + list(args)
            subprocess.run(cmd)
            return True
    
    print("❌ Gradle not found in embedded tools")
    return False

if __name__ == "__main__":
    if len(sys.argv) > 1:
        tool = sys.argv[1]
        args = sys.argv[2:]
        
        if tool == "dotnet":
            launch_dotnet(*args)
        elif tool == "gradle":
            launch_gradle(*args)
        else:
            print(f"Unknown tool: {tool}")
    else:
        print("RawrZ IDE - Embedded Tools Launcher")
        print("Usage: python launcher.py <tool> <args>")
        print("Tools: dotnet, gradle")
'''
    
    with open(embedded_dir / "launcher.py", 'w') as f:
        f.write(launcher_script)
    
    print("\\n🎉 Embedded Tools Safety Net Complete!")
    print("=" * 60)
    print("✅ .NET SDK 8.0, 7.0, 6.0 (Multiple versions)")
    print("✅ .NET Runtime 8.0, 7.0")
    print("✅ ASP.NET Core 8.0")
    print("✅ Gradle 8.3")
    print("✅ Android SDK")
    print("✅ Java JDK 11")
    print("✅ Launcher scripts created")
    print("\\n🚨 Safety Net: Multiple .NET versions ensure compatibility!")

if __name__ == "__main__":
    setup_embedded_tools()
'''

    def create_emergency_recovery_script(self):
        """Create emergency recovery script"""
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
        
        recovery_path = self.backup_dir / "emergency_recovery.py"
        with open(recovery_path, 'w') as f:
            f.write(recovery_script)
        print(f"✅ Emergency recovery script created: {recovery_path}")

    def create_comprehensive_backup(self):
        """Create comprehensive backup of all compilers"""
        print("🚀 Creating Comprehensive Compiler Backup...")
        print("=" * 50)
        
        backups = []
        
        # Create individual backups
        backups.append(self.create_production_compiler_backup())
        backups.append(self.create_external_compiler_backup())
        backups.append(self.create_embedded_tools_backup())
        
        # Create emergency recovery script
        self.create_emergency_recovery_script()
        
        # Create master backup
        master_backup_name = f"master_compiler_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
        master_backup_path = self.backup_dir / master_backup_name
        
        with zipfile.ZipFile(master_backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Add all individual backups
            for backup in backups:
                zipf.write(backup, f"backups/{backup.name}")
            
            # Add emergency recovery script
            recovery_script = self.backup_dir / "emergency_recovery.py"
            if recovery_script.exists():
                zipf.write(recovery_script, "emergency_recovery.py")
            
            # Add comprehensive metadata
            master_metadata = {
                'backup_type': 'master_comprehensive',
                'created': datetime.now().isoformat(),
                'individual_backups': [b.name for b in backups],
                'version': '1.0.0',
                'description': 'Master Compiler Backup - Complete System',
                'recovery_instructions': [
                    '1. Extract this ZIP file',
                    '2. Run emergency_recovery.py',
                    '3. Follow the prompts to restore compilers',
                    '4. Restart the IDE'
                ]
            }
            
            zipf.writestr('master_metadata.json', json.dumps(master_metadata, indent=2))
            print(f"  ✅ Added master metadata")
        
        print(f"🎉 Master backup created: {master_backup_name}")
        return master_backup_path

    def list_backups(self):
        """List all available backups"""
        print("📦 Available Compiler Backups:")
        print("=" * 40)
        
        if not self.backup_dir.exists():
            print("❌ No backup directory found")
            return
        
        backups = list(self.backup_dir.glob("*.zip"))
        if not backups:
            print("❌ No backup files found")
            return
        
        for backup in sorted(backups, key=lambda x: x.stat().st_mtime, reverse=True):
            size = backup.stat().st_size / (1024 * 1024)  # MB
            modified = datetime.fromtimestamp(backup.stat().st_mtime)
            print(f"📁 {backup.name}")
            print(f"   Size: {size:.1f} MB")
            print(f"   Modified: {modified.strftime('%Y-%m-%d %H:%M:%S')}")
            print()

def main():
    """Main function"""
    print("🔧 RawrZ Universal IDE - Compiler Backup System")
    print("=" * 60)
    
    backup_system = CompilerBackupSystem()
    
    # Create comprehensive backup
    master_backup = backup_system.create_comprehensive_backup()
    
    # List all backups
    backup_system.list_backups()
    
    print("✅ Compiler backup system complete!")
    print(f"📁 Backups stored in: {backup_system.backup_dir}")
    print("🚨 Emergency recovery available via emergency_recovery.py")

if __name__ == "__main__":
    main()
