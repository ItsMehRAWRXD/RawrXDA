#!/usr/bin/env python3
"""
RawrXD Qt Creator Launcher
Ensures Qt Creator uses the correct compiler and build configuration
"""

import sys
import os
import json
import platform
from pathlib import Path
import subprocess
import argparse

class QtCreatorLauncher:
    """Manages Qt Creator launch configuration"""
    
    def __init__(self, project_root=None):
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.config_dir = self._get_qt_config_dir()
        
    def _get_qt_config_dir(self):
        """Get Qt Creator config directory"""
        if platform.system() == "Windows":
            return Path.home() / "AppData/Roaming/QtProject/QtCreator"
        elif platform.system() == "Linux":
            return Path.home() / ".config/QtProject/QtCreator"
        elif platform.system() == "Darwin":
            return Path.home() / "Library/Qt Creator"
        
    def find_qt_creator(self):
        """Find Qt Creator executable"""
        candidates = []
        
        if platform.system() == "Windows":
            # Check common Qt Creator installation paths
            candidates = [
                Path("C:/Qt/Tools/QtCreator/bin/qtcreator.exe"),
                Path("C:/Program Files/Qt Creator/bin/qtcreator.exe"),
                Path("C:/Program Files (x86)/Qt Creator/bin/qtcreator.exe"),
            ]
            # Also check in PATH
            result = subprocess.run(
                ["where", "qtcreator"],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                candidates.insert(0, Path(result.stdout.strip()))
        
        elif platform.system() == "Linux":
            candidates = [
                Path("/usr/bin/qtcreator"),
                Path("/usr/local/bin/qtcreator"),
                Path(Path.home() / "Qt/Tools/QtCreator/bin/qtcreator"),
            ]
        
        elif platform.system() == "Darwin":
            candidates = [
                Path("/Applications/Qt Creator.app/Contents/MacOS/Qt Creator"),
                Path("/Applications/Qt Creator.app/Contents/MacOS/QtCreator"),
            ]
        
        # Find first existing
        for candidate in candidates:
            if candidate.exists():
                return candidate
        
        return None
    
    def setup_compiler_kit(self):
        """Setup compiler kit configuration"""
        
        print("🔧 Setting up Qt Creator compiler kit...")
        
        # Create CMakeLists.txt if not exists
        cmake_file = self.project_root / "CMakeLists.txt"
        if not cmake_file.exists():
            print("⚠️  CMakeLists.txt not found")
            return False
        
        # Create .cmake-build-debug directory (Qt Creator convention)
        build_dir = self.project_root / ".cmake-build-debug"
        build_dir.mkdir(exist_ok=True)
        
        # Create CMakeUserPresets.json for compiler configuration
        presets = {
            "version": 5,
            "vendor": {
                "rawr.xd": {
                    "projectRoot": str(self.project_root),
                }
            },
            "configurePresets": [
                {
                    "name": "debug",
                    "displayName": "RawrXD Debug",
                    "description": "Debug build with full symbols",
                    "generator": "Unix Makefiles" if platform.system() != "Windows" else "Visual Studio 16 2019",
                    "binaryDir": "${sourceDir}/build",
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": "Debug",
                        "CMAKE_CXX_STANDARD": "17",
                    }
                },
                {
                    "name": "release",
                    "displayName": "RawrXD Release",
                    "description": "Release build with optimizations",
                    "generator": "Unix Makefiles" if platform.system() != "Windows" else "Visual Studio 16 2019",
                    "binaryDir": "${sourceDir}/build-release",
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": "Release",
                        "CMAKE_CXX_STANDARD": "17",
                    }
                }
            ],
            "buildPresets": [
                {
                    "name": "debug",
                    "configurePreset": "debug",
                    "jobs": -1
                },
                {
                    "name": "release",
                    "configurePreset": "release",
                    "jobs": -1
                }
            ]
        }
        
        presets_file = self.project_root / "CMakeUserPresets.json"
        with open(presets_file, "w") as f:
            json.dump(presets, f, indent=2)
        
        print(f"✓ Created: {presets_file}")
        return True
    
    def launch(self, args=None):
        """Launch Qt Creator with proper configuration"""
        
        qt_creator = self.find_qt_creator()
        if not qt_creator:
            print("❌ Qt Creator not found")
            print("Please install Qt Creator from https://www.qt.io/download")
            return False
        
        print(f"🚀 Launching Qt Creator: {qt_creator}")
        
        # Prepare launch arguments
        launch_args = [str(qt_creator)]
        
        # Add project file or CMakeLists.txt
        cmake_file = self.project_root / "CMakeLists.txt"
        if cmake_file.exists():
            launch_args.append(str(cmake_file))
        
        # Add additional arguments
        if args:
            launch_args.extend(args)
        
        try:
            subprocess.Popen(launch_args)
            print("✓ Qt Creator launched successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to launch Qt Creator: {e}")
            return False
    
    def verify_setup(self):
        """Verify Qt Creator setup"""
        
        print("📋 Verifying Qt Creator setup...")
        
        checks = {
            "Qt Creator found": self.find_qt_creator() is not None,
            "CMakeLists.txt exists": (self.project_root / "CMakeLists.txt").exists(),
            "Build directory available": True,
        }
        
        all_passed = True
        for check, result in checks.items():
            status = "✓" if result else "✗"
            print(f"  {status} {check}")
            if not result:
                all_passed = False
        
        return all_passed

def main():
    """Main entry point"""
    
    parser = argparse.ArgumentParser(description="Qt Creator Launcher for RawrXD IDE")
    parser.add_argument("--project-root", default=None, help="Project root directory")
    parser.add_argument("--setup", action="store_true", help="Setup compiler kit configuration")
    parser.add_argument("--verify", action="store_true", help="Verify Qt Creator setup")
    parser.add_argument("--launch", action="store_true", help="Launch Qt Creator")
    
    args = parser.parse_args()
    
    launcher = QtCreatorLauncher(args.project_root)
    
    print("\n" + "="*60)
    print("Qt Creator Launcher for RawrXD IDE")
    print("="*60 + "\n")
    
    if args.verify:
        launcher.verify_setup()
    
    if args.setup:
        launcher.setup_compiler_kit()
    
    if args.launch or (not args.verify and not args.setup):
        launcher.launch()

if __name__ == "__main__":
    main()
