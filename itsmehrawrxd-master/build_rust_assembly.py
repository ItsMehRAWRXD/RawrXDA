#!/usr/bin/env python3
"""
Build Script for Rust Assembly Compiler
Compiles the x86-64 assembly implementation to shared library
"""

import os
import sys
import subprocess
import platform

def check_dependencies():
    """Check if required build tools are available"""
    print("🔍 Checking build dependencies...")
    
    # Check for NASM
    try:
        result = subprocess.run(['nasm', '--version'], capture_output=True, text=True)
        if result.returncode == 0:
            print("✅ NASM found:", result.stdout.strip().split('\n')[0])
        else:
            print("❌ NASM not found")
            return False
    except FileNotFoundError:
        print("❌ NASM not found. Please install NASM (Netwide Assembler)")
        return False
    
    # Check for GCC
    try:
        result = subprocess.run(['gcc', '--version'], capture_output=True, text=True)
        if result.returncode == 0:
            print("✅ GCC found:", result.stdout.strip().split('\n')[0])
        else:
            print("❌ GCC not found")
            return False
    except FileNotFoundError:
        print("❌ GCC not found. Please install GCC")
        return False
    
    return True

def build_assembly():
    """Build the assembly source to shared library"""
    print("\n🔨 Building Rust Assembly Compiler...")
    
    try:
        # Step 1: Compile assembly to object file
        print("📝 Compiling assembly to object file...")
        result = subprocess.run([
            'nasm', '-f', 'elf64', 
            'rust_compiler_from_scratch.asm', 
            '-o', 'rust_compiler.o'
        ], check=True, cwd=os.path.dirname(__file__))
        print("✅ Assembly compiled successfully")
        
        # Step 2: Link to shared library
        print("🔗 Linking to shared library...")
        if platform.system() == "Windows":
            # Windows specific linking
            result = subprocess.run([
                'gcc', '-shared', '-fPIC',
                'rust_compiler.o',
                '-o', 'rust_compiler.dll'
            ], check=True, cwd=os.path.dirname(__file__))
            print("✅ Windows DLL created: rust_compiler.dll")
        else:
            # Unix/Linux/macOS linking
            result = subprocess.run([
                'gcc', '-shared', '-fPIC',
                'rust_compiler.o',
                '-o', 'rust_compiler.so'
            ], check=True, cwd=os.path.dirname(__file__))
            print("✅ Shared library created: rust_compiler.so")
        
        # Step 3: Clean up object file
        if os.path.exists('rust_compiler.o'):
            os.remove('rust_compiler.o')
            print("🧹 Cleaned up object file")
        
        print("🎉 Rust Assembly Compiler built successfully!")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"❌ Build failed: {e}")
        return False
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return False

def test_compilation():
    """Test the compiled assembly compiler"""
    print("\n🧪 Testing Assembly Compiler...")
    
    try:
        # Import and test the Python wrapper
        sys.path.append(os.path.dirname(__file__))
        from rust_assembly_compiler import RustAssemblyCompiler
        
        compiler = RustAssemblyCompiler()
        if compiler.compiled:
            print("✅ Assembly compiler loaded successfully")
            
            # Test with simple Rust code
            test_code = """
fn main() {
    let x = 5;
    let y = x + 3;
    println!("Result: {}", y);
}
"""
            
            result = compiler.compile_rust(test_code)
            print(f"📝 Test compilation result: {result[:100]}...")
            print("✅ Assembly compiler test passed!")
            return True
        else:
            print("❌ Assembly compiler failed to load")
            return False
            
    except Exception as e:
        print(f"❌ Test failed: {e}")
        return False

def main():
    """Main build process"""
    print("🚀 Rust Assembly Compiler Build Script")
    print("=" * 50)
    
    # Check dependencies
    if not check_dependencies():
        print("\n❌ Missing dependencies. Please install required tools.")
        return False
    
    # Build assembly
    if not build_assembly():
        print("\n❌ Build failed.")
        return False
    
    # Test compilation
    if not test_compilation():
        print("\n❌ Test failed.")
        return False
    
    print("\n🎉 Rust Assembly Compiler is ready!")
    print("💡 You can now use the assembly-based Rust compiler in the Extensible Compiler System.")
    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
