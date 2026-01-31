#!/usr/bin/env python3
"""
Java to Rust Code Generator
Transpiles Java AST to Rust source code
"""

from extensible_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall
from ast_visitor import ASTVisitor
from typing import List
import io

BACKEND_TARGET_INFO = {
    'java_to_rust': BackendTargetInfo(
        name='Java to Rust Transpiler',
        target_type=TargetType.EXECUTABLE,
        description='Generates Rust code from a Java AST.',
        codegen_class='JavaToRustCodeGenerator',
        file_extension='.rs',
        platform='cross-platform'
    )
}

class JavaToRustCodeGenerator(BaseCodeGenerator, ASTVisitor):
    """Java to Rust code generator"""
    
    def __init__(self):
        self.output_stream = io.StringIO()
        self.indent_level = 0

    def generate(self, ast: Program, target_info: BackendTargetInfo) -> str:
        """Generate Rust code from Java AST"""
        self.output_stream = io.StringIO()
        self.indent_level = 0
        
        self._writeline("fn main() {")
        self.indent_level += 1
        
        for statement in ast.statements:
            self.visit(statement)
        
        self.indent_level -= 1
        self._writeline("}")
        return self.output_stream.getvalue()

    def _write(self, text: str):
        """Write text with proper indentation"""
        self.output_stream.write("    " * self.indent_level)
        self.output_stream.write(text)

    def _writeline(self, text: str = ""):
        """Write line with proper indentation"""
        self._write(text + "\n")

    def visit_Program(self, node: Program):
        """Visit program node"""
        pass  # Handled in `generate`

    def visit_Assignment(self, node: Assignment):
        """Visit assignment node"""
        self._write(f"let mut {node.name} = ")
        self.visit(node.value)
        self._writeline(";")

    def visit_BinaryOperation(self, node: BinaryOperation):
        """Visit binary operation node"""
        self.visit(node.left)
        self.output_stream.write(f" {node.op} ")
        self.visit(node.right)

    def visit_Literal(self, node: Literal):
        """Visit literal node"""
        if isinstance(node.value, str):
            self.output_stream.write(f'"{node.value}"')
        else:
            self.output_stream.write(str(node.value))

    def visit_Identifier(self, node: Identifier):
        """Visit identifier node"""
        if node.name == 'System.out.println':
            self.output_stream.write("println!")
        else:
            self.output_stream.write(node.name)

    def visit_FunctionCall(self, node: FunctionCall):
        """Visit function call node"""
        if isinstance(node.callee, Identifier) and node.callee.name == 'System.out.println':
            self._write("println!(\"{}\", ")
            self.visit(node.arguments[0])
            self._writeline(");")
        else:
            self.visit(node.callee)
            self.output_stream.write("(")
            for i, arg in enumerate(node.arguments):
                self.visit(arg)
                if i < len(node.arguments) - 1:
                    self.output_stream.write(", ")
            self._writeline(");")

    def get_supported_targets(self) -> List[TargetType]:
        """Get supported target types"""
        return [TargetType.EXECUTABLE]
