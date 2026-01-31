#!/usr/bin/env python3
"""
External Worktools for Journal IDE
Compatible compilation and development tools
"""

import os
import sys
import subprocess
import tempfile
import shutil
import json
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
import threading
import queue
import logging

class ExternalWorktools:
    """External worktools compatible with Journal IDE"""
    
    def __init__(self, journal_ide=None):
        self.journal_ide = journal_ide
        self.temp_dir = None
        self.active_processes = {}
        self.logger = self._setup_logger()
        
    def _setup_logger(self):
        """Setup logging for worktools"""
        logger = logging.getLogger('ExternalWorktools')
        logger.setLevel(logging.INFO)
        
        if not logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            handler.setFormatter(formatter)
            logger.addHandler(handler)
        
        return logger
    
    def detect_compilers(self) -> Dict[str, Dict[str, Any]]:
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
                    'available': True,
                    'type': 'gnu'
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
                    'available': True,
                    'type': 'llvm'
                }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        # Check for MSVC (Windows)
        if os.name == 'nt':
            try:
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
                            'available': True,
                            'type': 'msvc'
                        }
                        break
            except:
                pass
        
        # Check for Python
        try:
            result = subprocess.run([sys.executable, '--version'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stdout.strip()
                compilers['python'] = {
                    'name': 'Python',
                    'version': version,
                    'interpreter': sys.executable,
                    'available': True,
                    'type': 'interpreted'
                }
        except:
            pass
        
        # Check for Node.js
        try:
            result = subprocess.run(['node', '--version'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stdout.strip()
                compilers['nodejs'] = {
                    'name': 'Node.js',
                    'version': version,
                    'interpreter': 'node',
                    'available': True,
                    'type': 'interpreted'
                }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        # Check for Java
        try:
            result = subprocess.run(['java', '-version'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stderr.split('\n')[0]  # Java version goes to stderr
                compilers['java'] = {
                    'name': 'Java',
                    'version': version,
                    'compiler': 'javac',
                    'interpreter': 'java',
                    'available': True,
                    'type': 'bytecode'
                }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        return compilers
    
    def compile_cpp(self, source_file: str, output_file: str = None, 
                   compiler: str = 'auto', optimization: str = 'O2',
                   include_dirs: List[str] = None, libraries: List[str] = None,
                   defines: List[str] = None) -> Dict[str, Any]:
        """Compile C++ source file"""
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output_file': None
            }
        
        compilers = self.detect_compilers()
        
        # Auto-detect compiler
        if compiler == 'auto':
            if 'gcc' in compilers and compilers['gcc']['available']:
                compiler = 'gcc'
            elif 'clang' in compilers and compilers['clang']['available']:
                compiler = 'clang'
            elif 'msvc' in compilers and compilers['msvc']['available']:
                compiler = 'msvc'
            else:
                return {
                    'success': False,
                    'error': 'No C++ compiler found on system',
                    'output_file': None
                }
        
        if compiler not in compilers or not compilers[compiler]['available']:
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
        
        # Create temporary directory
        self.temp_dir = tempfile.mkdtemp(prefix='worktools_')
        
        try:
            if compiler == 'gcc':
                return self._compile_cpp_with_gcc(source_file, output_file, optimization, 
                                               include_dirs, libraries, defines, compilers[compiler])
            elif compiler == 'clang':
                return self._compile_cpp_with_clang(source_file, output_file, optimization,
                                                  include_dirs, libraries, defines, compilers[compiler])
            elif compiler == 'msvc':
                return self._compile_cpp_with_msvc(source_file, output_file, optimization,
                                                 include_dirs, libraries, defines, compilers[compiler])
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
    
    def _compile_cpp_with_gcc(self, source_file: str, output_file: str, optimization: str,
                            include_dirs: List[str], libraries: List[str], defines: List[str],
                            compiler_info: Dict[str, Any]) -> Dict[str, Any]:
        """Compile C++ with GCC"""
        cmd = [compiler_info['cpp_compiler']]
        
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
        
        self.logger.info(f"Compiling with GCC: {' '.join(cmd)}")
        
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
    
    def _compile_cpp_with_clang(self, source_file: str, output_file: str, optimization: str,
                               include_dirs: List[str], libraries: List[str], defines: List[str],
                               compiler_info: Dict[str, Any]) -> Dict[str, Any]:
        """Compile C++ with Clang"""
        cmd = [compiler_info['cpp_compiler']]
        
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
        
        self.logger.info(f"Compiling with Clang: {' '.join(cmd)}")
        
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
    
    def _compile_cpp_with_msvc(self, source_file: str, output_file: str, optimization: str,
                              include_dirs: List[str], libraries: List[str], defines: List[str],
                              compiler_info: Dict[str, Any]) -> Dict[str, Any]:
        """Compile C++ with MSVC"""
        vcvars_path = compiler_info['vcvars_path']
        
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
        
        self.logger.info(f"Compiling with MSVC: {batch_file}")
        
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
    
    def run_python(self, source_file: str, args: List[str] = None) -> Dict[str, Any]:
        """Run Python script"""
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output': None
            }
        
        try:
            cmd = [sys.executable, source_file]
            if args:
                cmd.extend(args)
            
            self.logger.info(f"Running Python: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            return {
                'success': True,
                'output': result.stdout,
                'error': result.stderr,
                'return_code': result.returncode,
                'command': ' '.join(cmd)
            }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Python execution timed out',
                'output': None
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Python execution failed: {str(e)}",
                'output': None
            }
    
    def run_nodejs(self, source_file: str, args: List[str] = None) -> Dict[str, Any]:
        """Run Node.js script"""
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output': None
            }
        
        try:
            cmd = ['node', source_file]
            if args:
                cmd.extend(args)
            
            self.logger.info(f"Running Node.js: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            return {
                'success': True,
                'output': result.stdout,
                'error': result.stderr,
                'return_code': result.returncode,
                'command': ' '.join(cmd)
            }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Node.js execution timed out',
                'output': None
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Node.js execution failed: {str(e)}",
                'output': None
            }
    
    def compile_java(self, source_file: str, output_dir: str = None) -> Dict[str, Any]:
        """Compile Java source file"""
        if not os.path.exists(source_file):
            return {
                'success': False,
                'error': f"Source file '{source_file}' not found",
                'output_file': None
            }
        
        # Set default output directory
        if not output_dir:
            output_dir = os.path.dirname(source_file)
        
        try:
            cmd = ['javac', '-d', output_dir, source_file]
            
            self.logger.info(f"Compiling Java: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                # Find the generated class file
                class_name = os.path.splitext(os.path.basename(source_file))[0]
                class_file = os.path.join(output_dir, f"{class_name}.class")
                
                return {
                    'success': True,
                    'output_file': class_file,
                    'output_dir': output_dir,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            else:
                return {
                    'success': False,
                    'error': f"Java compilation failed: {result.stderr}",
                    'output_file': None,
                    'command': ' '.join(cmd),
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Java compilation timed out',
                'output_file': None
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Java compilation failed: {str(e)}",
                'output_file': None
            }
    
    def run_java(self, class_file: str, classpath: str = None, args: List[str] = None) -> Dict[str, Any]:
        """Run Java class file"""
        if not os.path.exists(class_file):
            return {
                'success': False,
                'error': f"Class file '{class_file}' not found",
                'output': None
            }
        
        try:
            # Get class name from file path
            class_name = os.path.splitext(os.path.basename(class_file))[0]
            class_dir = os.path.dirname(class_file)
            
            cmd = ['java']
            
            # Add classpath if specified
            if classpath:
                cmd.extend(['-cp', classpath])
            elif class_dir:
                cmd.extend(['-cp', class_dir])
            
            cmd.append(class_name)
            
            if args:
                cmd.extend(args)
            
            self.logger.info(f"Running Java: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            return {
                'success': True,
                'output': result.stdout,
                'error': result.stderr,
                'return_code': result.returncode,
                'command': ' '.join(cmd)
            }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Java execution timed out',
                'output': None
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Java execution failed: {str(e)}",
                'output': None
            }
    
    def execute_command(self, command: str, working_dir: str = None, timeout: int = 30) -> Dict[str, Any]:
        """Execute arbitrary command"""
        try:
            self.logger.info(f"Executing command: {command}")
            
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=timeout,
                cwd=working_dir
            )
            
            return {
                'success': True,
                'output': result.stdout,
                'error': result.stderr,
                'return_code': result.returncode,
                'command': command
            }
        
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': f"Command timed out after {timeout} seconds",
                'output': None
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Command execution failed: {str(e)}",
                'output': None
            }
    
    def get_system_info(self) -> Dict[str, Any]:
        """Get system information"""
        compilers = self.detect_compilers()
        
        return {
            'platform': os.name,
            'python_version': sys.version,
            'available_compilers': {k: v for k, v in compilers.items() if v['available']},
            'working_directory': os.getcwd(),
            'temp_directory': self.temp_dir
        }
    
    def create_build_script(self, project_dir: str, build_config: Dict[str, Any]) -> str:
        """Create build script for project"""
        script_path = os.path.join(project_dir, 'build.py')
        
        with open(script_path, 'w') as f:
            f.write('#!/usr/bin/env python3\n')
            f.write('"""\n')
            f.write('Auto-generated build script\n')
            f.write('"""\n\n')
            f.write('import sys\n')
            f.write('import os\n')
            f.write('from external_worktools import ExternalWorktools\n\n')
            f.write('def main():\n')
            f.write('    worktools = ExternalWorktools()\n')
            f.write('    \n')
            
            # Add build steps based on configuration
            if 'cpp_files' in build_config:
                for cpp_file in build_config['cpp_files']:
                    f.write(f'    # Compile {cpp_file}\n')
                    f.write(f'    result = worktools.compile_cpp("{cpp_file}")\n')
                    f.write(f'    if not result["success"]:\n')
                    f.write(f'        print(f"Failed to compile {cpp_file}: {{result[\'error\']}}")\n')
                    f.write(f'        sys.exit(1)\n')
                    f.write(f'    print(f"Compiled {cpp_file} -> {{result[\'output_file\']}}")\n')
                    f.write(f'    \n')
            
            if 'python_files' in build_config:
                for py_file in build_config['python_files']:
                    f.write(f'    # Run {py_file}\n')
                    f.write(f'    result = worktools.run_python("{py_file}")\n')
                    f.write(f'    if not result["success"]:\n')
                    f.write(f'        print(f"Failed to run {py_file}: {{result[\'error\']}}")\n')
                    f.write(f'        sys.exit(1)\n')
                    f.write(f'    print(f"Output: {{result[\'output\']}}")\n')
                    f.write(f'    \n')
            
            f.write('    print("Build completed successfully!")\n\n')
            f.write('if __name__ == "__main__":\n')
            f.write('    main()\n')
        
        # Make script executable
        os.chmod(script_path, 0o755)
        
        return script_path

def main():
    """Main function for testing worktools"""
    if len(sys.argv) < 2:
        print("Usage: python external_worktools.py <command> [args...]")
        print("\nCommands:")
        print("  detect-compilers    - Detect available compilers")
        print("  compile-cpp <file>  - Compile C++ file")
        print("  run-python <file>   - Run Python file")
        print("  run-nodejs <file>   - Run Node.js file")
        print("  compile-java <file> - Compile Java file")
        print("  run-java <file>     - Run Java class file")
        print("  system-info         - Show system information")
        sys.exit(1)
    
    command = sys.argv[1]
    worktools = ExternalWorktools()
    
    if command == 'detect-compilers':
        compilers = worktools.detect_compilers()
        print("🔍 Available Compilers:")
        for name, info in compilers.items():
            if info['available']:
                print(f"   {name}: {info['name']} - {info['version']}")
    
    elif command == 'compile-cpp':
        if len(sys.argv) < 3:
            print("Usage: python external_worktools.py compile-cpp <source_file>")
            sys.exit(1)
        
        source_file = sys.argv[2]
        result = worktools.compile_cpp(source_file)
        
        if result['success']:
            print(f"✅ Compilation successful!")
            print(f"   Output: {result['output_file']}")
            print(f"   Compiler: {result['compiler']}")
        else:
            print(f"❌ Compilation failed: {result['error']}")
            sys.exit(1)
    
    elif command == 'run-python':
        if len(sys.argv) < 3:
            print("Usage: python external_worktools.py run-python <source_file>")
            sys.exit(1)
        
        source_file = sys.argv[2]
        result = worktools.run_python(source_file)
        
        if result['success']:
            print(f"✅ Python execution successful!")
            print(f"   Output: {result['output']}")
        else:
            print(f"❌ Python execution failed: {result['error']}")
            sys.exit(1)
    
    elif command == 'run-nodejs':
        if len(sys.argv) < 3:
            print("Usage: python external_worktools.py run-nodejs <source_file>")
            sys.exit(1)
        
        source_file = sys.argv[2]
        result = worktools.run_nodejs(source_file)
        
        if result['success']:
            print(f"✅ Node.js execution successful!")
            print(f"   Output: {result['output']}")
        else:
            print(f"❌ Node.js execution failed: {result['error']}")
            sys.exit(1)
    
    elif command == 'compile-java':
        if len(sys.argv) < 3:
            print("Usage: python external_worktools.py compile-java <source_file>")
            sys.exit(1)
        
        source_file = sys.argv[2]
        result = worktools.compile_java(source_file)
        
        if result['success']:
            print(f"✅ Java compilation successful!")
            print(f"   Output: {result['output_file']}")
        else:
            print(f"❌ Java compilation failed: {result['error']}")
            sys.exit(1)
    
    elif command == 'run-java':
        if len(sys.argv) < 3:
            print("Usage: python external_worktools.py run-java <class_file>")
            sys.exit(1)
        
        class_file = sys.argv[2]
        result = worktools.run_java(class_file)
        
        if result['success']:
            print(f"✅ Java execution successful!")
            print(f"   Output: {result['output']}")
        else:
            print(f"❌ Java execution failed: {result['error']}")
            sys.exit(1)
    
    elif command == 'system-info':
        info = worktools.get_system_info()
        print("🖥️ System Information:")
        print(f"   Platform: {info['platform']}")
        print(f"   Python: {info['python_version']}")
        print(f"   Working Directory: {info['working_directory']}")
        print(f"   Available Compilers: {len(info['available_compilers'])}")
        for name, details in info['available_compilers'].items():
            print(f"     {name}: {details['name']}")
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)

if __name__ == "__main__":
    main()
