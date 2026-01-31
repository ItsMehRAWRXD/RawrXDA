#!/usr/bin/env python3
"""
Python to Rust Transpiler
Converts Python AST to Rust code
"""

import io
from typing import List, Any, Dict, Optional
from extensible_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_visitor import CodeGenerator
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionDefinition, FunctionCall

BACKEND_TARGET_INFO = {
    'py_to_rust': BackendTargetInfo(
        name='Python to Rust Transpiler',
        target_type=TargetType.EXECUTABLE,
        description='Generates Rust code from a Python AST',
        codegen_class='PyToRustCodeGenerator',
        file_extension='.rs',
        platform='cross-platform'
    )
}

class PyToRustCodeGenerator(BaseCodeGenerator, CodeGenerator):
    """Python to Rust code generator"""
    
    def __init__(self):
        super().__init__()
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()
        self.function_declarations = []
        self.variable_types = {}
        self.imports = set()
    
    def generate(self, ast: Program, target_info: BackendTargetInfo) -> str:
        """Generate Rust code from Python AST"""
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()
        self.function_declarations = []
        self.variable_types = {}
        self.imports = set()
        
        # Add standard imports
        self._add_import("std::collections::HashMap")
        self._add_import("std::vec::Vec")
        
        # Generate code
        self.visit(ast)
        
        # Build final output
        import_str = ""
        if self.imports:
            import_str = "\n".join([f"use {imp};" for imp in sorted(list(self.imports))]) + "\n\n"
        
        return import_str + self.output_stream.getvalue()
    
    def visit_Program(self, node: Program):
        """Generate Rust main function"""
        # Generate function definitions first
        for stmt in node.statements:
            if isinstance(stmt, FunctionDefinition):
                self.visit(stmt)
                self._writeline("")
        
        # Generate main function
        self._writeline("fn main() {")
        self._indent()
        
        for statement in node.statements:
            if not isinstance(statement, FunctionDefinition):
                self.visit(statement)
        
        self._dedent()
        self._writeline("}")
    
    def visit_VariableDeclaration(self, node: VariableDeclaration):
        """Generate Rust variable declaration"""
        # Infer type from initializer
        var_type = "let"
        if node.initializer:
            inferred_type = self._get_type_for_expression(node.initializer)
            if inferred_type != "auto":
                var_type = f"let: {self._rust_type_from_python_type(inferred_type)}"
        
        # Check if variable is mutable (simplified - assume all are mutable for now)
        var_type = var_type.replace("let", "let mut")
        
        self.variable_types[node.name] = var_type
        
        self._write(f"{var_type} {node.name}")
        if node.initializer:
            self.output_stream.write(" = ")
            self.visit(node.initializer)
        self._writeline(";")
    
    def visit_Assignment(self, node: Assignment):
        """Generate Rust assignment"""
        self._write(f"{node.name} = ")
        self.visit(node.value)
        self._writeline(";")
    
    def visit_BinaryOperation(self, node: BinaryOperation):
        """Generate Rust binary operation"""
        if node.operator == '**':
            # Power operator
            self._add_import("std::f64::powi")
            self.output_stream.write(f"({self.visit(node.left)}.powi({self.visit(node.right)}))")
        elif node.operator == '//':
            # Integer division
            self.output_stream.write(f"({self.visit(node.left)} / {self.visit(node.right)})")
        elif node.operator == 'and':
            self.output_stream.write(f"({self.visit(node.left)} && {self.visit(node.right)})")
        elif node.operator == 'or':
            self.output_stream.write(f"({self.visit(node.left)} || {self.visit(node.right)})")
        elif node.operator == 'not':
            self.output_stream.write(f"!({self.visit(node.left)})")
        else:
            self.output_stream.write(f"({self.visit(node.left)} {self._get_operator_mapping(node.operator)} {self.visit(node.right)})")
    
    def visit_Literal(self, node: Literal):
        """Generate Rust literal"""
        if isinstance(node.value, str):
            self.output_stream.write(f'"{self._escape_string(node.value)}"')
        elif isinstance(node.value, bool):
            self.output_stream.write("true" if node.value else "false")
        elif node.value is None:
            self.output_stream.write("None")
        elif isinstance(node.value, int):
            self.output_stream.write(str(node.value))
        elif isinstance(node.value, float):
            self.output_stream.write(str(node.value))
        else:
            self.output_stream.write(self._format_number(node.value))
    
    def visit_Identifier(self, node: Identifier):
        """Generate Rust identifier"""
        self.output_stream.write(node.name)
    
    def visit_FunctionDefinition(self, node: FunctionDefinition):
        """Generate Rust function definition"""
        # Determine return type
        return_type = "()"
        if node.return_type:
            return_type = self._rust_type_from_python_type(node.return_type)
        else:
            # Try to infer return type from function body
            inferred_type = self._infer_function_return_type(node)
            if inferred_type != "void":
                return_type = inferred_type
        
        # Generate function signature
        self._write(f"fn {node.name}(")
        
        # Generate parameters
        params = []
        for param in node.parameters:
            param_type = "i32"  # Default type
            if isinstance(param, dict) and 'type' in param:
                param_type = self._rust_type_from_python_type(param['type'])
            elif isinstance(param, str):
                param_type = "i32"
            
            param_name = param if isinstance(param, str) else param.get('name', 'param')
            params.append(f"{param_name}: {param_type}")
        
        self.output_stream.write(", ".join(params))
        
        if return_type != "()":
            self.output_stream.write(f") -> {return_type} {{")
        else:
            self.output_stream.write(") {")
        self.output_stream.write("\n")
        
        # Generate function body
        self._indent()
        for stmt in node.body:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_FunctionCall(self, node: FunctionCall):
        """Generate Rust function call"""
        self.visit(node.callee)
        self.output_stream.write("(")
        
        for i, arg in enumerate(node.arguments):
            self.visit(arg)
            if i < len(node.arguments) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write(")")
    
    def visit_IfStatement(self, node: Dict):
        """Generate Rust if statement"""
        self._write("if ")
        self.visit(node['condition'])
        self.output_stream.write(" {\n")
        
        self._indent()
        for stmt in node['if_body']:
            self.visit(stmt)
        self._dedent()
        
        if node.get('else_body'):
            self._writeline("} else {")
            self._indent()
            for stmt in node['else_body']:
                self.visit(stmt)
            self._dedent()
        
        self._writeline("}")
    
    def visit_ForStatement(self, node: Dict):
        """Generate Rust for statement"""
        if 'variable' in node and 'iterable' in node:
            # Python-style for loop: for var in iterable
            self._write("for ")
            self.output_stream.write(node['variable'])
            self.output_stream.write(" in ")
            self.visit(node['iterable'])
            self.output_stream.write(" {\n")
        else:
            # C-style for loop: for (init; condition; increment)
            self._write("for ")
            if node.get('init'):
                self.visit(node['init'])
            self.output_stream.write("; ")
            if node.get('condition'):
                self.visit(node['condition'])
            self.output_stream.write("; ")
            if node.get('increment'):
                self.visit(node['increment'])
            self.output_stream.write(" {\n")
        
        self._indent()
        for stmt in node['body']:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_WhileStatement(self, node: Dict):
        """Generate Rust while statement"""
        self._write("while ")
        self.visit(node['condition'])
        self.output_stream.write(" {\n")
        
        self._indent()
        for stmt in node['body']:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_ReturnStatement(self, node: Dict):
        """Generate Rust return statement"""
        if node.get('value'):
            self._write("return ")
            self.visit(node['value'])
            self._writeline(";")
        else:
            self._writeline("return;")
    
    def visit_ArrayLiteral(self, node: Dict):
        """Generate Rust vector literal"""
        self._add_import("std::vec::Vec")
        
        # Determine element type
        element_type = "i32"  # Default
        if node['elements']:
            element_type = self._rust_type_from_python_type(
                self._get_type_for_expression(node['elements'][0])
            )
        
        self.output_stream.write(f"vec![")
        
        for i, element in enumerate(node['elements']):
            self.visit(element)
            if i < len(node['elements']) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write("]")
    
    def visit_ObjectLiteral(self, node: Dict):
        """Generate Rust struct literal (simplified)"""
        # For now, just generate a comment - full object literal support is complex
        self.output_stream.write("/* Object literal - requires custom struct definition */")
    
    def visit_StructLiteral(self, node: Dict):
        """Generate Rust struct literal"""
        self.output_stream.write("{")
        
        for i, field in enumerate(node['fields']):
            self.output_stream.write(f"{field['name']}: ")
            self.visit(field['value'])
            if i < len(node['fields']) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write("}")
    
    def visit_MemberAccess(self, node: Dict):
        """Generate Rust member access"""
        self.visit(node['object'])
        self.output_stream.write(".")
        self.visit(node['property'])
    
    def _rust_type_from_python_type(self, python_type: str) -> str:
        """Convert Python type to Rust type"""
        type_mappings = {
            'int': 'i32',
            'float': 'f64',
            'str': 'String',
            'bool': 'bool',
            'list': 'Vec',
            'dict': 'HashMap',
            'None': '()',
            'void': '()'
        }
        return type_mappings.get(python_type, 'i32')
    
    def _infer_function_return_type(self, node: FunctionDefinition) -> str:
        """Infer function return type from function body"""
        # Look for return statements in the function body
        for stmt in node.body:
            if isinstance(stmt, dict) and stmt.get('type') == 'ReturnStatement':
                if stmt.get('value'):
                    return self._rust_type_from_python_type(
                        self._get_type_for_expression(stmt['value'])
                    )
                else:
                    return '()'
        
        return '()'
    
    def _get_operator_mapping(self, op: str) -> str:
        """Get Rust equivalent of Python operator"""
        operator_mappings = {
            'and': '&&',
            'or': '||',
            'not': '!',
            '==': '==',
            '!=': '!=',
            '<': '<',
            '>': '>',
            '<=': '<=',
            '>=': '>=',
            '+': '+',
            '-': '-',
            '*': '*',
            '/': '/',
            '%': '%',
            'is': '==',  # Simplified
            'is not': '!=',  # Simplified
            'in': '/* in operator - requires custom implementation */',
        }
        return operator_mappings.get(op, op)
    
    def _escape_string(self, value: str) -> str:
        """Escape string for Rust"""
        return value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')

# Export backend target info
def get_backend_target_info():
    """Get Python to Rust transpiler backend target info"""
    return BACKEND_TARGET_INFO['py_to_rust']
