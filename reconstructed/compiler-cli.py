#!/usr/bin/env python3
"""
RawrXD IDE Universal Compiler CLI
Provides unified access to compilers from terminal, IDE, and standalone

Features:
- Universal compiler detection and configuration
- Build system integration (CMake, QMake, Ninja)
- Compiler benchmarking and diagnostics
- Integration with Qt IDE and Visual Studio
- Cross-platform support (Windows, Linux, macOS)
"""

import sys
import os
import subprocess
import json
import platform
import shutil
import argparse
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple
import multiprocessing

# ============================================================
# Configuration
# ============================================================

PROJECT_ROOT = Path(__file__).parent
BUILD_DIR = PROJECT_ROOT / "build"
REPORTS_DIR = PROJECT_ROOT / "reports"

# Known compiler locations by platform
COMPILER_PATHS = {
    "Windows": {
        "msvc": [
            r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC",
        ],
        "gcc": [
            r"C:\ProgramData\mingw64\mingw64\bin",
            r"C:\MinGW\bin",
        ],
        "clang": [
            r"C:\Program Files\LLVM\bin",
        ],
        "cmake": [
            r"C:\Program Files\CMake\bin",
        ],
    },
    "Linux": {
        "gcc": ["/usr/bin", "/usr/local/bin"],
        "clang": ["/usr/bin", "/usr/local/bin"],
        "cmake": ["/usr/bin", "/usr/local/bin"],
    },
}

# ============================================================
# Compiler Detection
# ============================================================

class CompilerDetector:
    """Detects available compilers on the system"""
    
    def __init__(self):
        self.system = platform.system()
        self.arch = platform.architecture()[0]
        self.compilers: Dict[str, Dict] = {}
    
    def detect_all(self) -> Dict[str, Dict]:
        """Detect all available compilers"""
        print("🔍 Detecting compilers...", file=sys.stderr)
        
        # Check system PATH first
        self.check_path()
        
        # Check known paths
        if self.system in COMPILER_PATHS:
            for compiler_name, paths in COMPILER_PATHS[self.system].items():
                for path in paths:
                    self.check_path(path, compiler_name)
        
        return self.compilers
    
    def check_path(self, path: str = None, compiler_type: str = None):
        """Check for compilers in a specific path or system PATH"""
        
        compiler_names = {
            "gcc": ["g++", "gcc"],
            "clang": ["clang++", "clang"],
            "msvc": ["cl"],
            "cmake": ["cmake"],
            "qmake": ["qmake"],
        }
        
        search_names = compiler_names.get(compiler_type, list(compiler_names.keys()))
        
        for name in search_names:
            try:
                if path:
                    full_path = os.path.join(path, name)
                    if not os.path.exists(full_path):
                        full_path = shutil.which(name)
                else:
                    full_path = shutil.which(name)
                
                if full_path:
                    info = self.get_compiler_info(full_path)
                    if info:
                        compiler_key = compiler_type or self.identify_compiler(name)
                        if compiler_key not in self.compilers:
                            self.compilers[compiler_key] = info
            except Exception as e:
                pass
    
    def get_compiler_info(self, compiler_path: str) -> Optional[Dict]:
        """Get compiler version and info"""
        try:
            result = subprocess.run(
                [compiler_path, "--version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            version = result.stdout.split('\n')[0] if result.stdout else "Unknown"
            
            return {
                "path": compiler_path,
                "version": version,
                "type": self.identify_compiler(compiler_path),
            }
        except Exception:
            return None
    
    def identify_compiler(self, name: str) -> str:
        """Identify compiler type from name"""
        name_lower = name.lower()
        if "cl.exe" in name_lower or "clang-cl" in name_lower:
            return "msvc"
        elif "g++" in name_lower or "gcc" in name_lower:
            return "gcc"
        elif "clang" in name_lower:
            return "clang"
        elif "cmake" in name_lower:
            return "cmake"
        elif "qmake" in name_lower:
            return "qmake"
        return "unknown"

# ============================================================
# Build System Manager
# ============================================================

class BuildManager:
    """Manages build system configuration and execution"""
    
    def __init__(self, detector: CompilerDetector):
        self.detector = detector
        self.selected_compiler = None
        self.build_dir = BUILD_DIR
        self.config = "Debug"
    
    def configure(self, compiler: str = None, config: str = "Debug") -> bool:
        """Configure build with CMake"""
        
        if compiler:
            self.selected_compiler = compiler
        elif "cmake" not in self.detector.compilers:
            print("❌ CMake not found!", file=sys.stderr)
            return False
        
        self.config = config
        
        # Create build directory
        self.build_dir.mkdir(parents=True, exist_ok=True)
        
        # Run CMake
        try:
            os.chdir(self.build_dir)
            
            cmake_cmd = [
                "cmake",
                "..",
                f"-DCMAKE_BUILD_TYPE={config}",
            ]
            
            print(f"⚙️  Configuring: {' '.join(cmake_cmd)}")
            result = subprocess.run(cmake_cmd, check=True)
            
            return result.returncode == 0
        except Exception as e:
            print(f"❌ Configuration failed: {e}", file=sys.stderr)
            return False
    
    def build(self, config: str = "Debug", parallel: int = None) -> bool:
        """Build project"""
        
        if parallel is None:
            parallel = multiprocessing.cpu_count()
        
        try:
            os.chdir(self.build_dir)
            
            build_cmd = [
                "cmake",
                "--build",
                ".",
                "--config", config,
                "--parallel", str(parallel),
            ]
            
            print(f"🔨 Building: {' '.join(build_cmd)}")
            result = subprocess.run(build_cmd, check=True)
            
            return result.returncode == 0
        except Exception as e:
            print(f"❌ Build failed: {e}", file=sys.stderr)
            return False
    
    def clean(self) -> bool:
        """Clean build artifacts"""
        try:
            if self.build_dir.exists():
                import shutil
                shutil.rmtree(self.build_dir)
            self.build_dir.mkdir(parents=True, exist_ok=True)
            print("✓ Build directory cleaned")
            return True
        except Exception as e:
            print(f"❌ Clean failed: {e}", file=sys.stderr)
            return False

# ============================================================
# CLI Interface
# ============================================================

class CompilerCLI:
    """Command-line interface for compiler management"""
    
    def __init__(self):
        self.detector = CompilerDetector()
        self.builder = BuildManager(self.detector)
    
    def cmd_detect(self, args) -> int:
        """Detect available compilers"""
        compilers = self.detector.detect_all()
        
        print("\n" + "="*60)
        print("COMPILER DETECTION REPORT")
        print("="*60)
        print(f"System: {self.detector.system} ({self.detector.arch})\n")
        
        if compilers:
            for compiler_type, info in compilers.items():
                print(f"✓ {compiler_type.upper()}")
                print(f"  Path: {info['path']}")
                print(f"  Version: {info['version']}\n")
        else:
            print("❌ No compilers found!\n")
            return 1
        
        return 0
    
    def cmd_audit(self, args) -> int:
        """Run full system audit"""
        print("\n" + "="*60)
        print("RAWRXD IDE - FULL SYSTEM AUDIT")
        print("="*60 + "\n")
        
        # Detect compilers
        print("1️⃣  COMPILER DETECTION")
        print("-" * 40)
        compilers = self.detector.detect_all()
        
        for compiler_type, info in compilers.items():
            print(f"✓ {compiler_type}: {info['path']}")
        
        if not compilers:
            print("❌ No compilers found!")
            return 1
        
        print(f"\n2️⃣  BUILD SYSTEM")
        print("-" * 40)
        
        # Check CMake
        if "cmake" in compilers:
            print(f"✓ CMake found: {compilers['cmake']['path']}")
        else:
            print("❌ CMake not found")
            return 1
        
        # Check project files
        print(f"\n3️⃣  PROJECT FILES")
        print("-" * 40)
        
        cmakelists = PROJECT_ROOT / "CMakeLists.txt"
        if cmakelists.exists():
            print(f"✓ CMakeLists.txt: {cmakelists}")
        else:
            print("❌ CMakeLists.txt not found")
        
        # Check build directory
        print(f"\n4️⃣  BUILD DIRECTORY")
        print("-" * 40)
        print(f"Location: {self.builder.build_dir}")
        print(f"Status: {'Exists' if self.builder.build_dir.exists() else 'Not created'}")
        
        print("\n" + "="*60)
        print("✓ Audit complete")
        print("="*60 + "\n")
        
        return 0
    
    def cmd_build(self, args) -> int:
        """Build project"""
        config = args.config or "Debug"
        
        print(f"\n🏗️  Building in {config} configuration...\n")
        
        if not self.builder.configure(config=config):
            print("❌ Configuration failed")
            return 1
        
        if not self.builder.build(config=config, parallel=args.jobs):
            print("❌ Build failed")
            return 1
        
        print("\n✓ Build successful!\n")
        return 0
    
    def cmd_clean(self, args) -> int:
        """Clean build artifacts"""
        if self.builder.clean():
            print("✓ Clean complete")
            return 0
        else:
            return 1
    
    def cmd_test(self, args) -> int:
        """Run tests"""
        print("🧪 Running tests...\n")
        
        # First build
        if self.cmd_build(args) != 0:
            return 1
        
        # Run tests
        try:
            os.chdir(self.builder.build_dir)
            result = subprocess.run(["ctest", "--output-on-failure"], check=True)
            print("\n✓ Tests passed!\n")
            return 0
        except Exception as e:
            print(f"❌ Tests failed: {e}\n", file=sys.stderr)
            return 1
    
    def main(self):
        """Main entry point"""
        parser = argparse.ArgumentParser(
            description="RawrXD IDE Universal Compiler CLI",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
EXAMPLES:
  # Detect available compilers
  python compiler-cli.py detect
  
  # Run full audit
  python compiler-cli.py audit
  
  # Build project
  python compiler-cli.py build --config Release
  
  # Clean build
  python compiler-cli.py clean
  
  # Run tests
  python compiler-cli.py test
"""
        )
        
        subparsers = parser.add_subparsers(dest="command", help="Commands")
        
        # Detect command
        subparsers.add_parser("detect", help="Detect available compilers")
        
        # Audit command
        subparsers.add_parser("audit", help="Run full system audit")
        
        # Build command
        build_parser = subparsers.add_parser("build", help="Build project")
        build_parser.add_argument("-c", "--config", default="Debug",
                                 help="Build configuration (Debug/Release)")
        build_parser.add_argument("-j", "--jobs", type=int,
                                 help="Number of parallel jobs")
        
        # Clean command
        subparsers.add_parser("clean", help="Clean build artifacts")
        
        # Test command
        test_parser = subparsers.add_parser("test", help="Run tests")
        test_parser.add_argument("-c", "--config", default="Debug",
                                help="Build configuration")
        
        args = parser.parse_args()
        
        if args.command == "detect":
            return self.cmd_detect(args)
        elif args.command == "audit":
            return self.cmd_audit(args)
        elif args.command == "build":
            return self.cmd_build(args)
        elif args.command == "clean":
            return self.cmd_clean(args)
        elif args.command == "test":
            return self.cmd_test(args)
        else:
            parser.print_help()
            return 0

# ============================================================
# Main Entry Point
# ============================================================

if __name__ == "__main__":
    cli = CompilerCLI()
    sys.exit(cli.main())
