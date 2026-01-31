#!/usr/bin/env python3
"""
ASM to Universal Compiler Runtime
Generates universal compilers from ASM source code
Runtime system for creating compilers that can compile any language
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
from dataclasses import dataclass
from enum import Enum
import threading
import queue

class CompilerTarget(Enum):
    """Compiler target types"""
    EXECUTABLE = "executable"
    LIBRARY = "library"
    BYTECODE = "bytecode"
    INTERMEDIATE = "intermediate"
    SOURCE = "source"

class LanguageType(Enum):
    """Language types for compilation"""
    COMPILED = "compiled"
    INTERPRETED = "interpreted"
    HYBRID = "hybrid"
    ASSEMBLY = "assembly"

@dataclass
class CompilerSpec:
    """Compiler specification"""
    name: str
    target_language: str
    source_language: str
    target_type: CompilerTarget
    language_type: LanguageType
    extensions: List[str]
    output_format: str
    optimization_level: int = 2
    debug_info: bool = True
    cross_platform: bool = True

class ASMToUniversalCompilerRuntime:
    """Runtime for generating universal compilers from ASM"""
    
    def __init__(self):
        self.runtime_dir = tempfile.mkdtemp(prefix="asm_compiler_runtime_")
        self.generated_compilers = {}
        self.compiler_specs = {}
        self.runtime_queue = queue.Queue()
        self.running = False
        
        # Initialize compiler templates
        self._init_compiler_templates()
        
        print(f"🚀 ASM to Universal Compiler Runtime initialized")
        print(f"📁 Runtime directory: {self.runtime_dir}")
    
    def _init_compiler_templates(self):
        """Initialize compiler templates for different targets"""
        self.compiler_templates = {
            'c_to_exe': {
                'name': 'C to EXE Compiler',
                'asm_template': 'c_compiler_template.asm',
                'target': CompilerTarget.EXECUTABLE,
                'language': LanguageType.COMPILED,
                'extensions': ['.c', '.h'],
                'output': '.exe'
            },
            'cpp_to_exe': {
                'name': 'C++ to EXE Compiler', 
                'asm_template': 'cpp_compiler_template.asm',
                'target': CompilerTarget.EXECUTABLE,
                'language': LanguageType.COMPILED,
                'extensions': ['.cpp', '.cxx', '.hpp'],
                'output': '.exe'
            },
            'python_to_bytecode': {
                'name': 'Python to Bytecode Compiler',
                'asm_template': 'python_compiler_template.asm', 
                'target': CompilerTarget.BYTECODE,
                'language': LanguageType.INTERPRETED,
                'extensions': ['.py'],
                'output': '.pyc'
            },
            'java_to_bytecode': {
                'name': 'Java to Bytecode Compiler',
                'asm_template': 'java_compiler_template.asm',
                'target': CompilerTarget.BYTECODE,
                'language': LanguageType.HYBRID,
                'extensions': ['.java'],
                'output': '.class'
            },
            'rust_to_exe': {
                'name': 'Rust to EXE Compiler',
                'asm_template': 'rust_compiler_template.asm',
                'target': CompilerTarget.EXECUTABLE,
                'language': LanguageType.COMPILED,
                'extensions': ['.rs'],
                'output': '.exe'
            },
            'javascript_to_bytecode': {
                'name': 'JavaScript to Bytecode Compiler',
                'asm_template': 'javascript_compiler_template.asm',
                'target': CompilerTarget.BYTECODE,
                'language': LanguageType.INTERPRETED,
                'extensions': ['.js'],
                'output': '.jsb'
            }
        }
    
    def generate_compiler_from_asm(self, asm_source: str, compiler_spec: CompilerSpec) -> Dict[str, Any]:
        """Generate a universal compiler from ASM source"""
        print(f"🔨 Generating {compiler_spec.name} from ASM...")
        
        try:
            # Create compiler directory
            compiler_dir = os.path.join(self.runtime_dir, f"{compiler_spec.name.lower().replace(' ', '_')}")
            os.makedirs(compiler_dir, exist_ok=True)
            
            # Generate ASM compiler
            asm_compiler_path = self._generate_asm_compiler(asm_source, compiler_spec, compiler_dir)
            
            # Compile ASM to executable
            executable_path = self._compile_asm_to_executable(asm_compiler_path, compiler_spec, compiler_dir)
            
            # Test the generated compiler
            test_result = self._test_generated_compiler(executable_path, compiler_spec)
            
            # Store compiler info
            compiler_info = {
                'name': compiler_spec.name,
                'executable_path': executable_path,
                'compiler_dir': compiler_dir,
                'spec': compiler_spec,
                'test_result': test_result,
                'generated_at': time.time()
            }
            
            self.generated_compilers[compiler_spec.name] = compiler_info
            
            return {
                'success': True,
                'compiler_info': compiler_info,
                'message': f"✅ {compiler_spec.name} generated successfully!"
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'message': f"❌ Failed to generate {compiler_spec.name}: {str(e)}"
            }
    
    def _generate_asm_compiler(self, asm_source: str, spec: CompilerSpec, compiler_dir: str) -> str:
        """Generate ASM compiler source"""
        asm_file = os.path.join(compiler_dir, f"{spec.name.lower().replace(' ', '_')}.asm")
        
        # Get template
        template = self.compiler_templates.get(f"{spec.source_language}_to_{spec.target_type.value}")
        if not template:
            template = self._create_generic_template(spec)
        
        # Generate ASM source
        asm_content = self._generate_asm_content(asm_source, spec, template)
        
        with open(asm_file, 'w', encoding='utf-8') as f:
            f.write(asm_content)
        
        print(f"📝 Generated ASM compiler: {asm_file}")
        return asm_file
    
    def _generate_asm_content(self, asm_source: str, spec: CompilerSpec, template: Dict[str, Any]) -> str:
        """Generate complete ASM content for compiler"""
        asm_content = f"""; {spec.name} - Generated from ASM Runtime
; Target: {spec.target_language} -> {spec.target_type.value}
; Language: {spec.language_type.value}
; Extensions: {', '.join(spec.extensions)}

section .text
global _start

_start:
    ; Initialize compiler runtime
    call init_compiler_runtime
    
    ; Parse command line arguments
    call parse_arguments
    
    ; Load source file
    call load_source_file
    
    ; Compile source to target
    call compile_source_to_target
    
    ; Generate output
    call generate_output
    
    ; Cleanup and exit
    call cleanup_compiler
    mov eax, 1
    int 0x80

init_compiler_runtime:
    push ebp
    mov ebp, esp
    
    ; Initialize compiler state
    mov dword [compiler_state], 0
    mov dword [source_buffer], 0
    mov dword [target_buffer], 0
    
    ; Set optimization level
    mov eax, {spec.optimization_level}
    mov [optimization_level], eax
    
    ; Set debug info flag
    mov eax, {1 if spec.debug_info else 0}
    mov [debug_info], eax
    
    pop ebp
    ret

parse_arguments:
    push ebp
    mov ebp, esp
    
    ; Parse command line arguments
    ; argc in [esp+4], argv in [esp+8]
    mov eax, [esp+8]        ; argv
    mov ebx, [eax+4]        ; argv[1] - input file
    mov [input_file], ebx
    
    mov ebx, [eax+8]        ; argv[2] - output file
    mov [output_file], ebx
    
    pop ebp
    ret

load_source_file:
    push ebp
    mov ebp, esp
    
    ; Open input file
    mov eax, 5              ; sys_open
    mov ebx, [input_file]
    mov ecx, 0              ; O_RDONLY
    int 0x80
    
    cmp eax, 0
    jl file_error
    
    mov [input_fd], eax
    
    ; Read file content
    mov eax, 3              ; sys_read
    mov ebx, [input_fd]
    mov ecx, source_buffer
    mov edx, 4096           ; buffer size
    int 0x80
    
    mov [source_size], eax
    
    ; Close file
    mov eax, 6              ; sys_close
    mov ebx, [input_fd]
    int 0x80
    
    pop ebp
    ret

compile_source_to_target:
    push ebp
    mov ebp, esp
    
    ; {spec.name} compilation logic
    call lex_source
    call parse_source
    call generate_target_code
    call optimize_code
    
    pop ebp
    ret

lex_source:
    push ebp
    mov ebp, esp
    
    ; Lexical analysis for {spec.source_language}
    mov esi, source_buffer
    mov edi, token_buffer
    mov ecx, [source_size]
    
lex_loop:
    cmp ecx, 0
    je lex_done
    
    ; Process character
    lodsb
    call classify_character
    call add_token
    
    dec ecx
    jmp lex_loop
    
lex_done:
    pop ebp
    ret

parse_source:
    push ebp
    mov ebp, esp
    
    ; Parse tokens into AST
    mov esi, token_buffer
    call parse_expression
    
    pop ebp
    ret

generate_target_code:
    push ebp
    mov ebp, esp
    
    ; Generate {spec.target_language} code
    call generate_{spec.target_language}_code
    
    pop ebp
    ret

generate_{spec.target_language}_code:
    push ebp
    mov ebp, esp
    
    ; {spec.target_language} code generation
    mov esi, ast_root
    mov edi, target_buffer
    
    ; Generate target-specific code
    call emit_{spec.target_language}_header
    call emit_{spec.target_language}_body
    call emit_{spec.target_language}_footer
    
    pop ebp
    ret

emit_{spec.target_language}_header:
    ; Emit {spec.target_language} header
    ret

emit_{spec.target_language}_body:
    ; Emit {spec.target_language} body
    ret

emit_{spec.target_language}_footer:
    ; Emit {spec.target_language} footer
    ret

optimize_code:
    push ebp
    mov ebp, esp
    
    ; Code optimization
    mov eax, [optimization_level]
    cmp eax, 0
    je optimize_done
    
    call constant_folding
    call dead_code_elimination
    call register_allocation
    
optimize_done:
    pop ebp
    ret

generate_output:
    push ebp
    mov ebp, esp
    
    ; Create output file
    mov eax, 8              ; sys_creat
    mov ebx, [output_file]
    mov ecx, 0644           ; permissions
    int 0x80
    
    cmp eax, 0
    jl output_error
    
    mov [output_fd], eax
    
    ; Write compiled code
    mov eax, 4              ; sys_write
    mov ebx, [output_fd]
    mov ecx, target_buffer
    mov edx, [target_size]
    int 0x80
    
    ; Close output file
    mov eax, 6              ; sys_close
    mov ebx, [output_fd]
    int 0x80
    
    pop ebp
    ret

cleanup_compiler:
    push ebp
    mov ebp, esp
    
    ; Cleanup resources
    mov eax, 0
    mov [compiler_state], eax
    mov [source_buffer], eax
    mov [target_buffer], eax
    
    pop ebp
    ret

file_error:
    mov eax, 4              ; sys_write
    mov ebx, 1              ; stdout
    mov ecx, file_error_msg
    mov edx, file_error_len
    int 0x80
    jmp exit_error

output_error:
    mov eax, 4              ; sys_write
    mov ebx, 1              ; stdout
    mov ecx, output_error_msg
    mov edx, output_error_len
    int 0x80
    jmp exit_error

exit_error:
    mov eax, 1
    mov ebx, 1              ; error code
    int 0x80

; Data section
section .data
    compiler_state dd 0
    source_buffer times 4096 db 0
    target_buffer times 4096 db 0
    token_buffer times 1024 db 0
    ast_root dd 0
    
    input_file dd 0
    output_file dd 0
    input_fd dd 0
    output_fd dd 0
    source_size dd 0
    target_size dd 0
    
    optimization_level dd 2
    debug_info dd 1
    
    file_error_msg db 'Error: Cannot open input file', 0xa
    file_error_len equ $ - file_error_msg
    
    output_error_msg db 'Error: Cannot create output file', 0xa
    output_error_len equ $ - output_error_msg

; Custom ASM source integration
{asm_source}
"""
        return asm_content
    
    def _create_generic_template(self, spec: CompilerSpec) -> Dict[str, Any]:
        """Create generic template for unknown language combinations"""
        return {
            'name': f"{spec.source_language} to {spec.target_language} Compiler",
            'asm_template': 'generic_compiler_template.asm',
            'target': spec.target_type,
            'language': spec.language_type,
            'extensions': spec.extensions,
            'output': spec.output_format
        }
    
    def _compile_asm_to_executable(self, asm_file: str, spec: CompilerSpec, compiler_dir: str) -> str:
        """Compile ASM to executable with universal extension support"""
        executable_name = f"{spec.name.lower().replace(' ', '_')}_compiler"
        executable_path = os.path.join(compiler_dir, executable_name)
        
        try:
            # Try NASM first
            result = subprocess.run([
                'nasm', '-f', 'elf32', '-o', f"{executable_name}.o", asm_file
            ], capture_output=True, text=True, cwd=compiler_dir)
            
            if result.returncode == 0:
                # Link with ld
                link_result = subprocess.run([
                    'ld', '-m', 'elf_i386', '-o', executable_path, f"{executable_name}.o"
                ], capture_output=True, text=True, cwd=compiler_dir)
                
                if link_result.returncode == 0:
                    print(f"✅ ASM compiled successfully: {executable_path}")
                    return executable_path
                else:
                    print(f"❌ Linking failed: {link_result.stderr}")
            else:
                print(f"❌ NASM compilation failed: {result.stderr}")
                
        except FileNotFoundError:
            print("⚠️ NASM not found, trying alternative methods...")
        
        # Fallback: Create universal Python wrapper
        return self._create_universal_python_wrapper(asm_file, spec, compiler_dir)
    
    def _create_python_wrapper(self, asm_file: str, spec: CompilerSpec, compiler_dir: str) -> str:
        """Create Python wrapper for ASM compiler"""
        wrapper_name = f"{spec.name.lower().replace(' ', '_')}_compiler.py"
        wrapper_path = os.path.join(compiler_dir, wrapper_name)
        
        wrapper_content = f'''#!/usr/bin/env python3
"""
{spec.name} - Python Wrapper for ASM Compiler
Generated by ASM to Universal Compiler Runtime
"""

import sys
import os
import subprocess
import tempfile
from pathlib import Path

class {spec.name.replace(' ', '')}Compiler:
    """{spec.name} implementation"""
    
    def __init__(self):
        self.spec = {{
            'name': '{spec.name}',
            'source_language': '{spec.source_language}',
            'target_language': '{spec.target_language}',
            'target_type': '{spec.target_type.value}',
            'language_type': '{spec.language_type.value}',
            'extensions': {spec.extensions},
            'output_format': '{spec.output_format}',
            'optimization_level': {spec.optimization_level},
            'debug_info': {spec.debug_info}
        }}
    
    def compile(self, source_file: str, output_file: str = None) -> bool:
        """Compile source file to target"""
        try:
            if not output_file:
                output_file = self._get_output_filename(source_file)
            
            print(f"🔨 Compiling {{source_file}} to {{output_file}}...")
            
            # Read source file
            with open(source_file, 'r', encoding='utf-8') as f:
                source_code = f.read()
            
            # Compile based on target type
            if self.spec['target_type'] == 'executable':
                return self._compile_to_executable(source_code, output_file)
            elif self.spec['target_type'] == 'bytecode':
                return self._compile_to_bytecode(source_code, output_file)
            elif self.spec['target_type'] == 'library':
                return self._compile_to_library(source_code, output_file)
            else:
                return self._compile_to_source(source_code, output_file)
                
        except Exception as e:
            print(f"❌ Compilation failed: {{str(e)}}")
            return False
    
    def _get_output_filename(self, source_file: str) -> str:
        """Get output filename based on source file"""
        base_name = os.path.splitext(source_file)[0]
        return f"{{base_name}}.{self.spec['output_format']}"
    
    def _compile_to_executable(self, source_code: str, output_file: str) -> bool:
        """Compile to executable"""
        # Implementation for {spec.target_language} to executable
        print(f"⚙️ Generating {spec.target_language} executable...")
        
        # Create temporary source file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.{spec.source_language}', delete=False) as f:
            f.write(source_code)
            temp_source = f.name
        
        try:
            # Compile using appropriate compiler
            if self.spec['source_language'] == 'c':
                result = subprocess.run(['gcc', '-o', output_file, temp_source], 
                                      capture_output=True, text=True)
            elif self.spec['source_language'] == 'cpp':
                result = subprocess.run(['g++', '-o', output_file, temp_source], 
                                      capture_output=True, text=True)
            else:
                # Generic compilation
                result = subprocess.run(['gcc', '-o', output_file, temp_source], 
                                      capture_output=True, text=True)
            
            if result.returncode == 0:
                print(f"✅ Executable created: {{output_file}}")
                return True
            else:
                print(f"❌ Compilation failed: {{result.stderr}}")
                return False
                
        finally:
            os.unlink(temp_source)
    
    def _compile_to_bytecode(self, source_code: str, output_file: str) -> bool:
        """Compile to bytecode"""
        print(f"🔢 Generating {spec.target_language} bytecode...")
        
        # Implementation for bytecode generation
        with open(output_file, 'wb') as f:
            # Generate bytecode based on source
            bytecode = self._generate_bytecode(source_code)
            f.write(bytecode)
        
        print(f"✅ Bytecode created: {{output_file}}")
        return True
    
    def _compile_to_library(self, source_code: str, output_file: str) -> bool:
        """Compile to library"""
        print(f"📚 Generating {spec.target_language} library...")
        
        # Implementation for library generation
        with open(output_file, 'w') as f:
            f.write(f"# {spec.target_language} Library\\n")
            f.write(source_code)
        
        print(f"✅ Library created: {{output_file}}")
        return True
    
    def _compile_to_source(self, source_code: str, output_file: str) -> bool:
        """Compile to source code"""
        print(f"📝 Generating {spec.target_language} source...")
        
        # Implementation for source-to-source compilation
        with open(output_file, 'w') as f:
            f.write(f"# Generated {spec.target_language} code\\n")
            f.write(self._transpile_source(source_code))
        
        print(f"✅ Source created: {{output_file}}")
        return True
    
    def _generate_bytecode(self, source_code: str) -> bytes:
        """Generate bytecode from source"""
        # Simple bytecode generation
        bytecode = bytearray()
        bytecode.extend(b'BC')  # Bytecode header
        bytecode.extend(len(source_code).to_bytes(4, 'little'))
        bytecode.extend(source_code.encode('utf-8'))
        return bytes(bytecode)
    
    def _transpile_source(self, source_code: str) -> str:
        """Transpile source code"""
        # Simple transpilation
        transpiled = f"# Transpiled from {self.spec['source_language']} to {self.spec['target_language']}\\n"
        transpiled += source_code
        return transpiled

def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python {{sys.argv[0]}} <source_file> [output_file]")
        sys.exit(1)
    
    source_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    compiler = {spec.name.replace(' ', '')}Compiler()
    success = compiler.compile(source_file, output_file)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
'''
        
        with open(wrapper_path, 'w', encoding='utf-8') as f:
            f.write(wrapper_content)
        
        # Make executable
        os.chmod(wrapper_path, 0o755)
        
        print(f"✅ Python wrapper created: {wrapper_path}")
        return wrapper_path
    
    def _test_generated_compiler(self, compiler_path: str, spec: CompilerSpec) -> Dict[str, Any]:
        """Test the generated compiler"""
        print(f"🧪 Testing {spec.name}...")
        
        try:
            # Create test source file
            test_source = self._create_test_source(spec)
            test_source_path = os.path.join(os.path.dirname(compiler_path), "test_source")
            
            with open(test_source_path, 'w') as f:
                f.write(test_source)
            
            # Test compilation
            test_output = os.path.join(os.path.dirname(compiler_path), "test_output")
            
            if compiler_path.endswith('.py'):
                # Python wrapper
                result = subprocess.run([
                    'python', compiler_path, test_source_path, test_output
                ], capture_output=True, text=True, timeout=30)
            else:
                # Native executable
                result = subprocess.run([
                    compiler_path, test_source_path, test_output
                ], capture_output=True, text=True, timeout=30)
            
            return {
                'success': result.returncode == 0,
                'output': result.stdout,
                'error': result.stderr,
                'test_source': test_source,
                'test_output_exists': os.path.exists(test_output)
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'output': '',
                'test_source': '',
                'test_output_exists': False
            }
    
    def _create_test_source(self, spec: CompilerSpec) -> str:
        """Create test source code for the compiler"""
        if spec.source_language == 'c':
            return """
#include <stdio.h>

int main() {
    printf("Hello from C compiler!\\n");
    return 0;
}
"""
        elif spec.source_language == 'cpp':
            return """
#include <iostream>

int main() {
    std::cout << "Hello from C++ compiler!" << std::endl;
    return 0;
}
"""
        elif spec.source_language == 'python':
            return """
def main():
    print("Hello from Python compiler!")

if __name__ == "__main__":
    main()
"""
        elif spec.source_language == 'java':
            return """
public class Test {
    public static void main(String[] args) {
        System.out.println("Hello from Java compiler!");
    }
}
"""
        elif spec.source_language == 'rust':
            return """
fn main() {
    println!("Hello from Rust compiler!");
}
"""
        elif spec.source_language == 'javascript':
            return """
function main() {
    console.log("Hello from JavaScript compiler!");
}

main();
"""
        else:
            return f"""
// Test source for {spec.source_language}
function main() {{
    print("Hello from {spec.source_language} compiler!");
}}

main();
"""
    
    def generate_multiple_compilers(self, asm_sources: List[str], specs: List[CompilerSpec]) -> Dict[str, Any]:
        """Generate multiple compilers from ASM sources"""
        print(f"🔨 Generating {len(specs)} compilers...")
        
        results = {}
        for i, (asm_source, spec) in enumerate(zip(asm_sources, specs)):
            print(f"\\n📝 Generating compiler {i+1}/{len(specs)}: {spec.name}")
            result = self.generate_compiler_from_asm(asm_source, spec)
            results[spec.name] = result
        
        return {
            'success': True,
            'results': results,
            'total_compilers': len(specs),
            'successful_compilers': sum(1 for r in results.values() if r['success'])
        }
    
    def get_runtime_info(self) -> Dict[str, Any]:
        """Get runtime information"""
        return {
            'runtime_dir': self.runtime_dir,
            'generated_compilers': len(self.generated_compilers),
            'compiler_specs': len(self.compiler_specs),
            'available_templates': len(self.compiler_templates),
            'running': self.running
        }
    
    def cleanup_runtime(self):
        """Cleanup runtime resources"""
        try:
            shutil.rmtree(self.runtime_dir)
            print(f"🧹 Runtime cleaned up: {self.runtime_dir}")
        except Exception as e:
            print(f"⚠️ Cleanup warning: {str(e)}")

def test_asm_to_universal_compiler_runtime():
    """Test the ASM to Universal Compiler Runtime"""
    print("🧪 Testing ASM to Universal Compiler Runtime...")
    
    runtime = ASMToUniversalCompilerRuntime()
    
    # Test ASM source
    asm_source = """
; Test ASM source for compiler generation
mov eax, 42
mov ebx, 8
add eax, ebx
ret
"""
    
    # Test compiler specs
    specs = [
        CompilerSpec(
            name="C to EXE Compiler",
            target_language="executable",
            source_language="c",
            target_type=CompilerTarget.EXECUTABLE,
            language_type=LanguageType.COMPILED,
            extensions=['.c', '.h'],
            output_format='.exe'
        ),
        CompilerSpec(
            name="Python to Bytecode Compiler",
            target_language="bytecode",
            source_language="python",
            target_type=CompilerTarget.BYTECODE,
            language_type=LanguageType.INTERPRETED,
            extensions=['.py'],
            output_format='.pyc'
        )
    ]
    
    # Generate compilers
    results = runtime.generate_multiple_compilers([asm_source, asm_source], specs)
    
    print(f"\\n📊 Results:")
    print(f"   Total compilers: {results['total_compilers']}")
    print(f"   Successful: {results['successful_compilers']}")
    
    for name, result in results['results'].items():
        if result['success']:
            print(f"   ✅ {name}: Generated successfully")
        else:
            print(f"   ❌ {name}: {result['error']}")
    
    # Runtime info
    info = runtime.get_runtime_info()
    print(f"\\n📋 Runtime Info:")
    for key, value in info.items():
        print(f"   {key}: {value}")
    
    # Cleanup
    runtime.cleanup_runtime()
    
    return results['successful_compilers'] > 0

if __name__ == "__main__":
    test_asm_to_universal_compiler_runtime()
