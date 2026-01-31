#!/usr/bin/env python3
"""
Python to C++ Transpiler
Converts Python AST to C++ code
"""

import io
from typing import List, Any, Dict, Optional
from extensible_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_visitor import CodeGenerator
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionDefinition, FunctionCall

BACKEND_TARGET_INFO = {
    'py_to_cpp': BackendTargetInfo(
        name='Python to C++ Transpiler',
        target_type=TargetType.EXECUTABLE,
        description='Generates C++ code from a Python AST',
        codegen_class='PyToCppCodeGenerator',
        file_extension='.cpp',
        platform='cross-platform'
    )
}

class PyToCppCodeGenerator(BaseCodeGenerator, CodeGenerator):
    """Python to C++ code generator"""
    
    def __init__(self):
        super().__init__()
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()
        self.function_declarations = []
        self.variable_types = {}  # Track variable types for better code generation
    
    def generate(self, ast: Program, target_info: BackendTargetInfo) -> str:
        """Generate C++ code from Python AST"""
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()
        self.function_declarations = []
        self.variable_types = {}
        
        # Add standard headers
        self._add_header("iostream")
        self._add_header("vector")
        self._add_header("string")
        self._add_header("cmath")
        
        # Generate code
        self.visit(ast)
        
        # Build final output
        header_str = "".join([f"#include <{h}>\n" for h in sorted(list(self.headers))])
        if header_str:
            header_str += "\n"
        
        # Add function declarations if any
        declarations = ""
        if self.function_declarations:
            declarations = "\n".join(self.function_declarations) + "\n\n"
        
        return header_str + declarations + self.output_stream.getvalue()
    
    def visit_Program(self, node: Program):
        """Generate C++ main function"""
        # Generate function declarations first
        for stmt in node.statements:
            if isinstance(stmt, FunctionDefinition):
                self._generate_function_declaration(stmt)
        
        # Generate main function
        self._writeline("int main() {")
        self._indent()
        
        for statement in node.statements:
            if not isinstance(statement, FunctionDefinition):
                self.visit(statement)
        
        self._dedent()
        self._writeline("    return 0;")
        self._writeline("}")
        
        # Generate function definitions
        for stmt in node.statements:
            if isinstance(stmt, FunctionDefinition):
                self._writeline("")
                self.visit(stmt)
    
    def visit_VariableDeclaration(self, node: VariableDeclaration):
        """Generate C++ variable declaration"""
        # Infer type from initializer
        var_type = "auto"
        if node.initializer:
            var_type = self._get_type_for_expression(node.initializer)
        
        self.variable_types[node.name] = var_type
        
        self._write(f"{var_type} {node.name}")
        if node.initializer:
            self.output_stream.write(" = ")
            self.visit(node.initializer)
        self._writeline(";")
    
    def visit_Assignment(self, node: Assignment):
        """Generate C++ assignment"""
        self._write(f"{node.name} = ")
        self.visit(node.value)
        self._writeline(";")
    
    def visit_BinaryOperation(self, node: BinaryOperation):
        """Generate C++ binary operation"""
        if node.operator == '**':
            # Power operator
            self._add_header("cmath")
            self.output_stream.write(f"std::pow({self.visit(node.left)}, {self.visit(node.right)})")
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
        """Generate C++ literal"""
        if isinstance(node.value, str):
            self.output_stream.write(f'"{self._escape_string(node.value)}"')
        elif isinstance(node.value, bool):
            self.output_stream.write("true" if node.value else "false")
        elif node.value is None:
            self.output_stream.write("nullptr")
        else:
            self.output_stream.write(self._format_number(node.value))
    
    def visit_Identifier(self, node: Identifier):
        """Generate C++ identifier"""
        self.output_stream.write(node.name)
    
    def visit_FunctionDefinition(self, node: FunctionDefinition):
        """Generate C++ function definition"""
        # Determine return type
        return_type = "void"
        if node.return_type:
            return_type = self._cpp_type_from_python_type(node.return_type)
        else:
            # Try to infer return type from function body
            return_type = self._infer_function_return_type(node)
        
        # Generate function signature
        self._write(f"{return_type} {node.name}(")
        
        # Generate parameters
        params = []
        for param in node.parameters:
            param_type = "auto"  # Default type
            if isinstance(param, dict) and 'type' in param:
                param_type = self._cpp_type_from_python_type(param['type'])
            elif isinstance(param, str):
                param_type = "auto"
            
            params.append(f"{param_type} {param if isinstance(param, str) else param.get('name', 'param')}")
        
        self.output_stream.write(", ".join(params))
        self.output_stream.write(") {\n")
        
        # Generate function body
        self._indent()
        for stmt in node.body:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_FunctionCall(self, node: FunctionCall):
        """Generate C++ function call"""
        self.visit(node.callee)
        self.output_stream.write("(")
        
        for i, arg in enumerate(node.arguments):
            self.visit(arg)
            if i < len(node.arguments) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write(")")
    
    def visit_IfStatement(self, node: Dict):
        """Generate C++ if statement"""
        self._write("if (")
        self.visit(node['condition'])
        self.output_stream.write(") {\n")
        
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
        """Generate C++ for statement"""
        if 'variable' in node and 'iterable' in node:
            # Python-style for loop: for var in iterable
            self._write("for (auto ")
            self.output_stream.write(node['variable'])
            self.output_stream.write(" : ")
            self.visit(node['iterable'])
            self.output_stream.write(") {\n")
        else:
            # C-style for loop: for (init; condition; increment)
            self._write("for (")
            if node.get('init'):
                self.visit(node['init'])
            self.output_stream.write("; ")
            if node.get('condition'):
                self.visit(node['condition'])
            self.output_stream.write("; ")
            if node.get('increment'):
                self.visit(node['increment'])
            self.output_stream.write(") {\n")
        
        self._indent()
        for stmt in node['body']:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_WhileStatement(self, node: Dict):
        """Generate C++ while statement"""
        self._write("while (")
        self.visit(node['condition'])
        self.output_stream.write(") {\n")
        
        self._indent()
        for stmt in node['body']:
            self.visit(stmt)
        self._dedent()
        self._writeline("}")
    
    def visit_ReturnStatement(self, node: Dict):
        """Generate C++ return statement"""
        self._write("return")
        if node.get('value'):
            self.output_stream.write(" ")
            self.visit(node['value'])
        self._writeline(";")
    
    def visit_ArrayLiteral(self, node: Dict):
        """Generate C++ array/vector literal"""
        self._add_header("vector")
        
        # Determine element type
        element_type = "auto"
        if node['elements']:
            element_type = self._get_type_for_expression(node['elements'][0])
        
        self.output_stream.write(f"std::vector<{element_type}>{{")
        
        for i, element in enumerate(node['elements']):
            self.visit(element)
            if i < len(node['elements']) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write("}")
    
    def visit_ObjectLiteral(self, node: Dict):
        """Generate C++ struct/class literal (simplified)"""
        # For now, just generate a comment - full object literal support is complex
        self.output_stream.write("/* Object literal - requires custom struct definition */")
    
    def visit_StructLiteral(self, node: Dict):
        """Generate C++ struct literal"""
        self.output_stream.write("{")
        
        for i, field in enumerate(node['fields']):
            self.output_stream.write(f".{field['name']} = ")
            self.visit(field['value'])
            if i < len(node['fields']) - 1:
                self.output_stream.write(", ")
        
        self.output_stream.write("}")
    
    def visit_MemberAccess(self, node: Dict):
        """Generate C++ member access"""
        self.visit(node['object'])
        self.output_stream.write(".")
        self.visit(node['property'])
    
    def _generate_function_declaration(self, node: FunctionDefinition):
        """Generate function declaration"""
        return_type = "void"
        if node.return_type:
            return_type = self._cpp_type_from_python_type(node.return_type)
        
        params = []
        for param in node.parameters:
            param_type = "auto"
            if isinstance(param, dict) and 'type' in param:
                param_type = self._cpp_type_from_python_type(param['type'])
            
            param_name = param if isinstance(param, str) else param.get('name', 'param')
            params.append(f"{param_type} {param_name}")
        
        declaration = f"{return_type} {node.name}({', '.join(params)});"
        self.function_declarations.append(declaration)
    
    def _cpp_type_from_python_type(self, python_type: str) -> str:
        """Convert Python type to C++ type"""
        type_mappings = {
            'int': 'int',
            'float': 'double',
            'str': 'std::string',
            'bool': 'bool',
            'list': 'std::vector',
            'dict': 'std::map',
            'None': 'void'
        }
        return type_mappings.get(python_type, 'auto')
    
    def _infer_function_return_type(self, node: FunctionDefinition) -> str:
        """Infer function return type from function body"""
        # Look for return statements in the function body
        for stmt in node.body:
            if isinstance(stmt, dict) and stmt.get('type') == 'ReturnStatement':
                if stmt.get('value'):
                    return self._get_type_for_expression(stmt['value'])
                else:
                    return 'void'
        
        return 'void'
    
    def _get_operator_mapping(self, op: str) -> str:
        """Get C++ equivalent of Python operator"""
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

# Export backend target info
def get_backend_target_info():
    """Get Python to C++ transpiler backend target info"""
    return BACKEND_TARGET_INFO['py_to_cpp']
