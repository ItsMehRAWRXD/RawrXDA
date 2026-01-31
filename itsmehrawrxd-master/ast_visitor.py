#!/usr/bin/env python3
"""
AST Visitor Pattern for Code Generation
Generic visitor for traversing AST nodes and generating code
"""

from typing import Any, List, Dict, Optional
from ast_nodes import (
    Program, VariableDeclaration, Assignment, BinaryOperation, 
    Literal, Identifier, FunctionDefinition, FunctionCall,
    IfStatement, ForStatement, WhileStatement, ReturnStatement,
    ArrayLiteral, ObjectLiteral, StructLiteral, MemberAccess
)

class ASTVisitor:
    """A generic visitor for AST nodes"""
    
    def visit(self, node: Any) -> Any:
        """Visit a node using the appropriate visitor method"""
        if node is None:
            return None
        
        method_name = f"visit_{node.__class__.__name__}"
        visitor_method = getattr(self, method_name, self.generic_visit)
        return visitor_method(node)
    
    def generic_visit(self, node: Any) -> Any:
        """Generic visitor for unknown node types"""
        raise NotImplementedError(f"No visit method for node type: {node.__class__.__name__}")
    
    def visit_Program(self, node: Program) -> Any:
        """Visit a Program node"""
        raise NotImplementedError("visit_Program must be implemented by subclasses")
    
    def visit_VariableDeclaration(self, node: VariableDeclaration) -> Any:
        """Visit a VariableDeclaration node"""
        raise NotImplementedError("visit_VariableDeclaration must be implemented by subclasses")
    
    def visit_Assignment(self, node: Assignment) -> Any:
        """Visit an Assignment node"""
        raise NotImplementedError("visit_Assignment must be implemented by subclasses")
    
    def visit_BinaryOperation(self, node: BinaryOperation) -> Any:
        """Visit a BinaryOperation node"""
        raise NotImplementedError("visit_BinaryOperation must be implemented by subclasses")
    
    def visit_Literal(self, node: Literal) -> Any:
        """Visit a Literal node"""
        raise NotImplementedError("visit_Literal must be implemented by subclasses")
    
    def visit_Identifier(self, node: Identifier) -> Any:
        """Visit an Identifier node"""
        raise NotImplementedError("visit_Identifier must be implemented by subclasses")
    
    def visit_FunctionDefinition(self, node: FunctionDefinition) -> Any:
        """Visit a FunctionDefinition node"""
        raise NotImplementedError("visit_FunctionDefinition must be implemented by subclasses")
    
    def visit_FunctionCall(self, node: FunctionCall) -> Any:
        """Visit a FunctionCall node"""
        raise NotImplementedError("visit_FunctionCall must be implemented by subclasses")
    
    def visit_IfStatement(self, node: Dict) -> Any:
        """Visit an IfStatement node"""
        raise NotImplementedError("visit_IfStatement must be implemented by subclasses")
    
    def visit_ForStatement(self, node: Dict) -> Any:
        """Visit a ForStatement node"""
        raise NotImplementedError("visit_ForStatement must be implemented by subclasses")
    
    def visit_WhileStatement(self, node: Dict) -> Any:
        """Visit a WhileStatement node"""
        raise NotImplementedError("visit_WhileStatement must be implemented by subclasses")
    
    def visit_ReturnStatement(self, node: Dict) -> Any:
        """Visit a ReturnStatement node"""
        raise NotImplementedError("visit_ReturnStatement must be implemented by subclasses")
    
    def visit_ArrayLiteral(self, node: Dict) -> Any:
        """Visit an ArrayLiteral node"""
        raise NotImplementedError("visit_ArrayLiteral must be implemented by subclasses")
    
    def visit_ObjectLiteral(self, node: Dict) -> Any:
        """Visit an ObjectLiteral node"""
        raise NotImplementedError("visit_ObjectLiteral must be implemented by subclasses")
    
    def visit_StructLiteral(self, node: Dict) -> Any:
        """Visit a StructLiteral node"""
        raise NotImplementedError("visit_StructLiteral must be implemented by subclasses")
    
    def visit_MemberAccess(self, node: Dict) -> Any:
        """Visit a MemberAccess node"""
        raise NotImplementedError("visit_MemberAccess must be implemented by subclasses")

class CodeGenerator(ASTVisitor):
    """Base class for code generators"""
    
    def __init__(self):
        self.output_stream = None
        self.indent_level = 0
        self.headers = set()
        self.imports = set()
        self.namespace = None
    
    def generate(self, ast: Program) -> str:
        """Generate code from AST"""
        raise NotImplementedError("generate must be implemented by subclasses")
    
    def _write(self, text: str):
        """Write text to output stream"""
        if self.output_stream:
            self.output_stream.write("    " * self.indent_level)
            self.output_stream.write(text)
    
    def _writeline(self, text: str = ""):
        """Write a line to output stream"""
        if self.output_stream:
            self.output_stream.write("    " * self.indent_level)
            self.output_stream.write(text + "\n")
    
    def _indent(self):
        """Increase indentation level"""
        self.indent_level += 1
    
    def _dedent(self):
        """Decrease indentation level"""
        self.indent_level = max(0, self.indent_level - 1)
    
    def _add_header(self, header: str):
        """Add a header/include to the generated code"""
        self.headers.add(header)
    
    def _add_import(self, import_name: str):
        """Add an import to the generated code"""
        self.imports.add(import_name)
    
    def _get_type_for_literal(self, value: Any) -> str:
        """Get the appropriate type for a literal value"""
        if isinstance(value, bool):
            return "bool"
        elif isinstance(value, int):
            return "int"
        elif isinstance(value, float):
            return "double"
        elif isinstance(value, str):
            return "std::string"
        else:
            return "auto"
    
    def _get_type_for_expression(self, expr: Any) -> str:
        """Infer type for an expression (simplified type inference)"""
        if isinstance(expr, Literal):
            return self._get_type_for_literal(expr.value)
        elif isinstance(expr, Identifier):
            # In a real implementation, this would look up the variable's type
            return "auto"
        elif isinstance(expr, BinaryOperation):
            # Simple type inference for binary operations
            left_type = self._get_type_for_expression(expr.left)
            right_type = self._get_type_for_expression(expr.right)
            
            # If both sides are the same type, return that type
            if left_type == right_type:
                return left_type
            
            # If one is double and the other is int, return double
            if (left_type == "double" and right_type == "int") or (left_type == "int" and right_type == "double"):
                return "double"
            
            # Default to auto for complex cases
            return "auto"
        else:
            return "auto"
    
    def _escape_string(self, value: str) -> str:
        """Escape string for the target language"""
        # Basic string escaping - subclasses can override for language-specific needs
        return value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')
    
    def _format_number(self, value: Any) -> str:
        """Format number for the target language"""
        if isinstance(value, float):
            return str(value)
        else:
            return str(value)
    
    def _get_operator_mapping(self, op: str) -> str:
        """Get the target language equivalent of an operator"""
        # Default mapping - subclasses can override
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
            '**': '**',  # Will need special handling
            '//': '/',    # Integer division
        }
        return operator_mappings.get(op, op)
    
    def _handle_special_operators(self, op: str, left: Any, right: Any) -> str:
        """Handle special operators that need custom code generation"""
        if op == '**':
            # Power operator - needs special handling in most languages
            return f"std::pow({self.visit(left)}, {self.visit(right)})"
        elif op == '//':
            # Integer division
            return f"({self.visit(left)} / {self.visit(right)})"
        else:
            return f"{self.visit(left)} {self._get_operator_mapping(op)} {self.visit(right)}"
