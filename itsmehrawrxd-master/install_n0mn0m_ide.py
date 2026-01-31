#!/usr/bin/env python3
"""
n0mn0m IDE Installation Script
Automatically installs all required dependencies
"""

import subprocess
import sys
import os
from pathlib import Path

def install_requirements():
    """Install required packages"""
    
    print("🚀 n0mn0m IDE Installation Script")
    print("=" * 50)
    print("The Only IDE Created From Reverse Engineering!")
    print("=" * 50)
    
    # Check Python version
    python_version = sys.version_info
    if python_version < (3, 8):
        print("❌ Python 3.8+ required!")
        print(f"Current version: {python_version.major}.{python_version.minor}")
        return False
    
    print(f"✅ Python {python_version.major}.{python_version.minor} detected")
    
    # Required packages
    packages = [
        "psutil>=5.9.0",
        "requests>=2.28.0", 
        "flask>=2.2.0"
    ]
    
    print("\n📦 Installing required packages...")
    
    for package in packages:
        try:
            print(f"Installing {package}...")
            subprocess.check_call([
                sys.executable, "-m", "pip", "install", package, "--upgrade"
            ])
            print(f"✅ {package} installed successfully")
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to install {package}: {e}")
            return False
    
    # Test imports
    print("\n🧪 Testing imports...")
    
    try:
        import tkinter
        print("✅ tkinter available")
    except ImportError:
        print("❌ tkinter not available (should be included with Python)")
        return False
    
    try:
        import psutil
        print("✅ psutil available")
    except ImportError:
        print("❌ psutil import failed")
        return False
    
    try:
        import requests
        print("✅ requests available")
    except ImportError:
        print("❌ requests import failed")
        return False
    
    try:
        import flask
        print("✅ flask available")
    except ImportError:
        print("❌ flask import failed")
        return False
    
    # Check for IDE files
    print("\n🔍 Checking IDE files...")
    
    ide_files = [
        "n0mn0m_ide.py",
        "launch_n0mn0m_ide.bat",
        "N0MN0M_IDE_DOCUMENTATION.md"
    ]
    
    missing_files = []
    for file in ide_files:
        if Path(file).exists():
            print(f"✅ {file} found")
        else:
            print(f"❌ {file} not found")
            missing_files.append(file)
    
    if missing_files:
        print(f"\n⚠️ Missing files: {', '.join(missing_files)}")
        print("Please ensure all IDE files are in the current directory")
        return False
    
    # Create desktop shortcut (Windows)
    if os.name == 'nt':
        try:
            create_desktop_shortcut()
        except Exception as e:
            print(f"⚠️ Could not create desktop shortcut: {e}")
    
    print("\n🎉 Installation completed successfully!")
    print("\n📋 Next steps:")
    print("1. Run: python n0mn0m_ide.py")
    print("2. Or double-click: launch_n0mn0m_ide.bat")
    print("3. Enjoy n0mn0m IDE - The Only IDE Created From Reverse Engineering!")
    
    return True

def create_desktop_shortcut():
    """Create desktop shortcut on Windows"""
    
    try:
        import winshell
        from win32com.client import Dispatch
        
        desktop = winshell.desktop()
        path = os.path.join(desktop, "n0mn0m IDE.lnk")
        target = os.path.join(os.getcwd(), "launch_n0mn0m_ide.bat")
        wDir = os.getcwd()
        icon = target
        
        shell = Dispatch('WScript.Shell')
        shortcut = shell.CreateShortCut(path)
        shortcut.Targetpath = target
        shortcut.WorkingDirectory = wDir
        shortcut.IconLocation = icon
        shortcut.save()
        
        print("✅ Desktop shortcut created")
        
    except ImportError:
        print("⚠️ winshell not available - skipping desktop shortcut")
    except Exception as e:
        print(f"⚠️ Could not create shortcut: {e}")

def main():
    """Main installation function"""
    
    try:
        success = install_requirements()
        if success:
            print("\n🚀 Ready to launch n0mn0m IDE!")
            input("Press Enter to continue...")
        else:
            print("\n❌ Installation failed!")
            input("Press Enter to exit...")
            sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n👋 Installation cancelled by user")
    except Exception as e:
        print(f"\n❌ Installation error: {e}")
        input("Press Enter to exit...")
        sys.exit(1)

if __name__ == "__main__":
    main()
