#!/usr/bin/env python3
"""
Real Working Compiler
A proper compiler that actually works by using existing toolchains correctly
"""

import sys
import os
import subprocess
import tempfile
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
import json

class RealWorkingCompiler:
    """A compiler that actually works by leveraging existing tools properly"""
    
    def __init__(self):
        self.compilers = self._detect_available_compilers()
        self.temp_dir = None
        self.build_info = {}
    
    def _detect_available_compilers(self) -> Dict[str, Dict[str, Any]]:
        """Detect available compilers on the system"""
        compilers = {}
        
        # Check for GCC
        try:
            result = subprocess.run(['gcc', '--version'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stdout.split('\n')[0]
                compilers['gcc'] = {
                    'name': 'GCC',
                    'version': version,
                    'c_compiler': 'gcc',
                    'cpp_compiler': 'g++',
                    'available': True
                }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        # Check for Clang
        try:
            result = subprocess.run(['clang', '--version'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stdout.split('\n')[0]
                compilers['clang'] = {
                    'name': 'Clang',
                    'version': version,
                    'c_compiler': 'clang',
                    'cpp_compiler': 'clang++',
                    'available': True
                }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        # Check for MSVC (Windows)
        if os.name == 'nt':
            try:
                # Try to find Visual Studio
                vs_paths = [
                    r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
                    r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
                    r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
                    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
                    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
                    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
                ]
                
                for vs_path in vs_paths:
                    if os.path.exists(vs_path):
                        compilers['msvc'] = {
                            'name': 'Microsoft Visual C++',
                            'version': 'MSVC (Visual Studio)',
                            'c_compiler': 'cl',
                            'cpp_compiler': 'cl',
                            'vcvars_path': vs_path,
                            'available': True
                        }
                        break
            except:
                pass
        
        return compilers
    
    def compile_cpp(self, source_file: str, output_file: str = None, 
                   compiler: str = 'auto', optimization: str = 'O2',
                   include_dirs: List[str] = None, libraries: List[str] = None,
                   defines: List[str] = None) -> Dict[str, Any]:
        """Compile C++ source file to executable"""
        
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output_file': None
            }
        
        # Auto-detect compiler if not specified
        if compiler == 'auto':
            if 'gcc' in self.compilers and self.compilers['gcc']['available']:
                compiler = 'gcc'
            elif 'clang' in self.compilers and self.compilers['clang']['available']:
                compiler = 'clang'
            elif 'msvc' in self.compilers and self.compilers['msvc']['available']:
                compiler = 'msvc'
            else:
                return {
                    'success': False,
                    'error': 'No C++ compiler found on system',
                    'output_file': None
                }
        
        if compiler not in self.compilers or not self.compilers[compiler]['available']:
            return {
                'success': False,
                'error': f"Compiler '{compiler}' not available",
                'output_file': None
            }
        
        # Set default output file
        if not output_file:
            output_file = os.path.splitext(source_file)[0]
            if os.name == 'nt':
                output_file += '.exe'
        
        # Create temporary directory for build
        self.temp_dir = tempfile.mkdtemp(prefix='compiler_')
        
        try:
            if compiler == 'gcc':
                return self._compile_with_gcc(source_file, output_file, optimization, 
                                           include_dirs, libraries, defines)
            elif compiler == 'clang':
                return self._compile_with_clang(source_file, output_file, optimization,
                                              include_dirs, libraries, defines)
            elif compiler == 'msvc':
                return self._compile_with_msvc(source_file, output_file, optimization,
                                             include_dirs, libraries, defines)
            else:
                return {
                    'success': False,
                    'error': f"Unknown compiler: {compiler}",
                    'output_file': None
                }
        
        except Exception as e:
            return {
                'success': False,
                'error': f"Compilation failed: {str(e)}",
                'output_file': None
            }
        
        finally:
            # Cleanup
            if self.temp_dir and os.path.exists(self.temp_dir):
                shutil.rmtree(self.temp_dir, ignore_errors=True)
    
    def _compile_with_gcc(self, source_file: str, output_file: str, optimization: str,
                         include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile with GCC"""
        cmd = [self.compilers['gcc']['cpp_compiler']]
        
        # Add optimization flags
        if optimization == 'O0':
            cmd.append('-O0')
        elif optimization == 'O1':
            cmd.append('-O1')
        elif optimization == 'O2':
            cmd.append('-O2')
        elif optimization == 'O3':
            cmd.append('-O3')
        elif optimization == 'Os':
            cmd.append('-Os')
        
        # Add include directories
        if include_dirs:
            for include_dir in include_dirs:
                cmd.extend(['-I', include_dir])
        
        # Add preprocessor definitions
        if defines:
            for define in defines:
                cmd.extend(['-D', define])
        
        # Add source file
        cmd.append(source_file)
        
        # Add output file
        cmd.extend(['-o', output_file])
        
        # Add libraries
        if libraries:
            for library in libraries:
                cmd.extend(['-l', library])
        
        # Add standard C++ library
        cmd.append('-lstdc++')
        
        # Add debug information
        cmd.append('-g')
        
        # Add warnings
        cmd.extend(['-Wall', '-Wextra', '-Wpedantic'])
        
        print(f"🔨 Compiling with GCC: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'gcc',
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"GCC compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'GCC compilation timed out',
                'output_file': None
            }
    
    def _compile_with_clang(self, source_file: str, output_file: str, optimization: str,
                           include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile with Clang"""
        cmd = [self.compilers['clang']['cpp_compiler']]
        
        # Add optimization flags
        if optimization == 'O0':
            cmd.append('-O0')
        elif optimization == 'O1':
            cmd.append('-O1')
        elif optimization == 'O2':
            cmd.append('-O2')
        elif optimization == 'O3':
            cmd.append('-O3')
        elif optimization == 'Os':
            cmd.append('-Os')
        
        # Add include directories
        if include_dirs:
            for include_dir in include_dirs:
                cmd.extend(['-I', include_dir])
        
        # Add preprocessor definitions
        if defines:
            for define in defines:
                cmd.extend(['-D', define])
        
        # Add source file
        cmd.append(source_file)
        
        # Add output file
        cmd.extend(['-o', output_file])
        
        # Add libraries
        if libraries:
            for library in libraries:
                cmd.extend(['-l', library])
        
        # Add standard C++ library
        cmd.append('-lstdc++')
        
        # Add debug information
        cmd.append('-g')
        
        # Add warnings
        cmd.extend(['-Wall', '-Wextra', '-Wpedantic'])
        
        print(f"🔨 Compiling with Clang: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'clang',
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"Clang compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Clang compilation timed out',
                'output_file': None
            }
    
    def _compile_with_msvc(self, source_file: str, output_file: str, optimization: str,
                          include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile with MSVC"""
        # Set up Visual Studio environment
        vcvars_path = self.compilers['msvc']['vcvars_path']
        
        # Create batch file to set up environment and compile
        batch_file = os.path.join(self.temp_dir, 'compile.bat')
        
        with open(batch_file, 'w') as f:
            f.write(f'@echo off\n')
            f.write(f'call "{vcvars_path}"\n')
            
            # Add optimization flags
            if optimization == 'O0':
                f.write('set OPT_FLAGS=/Od\n')
            elif optimization == 'O1':
                f.write('set OPT_FLAGS=/O1\n')
            elif optimization == 'O2':
                f.write('set OPT_FLAGS=/O2\n')
            elif optimization == 'O3':
                f.write('set OPT_FLAGS=/O2\n')  # MSVC doesn't have O3
            elif optimization == 'Os':
                f.write('set OPT_FLAGS=/Os\n')
            else:
                f.write('set OPT_FLAGS=/O2\n')
            
            # Build command
            cmd_parts = ['cl', '/EHsc', '/std:c++17', '%OPT_FLAGS%']
            
            # Add include directories
            if include_dirs:
                for include_dir in include_dirs:
                    cmd_parts.append(f'/I"{include_dir}"')
            
            # Add preprocessor definitions
            if defines:
                for define in defines:
                    cmd_parts.append(f'/D{define}')
            
            # Add source file
            cmd_parts.append(f'"{source_file}"')
            
            # Add output file
            cmd_parts.append(f'/Fe"{output_file}"')
            
            # Add libraries
            if libraries:
                for library in libraries:
                    cmd_parts.append(f'"{library}.lib"')
            
            f.write(' '.join(cmd_parts) + '\n')
            f.write('echo Compilation completed\n')
        
        print(f"🔨 Compiling with MSVC: {batch_file}")
        
        try:
            result = subprocess.run([batch_file], capture_output=True, text=True, timeout=60, shell=True)
            
            if result.returncode == 0 and os.path.exists(output_file):
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'msvc',
                    'command': batch_file,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"MSVC compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': batch_file,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'MSVC compilation timed out',
                'output_file': None
            }
    
    def compile_c(self, source_file: str, output_file: str = None,
                  compiler: str = 'auto', optimization: str = 'O2',
                  include_dirs: List[str] = None, libraries: List[str] = None,
                  defines: List[str] = None) -> Dict[str, Any]:
        """Compile C source file to executable"""
        
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output_file': None
            }
        
        # Auto-detect compiler if not specified
        if compiler == 'auto':
            if 'gcc' in self.compilers and self.compilers['gcc']['available']:
                compiler = 'gcc'
            elif 'clang' in self.compilers and self.compilers['clang']['available']:
                compiler = 'clang'
            elif 'msvc' in self.compilers and self.compilers['msvc']['available']:
                compiler = 'msvc'
            else:
                return {
                    'success': False,
                    'error': 'No C compiler found on system',
                    'output_file': None
                }
        
        if compiler not in self.compilers or not self.compilers[compiler]['available']:
            return {
                'success': False,
                'error': f"Compiler '{compiler}' not available",
                'output_file': None
            }
        
        # Set default output file
        if not output_file:
            output_file = os.path.splitext(source_file)[0]
            if os.name == 'nt':
                output_file += '.exe'
        
        # Create temporary directory for build
        self.temp_dir = tempfile.mkdtemp(prefix='compiler_')
        
        try:
            if compiler == 'gcc':
                return self._compile_c_with_gcc(source_file, output_file, optimization,
                                              include_dirs, libraries, defines)
            elif compiler == 'clang':
                return self._compile_c_with_clang(source_file, output_file, optimization,
                                                 include_dirs, libraries, defines)
            elif compiler == 'msvc':
                return self._compile_c_with_msvc(source_file, output_file, optimization,
                                                include_dirs, libraries, defines)
            else:
                return {
                    'success': False,
                    'error': f"Unknown compiler: {compiler}",
                    'output_file': None
                }
        
        except Exception as e:
            return {
                'success': False,
                'error': f"Compilation failed: {str(e)}",
                'output_file': None
            }
        
        finally:
            # Cleanup
            if self.temp_dir and os.path.exists(self.temp_dir):
                shutil.rmtree(self.temp_dir, ignore_errors=True)
    
    def _compile_c_with_gcc(self, source_file: str, output_file: str, optimization: str,
                           include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile C with GCC"""
        cmd = [self.compilers['gcc']['c_compiler']]
        
        # Add optimization flags
        if optimization == 'O0':
            cmd.append('-O0')
        elif optimization == 'O1':
            cmd.append('-O1')
        elif optimization == 'O2':
            cmd.append('-O2')
        elif optimization == 'O3':
            cmd.append('-O3')
        elif optimization == 'Os':
            cmd.append('-Os')
        
        # Add include directories
        if include_dirs:
            for include_dir in include_dirs:
                cmd.extend(['-I', include_dir])
        
        # Add preprocessor definitions
        if defines:
            for define in defines:
                cmd.extend(['-D', define])
        
        # Add source file
        cmd.append(source_file)
        
        # Add output file
        cmd.extend(['-o', output_file])
        
        # Add libraries
        if libraries:
            for library in libraries:
                cmd.extend(['-l', library])
        
        # Add debug information
        cmd.append('-g')
        
        # Add warnings
        cmd.extend(['-Wall', '-Wextra', '-Wpedantic'])
        
        print(f"🔨 Compiling C with GCC: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'gcc',
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"GCC compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'GCC compilation timed out',
                'output_file': None
            }
    
    def _compile_c_with_clang(self, source_file: str, output_file: str, optimization: str,
                             include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile C with Clang"""
        cmd = [self.compilers['clang']['c_compiler']]
        
        # Add optimization flags
        if optimization == 'O0':
            cmd.append('-O0')
        elif optimization == 'O1':
            cmd.append('-O1')
        elif optimization == 'O2':
            cmd.append('-O2')
        elif optimization == 'O3':
            cmd.append('-O3')
        elif optimization == 'Os':
            cmd.append('-Os')
        
        # Add include directories
        if include_dirs:
            for include_dir in include_dirs:
                cmd.extend(['-I', include_dir])
        
        # Add preprocessor definitions
        if defines:
            for define in defines:
                cmd.extend(['-D', define])
        
        # Add source file
        cmd.append(source_file)
        
        # Add output file
        cmd.extend(['-o', output_file])
        
        # Add libraries
        if libraries:
            for library in libraries:
                cmd.extend(['-l', library])
        
        # Add debug information
        cmd.append('-g')
        
        # Add warnings
        cmd.extend(['-Wall', '-Wextra', '-Wpedantic'])
        
        print(f"🔨 Compiling C with Clang: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'clang',
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"Clang compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Clang compilation timed out',
                'output_file': None
            }
    
    def _compile_c_with_msvc(self, source_file: str, output_file: str, optimization: str,
                            include_dirs: List[str], libraries: List[str], defines: List[str]) -> Dict[str, Any]:
        """Compile C with MSVC"""
        # Set up Visual Studio environment
        vcvars_path = self.compilers['msvc']['vcvars_path']
        
        # Create batch file to set up environment and compile
        batch_file = os.path.join(self.temp_dir, 'compile_c.bat')
        
        with open(batch_file, 'w') as f:
            f.write(f'@echo off\n')
            f.write(f'call "{vcvars_path}"\n')
            
            # Add optimization flags
            if optimization == 'O0':
                f.write('set OPT_FLAGS=/Od\n')
            elif optimization == 'O1':
                f.write('set OPT_FLAGS=/O1\n')
            elif optimization == 'O2':
                f.write('set OPT_FLAGS=/O2\n')
            elif optimization == 'O3':
                f.write('set OPT_FLAGS=/O2\n')  # MSVC doesn't have O3
            elif optimization == 'Os':
                f.write('set OPT_FLAGS=/Os\n')
            else:
                f.write('set OPT_FLAGS=/O2\n')
            
            # Build command
            cmd_parts = ['cl', '/TC', '%OPT_FLAGS%']
            
            # Add include directories
            if include_dirs:
                for include_dir in include_dirs:
                    cmd_parts.append(f'/I"{include_dir}"')
            
            # Add preprocessor definitions
            if defines:
                for define in defines:
                    cmd_parts.append(f'/D{define}')
            
            # Add source file
            cmd_parts.append(f'"{source_file}"')
            
            # Add output file
            cmd_parts.append(f'/Fe"{output_file}"')
            
            # Add libraries
            if libraries:
                for library in libraries:
                    cmd_parts.append(f'"{library}.lib"')
            
            f.write(' '.join(cmd_parts) + '\n')
            f.write('echo Compilation completed\n')
        
        print(f"🔨 Compiling C with MSVC: {batch_file}")
        
        try:
            result = subprocess.run([batch_file], capture_output=True, text=True, timeout=60, shell=True)
            
            if result.returncode == 0 and os.path.exists(output_file):
                return {
                    'success': True,
                    'output_file': output_file,
                    'compiler': 'msvc',
                    'command': batch_file,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"MSVC compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': batch_file,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'MSVC compilation timed out',
                'output_file': None
            }
    
    def get_available_compilers(self) -> Dict[str, Dict[str, Any]]:
        """Get list of available compilers"""
        return {k: v for k, v in self.compilers.items() if v['available']}
    
    def get_compiler_info(self, compiler: str) -> Dict[str, Any]:
        """Get information about a specific compiler"""
        if compiler in self.compilers:
            return self.compilers[compiler]
        return {}
    
    def test_compiler(self, compiler: str) -> Dict[str, Any]:
        """Test if a compiler is working"""
        if compiler not in self.compilers or not self.compilers[compiler]['available']:
            return {
                'success': False,
                'error': f"Compiler '{compiler}' not available"
            }
        
        # Create a simple test program
        test_source = os.path.join(self.temp_dir or tempfile.mkdtemp(), 'test.cpp')
        with open(test_source, 'w') as f:
            f.write('#include <iostream>\n')
            f.write('int main() {\n')
            f.write('    std::cout << "Hello, World!" << std::endl;\n')
            f.write('    return 0;\n')
            f.write('}\n')
        
        # Try to compile
        result = self.compile_cpp(test_source, 'test_output', compiler)
        
        # Cleanup
        if os.path.exists(test_source):
            os.remove(test_source)
        if 'test_output' in result and result['test_output'] and os.path.exists(result['test_output']):
            os.remove(result['test_output'])
        
        return result

def main():
    """Main function for testing"""
    if len(sys.argv) < 2:
        print("Usage: python real_working_compiler.py <source_file> [output_file] [compiler]")
        print("\nAvailable compilers:")
        compiler = RealWorkingCompiler()
        for name, info in compiler.get_available_compilers().items():
            print(f"  {name}: {info['name']} - {info['version']}")
        sys.exit(1)
    
    source_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    compiler_name = sys.argv[3] if len(sys.argv) > 3 else 'auto'
    
    print(f"🚀 Real Working Compiler")
    print(f"📁 Source: {source_file}")
    print(f"📁 Output: {output_file or 'auto'}")
    print(f"🔧 Compiler: {compiler_name}")
    
    # Create compiler
    compiler = RealWorkingCompiler()
    
    # Show available compilers
    print(f"\n🔍 Available compilers:")
    for name, info in compiler.get_available_compilers().items():
        print(f"   {name}: {info['name']} - {info['version']}")
    
    # Determine file type and compile
    if source_file.endswith('.cpp') or source_file.endswith('.cc') or source_file.endswith('.cxx'):
        result = compiler.compile_cpp(source_file, output_file, compiler_name)
    elif source_file.endswith('.c'):
        result = compiler.compile_c(source_file, output_file, compiler_name)
    else:
        print(f"❌ Unknown file type: {source_file}")
        sys.exit(1)
    
    # Show results
    if result['success']:
        print(f"\n✅ Compilation successful!")
        print(f"   Output: {result['output_file']}")
        print(f"   Compiler: {result['compiler']}")
        print(f"   Command: {result['command']}")
        
        # Test the executable
        if result['output_file'] and os.path.exists(result['output_file']):
            print(f"\n🧪 Testing executable...")
            try:
                test_result = subprocess.run([result['output_file']], capture_output=True, text=True, timeout=10)
                if test_result.returncode == 0:
                    print(f"✅ Executable runs successfully!")
                    print(f"   Output: {test_result.stdout}")
                else:
                    print(f"⚠️ Executable failed with return code {test_result.returncode}")
                    print(f"   Error: {test_result.stderr}")
            except subprocess.TimeoutExpired:
                print(f"⚠️ Executable timed out")
            except Exception as e:
                print(f"⚠️ Error running executable: {e}")
    else:
        print(f"\n❌ Compilation failed: {result['error']}")
        if 'command' in result:
            print(f"   Command: {result['command']}")
        if 'stderr' in result and result['stderr']:
            print(f"   Error output: {result['stderr']}")
        sys.exit(1)

if __name__ == "__main__":
    main()
