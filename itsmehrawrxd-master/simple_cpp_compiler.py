#!/usr/bin/env python3
"""
Simple C++ Compiler
A minimal C++ compiler that generates working executables
"""

import os
import sys
import subprocess
import tempfile
import shutil
from pathlib import Path

class SimpleCppCompiler:
    """Simple C++ compiler that works without complex toolchain setup"""
    
    def __init__(self):
        self.compilation_results = {}
    
    def compile_cpp_to_exe(self, cpp_source: str, output_file: str) -> dict:
        """Compile C++ source to EXE using the simplest approach"""
        print(f"🔨 Simple C++ Compiler - Compiling to {output_file}")
        
        try:
            # Create temporary directory
            temp_dir = tempfile.mkdtemp(prefix="simple_cpp_")
            cpp_file = os.path.join(temp_dir, "main.cpp")
            
            # Write C++ source
            with open(cpp_file, 'w', encoding='utf-8') as f:
                f.write(cpp_source)
            
            # Try multiple compilation approaches
            result = self._try_compilation_methods(cpp_file, output_file, temp_dir)
            
            return result
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "output": "",
                "executable_path": None
            }
        finally:
            # Cleanup
            if 'temp_dir' in locals() and os.path.exists(temp_dir):
                shutil.rmtree(temp_dir, ignore_errors=True)
    
    def _try_compilation_methods(self, cpp_file: str, output_file: str, temp_dir: str) -> dict:
        """Try different compilation methods"""
        
        # Method 1: Try g++ if available
        if self._try_gcc_compilation(cpp_file, output_file, temp_dir):
            return {
                "success": True,
                "output": "Compiled with g++",
                "executable_path": output_file,
                "compiler": "g++"
            }
        
        # Method 2: Try cl.exe with minimal flags
        if self._try_cl_compilation(cpp_file, output_file, temp_dir):
            return {
                "success": True,
                "output": "Compiled with cl.exe",
                "executable_path": output_file,
                "compiler": "cl.exe"
            }
        
        # Method 3: Try creating a simple batch file approach
        if self._try_batch_compilation(cpp_file, output_file, temp_dir):
            return {
                "success": True,
                "output": "Compiled with batch method",
                "executable_path": output_file,
                "compiler": "batch"
            }
        
        return {
            "success": False,
            "error": "All compilation methods failed",
            "output": "",
            "executable_path": None
        }
    
    def _try_gcc_compilation(self, cpp_file: str, output_file: str, temp_dir: str) -> bool:
        """Try compiling with g++"""
        try:
            cmd = f'g++ -o "{output_file}" "{cpp_file}"'
            print(f"🔧 Trying g++: {cmd}")
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=temp_dir)
            
            if result.returncode == 0 and os.path.exists(output_file):
                print("✅ g++ compilation successful!")
                return True
            else:
                print(f"❌ g++ failed: {result.stderr}")
                return False
        except:
            print("❌ g++ not available")
            return False
    
    def _try_cl_compilation(self, cpp_file: str, output_file: str, temp_dir: str) -> bool:
        """Try compiling with cl.exe"""
        try:
            # Find Visual Studio
            vs_paths = [
                r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
                r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
                r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
            ]
            
            vcvars = None
            for path in vs_paths:
                if os.path.exists(path):
                    vcvars = path
                    break
            
            if not vcvars:
                print("❌ Visual Studio not found")
                return False
            
            # Simple compilation
            cmd = f'"{vcvars}" x64 && cl.exe "{cpp_file}" /Fe:"{output_file}"'
            print(f"🔧 Trying cl.exe: {cmd}")
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=temp_dir)
            
            if result.returncode == 0 and os.path.exists(output_file):
                print("✅ cl.exe compilation successful!")
                return True
            else:
                print(f"❌ cl.exe failed: {result.stderr}")
                return False
        except Exception as e:
            print(f"❌ cl.exe error: {e}")
            return False
    
    def _try_batch_compilation(self, cpp_file: str, output_file: str, temp_dir: str) -> bool:
        """Try batch compilation approach"""
        try:
            # Create a simple batch file
            batch_file = os.path.join(temp_dir, "compile.bat")
            with open(batch_file, 'w') as f:
                f.write(f'@echo off\n')
                f.write(f'cl.exe "{cpp_file}" /Fe:"{output_file}"\n')
            
            cmd = f'"{batch_file}"'
            print(f"🔧 Trying batch: {cmd}")
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=temp_dir)
            
            if os.path.exists(output_file):
                print("✅ Batch compilation successful!")
                return True
            else:
                print(f"❌ Batch failed: {result.stderr}")
                return False
        except Exception as e:
            print(f"❌ Batch error: {e}")
            return False

def test_simple_cpp_compiler():
    """Test the simple C++ compiler"""
    print("🧪 Testing Simple C++ Compiler...")
    
    compiler = SimpleCppCompiler()
    
    # Test C++ source
    cpp_source = """
#include <iostream>
#include <string>

int main() {
    std::string message = "Hello from Simple C++ Compiler!";
    std::cout << message << std::endl;
    
    int x = 42;
    int y = 8;
    int result = x + y;
    
    std::cout << "42 + 8 = " << result << std::endl;
    return 0;
}
"""
    
    print("🔨 Compiling C++ with simple compiler...")
    result = compiler.compile_cpp_to_exe(cpp_source, "simple_cpp_test.exe")
    
    if result["success"]:
        print("✅ Simple C++ compilation successful!")
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
        print("❌ Simple C++ compilation failed!")
        print(f"Error: {result['error']}")
        return False

if __name__ == "__main__":
    test_simple_cpp_compiler()
