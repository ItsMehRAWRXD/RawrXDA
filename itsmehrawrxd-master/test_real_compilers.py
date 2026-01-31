#!/usr/bin/env python3
"""
Test Real Compilers
Quick test to verify all real compilers work
"""

import sys
import os
from pathlib import Path

def test_imports():
    """Test importing all real compilers"""
    
    print("🔧 Testing Real Compiler Imports...")
    print("=" * 50)
    
    try:
        # Test C++ Compiler
        print("📝 Testing C++ Compiler import...")
        from real_cpp_compiler import RealCppCompiler
        cpp_compiler = RealCppCompiler()
        print("✅ C++ Compiler: OK")
        
        # Test Python Compiler
        print("📝 Testing Python Compiler import...")
        from real_python_compiler import RealPythonCompiler
        python_compiler = RealPythonCompiler()
        print("✅ Python Compiler: OK")
        
        # Test JavaScript Transpiler
        print("📝 Testing JavaScript Transpiler import...")
        from real_javascript_transpiler import JavaScriptTranspiler
        js_transpiler = JavaScriptTranspiler()
        print("✅ JavaScript Transpiler: OK")
        
        # Test Solidity Compiler
        print("📝 Testing Solidity Compiler import...")
        from real_solidity_compiler import RealSolidityCompiler
        solidity_compiler = RealSolidityCompiler()
        print("✅ Solidity Compiler: OK")
        
        # Test Code Generator
        print("📝 Testing Code Generator import...")
        from real_code_generation_system import RealCodeGenerator
        code_generator = RealCodeGenerator()
        print("✅ Code Generator: OK")
        
        # Test Unified System
        print("📝 Testing Unified Compiler System import...")
        from unified_real_compiler_system import UnifiedRealCompilerSystem, CompilerType, BuildTarget
        unified_system = UnifiedRealCompilerSystem()
        print("✅ Unified Compiler System: OK")
        
        return True
        
    except Exception as e:
        print(f"❌ Import Error: {e}")
        return False

def test_cpp_compilation():
    """Test C++ compilation"""
    
    print("\n🔧 Testing C++ Compilation...")
    print("=" * 30)
    
    try:
        from real_cpp_compiler import RealCppCompiler
        
        compiler = RealCppCompiler()
        
        # Simple C++ code
        cpp_code = """
        int main() {
            int x = 5 + 3;
            return x;
        }
        """
        
        print("📝 Compiling C++ code...")
        success = compiler.compile_to_exe(cpp_code, "test_output.exe")
        
        if success:
            print("✅ C++ Compilation: SUCCESS")
            if os.path.exists("test_output.exe"):
                print("📁 Executable file created")
            else:
                print("⚠️ No executable file found")
        else:
            print("❌ C++ Compilation: FAILED")
        
        return success
        
    except Exception as e:
        print(f"❌ C++ Test Error: {e}")
        return False

def test_python_compilation():
    """Test Python compilation"""
    
    print("\n🐍 Testing Python Compilation...")
    print("=" * 30)
    
    try:
        from real_python_compiler import RealPythonCompiler
        
        compiler = RealPythonCompiler()
        
        # Simple Python code
        python_code = """
        def add_numbers(a, b):
            return a + b
        
        result = add_numbers(5, 3)
        print(result)
        """
        
        print("📝 Compiling Python code...")
        success = compiler.compile_to_pyc(python_code, "test_output.pyc")
        
        if success:
            print("✅ Python Compilation: SUCCESS")
            if os.path.exists("test_output.pyc"):
                print("📁 Bytecode file created")
            else:
                print("⚠️ No bytecode file found")
        else:
            print("❌ Python Compilation: FAILED")
        
        return success
        
    except Exception as e:
        print(f"❌ Python Test Error: {e}")
        return False

def test_javascript_transpilation():
    """Test JavaScript transpilation"""
    
    print("\n⚡ Testing JavaScript Transpilation...")
    print("=" * 30)
    
    try:
        from real_javascript_transpiler import JavaScriptTranspiler
        
        transpiler = JavaScriptTranspiler()
        
        # TypeScript code
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
        
        print("📝 Transpiling TypeScript to JavaScript...")
        js_code = transpiler.transpile(typescript_code, 'typescript')
        
        if js_code:
            print("✅ JavaScript Transpilation: SUCCESS")
            print("📝 Generated JavaScript:")
            print(js_code[:200] + "..." if len(js_code) > 200 else js_code)
        else:
            print("❌ JavaScript Transpilation: FAILED")
        
        return bool(js_code)
        
    except Exception as e:
        print(f"❌ JavaScript Test Error: {e}")
        return False

def test_solidity_compilation():
    """Test Solidity compilation"""
    
    print("\n🔗 Testing Solidity Compilation...")
    print("=" * 30)
    
    try:
        from real_solidity_compiler import RealSolidityCompiler
        
        compiler = RealSolidityCompiler()
        
        # Simple Solidity code
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
        
        print("📝 Compiling Solidity code...")
        bytecode = compiler.compile_to_bytecode(solidity_code)
        
        if bytecode:
            print("✅ Solidity Compilation: SUCCESS")
            print(f"📁 Generated {len(bytecode.bytecode)} bytes of bytecode")
            print(f"📁 Generated {len(bytecode.abi)} ABI entries")
        else:
            print("❌ Solidity Compilation: FAILED")
        
        return bytecode is not None
        
    except Exception as e:
        print(f"❌ Solidity Test Error: {e}")
        return False

def test_unified_system():
    """Test unified compiler system"""
    
    print("\n🔧 Testing Unified Compiler System...")
    print("=" * 30)
    
    try:
        from unified_real_compiler_system import UnifiedRealCompilerSystem, CompilerType, BuildTarget
        
        system = UnifiedRealCompilerSystem()
        
        print(f"📝 Supported languages: {', '.join(system.get_supported_languages())}")
        
        # Test language detection
        test_file = "test.cpp"
        with open(test_file, 'w') as f:
            f.write("int main() { return 0; }")
        
        detected = system.detect_language(test_file)
        print(f"📝 Detected language for test.cpp: {detected.value}")
        
        # Clean up
        if os.path.exists(test_file):
            os.remove(test_file)
        
        print("✅ Unified System: SUCCESS")
        return True
        
    except Exception as e:
        print(f"❌ Unified System Test Error: {e}")
        return False

def main():
    """Run all tests"""
    
    print("🔧 Real Compiler System Test Suite")
    print("=" * 50)
    
    tests = [
        ("Import Test", test_imports),
        ("C++ Compilation", test_cpp_compilation),
        ("Python Compilation", test_python_compilation),
        ("JavaScript Transpilation", test_javascript_transpilation),
        ("Solidity Compilation", test_solidity_compilation),
        ("Unified System", test_unified_system)
    ]
    
    results = []
    
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ {test_name} crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 TEST RESULTS SUMMARY")
    print("=" * 50)
    
    passed = 0
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{test_name}: {status}")
        if result:
            passed += 1
    
    print(f"\n📈 Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 ALL TESTS PASSED! Real compilers are working!")
    else:
        print("⚠️ Some tests failed. Check the output above.")
    
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
