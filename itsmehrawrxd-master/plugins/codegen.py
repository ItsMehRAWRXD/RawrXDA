#!/usr/bin/env python3
"""
Code Generators for Extensible Compiler System
Implements various code generators for different targets
"""

from typing import List, Any, Dict
from main_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_nodes import *

BACKEND_TARGET_INFO = {
    'print_ast': {
        'name': 'Print AST',
        'target_type': TargetType.EXECUTABLE,
        'description': 'Print the AST to console',
        'codegen_class': 'PrintASTGenerator',
        'file_extension': '.ast',
        'platform': 'all'
    },
    'print_ir': {
        'name': 'Print IR',
        'target_type': TargetType.EXECUTABLE,
        'description': 'Print the IR to console',
        'codegen_class': 'PrintIRGenerator',
        'file_extension': '.ir',
        'platform': 'all'
    },
    'python_code': {
        'name': 'Python Code',
        'target_type': TargetType.EXECUTABLE,
        'description': 'Generate Python code',
        'codegen_class': 'PythonCodeGenerator',
        'file_extension': '.py',
        'platform': 'all'
    },
    'javascript_code': {
        'name': 'JavaScript Code',
        'target_type': TargetType.WEB,
        'description': 'Generate JavaScript code',
        'codegen_class': 'JavaScriptCodeGenerator',
        'file_extension': '.js',
        'platform': 'web'
    },
    'c_code': {
        'name': 'C Code',
        'target_type': TargetType.EXECUTABLE,
        'description': 'Generate C code',
        'codegen_class': 'CCodeGenerator',
        'file_extension': '.c',
        'platform': 'all'
    }
}

class PrintASTGenerator(BaseCodeGenerator):
    """Generate formatted AST output"""
    
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate formatted AST string"""
        from ast_nodes import print_ast
        return print_ast(ir)
    
    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.EXECUTABLE]

class PrintIRGenerator(BaseCodeGenerator):
    """Generate formatted IR output"""
    
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate formatted IR string"""
        return self._format_ir(ir, 0)
    
    def _format_ir(self, node, indent_level):
        """Format IR node with indentation"""
        indent = "  " * indent_level
        
        if isinstance(node, Program):
            result = f"{indent}Program:\n"
            for stmt in node.statements:
                result += self._format_ir(stmt, indent_level + 1)
            return result
        
        elif isinstance(node, VariableDeclaration):
            type_info = f" ({node.var_type})" if node.var_type else ""
            mut_info = " mut" if getattr(node, 'is_mutable', False) else ""
            const_info = " const" if getattr(node, 'is_const', False) else ""
            result = f"{indent}VariableDeclaration{type_info}{mut_info}{const_info}: {node.name}\n"
            if node.initializer:
                result += self._format_ir(node.initializer, indent_level + 1)
            return result
        
        elif isinstance(node, Assignment):
            result = f"{indent}Assignment: {node.name} {node.operator}\n"
            result += self._format_ir(node.value, indent_level + 1)
            return result
        
        elif isinstance(node, BinaryOperation):
            result = f"{indent}BinaryOperation: {node.operator}\n"
            result += self._format_ir(node.left, indent_level + 1)
            result += self._format_ir(node.right, indent_level + 1)
            return result
        
        elif isinstance(node, UnaryOperation):
            result = f"{indent}UnaryOperation: {node.operator}\n"
            result += self._format_ir(node.operand, indent_level + 1)
            return result
        
        elif isinstance(node, Literal):
            return f"{indent}Literal ({node.literal_type}): {node.value}\n"
        
        elif isinstance(node, Identifier):
            return f"{indent}Identifier: {node.name}\n"
        
        elif isinstance(node, FunctionDeclaration):
            async_info = " async" if getattr(node, 'is_async', False) else ""
            return_info = f" -> {node.return_type}" if node.return_type else ""
            result = f"{indent}FunctionDeclaration{async_info}{return_info}: {node.name}\n"
            if node.parameters:
                result += f"{indent}  Parameters:\n"
                for param in node.parameters:
                    result += self._format_ir(param, indent_level + 2)
            if node.body:
                result += f"{indent}  Body:\n"
                result += self._format_ir(node.body, indent_level + 2)
            return result
        
        elif isinstance(node, Block):
            result = f"{indent}Block:\n"
            for stmt in node.statements:
                result += self._format_ir(stmt, indent_level + 1)
            return result
        
        elif isinstance(node, IfStatement):
            result = f"{indent}IfStatement:\n"
            result += f"{indent}  Condition:\n"
            result += self._format_ir(node.condition, indent_level + 2)
            result += f"{indent}  Then:\n"
            result += self._format_ir(node.then_branch, indent_level + 2)
            if node.else_branch:
                result += f"{indent}  Else:\n"
                result += self._format_ir(node.else_branch, indent_level + 2)
            return result
        
        elif isinstance(node, WhileStatement):
            result = f"{indent}WhileStatement:\n"
            result += f"{indent}  Condition:\n"
            result += self._format_ir(node.condition, indent_level + 2)
            result += f"{indent}  Body:\n"
            result += self._format_ir(node.body, indent_level + 2)
            return result
        
        elif isinstance(node, ForStatement):
            result = f"{indent}ForStatement:\n"
            if node.initializer:
                result += f"{indent}  Initializer:\n"
                result += self._format_ir(node.initializer, indent_level + 2)
            if node.condition:
                result += f"{indent}  Condition:\n"
                result += self._format_ir(node.condition, indent_level + 2)
            if node.increment:
                result += f"{indent}  Increment:\n"
                result += self._format_ir(node.increment, indent_level + 2)
            result += f"{indent}  Body:\n"
            result += self._format_ir(node.body, indent_level + 2)
            return result
        
        elif isinstance(node, ReturnStatement):
            result = f"{indent}ReturnStatement\n"
            if node.value:
                result += self._format_ir(node.value, indent_level + 1)
            return result
        
        else:
            return f"{indent}Unknown node: {type(node).__name__}\n"
    
    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.EXECUTABLE]

class PythonCodeGenerator(BaseCodeGenerator):
    """Generate Python code from AST"""
    
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate Python code"""
        return self._generate_python_code(ir)
    
    def _generate_python_code(self, node):
        """Generate Python code for a node"""
        if isinstance(node, Program):
            lines = []
            for stmt in node.statements:
                code = self._generate_python_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, VariableDeclaration):
            if node.initializer:
                return f"{node.name} = {self._generate_python_code(node.initializer)}"
            else:
                return f"{node.name} = None"
        
        elif isinstance(node, Assignment):
            return f"{node.name} = {self._generate_python_code(node.value)}"
        
        elif isinstance(node, BinaryOperation):
            left = self._generate_python_code(node.left)
            right = self._generate_python_code(node.right)
            
            # Handle operator mapping
            operator_map = {
                '&&': 'and',
                '||': 'or',
                '!=': '!=',
                '==': '==',
                '!': 'not'
            }
            op = operator_map.get(node.operator, node.operator)
            
            return f"({left} {op} {right})"
        
        elif isinstance(node, UnaryOperation):
            operand = self._generate_python_code(node.operand)
            if node.operator == '!':
                return f"not {operand}"
            else:
                return f"{node.operator}{operand}"
        
        elif isinstance(node, Literal):
            if node.literal_type == 'string':
                return f'"{node.value}"'
            elif node.literal_type == 'bool':
                return 'True' if node.value else 'False'
            else:
                return str(node.value)
        
        elif isinstance(node, Identifier):
            return node.name
        
        elif isinstance(node, FunctionDeclaration):
            params = ', '.join([param.name for param in node.parameters])
            body = self._generate_python_code(node.body) if node.body else 'pass'
            return f"def {node.name}({params}):\n    {body.replace(chr(10), chr(10) + '    ')}"
        
        elif isinstance(node, Block):
            lines = []
            for stmt in node.statements:
                code = self._generate_python_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, IfStatement):
            condition = self._generate_python_code(node.condition)
            then_branch = self._generate_python_code(node.then_branch)
            else_branch = self._generate_python_code(node.else_branch) if node.else_branch else None
            
            result = f"if {condition}:\n    {then_branch.replace(chr(10), chr(10) + '    ')}"
            if else_branch:
                result += f"\nelse:\n    {else_branch.replace(chr(10), chr(10) + '    ')}"
            return result
        
        elif isinstance(node, WhileStatement):
            condition = self._generate_python_code(node.condition)
            body = self._generate_python_code(node.body)
            return f"while {condition}:\n    {body.replace(chr(10), chr(10) + '    ')}"
        
        elif isinstance(node, ForStatement):
            # Simplified for loop
            body = self._generate_python_code(node.body)
            return f"for i in range(10):\n    {body.replace(chr(10), chr(10) + '    ')}"
        
        elif isinstance(node, ReturnStatement):
            if node.value:
                return f"return {self._generate_python_code(node.value)}"
            else:
                return "return"
        
        else:
            return f"# Unknown node: {type(node).__name__}"
    
    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.EXECUTABLE]

class JavaScriptCodeGenerator(BaseCodeGenerator):
    """Generate JavaScript code from AST"""
    
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate JavaScript code"""
        return self._generate_javascript_code(ir)
    
    def _generate_javascript_code(self, node):
        """Generate JavaScript code for a node"""
        if isinstance(node, Program):
            lines = []
            for stmt in node.statements:
                code = self._generate_javascript_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, VariableDeclaration):
            if node.initializer:
                return f"let {node.name} = {self._generate_javascript_code(node.initializer)};"
            else:
                return f"let {node.name};"
        
        elif isinstance(node, Assignment):
            return f"{node.name} = {self._generate_javascript_code(node.value)};"
        
        elif isinstance(node, BinaryOperation):
            left = self._generate_javascript_code(node.left)
            right = self._generate_javascript_code(node.right)
            
            # Handle operator mapping
            operator_map = {
                '&&': '&&',
                '||': '||',
                '!=': '!=',
                '==': '==',
                '!': '!'
            }
            op = operator_map.get(node.operator, node.operator)
            
            return f"({left} {op} {right})"
        
        elif isinstance(node, UnaryOperation):
            operand = self._generate_javascript_code(node.operand)
            return f"{node.operator}{operand}"
        
        elif isinstance(node, Literal):
            if node.literal_type == 'string':
                return f'"{node.value}"'
            elif node.literal_type == 'bool':
                return 'true' if node.value else 'false'
            else:
                return str(node.value)
        
        elif isinstance(node, Identifier):
            return node.name
        
        elif isinstance(node, FunctionDeclaration):
            params = ', '.join([param.name for param in node.parameters])
            body = self._generate_javascript_code(node.body) if node.body else 'return;'
            return f"function {node.name}({params}) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, Block):
            lines = []
            for stmt in node.statements:
                code = self._generate_javascript_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, IfStatement):
            condition = self._generate_javascript_code(node.condition)
            then_branch = self._generate_javascript_code(node.then_branch)
            else_branch = self._generate_javascript_code(node.else_branch) if node.else_branch else None
            
            result = f"if ({condition}) {{\n    {then_branch.replace(chr(10), chr(10) + '    ')}\n}}"
            if else_branch:
                result += f" else {{\n    {else_branch.replace(chr(10), chr(10) + '    ')}\n}}"
            return result
        
        elif isinstance(node, WhileStatement):
            condition = self._generate_javascript_code(node.condition)
            body = self._generate_javascript_code(node.body)
            return f"while ({condition}) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, ForStatement):
            # Simplified for loop
            body = self._generate_javascript_code(node.body)
            return f"for (let i = 0; i < 10; i++) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, ReturnStatement):
            if node.value:
                return f"return {self._generate_javascript_code(node.value)};"
            else:
                return "return;"
        
        else:
            return f"// Unknown node: {type(node).__name__}"
    
    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.WEB]

class CCodeGenerator(BaseCodeGenerator):
    """Generate C code from AST"""
    
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate C code"""
        return self._generate_c_code(ir)
    
    def _generate_c_code(self, node):
        """Generate C code for a node"""
        if isinstance(node, Program):
            lines = []
            for stmt in node.statements:
                code = self._generate_c_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, VariableDeclaration):
            if node.initializer:
                return f"{node.var_type or 'int'} {node.name} = {self._generate_c_code(node.initializer)};"
            else:
                return f"{node.var_type or 'int'} {node.name};"
        
        elif isinstance(node, Assignment):
            return f"{node.name} = {self._generate_c_code(node.value)};"
        
        elif isinstance(node, BinaryOperation):
            left = self._generate_c_code(node.left)
            right = self._generate_c_code(node.right)
            
            # Handle operator mapping
            operator_map = {
                '&&': '&&',
                '||': '||',
                '!=': '!=',
                '==': '==',
                '!': '!'
            }
            op = operator_map.get(node.operator, node.operator)
            
            return f"({left} {op} {right})"
        
        elif isinstance(node, UnaryOperation):
            operand = self._generate_c_code(node.operand)
            return f"{node.operator}{operand}"
        
        elif isinstance(node, Literal):
            if node.literal_type == 'string':
                return f'"{node.value}"'
            elif node.literal_type == 'bool':
                return '1' if node.value else '0'
            else:
                return str(node.value)
        
        elif isinstance(node, Identifier):
            return node.name
        
        elif isinstance(node, FunctionDeclaration):
            params = ', '.join([f"{param.var_type or 'int'} {param.name}" for param in node.parameters])
            return_type = node.return_type or 'int'
            body = self._generate_c_code(node.body) if node.body else 'return 0;'
            return f"{return_type} {node.name}({params}) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, Block):
            lines = []
            for stmt in node.statements:
                code = self._generate_c_code(stmt)
                if code:
                    lines.append(code)
            return '\n'.join(lines)
        
        elif isinstance(node, IfStatement):
            condition = self._generate_c_code(node.condition)
            then_branch = self._generate_c_code(node.then_branch)
            else_branch = self._generate_c_code(node.else_branch) if node.else_branch else None
            
            result = f"if ({condition}) {{\n    {then_branch.replace(chr(10), chr(10) + '    ')}\n}}"
            if else_branch:
                result += f" else {{\n    {else_branch.replace(chr(10), chr(10) + '    ')}\n}}"
            return result
        
        elif isinstance(node, WhileStatement):
            condition = self._generate_c_code(node.condition)
            body = self._generate_c_code(node.body)
            return f"while ({condition}) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, ForStatement):
            # Simplified for loop
            body = self._generate_c_code(node.body)
            return f"for (int i = 0; i < 10; i++) {{\n    {body.replace(chr(10), chr(10) + '    ')}\n}}"
        
        elif isinstance(node, ReturnStatement):
            if node.value:
                return f"return {self._generate_c_code(node.value)};"
            else:
                return "return;"
        
        else:
            return f"/* Unknown node: {type(node).__name__} */"
    
    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.EXECUTABLE]
