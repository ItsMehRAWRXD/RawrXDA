#!/usr/bin/env python3
"""
Java to C++ Code Generator
Transpiles Java AST to C++ source code
"""

from extensible_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall
from ast_visitor import ASTVisitor
from typing import List
import io

BACKEND_TARGET_INFO = {
    'java_to_cpp': BackendTargetInfo(
        name='Java to C++ Transpiler',
        target_type=TargetType.EXECUTABLE,
        description='Generates C++ code from a Java AST.',
        codegen_class='JavaToCppCodeGenerator',
        file_extension='.cpp',
        platform='cross-platform'
    )
}

class JavaToCppCodeGenerator(BaseCodeGenerator, ASTVisitor):
    """Java to C++ code generator"""
    
    def __init__(self):
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()

    def generate(self, ast: Program, target_info: BackendTargetInfo) -> str:
        """Generate C++ code from Java AST"""
        self.output_stream = io.StringIO()
        self.indent_level = 0
        self.headers = set()
        self.headers.add("iostream")
        self.headers.add("string")

        # Write includes
        for header in sorted(self.headers):
            self._writeline(f"#include <{header}>")
        self._writeline("")
        
        # Write main function
        self._writeline("int main() {")
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
        self._write(f"auto {node.name} = ")
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
            self.output_stream.write("std::cout")
        else:
            self.output_stream.write(node.name)

    def visit_FunctionCall(self, node: FunctionCall):
        """Visit function call node"""
        if isinstance(node.callee, Identifier) and node.callee.name == "System.out.println":
            self.visit(node.callee)
            self.output_stream.write(" << ")
            self.visit(node.arguments[0])
            self._writeline(" << std::endl;")
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
