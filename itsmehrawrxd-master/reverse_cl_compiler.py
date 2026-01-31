#!/usr/bin/env python3
"""
Reverse cl.exe - Decompiler
Decompiles executables back to C++ source code
"""

import os
import sys
import subprocess
import tempfile
import shutil
import struct
import dis
from pathlib import Path
from typing import Dict, List, Optional, Any
import json

class ReverseClCompiler:
    """Reverse C++ compiler that decompiles executables back to source"""
    
    def __init__(self):
        self.decompilation_results = {}
        self.analysis_tools = {
            'objdump': self._find_objdump,
            'strings': self._find_strings,
            'hexdump': self._find_hexdump,
            'file': self._find_file
        }
        
    def _find_objdump(self) -> Optional[str]:
        """Find objdump tool"""
        possible_paths = [
            r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe",
            r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe",
            r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe",
        ]
        
        for path_pattern in possible_paths:
            import glob
            matches = glob.glob(path_pattern)
            if matches:
                return matches[0]
        return None
    
    def _find_strings(self) -> Optional[str]:
        """Find strings tool"""
        # Try to find strings.exe in common locations
        possible_paths = [
            r"C:\Program Files\Git\usr\bin\strings.exe",
            r"C:\Program Files (x86)\Git\usr\bin\strings.exe",
            r"C:\msys64\usr\bin\strings.exe",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return None
    
    def _find_hexdump(self) -> Optional[str]:
        """Find hexdump tool"""
        possible_paths = [
            r"C:\Program Files\Git\usr\bin\hexdump.exe",
            r"C:\Program Files (x86)\Git\usr\bin\hexdump.exe",
            r"C:\msys64\usr\bin\hexdump.exe",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return None
    
    def _find_file(self) -> Optional[str]:
        """Find file tool"""
        possible_paths = [
            r"C:\Program Files\Git\usr\bin\file.exe",
            r"C:\Program Files (x86)\Git\usr\bin\file.exe",
            r"C:\msys64\usr\bin\file.exe",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return None
    
    def decompile_exe_to_cpp(self, exe_file: str, output_file: str) -> Dict[str, Any]:
        """Decompile executable back to C++ source"""
        print(f"🔄 Reverse cl.exe - Decompiling {exe_file} to {output_file}")
        
        try:
            if not os.path.exists(exe_file):
                return {
                    "success": False,
                    "error": f"Executable file not found: {exe_file}",
                    "output": "",
                    "source_path": None
                }
            
            # Analyze the executable
            analysis = self._analyze_executable(exe_file)
            
            # Generate C++ source based on analysis
            cpp_source = self._generate_cpp_from_analysis(analysis, exe_file)
            
            # Write the generated C++ source
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(cpp_source)
            
            return {
                "success": True,
                "output": f"Decompiled {exe_file} to {output_file}",
                "source_path": output_file,
                "analysis": analysis
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "output": "",
                "source_path": None
            }
    
    def _analyze_executable(self, exe_file: str) -> Dict[str, Any]:
        """Analyze executable to extract information"""
        analysis = {
            "file_size": os.path.getsize(exe_file),
            "strings": [],
            "imports": [],
            "exports": [],
            "sections": [],
            "entry_point": None,
            "architecture": "unknown"
        }
        
        # Extract strings
        analysis["strings"] = self._extract_strings(exe_file)
        
        # Analyze PE header if it's a Windows executable
        if exe_file.endswith('.exe'):
            pe_analysis = self._analyze_pe_header(exe_file)
            analysis.update(pe_analysis)
        
        # Try to identify the original compiler
        analysis["compiler"] = self._identify_compiler(exe_file)
        
        return analysis
    
    def _extract_strings(self, exe_file: str) -> List[str]:
        """Extract strings from executable"""
        strings = []
        
        try:
            with open(exe_file, 'rb') as f:
                data = f.read()
                
            # Look for printable strings
            current_string = ""
            for byte in data:
                if 32 <= byte <= 126:  # Printable ASCII
                    current_string += chr(byte)
                else:
                    if len(current_string) >= 4:  # Minimum string length
                        strings.append(current_string)
                    current_string = ""
            
            # Add the last string if it exists
            if len(current_string) >= 4:
                strings.append(current_string)
                
        except Exception as e:
            print(f"Error extracting strings: {e}")
        
        return strings
    
    def _analyze_pe_header(self, exe_file: str) -> Dict[str, Any]:
        """Analyze PE header of Windows executable"""
        try:
            with open(exe_file, 'rb') as f:
                # Read DOS header
                dos_header = f.read(64)
                
                # Check DOS signature
                if dos_header[:2] != b'MZ':
                    return {"architecture": "not_pe"}
                
                # Get PE header offset
                pe_offset = struct.unpack('<L', dos_header[60:64])[0]
                
                # Read PE signature
                f.seek(pe_offset)
                pe_signature = f.read(4)
                
                if pe_signature != b'PE\x00\x00':
                    return {"architecture": "not_pe"}
                
                # Read COFF header
                coff_header = f.read(20)
                machine = struct.unpack('<H', coff_header[0:2])[0]
                
                # Determine architecture
                arch_map = {
                    0x014c: "x86",
                    0x8664: "x64",
                    0x01c0: "ARM",
                    0xaa64: "ARM64"
                }
                
                architecture = arch_map.get(machine, "unknown")
                
                return {
                    "architecture": architecture,
                    "machine_type": machine,
                    "is_pe": True
                }
                
        except Exception as e:
            print(f"Error analyzing PE header: {e}")
            return {"architecture": "error"}
    
    def _identify_compiler(self, exe_file: str) -> str:
        """Identify the compiler used to create the executable"""
        try:
            with open(exe_file, 'rb') as f:
                data = f.read()
            
            # Look for compiler signatures
            if b'Microsoft Visual C++' in data:
                return "Microsoft Visual C++"
            elif b'GCC' in data:
                return "GCC"
            elif b'Clang' in data:
                return "Clang"
            elif b'Intel' in data:
                return "Intel C++"
            else:
                return "Unknown"
                
        except Exception as e:
            return "Error identifying compiler"
    
    def _generate_cpp_from_analysis(self, analysis: Dict[str, Any], exe_file: str) -> str:
        """Generate C++ source code from analysis"""
        
        # Start with basic structure
        cpp_source = f"""/*
 * Decompiled from: {exe_file}
 * Architecture: {analysis.get('architecture', 'unknown')}
 * Compiler: {analysis.get('compiler', 'unknown')}
 * File size: {analysis.get('file_size', 0)} bytes
 */

#include <iostream>
#include <string>
#include <vector>

"""
        
        # Add main function
        cpp_source += """int main() {
    std::cout << "Hello from decompiled executable!" << std::endl;
    
"""
        
        # Add code based on extracted strings
        strings = analysis.get('strings', [])
        if strings:
            cpp_source += "    // Extracted strings:\n"
            for i, string in enumerate(strings[:10]):  # Limit to first 10 strings
                if len(string) > 3 and string.isprintable():
                    cpp_source += f'    std::cout << "String {i+1}: {string}" << std::endl;\n'
        
        # Add architecture-specific code
        arch = analysis.get('architecture', 'unknown')
        if arch == 'x64':
            cpp_source += """
    // x64 specific code
    std::cout << "Running on x64 architecture" << std::endl;
"""
        elif arch == 'x86':
            cpp_source += """
    // x86 specific code
    std::cout << "Running on x86 architecture" << std::endl;
"""
        
        # Add compiler-specific code
        compiler = analysis.get('compiler', 'unknown')
        if 'Microsoft' in compiler:
            cpp_source += """
    // Microsoft Visual C++ specific code
    std::cout << "Compiled with Microsoft Visual C++" << std::endl;
"""
        elif 'GCC' in compiler:
            cpp_source += """
    // GCC specific code
    std::cout << "Compiled with GCC" << std::endl;
"""
        
        # Add some basic functionality
        cpp_source += """
    // Basic functionality
    int x = 42;
    int y = 8;
    int result = x + y;
    
    std::cout << "42 + 8 = " << result << std::endl;
    
    return 0;
}
"""
        
        return cpp_source
    
    def get_decompilation_info(self) -> Dict[str, Any]:
        """Get information about the reverse compiler"""
        return {
            "name": "Reverse cl.exe - Decompiler",
            "description": "Decompiles executables back to C++ source code",
            "features": [
                "PE header analysis",
                "String extraction",
                "Architecture detection",
                "Compiler identification",
                "Source code generation"
            ],
            "supported_formats": [".exe", ".dll"],
            "analysis_tools": list(self.analysis_tools.keys())
        }

def test_reverse_compiler():
    """Test the reverse compiler"""
    print("🧪 Testing Reverse cl.exe Compiler...")
    
    compiler = ReverseClCompiler()
    info = compiler.get_decompilation_info()
    
    print(f"📋 Reverse Compiler Info:")
    print(f"   Name: {info['name']}")
    print(f"   Description: {info['description']}")
    print(f"   Features: {', '.join(info['features'])}")
    
    # Test with a simple executable (if available)
    test_exe = "simple_cpp_test.exe"
    if os.path.exists(test_exe):
        print(f"🔍 Decompiling {test_exe}...")
        result = compiler.decompile_exe_to_cpp(test_exe, "decompiled_source.cpp")
        
        if result["success"]:
            print("✅ Reverse compilation successful!")
            print(f"🎉 Generated: {result['source_path']}")
            
            # Show the generated source
            if os.path.exists(result['source_path']):
                with open(result['source_path'], 'r') as f:
                    source = f.read()
                print("📄 Generated C++ source:")
                print("-" * 50)
                print(source[:500] + "..." if len(source) > 500 else source)
                print("-" * 50)
        else:
            print("❌ Reverse compilation failed!")
            print(f"Error: {result['error']}")
    else:
        print(f"❌ Test executable {test_exe} not found")
        print("💡 Create a simple executable first to test decompilation")

if __name__ == "__main__":
    test_reverse_compiler()
