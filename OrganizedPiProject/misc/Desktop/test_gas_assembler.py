#!/usr/bin/env python3
"""
Test gASM Pure Python Assembler
"""

from gas_assembler import gASMCompiler

def test_simple_assembly():
    """Test with simple assembly code"""
    
    print("🔧 Testing gASM Pure Python Assembler")
    print("=" * 50)
    
    # Simple assembly test
    simple_asm = """
section .text
global _start

_start:
    mov rax, 42
    mov rbx, 10
    add rax, rbx
    ret
"""
    
    compiler = gASMCompiler()
    success = compiler.compile(simple_asm, "test_simple")
    
    if success:
        print("✅ gASM test successful!")
        return True
    else:
        print("❌ gASM test failed!")
        return False

def test_rust_compiler_asm():
    """Test with the rust compiler assembly"""
    
    print("\n🔧 Testing gASM with Rust Compiler Assembly")
    print("=" * 50)
    
    try:
        with open("rust_compiler_from_scratch.asm", "r") as f:
            rust_asm = f.read()
        
        compiler = gASMCompiler()
        success = compiler.compile(rust_asm, "rust_compiler")
        
        if success:
            print("✅ Rust compiler assembly compiled successfully!")
            return True
        else:
            print("❌ Rust compiler assembly compilation failed!")
            return False
            
    except FileNotFoundError:
        print("❌ rust_compiler_from_scratch.asm not found")
        return False

if __name__ == "__main__":
    print("🚀 gASM Pure Python Assembler Test Suite")
    print("=" * 60)
    
    # Test 1: Simple assembly
    test1_success = test_simple_assembly()
    
    # Test 2: Rust compiler assembly
    test2_success = test_rust_compiler_asm()
    
    # Summary
    print("\n" + "=" * 60)
    print("📊 TEST RESULTS")
    print("=" * 60)
    print(f"Simple Assembly Test: {'✅ PASS' if test1_success else '❌ FAIL'}")
    print(f"Rust Compiler Test: {'✅ PASS' if test2_success else '❌ FAIL'}")
    
    if test1_success and test2_success:
        print("\n🎉 All tests passed! gASM is working!")
    else:
        print("\n⚠️ Some tests failed. Check the output above.")
