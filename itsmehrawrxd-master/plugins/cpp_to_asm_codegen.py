"""
C++ to x86 Assembly Code Generator
Generates x86-64 assembly from C++ AST using GAS syntax
"""

from extensible_compiler_system import BaseCodeGenerator, BackendTargetInfo, TargetType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall
from ast_visitor import ASTVisitor
import io
from typing import List, Dict, Any

BACKEND_TARGET_INFO = {
    'cpp_x86_asm': BackendTargetInfo(
        name='C++ to x86 Assembly',
        target_type=TargetType.EXECUTABLE,
        description='Generates x86-64 assembly from C++ AST using GAS syntax.',
        codegen_class='CppToX86AssemblyCodeGenerator',
        file_extension='.s',
        platform='linux'
    )
}

class CppToX86AssemblyCodeGenerator(BaseCodeGenerator, ASTVisitor):
    def __init__(self):
        self.output_stream = io.StringIO()
        self.variables: Dict[str, int] = {}
        self.var_stack_offset = 0
        self.label_counter = 0

    def generate(self, ast: Program, target_info: BackendTargetInfo) -> str:
        self.output_stream = io.StringIO()
        self.variables = {}
        self.var_stack_offset = 0
        self.label_counter = 0

        # Assembly header
        self._writeline(".intel_syntax noprefix")
        self._writeline(".text")
        self._writeline(".global main")
        self._writeline("")
        self._writeline("main:")
        self._writeline("    push    rbp")
        self._writeline("    mov     rbp, rsp")

        # Process AST
        self.visit(ast)
        
        # Cleanup and return
        self._writeline("    mov     eax, 0")
        self._writeline("    leave")
        self._writeline("    ret")

        return self.output_stream.getvalue()

    def _write(self, text: str):
        self.output_stream.write(text)

    def _writeline(self, text: str = ""):
        self.output_stream.write(text + "\n")

    def _get_next_label(self) -> str:
        self.label_counter += 1
        return f"L{self.label_counter}"

    def visit_Program(self, node: Program):
        for statement in node.statements:
            self.visit(statement)

    def visit_VariableDeclaration(self, node: VariableDeclaration):
        # Allocate stack space for variable
        self._writeline("    sub rsp, 8")
        self.var_stack_offset -= 8
        self.variables[node.name] = self.var_stack_offset
        
        # Initialize if present
        if hasattr(node, 'initializer') and node.initializer:
            self.visit(node.initializer)
            self._writeline(f"    mov [rbp{self.var_stack_offset}], rax")

    def visit_Assignment(self, node: Assignment):
        # Evaluate right-hand side
        self.visit(node.value)
        
        # Store in variable
        if node.name in self.variables:
            offset = self.variables[node.name]
            self._writeline(f"    mov [rbp{offset}], rax")
        else:
            raise Exception(f"Undeclared variable: {node.name}")

    def visit_BinaryOperation(self, node: BinaryOperation):
        # Handle different operators
        if node.op == '+':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    add rax, rdi")
        elif node.op == '-':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    sub rax, rdi")
        elif node.op == '*':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    imul rax, rdi")
        elif node.op == '/':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    cqo")
            self._writeline("    idiv rdi")
        elif node.op == '==':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    cmp rax, rdi")
            self._writeline("    sete al")
            self._writeline("    movzx rax, al")
        elif node.op == '!=':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    cmp rax, rdi")
            self._writeline("    setne al")
            self._writeline("    movzx rax, al")
        elif node.op == '<':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    cmp rax, rdi")
            self._writeline("    setl al")
            self._writeline("    movzx rax, al")
        elif node.op == '>':
            self.visit(node.left)
            self._writeline("    push rax")
            self.visit(node.right)
            self._writeline("    pop rdi")
            self._writeline("    cmp rax, rdi")
            self._writeline("    setg al")
            self._writeline("    movzx rax, al")
        else:
            raise Exception(f"Unsupported operator: {node.op}")

    def visit_Literal(self, node: Literal):
        if isinstance(node.value, str):
            # String literals need special handling
            label = self._get_next_label()
            self._writeline(f"    lea rax, [{label}]")
            self._writeline(f"    jmp {label}_end")
            self._writeline(f"{label}:")
            self._writeline(f'    .string "{node.value}"')
            self._writeline(f"{label}_end:")
        else:
            self._writeline(f"    mov rax, {node.value}")

    def visit_Identifier(self, node: Identifier):
        if node.name in self.variables:
            offset = self.variables[node.name]
            self._writeline(f"    mov rax, [rbp{offset}]")
        else:
            raise Exception(f"Undeclared variable: {node.name}")

    def visit_FunctionCall(self, node: FunctionCall):
        # Handle function calls (simplified)
        if isinstance(node.callee, Identifier):
            if node.callee.name == "printf":
                # Handle printf calls
                self._writeline("    mov rdi, rax")  # Format string
                for i, arg in enumerate(node.arguments):
                    if i == 0:
                        self.visit(arg)
                        self._writeline("    mov rsi, rax")
                    elif i == 1:
                        self.visit(arg)
                        self._writeline("    mov rdx, rax")
                    # Add more registers as needed
                self._writeline("    call printf")
            else:
                # Generic function call
                for arg in node.arguments:
                    self.visit(arg)
                    self._writeline("    push rax")
                self._writeline(f"    call {node.callee.name}")
                # Clean up stack
                if node.arguments:
                    self._writeline(f"    add rsp, {len(node.arguments) * 8}")

    def get_supported_targets(self) -> List[TargetType]:
        return [TargetType.EXECUTABLE]