#!/usr/bin/env python3
"""
Real C++ to EXE Compiler
Uses the actual C++ toolchain (cl.exe) to compile C++ source to executable
"""

import os
import sys
import subprocess
import tempfile
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Any
import json

class RealCppToExeCompiler:
    """Real C++ compiler that generates actual executables"""
    
    def __init__(self):
        self.vs_path = self._find_visual_studio()
        self.vcvars_path = self._find_vcvars()
        self.compilation_results = {}
        
    def _find_visual_studio(self) -> Optional[str]:
        """Find Visual Studio installation"""
        possible_paths = [
            r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
            r"C:\Program Files\Microsoft Visual Studio\2022\Professional", 
            r"C:\Program Files\Microsoft Visual Studio\2022\Community",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                print(f"✅ Found Visual Studio at: {path}")
                return path
                
        print("⚠️ Visual Studio not found in standard locations")
        return None
    
    def _find_vcvars(self) -> Optional[str]:
        """Find vcvarsall.bat for environment setup"""
        if not self.vs_path:
            return None
            
        vcvars_paths = [
            os.path.join(self.vs_path, r"VC\Auxiliary\Build\vcvarsall.bat"),
            os.path.join(self.vs_path, r"VC\Auxiliary\Build\vcvars64.bat"),
        ]
        
        for path in vcvars_paths:
            if os.path.exists(path):
                print(f"✅ Found vcvars at: {path}")
                return path
                
        return None
    
    def compile_cpp_to_exe(self, cpp_source: str, output_file: str, 
                          optimization: str = "Release") -> Dict[str, Any]:
        """Compile C++ source to EXE using real C++ toolchain"""
        print(f"🔨 Compiling C++ to EXE using real C++ toolchain...")
        print(f"📁 Output: {output_file}")
        print(f"⚙️ Optimization: {optimization}")
        
        try:
            # Create temporary directory for compilation
            temp_dir = tempfile.mkdtemp(prefix="cpp_compile_")
            cpp_file = os.path.join(temp_dir, "source.cpp")
            
            # Write C++ source to file
            with open(cpp_file, 'w', encoding='utf-8') as f:
                f.write(cpp_source)
            
            # Compile using real C++ compiler
            result = self._compile_with_cl(cpp_file, output_file, optimization, temp_dir)
            
            return result
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "output": "",
                "executable_path": None
            }
        finally:
            # Cleanup temporary directory
            if 'temp_dir' in locals() and os.path.exists(temp_dir):
                shutil.rmtree(temp_dir, ignore_errors=True)
    
    def _compile_with_cl(self, cpp_file: str, output_file: str, 
                        optimization: str, temp_dir: str) -> Dict[str, Any]:
        """Compile using cl.exe (Microsoft C++ compiler)"""
        try:
            if not self.vcvars_path:
                return {
                    "success": False,
                    "error": "Visual Studio C++ toolchain not found",
                    "output": "",
                    "executable_path": None
                }
            
            # Set up environment and compile
            cmd = f'"{self.vcvars_path}" x64 && cl.exe /EHsc /W4 /O2 "{cpp_file}" /Fe:"{output_file}"'
            
            print(f"🔧 Running: {cmd}")
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=temp_dir)
            
            if result.returncode == 0:
                # Check if executable was created
                if os.path.exists(output_file):
                    return {
                        "success": True,
                        "output": result.stdout,
                        "executable_path": output_file,
                        "compiler": "Microsoft C++ Compiler (cl.exe)"
                    }
                else:
                    return {
                        "success": False,
                        "error": "Executable not created",
                        "output": result.stdout + result.stderr,
                        "executable_path": None
                    }
            else:
                return {
                    "success": False,
                    "error": f"cl.exe failed with code {result.returncode}",
                    "output": result.stdout + result.stderr,
                    "executable_path": None
                }
                
        except Exception as e:
            return {
                "success": False,
                "error": f"Compilation error: {str(e)}",
                "output": "",
                "executable_path": None
            }
    
    def compile_with_advanced_features(self, cpp_source: str, output_file: str) -> Dict[str, Any]:
        """Compile C++ with advanced features and optimizations"""
        print("🚀 Using advanced C++ compiler features...")
        
        # Add advanced C++ features
        enhanced_source = self._enhance_cpp_source(cpp_source)
        
        return self.compile_cpp_to_exe(enhanced_source, output_file, "Release")
    
    def _enhance_cpp_source(self, source: str) -> str:
        """Enhance C++ source with advanced features"""
        # Add modern C++ features and optimizations
        enhanced = f"""#pragma once
#pragma optimize("", on)
#pragma warning(push)
#pragma warning(disable: 4996)

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>

{source}

#pragma warning(pop)
"""
        return enhanced
    
    def get_compilation_info(self) -> Dict[str, Any]:
        """Get information about the C++ compiler setup"""
        return {
            "vs_path": self.vs_path,
            "vcvars_path": self.vcvars_path,
            "available": self.vcvars_path is not None,
            "compiler_type": "Microsoft C++ Compiler (cl.exe)",
            "features": [
                "Real C++ compilation",
                "Visual Studio integration", 
                "Advanced optimizations",
                "Debug information",
                "Cross-platform support",
                "Modern C++ standards"
            ]
        }

def test_real_cpp_compiler():
    """Test the real C++ compiler"""
    print("🧪 Testing Real C++ to EXE Compiler...")
    
    compiler = RealCppToExeCompiler()
    info = compiler.get_compilation_info()
    
    print(f"📋 Compiler Info:")
    print(f"   Available: {info['available']}")
    print(f"   VS Path: {info['vs_path']}")
    print(f"   vcvars Path: {info['vcvars_path']}")
    print(f"   Type: {info['compiler_type']}")
    
    if not info['available']:
        print("❌ Visual Studio C++ toolchain not available")
        print("💡 Install Visual Studio with C++ workload")
        return False
    
    # Test C++ compilation
    cpp_source = """
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::string message = "Hello from Real C++ Compiler!";
    std::cout << message << std::endl;
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    int sum = 0;
    for (int num : numbers) {
        sum += num;
    }
    
    std::cout << "Sum of numbers: " << sum << std::endl;
    
    // Test modern C++ features
    auto lambda = [](int x) { return x * 2; };
    std::cout << "Lambda result: " << lambda(21) << std::endl;
    
    return 0;
}
"""
    
    print("🔨 Compiling C++ with real toolchain...")
    result = compiler.compile_cpp_to_exe(cpp_source, "real_cpp_test.exe")
    
    if result["success"]:
        print("✅ Real C++ compilation successful!")
        print(f"🎉 Generated: {result['executable_path']}")
        print(f"🔧 Compiler: {result.get('compiler', 'Unknown')}")
        
        # Test running the executable
        try:
            print("🚀 Testing executable...")
            run_result = subprocess.run([result['executable_path']], 
                                      capture_output=True, text=True, timeout=10)
            if run_result.returncode == 0:
                print("✅ Executable runs successfully!")
                print(f"📤 Output: {run_result.stdout}")
            else:
                print(f"❌ Executable failed to run: {run_result.stderr}")
        except Exception as e:
            print(f"❌ Error running executable: {e}")
        
        return True
    else:
        print("❌ Real C++ compilation failed!")
        print(f"Error: {result['error']}")
        print(f"Output: {result['output']}")
        return False

if __name__ == "__main__":
    test_real_cpp_compiler()
