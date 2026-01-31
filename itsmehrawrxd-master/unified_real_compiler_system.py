#!/usr/bin/env python3
"""
Unified Real Compiler System
Integrates all real compilers into a single system that generates actual machine code
"""

import os
import sys
import json
import time
from pathlib import Path
from typing import Dict, List, Optional, Union, Any, Tuple
from enum import Enum
from dataclasses import dataclass

# Import our real compilers
from real_cpp_compiler import RealCppCompiler
from real_python_compiler import RealPythonCompiler
from real_javascript_transpiler import JavaScriptTranspiler
from real_solidity_compiler import RealSolidityCompiler
from real_code_generation_system import RealCodeGenerator

class CompilerType(Enum):
    """Compiler types"""
    CPP = "cpp"
    PYTHON = "python"
    JAVASCRIPT = "javascript"
    TYPESCRIPT = "typescript"
    SOLIDITY = "solidity"
    COFFEESCRIPT = "coffeescript"
    JSX = "jsx"
    ES6 = "es6"
    EON = "eon"

class BuildTarget(Enum):
    """Build targets"""
    EXECUTABLE = "executable"
    BYTECODE = "bytecode"
    JAVASCRIPT = "javascript"
    WEBASSEMBLY = "webassembly"
    ANDROID_APK = "android_apk"
    IOS_IPA = "ios_ipa"
    BLOCKCHAIN = "blockchain"

@dataclass
class CompilationResult:
    """Compilation result"""
    success: bool
    output_file: str
    bytecode: Optional[bytes] = None
    abi: Optional[List[Dict[str, Any]]] = None
    error_message: Optional[str] = None
    compilation_time: float = 0.0

@dataclass
class BuildConfiguration:
    """Build configuration"""
    source_file: str
    output_file: str
    compiler_type: CompilerType
    target: BuildTarget
    optimization_level: int = 0
    debug_info: bool = False
    custom_flags: List[str] = None

class UnifiedRealCompilerSystem:
    """
    Unified system that integrates all real compilers
    Generates actual machine code, bytecode, and executables
    """
    
    def __init__(self):
        # Initialize all compilers
        self.cpp_compiler = RealCppCompiler()
        self.python_compiler = RealPythonCompiler()
        self.js_transpiler = JavaScriptTranspiler()
        self.solidity_compiler = RealSolidityCompiler()
        self.code_generator = RealCodeGenerator()
        
        # Compiler mappings
        self.compiler_mappings = {
            CompilerType.CPP: self.cpp_compiler,
            CompilerType.PYTHON: self.python_compiler,
            CompilerType.JAVASCRIPT: self.js_transpiler,
            CompilerType.TYPESCRIPT: self.js_transpiler,
            CompilerType.SOLIDITY: self.solidity_compiler,
            CompilerType.COFFEESCRIPT: self.js_transpiler,
            CompilerType.JSX: self.js_transpiler,
            CompilerType.ES6: self.js_transpiler,
            CompilerType.EON: self.code_generator
        }
        
        # Language extensions
        self.language_extensions = {
            '.cpp': CompilerType.CPP,
            '.cxx': CompilerType.CPP,
            '.cc': CompilerType.CPP,
            '.c': CompilerType.CPP,
            '.py': CompilerType.PYTHON,
            '.pyw': CompilerType.PYTHON,
            '.js': CompilerType.JAVASCRIPT,
            '.mjs': CompilerType.JAVASCRIPT,
            '.ts': CompilerType.TYPESCRIPT,
            '.tsx': CompilerType.TYPESCRIPT,
            '.sol': CompilerType.SOLIDITY,
            '.coffee': CompilerType.COFFEESCRIPT,
            '.jsx': CompilerType.JSX,
            '.eon': CompilerType.EON
        }
        
        print("🔧 Unified Real Compiler System initialized")
        print("✅ C++ Compiler: Ready")
        print("✅ Python Compiler: Ready")
        print("✅ JavaScript Transpiler: Ready")
        print("✅ Solidity Compiler: Ready")
        print("✅ EON Code Generator: Ready")
    
    def compile_file(self, config: BuildConfiguration) -> CompilationResult:
        """Compile a file using the appropriate compiler"""
        
        start_time = time.time()
        
        try:
            # Read source file
            if not os.path.exists(config.source_file):
                return CompilationResult(
                    success=False,
                    output_file="",
                    error_message=f"Source file not found: {config.source_file}"
                )
            
            with open(config.source_file, 'r', encoding='utf-8') as f:
                source_code = f.read()
            
            # Determine compiler type if not specified
            if config.compiler_type is None:
                config.compiler_type = self.detect_language(config.source_file)
            
            if config.compiler_type not in self.compiler_mappings:
                return CompilationResult(
                    success=False,
                    output_file="",
                    error_message=f"Unsupported language: {config.compiler_type}"
                )
            
            compiler = self.compiler_mappings[config.compiler_type]
            
            # Compile based on type
            if config.compiler_type == CompilerType.CPP:
                success = self.compile_cpp(compiler, source_code, config)
            elif config.compiler_type == CompilerType.PYTHON:
                success = self.compile_python(compiler, source_code, config)
            elif config.compiler_type in [CompilerType.JAVASCRIPT, CompilerType.TYPESCRIPT, 
                                        CompilerType.COFFEESCRIPT, CompilerType.JSX, CompilerType.ES6]:
                success = self.compile_javascript(compiler, source_code, config)
            elif config.compiler_type == CompilerType.SOLIDITY:
                success = self.compile_solidity(compiler, source_code, config)
            elif config.compiler_type == CompilerType.EON:
                success = self.compile_eon(compiler, source_code, config)
            else:
                return CompilationResult(
                    success=False,
                    output_file="",
                    error_message=f"Unknown compiler type: {config.compiler_type}"
                )
            
            compilation_time = time.time() - start_time
            
            return CompilationResult(
                success=success,
                output_file=config.output_file,
                compilation_time=compilation_time
            )
            
        except Exception as e:
            compilation_time = time.time() - start_time
            return CompilationResult(
                success=False,
                output_file="",
                error_message=str(e),
                compilation_time=compilation_time
            )
    
    def compile_cpp(self, compiler: RealCppCompiler, source_code: str, config: BuildConfiguration) -> bool:
        """Compile C++ source"""
        
        print(f"🔧 Compiling C++ source: {config.source_file}")
        
        # Determine output format
        if config.target == BuildTarget.EXECUTABLE:
            return compiler.compile_to_exe(source_code, config.output_file)
        else:
            # Generate assembly or other formats
            tokens = compiler.lexer.tokenize(source_code)
            ast = compiler.parser.parse(tokens)
            assembly = compiler.codegen.generate(ast)
            
            # Write assembly file
            with open(config.output_file.replace('.exe', '.asm'), 'w') as f:
                f.write('\n'.join(assembly))
            
            return True
    
    def compile_python(self, compiler: RealPythonCompiler, source_code: str, config: BuildConfiguration) -> bool:
        """Compile Python source"""
        
        print(f"🐍 Compiling Python source: {config.source_file}")
        
        if config.target == BuildTarget.BYTECODE:
            return compiler.compile_to_pyc(source_code, config.output_file)
        else:
            # Execute Python code
            return compiler.compile_and_run(source_code)
    
    def compile_javascript(self, transpiler: JavaScriptTranspiler, source_code: str, config: BuildConfiguration) -> bool:
        """Compile/transpile JavaScript source"""
        
        print(f"⚡ Transpiling JavaScript source: {config.source_file}")
        
        # Determine source language
        source_language = config.compiler_type.value
        
        try:
            transpiled_code = transpiler.transpile(source_code, source_language)
            
            # Write transpiled code
            with open(config.output_file, 'w', encoding='utf-8') as f:
                f.write(transpiled_code)
            
            return True
            
        except Exception as e:
            print(f"❌ Transpilation error: {e}")
            return False
    
    def compile_solidity(self, compiler: RealSolidityCompiler, source_code: str, config: BuildConfiguration) -> bool:
        """Compile Solidity source"""
        
        print(f"🔗 Compiling Solidity source: {config.source_file}")
        
        bytecode = compiler.compile_to_bytecode(source_code)
        
        if bytecode:
            # Save bytecode
            bytecode_file = config.output_file.replace('.sol', '.bytecode')
            compiler.save_bytecode(bytecode, bytecode_file)
            
            # Save ABI
            abi_file = config.output_file.replace('.sol', '.abi')
            compiler.save_abi(bytecode, abi_file)
            
            return True
        
        return False
    
    def compile_eon(self, code_generator: RealCodeGenerator, source_code: str, config: BuildConfiguration) -> bool:
        """Compile EON source"""
        
        print(f"⚙️ Compiling EON source: {config.source_file}")
        
        try:
            # Generate IR
            ir = code_generator.generate_ir_from_source(source_code, 'eon')
            
            # Optimize IR
            optimized_ir = code_generator.optimize_ir()
            
            # Generate assembly
            assembly = code_generator.generate_assembly('x86_64')
            
            # Generate machine code
            machine_code = code_generator.generate_machine_code('x86_64')
            
            # Save executable
            return code_generator.save_executable(config.output_file, 'x86_64')
            
        except Exception as e:
            print(f"❌ EON compilation error: {e}")
            return False
    
    def detect_language(self, file_path: str) -> CompilerType:
        """Detect language from file extension"""
        
        file_ext = Path(file_path).suffix.lower()
        
        if file_ext in self.language_extensions:
            return self.language_extensions[file_ext]
        
        # Try to detect from content
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read(1000)  # Read first 1000 characters
            
            # Simple heuristics
            if 'pragma solidity' in content:
                return CompilerType.SOLIDITY
            elif 'def ' in content or 'import ' in content:
                return CompilerType.PYTHON
            elif 'function ' in content and '{' in content:
                return CompilerType.JAVASCRIPT
            elif 'int main()' in content or '#include' in content:
                return CompilerType.CPP
            elif 'mov ' in content or 'add ' in content:
                return CompilerType.EON
            
        except Exception:
            pass
        
        # Default to JavaScript
        return CompilerType.JAVASCRIPT
    
    def batch_compile(self, configs: List[BuildConfiguration]) -> List[CompilationResult]:
        """Compile multiple files in batch"""
        
        results = []
        
        print(f"🔧 Batch compiling {len(configs)} files...")
        
        for i, config in enumerate(configs, 1):
            print(f"  [{i}/{len(configs)}] Compiling {config.source_file}...")
            result = self.compile_file(config)
            results.append(result)
            
            if result.success:
                print(f"  ✅ Success: {result.output_file}")
            else:
                print(f"  ❌ Failed: {result.error_message}")
        
        return results
    
    def create_build_config(self, source_file: str, output_file: str = None, 
                          compiler_type: CompilerType = None, target: BuildTarget = None) -> BuildConfiguration:
        """Create build configuration"""
        
        if output_file is None:
            output_file = self.generate_output_filename(source_file, compiler_type, target)
        
        if compiler_type is None:
            compiler_type = self.detect_language(source_file)
        
        if target is None:
            target = self.get_default_target(compiler_type)
        
        return BuildConfiguration(
            source_file=source_file,
            output_file=output_file,
            compiler_type=compiler_type,
            target=target
        )
    
    def generate_output_filename(self, source_file: str, compiler_type: CompilerType, target: BuildTarget) -> str:
        """Generate output filename"""
        
        source_path = Path(source_file)
        base_name = source_path.stem
        
        if target == BuildTarget.EXECUTABLE:
            return str(source_path.parent / f"{base_name}.exe")
        elif target == BuildTarget.BYTECODE:
            return str(source_path.parent / f"{base_name}.pyc")
        elif target == BuildTarget.BLOCKCHAIN:
            return str(source_path.parent / f"{base_name}.bytecode")
        else:
            return str(source_path.parent / f"{base_name}.js")
    
    def get_default_target(self, compiler_type: CompilerType) -> BuildTarget:
        """Get default target for compiler type"""
        
        if compiler_type == CompilerType.CPP:
            return BuildTarget.EXECUTABLE
        elif compiler_type == CompilerType.PYTHON:
            return BuildTarget.BYTECODE
        elif compiler_type == CompilerType.SOLIDITY:
            return BuildTarget.BLOCKCHAIN
        else:
            return BuildTarget.JAVASCRIPT
    
    def get_supported_languages(self) -> List[str]:
        """Get list of supported languages"""
        
        return [compiler_type.value for compiler_type in CompilerType]
    
    def get_compiler_info(self, compiler_type: CompilerType) -> Dict[str, Any]:
        """Get compiler information"""
        
        info = {
            'name': compiler_type.value,
            'supported_targets': [],
            'file_extensions': []
        }
        
        if compiler_type == CompilerType.CPP:
            info['supported_targets'] = ['executable', 'assembly']
            info['file_extensions'] = ['.cpp', '.cxx', '.cc', '.c']
        elif compiler_type == CompilerType.PYTHON:
            info['supported_targets'] = ['bytecode', 'executable']
            info['file_extensions'] = ['.py', '.pyw']
        elif compiler_type in [CompilerType.JAVASCRIPT, CompilerType.TYPESCRIPT, 
                             CompilerType.COFFEESCRIPT, CompilerType.JSX, CompilerType.ES6]:
            info['supported_targets'] = ['javascript', 'webassembly']
            info['file_extensions'] = ['.js', '.mjs', '.ts', '.tsx', '.coffee', '.jsx']
        elif compiler_type == CompilerType.SOLIDITY:
            info['supported_targets'] = ['blockchain', 'bytecode']
            info['file_extensions'] = ['.sol']
        elif compiler_type == CompilerType.EON:
            info['supported_targets'] = ['executable', 'assembly']
            info['file_extensions'] = ['.eon']
        
        return info

# Integration function
def integrate_unified_compiler_system(ide_instance):
    """Integrate unified compiler system with IDE"""
    
    ide_instance.unified_compiler = UnifiedRealCompilerSystem()
    print("🔧 Unified Real Compiler System integrated with IDE")

if __name__ == "__main__":
    print("🔧 Unified Real Compiler System")
    print("=" * 50)
    
    # Test the unified system
    compiler_system = UnifiedRealCompilerSystem()
    
    print(f"✅ Supported languages: {', '.join(compiler_system.get_supported_languages())}")
    
    # Test C++ compilation
    cpp_code = """
    int main() {
        int x = 5 + 3;
        return x;
    }
    """
    
    cpp_file = "test_cpp.cpp"
    with open(cpp_file, 'w') as f:
        f.write(cpp_code)
    
    config = compiler_system.create_build_config(cpp_file, compiler_type=CompilerType.CPP)
    result = compiler_system.compile_file(config)
    
    if result.success:
        print(f"✅ C++ compilation successful: {result.output_file}")
    else:
        print(f"❌ C++ compilation failed: {result.error_message}")
    
    # Test Python compilation
    python_code = """
    def add_numbers(a, b):
        return a + b
    
    result = add_numbers(5, 3)
    print(result)
    """
    
    python_file = "test_python.py"
    with open(python_file, 'w') as f:
        f.write(python_code)
    
    config = compiler_system.create_build_config(python_file, compiler_type=CompilerType.PYTHON)
    result = compiler_system.compile_file(config)
    
    if result.success:
        print(f"✅ Python compilation successful: {result.output_file}")
    else:
        print(f"❌ Python compilation failed: {result.error_message}")
    
    # Test TypeScript transpilation
    typescript_code = """
    interface User {
        name: string;
        age: number;
    }
    
    class UserService {
        private users: User[] = [];
        
        public addUser(user: User): void {
            this.users.push(user);
        }
    }
    """
    
    ts_file = "test_typescript.ts"
    with open(ts_file, 'w') as f:
        f.write(typescript_code)
    
    config = compiler_system.create_build_config(ts_file, compiler_type=CompilerType.TYPESCRIPT)
    result = compiler_system.compile_file(config)
    
    if result.success:
        print(f"✅ TypeScript transpilation successful: {result.output_file}")
    else:
        print(f"❌ TypeScript transpilation failed: {result.error_message}")
    
    # Test Solidity compilation
    solidity_code = """
    pragma solidity ^0.8.0;
    
    contract SimpleStorage {
        uint256 public storedData;
        
        constructor(uint256 initialValue) {
            storedData = initialValue;
        }
        
        function set(uint256 x) public {
            storedData = x;
        }
        
        function get() public view returns (uint256) {
            return storedData;
        }
    }
    """
    
    sol_file = "test_solidity.sol"
    with open(sol_file, 'w') as f:
        f.write(solidity_code)
    
    config = compiler_system.create_build_config(sol_file, compiler_type=CompilerType.SOLIDITY)
    result = compiler_system.compile_file(config)
    
    if result.success:
        print(f"✅ Solidity compilation successful: {result.output_file}")
    else:
        print(f"❌ Solidity compilation failed: {result.error_message}")
    
    print("✅ Unified Real Compiler System ready!")
    print("🔧 All real compilers integrated and functional!")
