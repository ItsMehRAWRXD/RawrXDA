#!/usr/bin/env python3
"""
Universal AST Node Definitions
Core AST node classes used by all language parsers
"""

from dataclasses import dataclass, field
from typing import List, Any, Optional, Dict, Union
from abc import ABC, abstractmethod

@dataclass
class ASTNode(ABC):
    """Base AST node class"""
    node_type: str
    metadata: Dict[str, Any] = field(default_factory=dict)
    source_location: Optional[tuple] = None  # (line, column)
    
    def accept(self, visitor):
        """Accept a visitor for the visitor pattern"""
        method_name = f'visit_{self.node_type.lower()}'
        if hasattr(visitor, method_name):
            return getattr(visitor, method_name)(self)
        return visitor.visit_default(self)

@dataclass
class Program(ASTNode):
    """Root program node"""
    statements: List[ASTNode] = field(default_factory=list)
    
    def __post_init__(self):
        self.node_type = "Program"

@dataclass
class VariableDeclaration(ASTNode):
    """Variable declaration node"""
    var_type: Optional[str]
    name: str
    initializer: Optional[ASTNode]
    is_mutable: bool = False
    is_const: bool = False
    
    def __post_init__(self):
        self.node_type = "VariableDeclaration"

@dataclass
class Assignment(ASTNode):
    """Assignment expression node"""
    name: str
    value: ASTNode
    operator: str = "="
    
    def __post_init__(self):
        self.node_type = "Assignment"

@dataclass
class BinaryOperation(ASTNode):
    """Binary operation node"""
    operator: str
    left: ASTNode
    right: ASTNode
    
    def __post_init__(self):
        self.node_type = "BinaryOperation"

@dataclass
class UnaryOperation(ASTNode):
    """Unary operation node"""
    operator: str
    operand: ASTNode
    
    def __post_init__(self):
        self.node_type = "UnaryOperation"

@dataclass
class Literal(ASTNode):
    """Literal value node"""
    value: Any
    literal_type: str
    
    def __post_init__(self):
        self.node_type = "Literal"

@dataclass
class Identifier(ASTNode):
    """Identifier reference node"""
    name: str
    
    def __post_init__(self):
        self.node_type = "Identifier"

@dataclass
class FunctionDeclaration(ASTNode):
    """Function declaration node"""
    name: str
    parameters: List[ASTNode]
    return_type: Optional[str]
    body: Optional[ASTNode]
    is_async: bool = False
    
    def __post_init__(self):
        self.node_type = "FunctionDeclaration"

@dataclass
class FunctionCall(ASTNode):
    """Function call node"""
    name: str
    arguments: List[ASTNode]
    
    def __post_init__(self):
        self.node_type = "FunctionCall"

@dataclass
class Parameter(ASTNode):
    """Function parameter node"""
    name: str
    param_type: Optional[str]
    default_value: Optional[ASTNode]
    
    def __post_init__(self):
        self.node_type = "Parameter"

@dataclass
class Block(ASTNode):
    """Block statement node"""
    statements: List[ASTNode]
    
    def __post_init__(self):
        self.node_type = "Block"

@dataclass
class IfStatement(ASTNode):
    """If statement node"""
    condition: ASTNode
    then_branch: ASTNode
    else_branch: Optional[ASTNode]
    
    def __post_init__(self):
        self.node_type = "IfStatement"

@dataclass
class WhileStatement(ASTNode):
    """While statement node"""
    condition: ASTNode
    body: ASTNode
    
    def __post_init__(self):
        self.node_type = "WhileStatement"

@dataclass
class ForStatement(ASTNode):
    """For statement node"""
    initializer: Optional[ASTNode]
    condition: Optional[ASTNode]
    increment: Optional[ASTNode]
    body: ASTNode
    
    def __post_init__(self):
        self.node_type = "ForStatement"

@dataclass
class ReturnStatement(ASTNode):
    """Return statement node"""
    value: Optional[ASTNode]
    
    def __post_init__(self):
        self.node_type = "ReturnStatement"

@dataclass
class ClassDeclaration(ASTNode):
    """Class declaration node"""
    name: str
    superclass: Optional[str]
    methods: List[ASTNode]
    properties: List[ASTNode]
    
    def __post_init__(self):
        self.node_type = "ClassDeclaration"

@dataclass
class MethodDeclaration(ASTNode):
    """Method declaration node"""
    name: str
    parameters: List[ASTNode]
    return_type: Optional[str]
    body: Optional[ASTNode]
    is_static: bool = False
    is_async: bool = False
    
    def __post_init__(self):
        self.node_type = "MethodDeclaration"

@dataclass
class ArrayLiteral(ASTNode):
    """Array literal node"""
    elements: List[ASTNode]
    
    def __post_init__(self):
        self.node_type = "ArrayLiteral"

@dataclass
class ObjectLiteral(ASTNode):
    """Object literal node"""
    properties: List[ASTNode]
    
    def __post_init__(self):
        self.node_type = "ObjectLiteral"

@dataclass
class Property(ASTNode):
    """Object property node"""
    key: str
    value: ASTNode
    
    def __post_init__(self):
        self.node_type = "Property"

@dataclass
class MemberAccess(ASTNode):
    """Member access node"""
    object: ASTNode
    property: str
    
    def __post_init__(self):
        self.node_type = "MemberAccess"

@dataclass
class ArrayAccess(ASTNode):
    """Array access node"""
    array: ASTNode
    index: ASTNode
    
    def __post_init__(self):
        self.node_type = "ArrayAccess"

@dataclass
class ConditionalExpression(ASTNode):
    """Ternary operator node"""
    condition: ASTNode
    then_expr: ASTNode
    else_expr: ASTNode
    
    def __post_init__(self):
        self.node_type = "ConditionalExpression"

@dataclass
class ImportStatement(ASTNode):
    """Import statement node"""
    module: str
    imports: List[str]
    alias: Optional[str]
    
    def __post_init__(self):
        self.node_type = "ImportStatement"

@dataclass
class ExportStatement(ASTNode):
    """Export statement node"""
    declaration: Optional[ASTNode]
    name: Optional[str]
    
    def __post_init__(self):
        self.node_type = "ExportStatement"

# Visitor pattern for AST traversal
class ASTVisitor(ABC):
    """Base visitor class for AST traversal"""
    
    def visit_default(self, node: ASTNode):
        """Default visitor method"""
        return node
    
    def visit_program(self, node: Program):
        return self.visit_default(node)
    
    def visit_variabledeclaration(self, node: VariableDeclaration):
        return self.visit_default(node)
    
    def visit_assignment(self, node: Assignment):
        return self.visit_default(node)
    
    def visit_binaryoperation(self, node: BinaryOperation):
        return self.visit_default(node)
    
    def visit_unaryoperation(self, node: UnaryOperation):
        return self.visit_default(node)
    
    def visit_literal(self, node: Literal):
        return self.visit_default(node)
    
    def visit_identifier(self, node: Identifier):
        return self.visit_default(node)
    
    def visit_functiondeclaration(self, node: FunctionDeclaration):
        return self.visit_default(node)
    
    def visit_functioncall(self, node: FunctionCall):
        return self.visit_default(node)
    
    def visit_parameter(self, node: Parameter):
        return self.visit_default(node)
    
    def visit_block(self, node: Block):
        return self.visit_default(node)
    
    def visit_ifstatement(self, node: IfStatement):
        return self.visit_default(node)
    
    def visit_whilestatement(self, node: WhileStatement):
        return self.visit_default(node)
    
    def visit_forstatement(self, node: ForStatement):
        return self.visit_default(node)
    
    def visit_returnstatement(self, node: ReturnStatement):
        return self.visit_default(node)
    
    def visit_classdeclaration(self, node: ClassDeclaration):
        return self.visit_default(node)
    
    def visit_methoddeclaration(self, node: MethodDeclaration):
        return self.visit_default(node)
    
    def visit_arrayliteral(self, node: ArrayLiteral):
        return self.visit_default(node)
    
    def visit_objectliteral(self, node: ObjectLiteral):
        return self.visit_default(node)
    
    def visit_property(self, node: Property):
        return self.visit_default(node)
    
    def visit_memberaccess(self, node: MemberAccess):
        return self.visit_default(node)
    
    def visit_arrayaccess(self, node: ArrayAccess):
        return self.visit_default(node)
    
    def visit_conditionalexpression(self, node: ConditionalExpression):
        return self.visit_default(node)
    
    def visit_importstatement(self, node: ImportStatement):
        return self.visit_default(node)
    
    def visit_exportstatement(self, node: ExportStatement):
        return self.visit_default(node)

# Pretty printer for AST visualization
class ASTPrinter(ASTVisitor):
    """Pretty printer for AST nodes"""
    
    def __init__(self, indent: int = 0):
        self.indent = indent
        self.output = []
    
    def _indent(self):
        return "  " * self.indent
    
    def visit_program(self, node: Program):
        self.output.append(f"{self._indent()}Program:")
        self.indent += 1
        for stmt in node.statements:
            stmt.accept(self)
        self.indent -= 1
        return self.output
    
    def visit_variabledeclaration(self, node: VariableDeclaration):
        type_info = f" ({node.var_type})" if node.var_type else ""
        mut_info = " mut" if node.is_mutable else ""
        const_info = " const" if node.is_const else ""
        self.output.append(f"{self._indent()}VariableDeclaration{type_info}{mut_info}{const_info}: {node.name}")
        if node.initializer:
            self.indent += 1
            node.initializer.accept(self)
            self.indent -= 1
    
    def visit_assignment(self, node: Assignment):
        self.output.append(f"{self._indent()}Assignment: {node.name} {node.operator}")
        self.indent += 1
        node.value.accept(self)
        self.indent -= 1
    
    def visit_binaryoperation(self, node: BinaryOperation):
        self.output.append(f"{self._indent()}BinaryOperation: {node.operator}")
        self.indent += 1
        node.left.accept(self)
        node.right.accept(self)
        self.indent -= 1
    
    def visit_unaryoperation(self, node: UnaryOperation):
        self.output.append(f"{self._indent()}UnaryOperation: {node.operator}")
        self.indent += 1
        node.operand.accept(self)
        self.indent -= 1
    
    def visit_literal(self, node: Literal):
        self.output.append(f"{self._indent()}Literal ({node.literal_type}): {node.value}")
    
    def visit_identifier(self, node: Identifier):
        self.output.append(f"{self._indent()}Identifier: {node.name}")
    
    def visit_functiondeclaration(self, node: FunctionDeclaration):
        async_info = " async" if node.is_async else ""
        return_info = f" -> {node.return_type}" if node.return_type else ""
        self.output.append(f"{self._indent()}FunctionDeclaration{async_info}{return_info}: {node.name}")
        if node.parameters:
            self.indent += 1
            self.output.append(f"{self._indent()}Parameters:")
            self.indent += 1
            for param in node.parameters:
                param.accept(self)
            self.indent -= 2
        if node.body:
            self.indent += 1
            self.output.append(f"{self._indent()}Body:")
            self.indent += 1
            node.body.accept(self)
            self.indent -= 2
    
    def visit_functioncall(self, node: FunctionCall):
        self.output.append(f"{self._indent()}FunctionCall: {node.name}")
        if node.arguments:
            self.indent += 1
            for arg in node.arguments:
                arg.accept(self)
            self.indent -= 1
    
    def visit_parameter(self, node: Parameter):
        type_info = f": {node.param_type}" if node.param_type else ""
        default_info = f" = {node.default_value}" if node.default_value else ""
        self.output.append(f"{self._indent()}Parameter: {node.name}{type_info}{default_info}")
    
    def visit_block(self, node: Block):
        self.output.append(f"{self._indent()}Block:")
        self.indent += 1
        for stmt in node.statements:
            stmt.accept(self)
        self.indent -= 1
    
    def visit_ifstatement(self, node: IfStatement):
        self.output.append(f"{self._indent()}IfStatement:")
        self.indent += 1
        self.output.append(f"{self._indent()}Condition:")
        self.indent += 1
        node.condition.accept(self)
        self.indent -= 1
        self.output.append(f"{self._indent()}Then:")
        self.indent += 1
        node.then_branch.accept(self)
        self.indent -= 1
        if node.else_branch:
            self.output.append(f"{self._indent()}Else:")
            self.indent += 1
            node.else_branch.accept(self)
            self.indent -= 1
        self.indent -= 1
    
    def visit_whilestatement(self, node: WhileStatement):
        self.output.append(f"{self._indent()}WhileStatement:")
        self.indent += 1
        self.output.append(f"{self._indent()}Condition:")
        self.indent += 1
        node.condition.accept(self)
        self.indent -= 1
        self.output.append(f"{self._indent()}Body:")
        self.indent += 1
        node.body.accept(self)
        self.indent -= 2
    
    def visit_forstatement(self, node: ForStatement):
        self.output.append(f"{self._indent()}ForStatement:")
        self.indent += 1
        if node.initializer:
            self.output.append(f"{self._indent()}Initializer:")
            self.indent += 1
            node.initializer.accept(self)
            self.indent -= 1
        if node.condition:
            self.output.append(f"{self._indent()}Condition:")
            self.indent += 1
            node.condition.accept(self)
            self.indent -= 1
        if node.increment:
            self.output.append(f"{self._indent()}Increment:")
            self.indent += 1
            node.increment.accept(self)
            self.indent -= 1
        self.output.append(f"{self._indent()}Body:")
        self.indent += 1
        node.body.accept(self)
        self.indent -= 2
    
    def visit_returnstatement(self, node: ReturnStatement):
        self.output.append(f"{self._indent()}ReturnStatement")
        if node.value:
            self.indent += 1
            node.value.accept(self)
            self.indent -= 1
    
    def visit_classdeclaration(self, node: ClassDeclaration):
        super_info = f" extends {node.superclass}" if node.superclass else ""
        self.output.append(f"{self._indent()}ClassDeclaration: {node.name}{super_info}")
        if node.properties:
            self.indent += 1
            self.output.append(f"{self._indent()}Properties:")
            self.indent += 1
            for prop in node.properties:
                prop.accept(self)
            self.indent -= 2
        if node.methods:
            self.indent += 1
            self.output.append(f"{self._indent()}Methods:")
            self.indent += 1
            for method in node.methods:
                method.accept(self)
            self.indent -= 2
    
    def visit_methoddeclaration(self, node: MethodDeclaration):
        static_info = " static" if node.is_static else ""
        async_info = " async" if node.is_async else ""
        return_info = f" -> {node.return_type}" if node.return_type else ""
        self.output.append(f"{self._indent()}MethodDeclaration{static_info}{async_info}{return_info}: {node.name}")
        if node.parameters:
            self.indent += 1
            self.output.append(f"{self._indent()}Parameters:")
            self.indent += 1
            for param in node.parameters:
                param.accept(self)
            self.indent -= 2
        if node.body:
            self.indent += 1
            self.output.append(f"{self._indent()}Body:")
            self.indent += 1
            node.body.accept(self)
            self.indent -= 2
    
    def visit_arrayliteral(self, node: ArrayLiteral):
        self.output.append(f"{self._indent()}ArrayLiteral:")
        self.indent += 1
        for element in node.elements:
            element.accept(self)
        self.indent -= 1
    
    def visit_objectliteral(self, node: ObjectLiteral):
        self.output.append(f"{self._indent()}ObjectLiteral:")
        self.indent += 1
        for prop in node.properties:
            prop.accept(self)
        self.indent -= 1
    
    def visit_property(self, node: Property):
        self.output.append(f"{self._indent()}Property: {node.key}")
        self.indent += 1
        node.value.accept(self)
        self.indent -= 1
    
    def visit_memberaccess(self, node: MemberAccess):
        self.output.append(f"{self._indent()}MemberAccess: .{node.property}")
        self.indent += 1
        node.object.accept(self)
        self.indent -= 1
    
    def visit_arrayaccess(self, node: ArrayAccess):
        self.output.append(f"{self._indent()}ArrayAccess:")
        self.indent += 1
        self.output.append(f"{self._indent()}Array:")
        self.indent += 1
        node.array.accept(self)
        self.indent -= 1
        self.output.append(f"{self._indent()}Index:")
        self.indent += 1
        node.index.accept(self)
        self.indent -= 2
    
    def visit_conditionalexpression(self, node: ConditionalExpression):
        self.output.append(f"{self._indent()}ConditionalExpression:")
        self.indent += 1
        self.output.append(f"{self._indent()}Condition:")
        self.indent += 1
        node.condition.accept(self)
        self.indent -= 1
        self.output.append(f"{self._indent()}Then:")
        self.indent += 1
        node.then_expr.accept(self)
        self.indent -= 1
        self.output.append(f"{self._indent()}Else:")
        self.indent += 1
        node.else_expr.accept(self)
        self.indent -= 2
    
    def visit_importstatement(self, node: ImportStatement):
        alias_info = f" as {node.alias}" if node.alias else ""
        self.output.append(f"{self._indent()}ImportStatement: {node.module}{alias_info}")
        if node.imports:
            self.indent += 1
            for imp in node.imports:
                self.output.append(f"{self._indent()}{imp}")
            self.indent -= 1
    
    def visit_exportstatement(self, node: ExportStatement):
        if node.declaration:
            self.output.append(f"{self._indent()}ExportStatement:")
            self.indent += 1
            node.declaration.accept(self)
            self.indent -= 1
        else:
            self.output.append(f"{self._indent()}ExportStatement: {node.name}")
    
    def get_output(self) -> str:
        """Get the formatted output"""
        return "\n".join(self.output)

def print_ast(node: ASTNode) -> str:
    """Pretty print an AST node"""
    printer = ASTPrinter()
    node.accept(printer)
    return printer.get_output()
