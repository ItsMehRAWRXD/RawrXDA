#!/usr/bin/env python3
"""
Comprehensive Test System for Extensible Compiler
Tests all components including transpilation
"""

import os
import sys
import traceback
from typing import Dict, List, Any

def test_python_components():
    """Test Python language components"""
    print("🧪 Testing Python Components...")
    
    try:
        from plugins.python_components import PythonLexer, PythonParser, get_language_info
        
        # Test language info
        lang_info = get_language_info()
        print(f"✅ Language: {lang_info.name}")
        print(f"✅ Extension: {lang_info.extension}")
        print(f"✅ Type: {lang_info.language_type.value}")
        
        # Test lexer
        lexer = PythonLexer()
        source = """
def hello_world():
    x = 10
    y = 20
    result = x + y
    print(result)
"""
        
        tokens = lexer.tokenize(source)
        print(f"✅ Generated {len(tokens)} tokens")
        
        # Test parser
        parser = PythonParser()
        ast = parser.parse(tokens)
        print(f"✅ Generated AST with {len(ast.statements)} statements")
        
        return True
        
    except Exception as e:
        print(f"❌ Python components test failed: {e}")
        traceback.print_exc()
        return False

def test_javascript_components():
    """Test JavaScript language components"""
    print("\n🧪 Testing JavaScript Components...")
    
    try:
        from plugins.js_components import JavaScriptLexer, JavaScriptParser, get_language_info
        
        # Test language info
        lang_info = get_language_info()
        print(f"✅ Language: {lang_info.name}")
        print(f"✅ Extension: {lang_info.extension}")
        print(f"✅ Type: {lang_info.language_type.value}")
        
        # Test lexer
        lexer = JavaScriptLexer()
        source = """
function addNumbers(a, b) {
    let result = a + b;
    return result;
}

let x = 10;
let y = 20;
let sum = addNumbers(x, y);
console.log(sum);
"""
        
        tokens = lexer.tokenize(source)
        print(f"✅ Generated {len(tokens)} tokens")
        
        # Test parser
        parser = JavaScriptParser()
        ast = parser.parse(tokens)
        print(f"✅ Generated AST with {len(ast.statements)} statements")
        
        return True
        
    except Exception as e:
        print(f"❌ JavaScript components test failed: {e}")
        traceback.print_exc()
        return False

def test_rust_components():
    """Test Rust language components"""
    print("\n🧪 Testing Rust Components...")
    
    try:
        from plugins.rust_components import RustLexer, RustParser, get_language_info
        
        # Test language info
        lang_info = get_language_info()
        print(f"✅ Language: {lang_info.name}")
        print(f"✅ Extension: {lang_info.extension}")
        print(f"✅ Type: {lang_info.language_type.value}")
        
        # Test lexer
        lexer = RustLexer()
        source = """
fn main() {
    let mut x = 10;
    let y = 20;
    let result = x + y;
    println!("Result: {}", result);
}
"""
        
        tokens = lexer.tokenize(source)
        print(f"✅ Generated {len(tokens)} tokens")
        
        # Test parser
        parser = RustParser()
        ast = parser.parse(tokens)
        print(f"✅ Generated AST with {len(ast.statements)} statements")
        
        return True
        
    except Exception as e:
        print(f"❌ Rust components test failed: {e}")
        traceback.print_exc()
        return False

def test_python_to_cpp_transpiler():
    """Test Python to C++ transpiler"""
    print("\n🧪 Testing Python to C++ Transpiler...")
    
    try:
        from plugins.py_to_cpp_codegen import PyToCppCodeGenerator, get_backend_target_info
        from ast_nodes import Program, FunctionDefinition, Assignment, BinaryOperation, Literal, Identifier
        
        # Test backend target info
        target_info = get_backend_target_info()
        print(f"✅ Target: {target_info.name}")
        print(f"✅ Extension: {target_info.file_extension}")
        print(f"✅ Platform: {target_info.platform}")
        
        # Create test AST
        program = Program()
        
        # Add a simple function
        func = FunctionDefinition(
            name="add",
            parameters=["a", "b"],
            body=[
                Assignment("result", BinaryOperation("+", Identifier("a"), Identifier("b"))),
                {'type': 'ReturnStatement', 'value': Identifier("result")}
            ]
        )
        program.statements.append(func)
        
        # Add main function
        main_func = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                Assignment("x", Literal(10)),
                Assignment("y", Literal(20)),
                Assignment("sum", BinaryOperation("+", Identifier("x"), Identifier("y"))),
                {'type': 'PrintStatement', 'args': [Identifier("sum")]}
            ]
        )
        program.statements.append(main_func)
        
        # Test transpiler
        transpiler = PyToCppCodeGenerator()
        cpp_code = transpiler.generate(program, target_info)
        
        print(f"✅ Generated C++ code ({len(cpp_code)} characters)")
        print("📄 Generated C++ code preview:")
        print(cpp_code[:200] + "..." if len(cpp_code) > 200 else cpp_code)
        
        return True
        
    except Exception as e:
        print(f"❌ Python to C++ transpiler test failed: {e}")
        traceback.print_exc()
        return False

def test_python_to_rust_transpiler():
    """Test Python to Rust transpiler"""
    print("\n🧪 Testing Python to Rust Transpiler...")
    
    try:
        from plugins.py_to_rust_codegen import PyToRustCodeGenerator, get_backend_target_info
        from ast_nodes import Program, FunctionDefinition, Assignment, BinaryOperation, Literal, Identifier
        
        # Test backend target info
        target_info = get_backend_target_info()
        print(f"✅ Target: {target_info.name}")
        print(f"✅ Extension: {target_info.file_extension}")
        print(f"✅ Platform: {target_info.platform}")
        
        # Create test AST
        program = Program()
        
        # Add a simple function
        func = FunctionDefinition(
            name="add",
            parameters=["a", "b"],
            body=[
                Assignment("result", BinaryOperation("+", Identifier("a"), Identifier("b"))),
                {'type': 'ReturnStatement', 'value': Identifier("result")}
            ]
        )
        program.statements.append(func)
        
        # Add main function
        main_func = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                Assignment("x", Literal(10)),
                Assignment("y", Literal(20)),
                Assignment("sum", BinaryOperation("+", Identifier("x"), Identifier("y"))),
                {'type': 'PrintStatement', 'args': [Identifier("sum")]}
            ]
        )
        program.statements.append(main_func)
        
        # Test transpiler
        transpiler = PyToRustCodeGenerator()
        rust_code = transpiler.generate(program, target_info)
        
        print(f"✅ Generated Rust code ({len(rust_code)} characters)")
        print("📄 Generated Rust code preview:")
        print(rust_code[:200] + "..." if len(rust_code) > 200 else rust_code)
        
        return True
        
    except Exception as e:
        print(f"❌ Python to Rust transpiler test failed: {e}")
        traceback.print_exc()
        return False

def test_ast_visitor():
    """Test AST visitor pattern"""
    print("\n🧪 Testing AST Visitor Pattern...")
    
    try:
        from ast_visitor import CodeGenerator
        from ast_nodes import Program, Literal, Identifier, BinaryOperation
        
        # Create a simple test visitor
        class TestVisitor(CodeGenerator):
            def __init__(self):
                super().__init__()
                self.output = []
            
            def visit_Program(self, node):
                self.output.append("Program")
                for stmt in node.statements:
                    self.visit(stmt)
            
            def visit_Literal(self, node):
                self.output.append(f"Literal: {node.value}")
            
            def visit_Identifier(self, node):
                self.output.append(f"Identifier: {node.name}")
            
            def visit_BinaryOperation(self, node):
                self.output.append(f"BinaryOp: {node.operator}")
                self.visit(node.left)
                self.visit(node.right)
        
        # Test visitor
        visitor = TestVisitor()
        
        # Create test AST
        program = Program()
        program.statements.append(BinaryOperation("+", Literal(10), Literal(20)))
        
        visitor.visit(program)
        
        print(f"✅ Visitor processed {len(visitor.output)} nodes")
        print(f"✅ Output: {visitor.output}")
        
        return True
        
    except Exception as e:
        print(f"❌ AST visitor test failed: {e}")
        traceback.print_exc()
        return False

def test_user_component_manager():
    """Test user component manager"""
    print("\n🧪 Testing User Component Manager...")
    
    try:
        from user_compiler_components import UserComponentManager, LanguageType, TargetType
        
        # Create component manager
        manager = UserComponentManager()
        
        print(f"✅ Loaded {len(manager.languages)} languages")
        print(f"✅ Loaded {len(manager.ir_passes)} IR passes")
        print(f"✅ Loaded {len(manager.backend_targets)} backend targets")
        
        # Test creating a custom language
        lang_file = manager.create_language_component(
            "TestLang", ".test", LanguageType.COMPILED,
            "A test programming language", "Test Author", "1.0.0"
        )
        print(f"✅ Created custom language: {lang_file}")
        
        # Test creating a custom IR pass
        pass_file = manager.create_ir_pass_component(
            "TestPass", "A test optimization pass", 2, "Test Author", "1.0.0"
        )
        print(f"✅ Created custom IR pass: {pass_file}")
        
        # Test creating a custom backend target
        target_file = manager.create_backend_target_component(
            "TestTarget", TargetType.EXECUTABLE, "A test backend target",
            ".exe", "test", "Test Author", "1.0.0"
        )
        print(f"✅ Created custom backend target: {target_file}")
        
        return True
        
    except Exception as e:
        print(f"❌ User component manager test failed: {e}")
        traceback.print_exc()
        return False

def test_comprehensive_gui():
    """Test comprehensive GUI"""
    print("\n🧪 Testing Comprehensive GUI...")
    
    try:
        from comprehensive_compiler_gui import ComprehensiveCompilerGUI
        
        # Create GUI (but don't run it in test mode)
        gui = ComprehensiveCompilerGUI()
        print("✅ GUI created successfully")
        
        # Test basic GUI components
        print(f"✅ Language combo has {len(gui.language_combo['values'])} options")
        print(f"✅ Notebook has {gui.notebook.index('end')} tabs")
        
        return True
        
    except Exception as e:
        print(f"❌ Comprehensive GUI test failed: {e}")
        traceback.print_exc()
        return False

def run_all_tests():
    """Run all tests"""
    print("🚀 Running Comprehensive Compiler System Tests")
    print("=" * 60)
    
    tests = [
        ("Python Components", test_python_components),
        ("JavaScript Components", test_javascript_components),
        ("Rust Components", test_rust_components),
        ("Python to C++ Transpiler", test_python_to_cpp_transpiler),
        ("Python to Rust Transpiler", test_python_to_rust_transpiler),
        ("AST Visitor Pattern", test_ast_visitor),
        ("User Component Manager", test_user_component_manager),
        ("Comprehensive GUI", test_comprehensive_gui),
    ]
    
    results = []
    
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ {test_name} test crashed: {e}")
            results.append((test_name, False))
    
    # Print summary
    print("\n" + "=" * 60)
    print("📊 Test Results Summary")
    print("=" * 60)
    
    passed = 0
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"{test_name:<30} {status}")
        if result:
            passed += 1
    
    print("=" * 60)
    print(f"Total: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! The comprehensive compiler system is working!")
    else:
        print(f"⚠️  {total - passed} tests failed. Please check the implementation.")
    
    return passed == total

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
