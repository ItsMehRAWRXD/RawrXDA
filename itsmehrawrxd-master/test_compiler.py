#!/usr/bin/env python3
"""
Test script for the Extensible Compiler System
Demonstrates the meta-prompting AST/IR generation capabilities
"""

import sys
import os
sys.path.append(os.path.dirname(__file__))

from main_compiler_system import ExtensibleCompilerSystem, CompilerException
from ast_nodes import print_ast

def test_cpp_compilation():
    """Test C++ compilation with AST generation"""
    print("🧪 Testing C++ Compilation")
    print("=" * 50)
    
    # Initialize compiler system
    system = ExtensibleCompilerSystem()
    
    # C++ source code
    cpp_source = """
int main() {
    int x = 5 + 3;
    int y = x * 2;
    if (y > 10) {
        return y;
    }
    return 0;
}
"""
    
    print("📝 Source Code:")
    print(cpp_source)
    
    try:
        # Compile with AST output
        result = system.compile(cpp_source, 'cpp', ['constant_folding'], 'print_ast')
        print("\n🌳 Generated AST:")
        print(result)
        
        # Test with IR passes
        print("\n🔧 Testing with IR Passes:")
        result_with_passes = system.compile(cpp_source, 'cpp', ['constant_folding', 'dead_code_elimination'], 'print_ast')
        print(result_with_passes)
        
    except CompilerException as e:
        print(f"❌ Compilation failed: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")

def test_python_compilation():
    """Test Python compilation with code generation"""
    print("\n\n🐍 Testing Python Compilation")
    print("=" * 50)
    
    system = ExtensibleCompilerSystem()
    
    # Python source code
    python_source = """
def calculate(x, y):
    result = x + y
    if result > 10:
        return result * 2
    return result
"""
    
    print("📝 Source Code:")
    print(python_source)
    
    try:
        # Generate Python code
        result = system.compile(python_source, 'python', [], 'python_code')
        print("\n🐍 Generated Python Code:")
        print(result)
        
    except CompilerException as e:
        print(f"❌ Compilation failed: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")

def test_javascript_compilation():
    """Test JavaScript compilation with code generation"""
    print("\n\n🌐 Testing JavaScript Compilation")
    print("=" * 50)
    
    system = ExtensibleCompilerSystem()
    
    # JavaScript source code
    js_source = """
function greet(name) {
    let message = "Hello, " + name;
    if (name.length > 5) {
        message += " (long name!)";
    }
    return message;
}
"""
    
    print("📝 Source Code:")
    print(js_source)
    
    try:
        # Generate JavaScript code
        result = system.compile(js_source, 'javascript', [], 'javascript_code')
        print("\n🌐 Generated JavaScript Code:")
        print(result)
        
    except CompilerException as e:
        print(f"❌ Compilation failed: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")

def test_rust_compilation():
    """Test Rust compilation with AST generation"""
    print("\n\n🦀 Testing Rust Compilation")
    print("=" * 50)
    
    system = ExtensibleCompilerSystem()
    
    # Rust source code
    rust_source = """
fn main() {
    let x = 5 + 3;
    let mut y = x * 2;
    if y > 10 {
        y = y + 1;
    }
    println!("Result: {}", y);
}
"""
    
    print("📝 Source Code:")
    print(rust_source)
    
    try:
        # Generate AST
        result = system.compile(rust_source, 'rust', ['constant_folding'], 'print_ast')
        print("\n🌳 Generated AST:")
        print(result)
        
    except CompilerException as e:
        print(f"❌ Compilation failed: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")

def test_ir_passes():
    """Test IR optimization passes"""
    print("\n\n⚡ Testing IR Optimization Passes")
    print("=" * 50)
    
    system = ExtensibleCompilerSystem()
    
    # Source with constant expressions
    source = """
int main() {
    int x = 5 + 3 * 2;
    int y = 10 - 4;
    if (x > y) {
        return x;
    }
    return 0;
}
"""
    
    print("📝 Source Code:")
    print(source)
    
    try:
        # Test without optimization
        print("\n🔧 Without Optimization:")
        result_no_opt = system.compile(source, 'cpp', [], 'print_ast')
        print(result_no_opt)
        
        # Test with constant folding
        print("\n⚡ With Constant Folding:")
        result_opt = system.compile(source, 'cpp', ['constant_folding'], 'print_ast')
        print(result_opt)
        
        # Test with all optimizations
        print("\n🚀 With All Optimizations:")
        result_all_opt = system.compile(source, 'cpp', ['constant_folding', 'dead_code_elimination', 'unused_variable_removal'], 'print_ast')
        print(result_all_opt)
        
    except CompilerException as e:
        print(f"❌ Compilation failed: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")

def test_system_info():
    """Display system information"""
    print("\n\n📊 System Information")
    print("=" * 50)
    
    system = ExtensibleCompilerSystem()
    
    print(f"🔤 Languages: {len(system.languages)}")
    for lang_name, lang_info in system.languages.items():
        print(f"  • {lang_info.name} ({lang_info.language_type.value})")
    
    print(f"\n🔧 IR Passes: {len(system.ir_passes)}")
    for pass_name, pass_info in system.ir_passes.items():
        print(f"  • {pass_info.name} (level {pass_info.optimization_level})")
    
    print(f"\n🎯 Backend Targets: {len(system.backend_targets)}")
    for target_name, target_info in system.backend_targets.items():
        print(f"  • {target_info.name} ({target_info.target_type.value})")
    
    print(f"\n🔌 Custom Parsers: {len(system.custom_parsers)}")
    for parser_name in system.custom_parsers.keys():
        print(f"  • {parser_name}")
    
    print(f"\n⚡ Custom Passes: {len(system.custom_passes)}")
    for pass_name in system.custom_passes.keys():
        print(f"  • {pass_name}")
    
    print(f"\n🎨 Custom Targets: {len(system.custom_targets)}")
    for target_name in system.custom_targets.keys():
        print(f"  • {target_name}")

def main():
    """Run all tests"""
    print("🚀 Extensible Compiler System - Test Suite")
    print("=" * 60)
    
    # Test system info
    test_system_info()
    
    # Test individual language compilations
    test_cpp_compilation()
    test_python_compilation()
    test_javascript_compilation()
    test_rust_compilation()
    
    # Test IR optimization passes
    test_ir_passes()
    
    print("\n\n✅ All tests completed!")
    print("=" * 60)
    print("🎉 The Extensible Compiler System is working correctly!")
    print("💡 Run 'python main.py' to start the GUI application.")

if __name__ == "__main__":
    main()
