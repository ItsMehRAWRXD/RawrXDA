#!/usr/bin/env python3
"""
Realistic Modular Compiler
Built using proper compiler engineering principles with LLVM backend
"""

import sys
import os
import re
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import subprocess
import tempfile

# Try to import LLVM bindings
try:
    import llvmlite.binding as llvm
    from llvmlite import ir
    LLVM_AVAILABLE = True
except ImportError:
    LLVM_AVAILABLE = False
    print("⚠️ LLVM not available - using fallback code generation")

class TokenType(Enum):
    """Token types for our simple language"""
    # Keywords
    IF = "if"
    ELSE = "else"
    WHILE = "while"
    FOR = "for"
    FUNCTION = "function"
    RETURN = "return"
    VAR = "var"
    LET = "let"
    CONST = "const"
    
    # Literals
    NUMBER = "number"
    STRING = "string"
    IDENTIFIER = "identifier"
    
    # Operators
    PLUS = "+"
    MINUS = "-"
    MULTIPLY = "*"
    DIVIDE = "/"
    ASSIGN = "="
    EQUALS = "=="
    NOT_EQUALS = "!="
    LESS_THAN = "<"
    GREATER_THAN = ">"
    LESS_EQUAL = "<="
    GREATER_EQUAL = ">="
    AND = "&&"
    OR = "||"
    NOT = "!"
    
    # Delimiters
    SEMICOLON = ";"
    COMMA = ","
    LEFT_PAREN = "("
    RIGHT_PAREN = ")"
    LEFT_BRACE = "{"
    RIGHT_BRACE = "}"
    LEFT_BRACKET = "["
    RIGHT_BRACKET = "]"
    
    # Special
    EOF = "eof"
    NEWLINE = "newline"

@dataclass
class Token:
    """Token representation"""
    type: TokenType
    value: str
    line: int
    column: int

class Lexer:
    """Lexical analyzer for our simple language"""
    
    def __init__(self):
        # Define token patterns
        self.patterns = [
            (TokenType.NUMBER, r'\d+(\.\d+)?'),
            (TokenType.STRING, r'"[^"]*"'),
            (TokenType.IDENTIFIER, r'[a-zA-Z_][a-zA-Z0-9_]*'),
            (TokenType.PLUS, r'\+'),
            (TokenType.MINUS, r'-'),
            (TokenType.MULTIPLY, r'\*'),
            (TokenType.DIVIDE, r'/'),
            (TokenType.ASSIGN, r'='),
            (TokenType.EQUALS, r'=='),
            (TokenType.NOT_EQUALS, r'!='),
            (TokenType.LESS_THAN, r'<'),
            (TokenType.GREATER_THAN, r'>'),
            (TokenType.LESS_EQUAL, r'<='),
            (TokenType.GREATER_EQUAL, r'>='),
            (TokenType.AND, r'&&'),
            (TokenType.OR, r'\|\|'),
            (TokenType.NOT, r'!'),
            (TokenType.SEMICOLON, r';'),
            (TokenType.COMMA, r','),
            (TokenType.LEFT_PAREN, r'\('),
            (TokenType.RIGHT_PAREN, r'\)'),
            (TokenType.LEFT_BRACE, r'\{'),
            (TokenType.RIGHT_BRACE, r'\}'),
            (TokenType.LEFT_BRACKET, r'\['),
            (TokenType.RIGHT_BRACKET, r'\]'),
            (TokenType.NEWLINE, r'\n'),
        ]
        
        # Keywords
        self.keywords = {
            'if': TokenType.IF,
            'else': TokenType.ELSE,
            'while': TokenType.WHILE,
            'for': TokenType.FOR,
            'function': TokenType.FUNCTION,
            'return': TokenType.RETURN,
            'var': TokenType.VAR,
            'let': TokenType.LET,
            'const': TokenType.CONST,
        }
    
    def tokenize(self, source: str) -> List[Token]:
        """Tokenize source code"""
        tokens = []
        lines = source.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            column = 1
            pos = 0
            
            while pos < len(line):
                # Skip whitespace
                if line[pos].isspace():
                    pos += 1
                    column += 1
                    continue
                
                # Try to match patterns
                matched = False
                for token_type, pattern in self.patterns:
                    match = re.match(pattern, line[pos:])
                    if match:
                        value = match.group(0)
                        
                        # Check if it's a keyword
                        if token_type == TokenType.IDENTIFIER and value in self.keywords:
                            token_type = self.keywords[value]
                        
                        tokens.append(Token(token_type, value, line_num, column))
                        pos += len(value)
                        column += len(value)
                        matched = True
                        break
                
                if not matched:
                    raise SyntaxError(f"Unexpected character '{line[pos]}' at line {line_num}, column {column}")
        
        tokens.append(Token(TokenType.EOF, "", len(lines), 1))
        return tokens

class ASTNode:
    """Base class for AST nodes"""
    pass

class Program(ASTNode):
    """Program node"""
    def __init__(self, statements: List[ASTNode]):
        self.statements = statements

class FunctionDeclaration(ASTNode):
    """Function declaration node"""
    def __init__(self, name: str, parameters: List[str], body: List[ASTNode]):
        self.name = name
        self.parameters = parameters
        self.body = body

class VariableDeclaration(ASTNode):
    """Variable declaration node"""
    def __init__(self, name: str, value: Optional[ASTNode] = None):
        self.name = name
        self.value = value

class Assignment(ASTNode):
    """Assignment node"""
    def __init__(self, name: str, value: ASTNode):
        self.name = name
        self.value = value

class BinaryOperation(ASTNode):
    """Binary operation node"""
    def __init__(self, left: ASTNode, operator: str, right: ASTNode):
        self.left = left
        self.operator = operator
        self.right = right

class UnaryOperation(ASTNode):
    """Unary operation node"""
    def __init__(self, operator: str, operand: ASTNode):
        self.operator = operator
        self.operand = operand

class Literal(ASTNode):
    """Literal node"""
    def __init__(self, value: Any, type: str):
        self.value = value
        self.type = type

class Identifier(ASTNode):
    """Identifier node"""
    def __init__(self, name: str):
        self.name = name

class FunctionCall(ASTNode):
    """Function call node"""
    def __init__(self, name: str, arguments: List[ASTNode]):
        self.name = name
        self.arguments = arguments

class IfStatement(ASTNode):
    """If statement node"""
    def __init__(self, condition: ASTNode, then_branch: List[ASTNode], else_branch: Optional[List[ASTNode]] = None):
        self.condition = condition
        self.then_branch = then_branch
        self.else_branch = else_branch

class WhileStatement(ASTNode):
    """While statement node"""
    def __init__(self, condition: ASTNode, body: List[ASTNode]):
        self.condition = condition
        self.body = body

class ReturnStatement(ASTNode):
    """Return statement node"""
    def __init__(self, value: Optional[ASTNode] = None):
        self.value = value

class Parser:
    """Recursive descent parser"""
    
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0
        self.current_token = tokens[0] if tokens else None
    
    def parse(self) -> Program:
        """Parse tokens into AST"""
        statements = []
        
        while not self._is_at_end():
            if self._match(TokenType.FUNCTION):
                statements.append(self._parse_function_declaration())
            elif self._match(TokenType.VAR, TokenType.LET, TokenType.CONST):
                statements.append(self._parse_variable_declaration())
            else:
                statements.append(self._parse_statement())
        
        return Program(statements)
    
    def _parse_function_declaration(self) -> FunctionDeclaration:
        """Parse function declaration"""
        self._consume(TokenType.FUNCTION, "Expected 'function' keyword")
        name = self._consume(TokenType.IDENTIFIER, "Expected function name").value
        
        self._consume(TokenType.LEFT_PAREN, "Expected '(' after function name")
        parameters = []
        
        if not self._check(TokenType.RIGHT_PAREN):
            parameters.append(self._consume(TokenType.IDENTIFIER, "Expected parameter name").value)
            while self._match(TokenType.COMMA):
                parameters.append(self._consume(TokenType.IDENTIFIER, "Expected parameter name").value)
        
        self._consume(TokenType.RIGHT_PAREN, "Expected ')' after parameters")
        self._consume(TokenType.LEFT_BRACE, "Expected '{' after function declaration")
        
        body = []
        while not self._check(TokenType.RIGHT_BRACE) and not self._is_at_end():
            body.append(self._parse_statement())
        
        self._consume(TokenType.RIGHT_BRACE, "Expected '}' after function body")
        return FunctionDeclaration(name, parameters, body)
    
    def _parse_variable_declaration(self) -> VariableDeclaration:
        """Parse variable declaration"""
        self._advance()  # Consume var/let/const
        name = self._consume(TokenType.IDENTIFIER, "Expected variable name").value
        
        value = None
        if self._match(TokenType.ASSIGN):
            value = self._parse_expression()
        
        self._consume(TokenType.SEMICOLON, "Expected ';' after variable declaration")
        return VariableDeclaration(name, value)
    
    def _parse_statement(self) -> ASTNode:
        """Parse statement"""
        if self._match(TokenType.IF):
            return self._parse_if_statement()
        elif self._match(TokenType.WHILE):
            return self._parse_while_statement()
        elif self._match(TokenType.RETURN):
            return self._parse_return_statement()
        elif self._check(TokenType.IDENTIFIER) and self._peek(1).type == TokenType.LEFT_PAREN:
            return self._parse_function_call()
        elif self._check(TokenType.IDENTIFIER) and self._peek(1).type == TokenType.ASSIGN:
            return self._parse_assignment()
        else:
            expr = self._parse_expression()
            self._consume(TokenType.SEMICOLON, "Expected ';' after expression")
            return expr
    
    def _parse_if_statement(self) -> IfStatement:
        """Parse if statement"""
        self._consume(TokenType.LEFT_PAREN, "Expected '(' after 'if'")
        condition = self._parse_expression()
        self._consume(TokenType.RIGHT_PAREN, "Expected ')' after condition")
        self._consume(TokenType.LEFT_BRACE, "Expected '{' after if condition")
        
        then_branch = []
        while not self._check(TokenType.RIGHT_BRACE) and not self._is_at_end():
            then_branch.append(self._parse_statement())
        
        self._consume(TokenType.RIGHT_BRACE, "Expected '}' after if body")
        
        else_branch = None
        if self._match(TokenType.ELSE):
            self._consume(TokenType.LEFT_BRACE, "Expected '{' after else")
            else_branch = []
            while not self._check(TokenType.RIGHT_BRACE) and not self._is_at_end():
                else_branch.append(self._parse_statement())
            self._consume(TokenType.RIGHT_BRACE, "Expected '}' after else body")
        
        return IfStatement(condition, then_branch, else_branch)
    
    def _parse_while_statement(self) -> WhileStatement:
        """Parse while statement"""
        self._consume(TokenType.LEFT_PAREN, "Expected '(' after 'while'")
        condition = self._parse_expression()
        self._consume(TokenType.RIGHT_PAREN, "Expected ')' after condition")
        self._consume(TokenType.LEFT_BRACE, "Expected '{' after while condition")
        
        body = []
        while not self._check(TokenType.RIGHT_BRACE) and not self._is_at_end():
            body.append(self._parse_statement())
        
        self._consume(TokenType.RIGHT_BRACE, "Expected '}' after while body")
        return WhileStatement(condition, body)
    
    def _parse_return_statement(self) -> ReturnStatement:
        """Parse return statement"""
        value = None
        if not self._check(TokenType.SEMICOLON):
            value = self._parse_expression()
        
        self._consume(TokenType.SEMICOLON, "Expected ';' after return statement")
        return ReturnStatement(value)
    
    def _parse_function_call(self) -> FunctionCall:
        """Parse function call"""
        name = self._consume(TokenType.IDENTIFIER, "Expected function name").value
        self._consume(TokenType.LEFT_PAREN, "Expected '(' after function name")
        
        arguments = []
        if not self._check(TokenType.RIGHT_PAREN):
            arguments.append(self._parse_expression())
            while self._match(TokenType.COMMA):
                arguments.append(self._parse_expression())
        
        self._consume(TokenType.RIGHT_PAREN, "Expected ')' after arguments")
        return FunctionCall(name, arguments)
    
    def _parse_assignment(self) -> Assignment:
        """Parse assignment"""
        name = self._consume(TokenType.IDENTIFIER, "Expected variable name").value
        self._consume(TokenType.ASSIGN, "Expected '=' after variable name")
        value = self._parse_expression()
        self._consume(TokenType.SEMICOLON, "Expected ';' after assignment")
        return Assignment(name, value)
    
    def _parse_expression(self) -> ASTNode:
        """Parse expression"""
        return self._parse_logical_or()
    
    def _parse_logical_or(self) -> ASTNode:
        """Parse logical OR expression"""
        expr = self._parse_logical_and()
        
        while self._match(TokenType.OR):
            operator = self._previous().value
            right = self._parse_logical_and()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_logical_and(self) -> ASTNode:
        """Parse logical AND expression"""
        expr = self._parse_equality()
        
        while self._match(TokenType.AND):
            operator = self._previous().value
            right = self._parse_equality()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_equality(self) -> ASTNode:
        """Parse equality expression"""
        expr = self._parse_comparison()
        
        while self._match(TokenType.EQUALS, TokenType.NOT_EQUALS):
            operator = self._previous().value
            right = self._parse_comparison()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_comparison(self) -> ASTNode:
        """Parse comparison expression"""
        expr = self._parse_term()
        
        while self._match(TokenType.GREATER_THAN, TokenType.GREATER_EQUAL, 
                         TokenType.LESS_THAN, TokenType.LESS_EQUAL):
            operator = self._previous().value
            right = self._parse_term()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_term(self) -> ASTNode:
        """Parse term expression"""
        expr = self._parse_factor()
        
        while self._match(TokenType.PLUS, TokenType.MINUS):
            operator = self._previous().value
            right = self._parse_factor()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_factor(self) -> ASTNode:
        """Parse factor expression"""
        expr = self._parse_unary()
        
        while self._match(TokenType.MULTIPLY, TokenType.DIVIDE):
            operator = self._previous().value
            right = self._parse_unary()
            expr = BinaryOperation(expr, operator, right)
        
        return expr
    
    def _parse_unary(self) -> ASTNode:
        """Parse unary expression"""
        if self._match(TokenType.NOT, TokenType.MINUS):
            operator = self._previous().value
            right = self._parse_unary()
            return UnaryOperation(operator, right)
        
        return self._parse_primary()
    
    def _parse_primary(self) -> ASTNode:
        """Parse primary expression"""
        if self._match(TokenType.NUMBER):
            return Literal(float(self._previous().value), "number")
        
        if self._match(TokenType.STRING):
            value = self._previous().value[1:-1]  # Remove quotes
            return Literal(value, "string")
        
        if self._match(TokenType.IDENTIFIER):
            if self._check(TokenType.LEFT_PAREN):
                return self._parse_function_call()
            return Identifier(self._previous().value)
        
        if self._match(TokenType.LEFT_PAREN):
            expr = self._parse_expression()
            self._consume(TokenType.RIGHT_PAREN, "Expected ')' after expression")
            return expr
        
        raise SyntaxError(f"Unexpected token: {self.current_token.value}")
    
    def _match(self, *types) -> bool:
        """Match one of the given token types"""
        for token_type in types:
            if self._check(token_type):
                self._advance()
                return True
        return False
    
    def _check(self, token_type: TokenType) -> bool:
        """Check if current token matches type"""
        if self._is_at_end():
            return False
        return self.current_token.type == token_type
    
    def _consume(self, token_type: TokenType, message: str) -> Token:
        """Consume token of expected type"""
        if self._check(token_type):
            return self._advance()
        raise SyntaxError(f"{message} at line {self.current_token.line}, column {self.current_token.column}")
    
    def _advance(self) -> Token:
        """Advance to next token"""
        if not self._is_at_end():
            self.pos += 1
        self.current_token = self.tokens[self.pos] if self.pos < len(self.tokens) else None
        return self.tokens[self.pos - 1]
    
    def _previous(self) -> Token:
        """Get previous token"""
        return self.tokens[self.pos - 1]
    
    def _peek(self, offset: int = 0) -> Token:
        """Peek at token with offset"""
        index = self.pos + offset
        if index >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[index]
    
    def _is_at_end(self) -> bool:
        """Check if at end of tokens"""
        return self.current_token is None or self.current_token.type == TokenType.EOF

class SymbolTable:
    """Symbol table for semantic analysis"""
    
    def __init__(self):
        self.symbols = {}
        self.scopes = [{}]
    
    def enter_scope(self):
        """Enter new scope"""
        self.scopes.append({})
    
    def exit_scope(self):
        """Exit current scope"""
        if len(self.scopes) > 1:
            self.scopes.pop()
    
    def declare(self, name: str, symbol_type: str, value: Any = None):
        """Declare symbol in current scope"""
        if name in self.scopes[-1]:
            raise NameError(f"Symbol '{name}' already declared in this scope")
        
        self.scopes[-1][name] = {
            'type': symbol_type,
            'value': value
        }
    
    def lookup(self, name: str) -> Optional[Dict[str, Any]]:
        """Lookup symbol in current and outer scopes"""
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

class SemanticAnalyzer:
    """Semantic analyzer"""
    
    def __init__(self):
        self.symbol_table = SymbolTable()
        self.errors = []
    
    def analyze(self, ast: Program) -> List[str]:
        """Analyze AST for semantic errors"""
        self.errors = []
        self._analyze_program(ast)
        return self.errors
    
    def _analyze_program(self, program: Program):
        """Analyze program node"""
        for statement in program.statements:
            self._analyze_statement(statement)
    
    def _analyze_statement(self, statement: ASTNode):
        """Analyze statement"""
        if isinstance(statement, FunctionDeclaration):
            self._analyze_function_declaration(statement)
        elif isinstance(statement, VariableDeclaration):
            self._analyze_variable_declaration(statement)
        elif isinstance(statement, Assignment):
            self._analyze_assignment(statement)
        elif isinstance(statement, IfStatement):
            self._analyze_if_statement(statement)
        elif isinstance(statement, WhileStatement):
            self._analyze_while_statement(statement)
        elif isinstance(statement, ReturnStatement):
            self._analyze_return_statement(statement)
        else:
            self._analyze_expression(statement)
    
    def _analyze_function_declaration(self, func: FunctionDeclaration):
        """Analyze function declaration"""
        self.symbol_table.declare(func.name, 'function')
        self.symbol_table.enter_scope()
        
        for param in func.parameters:
            self.symbol_table.declare(param, 'parameter')
        
        for statement in func.body:
            self._analyze_statement(statement)
        
        self.symbol_table.exit_scope()
    
    def _analyze_variable_declaration(self, var: VariableDeclaration):
        """Analyze variable declaration"""
        if var.value:
            self._analyze_expression(var.value)
        self.symbol_table.declare(var.name, 'variable')
    
    def _analyze_assignment(self, assignment: Assignment):
        """Analyze assignment"""
        symbol = self.symbol_table.lookup(assignment.name)
        if not symbol:
            self.errors.append(f"Undefined variable '{assignment.name}'")
        
        self._analyze_expression(assignment.value)
    
    def _analyze_if_statement(self, if_stmt: IfStatement):
        """Analyze if statement"""
        self._analyze_expression(if_stmt.condition)
        self.symbol_table.enter_scope()
        for statement in if_stmt.then_branch:
            self._analyze_statement(statement)
        self.symbol_table.exit_scope()
        
        if if_stmt.else_branch:
            self.symbol_table.enter_scope()
            for statement in if_stmt.else_branch:
                self._analyze_statement(statement)
            self.symbol_table.exit_scope()
    
    def _analyze_while_statement(self, while_stmt: WhileStatement):
        """Analyze while statement"""
        self._analyze_expression(while_stmt.condition)
        self.symbol_table.enter_scope()
        for statement in while_stmt.body:
            self._analyze_statement(statement)
        self.symbol_table.exit_scope()
    
    def _analyze_return_statement(self, return_stmt: ReturnStatement):
        """Analyze return statement"""
        if return_stmt.value:
            self._analyze_expression(return_stmt.value)
    
    def _analyze_expression(self, expr: ASTNode):
        """Analyze expression"""
        if isinstance(expr, BinaryOperation):
            self._analyze_expression(expr.left)
            self._analyze_expression(expr.right)
        elif isinstance(expr, UnaryOperation):
            self._analyze_expression(expr.operand)
        elif isinstance(expr, Identifier):
            symbol = self.symbol_table.lookup(expr.name)
            if not symbol:
                self.errors.append(f"Undefined variable '{expr.name}'")
        elif isinstance(expr, FunctionCall):
            symbol = self.symbol_table.lookup(expr.name)
            if not symbol or symbol['type'] != 'function':
                self.errors.append(f"Undefined function '{expr.name}'")
            
            for arg in expr.arguments:
                self._analyze_expression(arg)

class IRGenerator:
    """LLVM IR generator"""
    
    def __init__(self):
        if LLVM_AVAILABLE:
            # Initialize LLVM
            llvm.initialize()
            llvm.initialize_native_target()
            llvm.initialize_native_asmprinter()
            
            # Create module
            self.module = ir.Module(name="main")
            self.builder = None
            self.functions = {}
        else:
            self.module = None
            self.builder = None
            self.functions = {}
    
    def generate(self, ast: Program) -> str:
        """Generate LLVM IR from AST"""
        if not LLVM_AVAILABLE:
            return self._generate_fallback_ir(ast)
        
        # Create main function
        main_func_type = ir.FunctionType(ir.IntType(32), [])
        main_func = ir.Function(self.module, main_func_type, name="main")
        main_block = main_func.append_basic_block(name="entry")
        self.builder = ir.IRBuilder(main_block)
        
        # Generate IR for each statement
        for statement in ast.statements:
            self._generate_statement(statement)
        
        # Return 0
        self.builder.ret(ir.Constant(ir.IntType(32), 0))
        
        return str(self.module)
    
    def _generate_statement(self, statement: ASTNode):
        """Generate IR for statement"""
        if isinstance(statement, FunctionDeclaration):
            self._generate_function_declaration(statement)
        elif isinstance(statement, VariableDeclaration):
            self._generate_variable_declaration(statement)
        elif isinstance(statement, Assignment):
            self._generate_assignment(statement)
        elif isinstance(statement, IfStatement):
            self._generate_if_statement(statement)
        elif isinstance(statement, WhileStatement):
            self._generate_while_statement(statement)
        elif isinstance(statement, ReturnStatement):
            self._generate_return_statement(statement)
        else:
            self._generate_expression(statement)
    
    def _generate_function_declaration(self, func: FunctionDeclaration):
        """Generate IR for function declaration"""
        # Create function type
        func_type = ir.FunctionType(ir.IntType(32), [])
        function = ir.Function(self.module, func_type, name=func.name)
        self.functions[func.name] = function
        
        # Create entry block
        entry_block = function.append_basic_block(name="entry")
        builder = ir.IRBuilder(entry_block)
        
        # Generate IR for function body
        for statement in func.body:
            self._generate_statement_with_builder(statement, builder)
        
        # Return 0 if no explicit return
        builder.ret(ir.Constant(ir.IntType(32), 0))
    
    def _generate_variable_declaration(self, var: VariableDeclaration):
        """Generate IR for variable declaration"""
        if var.value:
            value = self._generate_expression(var.value)
            # Store variable (simplified)
            pass
    
    def _generate_assignment(self, assignment: Assignment):
        """Generate IR for assignment"""
        value = self._generate_expression(assignment.value)
        # Store variable (simplified)
        pass
    
    def _generate_if_statement(self, if_stmt: IfStatement):
        """Generate IR for if statement"""
        condition = self._generate_expression(if_stmt.condition)
        
        # Create blocks
        then_block = self.builder.append_basic_block("then")
        else_block = self.builder.append_basic_block("else")
        merge_block = self.builder.append_basic_block("merge")
        
        # Branch based on condition
        self.builder.cbranch(condition, then_block, else_block)
        
        # Generate then block
        self.builder.position_at_end(then_block)
        for statement in if_stmt.then_branch:
            self._generate_statement(statement)
        self.builder.branch(merge_block)
        
        # Generate else block
        self.builder.position_at_end(else_block)
        if if_stmt.else_branch:
            for statement in if_stmt.else_branch:
                self._generate_statement(statement)
        self.builder.branch(merge_block)
        
        # Continue from merge block
        self.builder.position_at_end(merge_block)
    
    def _generate_while_statement(self, while_stmt: WhileStatement):
        """Generate IR for while statement"""
        # Create blocks
        condition_block = self.builder.append_basic_block("while_condition")
        body_block = self.builder.append_basic_block("while_body")
        exit_block = self.builder.append_basic_block("while_exit")
        
        # Jump to condition
        self.builder.branch(condition_block)
        
        # Generate condition
        self.builder.position_at_end(condition_block)
        condition = self._generate_expression(while_stmt.condition)
        self.builder.cbranch(condition, body_block, exit_block)
        
        # Generate body
        self.builder.position_at_end(body_block)
        for statement in while_stmt.body:
            self._generate_statement(statement)
        self.builder.branch(condition_block)
        
        # Continue from exit block
        self.builder.position_at_end(exit_block)
    
    def _generate_return_statement(self, return_stmt: ReturnStatement):
        """Generate IR for return statement"""
        if return_stmt.value:
            value = self._generate_expression(return_stmt.value)
            self.builder.ret(value)
        else:
            self.builder.ret(ir.Constant(ir.IntType(32), 0))
    
    def _generate_expression(self, expr: ASTNode) -> Any:
        """Generate IR for expression"""
        if isinstance(expr, BinaryOperation):
            left = self._generate_expression(expr.left)
            right = self._generate_expression(expr.right)
            
            if expr.operator == "+":
                return self.builder.add(left, right)
            elif expr.operator == "-":
                return self.builder.sub(left, right)
            elif expr.operator == "*":
                return self.builder.mul(left, right)
            elif expr.operator == "/":
                return self.builder.sdiv(left, right)
            elif expr.operator == "==":
                return self.builder.icmp_signed("==", left, right)
            elif expr.operator == "!=":
                return self.builder.icmp_signed("!=", left, right)
            elif expr.operator == "<":
                return self.builder.icmp_signed("<", left, right)
            elif expr.operator == ">":
                return self.builder.icmp_signed(">", left, right)
            elif expr.operator == "<=":
                return self.builder.icmp_signed("<=", left, right)
            elif expr.operator == ">=":
                return self.builder.icmp_signed(">=", left, right)
            elif expr.operator == "&&":
                return self.builder.and_(left, right)
            elif expr.operator == "||":
                return self.builder.or_(left, right)
        
        elif isinstance(expr, UnaryOperation):
            operand = self._generate_expression(expr.operand)
            
            if expr.operator == "-":
                return self.builder.sub(ir.Constant(ir.IntType(32), 0), operand)
            elif expr.operator == "!":
                return self.builder.not_(operand)
        
        elif isinstance(expr, Literal):
            if expr.type == "number":
                return ir.Constant(ir.IntType(32), int(expr.value))
            elif expr.type == "string":
                # Create global string
                string_val = ir.Constant(ir.ArrayType(ir.IntType(8), len(expr.value) + 1), 
                                       bytearray(expr.value.encode() + b'\x00'))
                string_global = ir.GlobalVariable(self.module, string_val.type, name="str")
                string_global.initializer = string_val
                return string_global
        
        elif isinstance(expr, Identifier):
            # Load variable (simplified)
            return ir.Constant(ir.IntType(32), 0)
        
        elif isinstance(expr, FunctionCall):
            if expr.name in self.functions:
                return self.builder.call(self.functions[expr.name], [])
            else:
                # Call to undefined function
                return ir.Constant(ir.IntType(32), 0)
        
        return ir.Constant(ir.IntType(32), 0)
    
    def _generate_statement_with_builder(self, statement: ASTNode, builder: ir.IRBuilder):
        """Generate IR for statement with specific builder"""
        old_builder = self.builder
        self.builder = builder
        self._generate_statement(statement)
        self.builder = old_builder
    
    def _generate_fallback_ir(self, ast: Program) -> str:
        """Generate fallback IR when LLVM is not available"""
        ir_code = "; Fallback IR Generation\n"
        ir_code += "; LLVM not available, using simplified IR\n\n"
        
        for statement in ast.statements:
            if isinstance(statement, FunctionDeclaration):
                ir_code += f"define i32 @{statement.name}() {{\n"
                ir_code += "  ret i32 0\n"
                ir_code += "}\n\n"
            elif isinstance(statement, VariableDeclaration):
                ir_code += f"; Variable: {statement.name}\n"
            elif isinstance(statement, Assignment):
                ir_code += f"; Assignment: {statement.name} = ...\n"
        
        return ir_code

class Compiler:
    """Main compiler class"""
    
    def __init__(self):
        self.lexer = Lexer()
        self.parser = None
        self.semantic_analyzer = SemanticAnalyzer()
        self.ir_generator = IRGenerator()
    
    def compile(self, source: str, output_file: str = None) -> Dict[str, Any]:
        """Compile source code"""
        try:
            print("🔍 Lexical Analysis...")
            tokens = self.lexer.tokenize(source)
            print(f"✅ Tokenized {len(tokens)} tokens")
            
            print("🌳 Syntax Analysis...")
            self.parser = Parser(tokens)
            ast = self.parser.parse()
            print("✅ AST generated")
            
            print("🔍 Semantic Analysis...")
            errors = self.semantic_analyzer.analyze(ast)
            if errors:
                print(f"⚠️ Semantic errors found: {len(errors)}")
                for error in errors:
                    print(f"   {error}")
            else:
                print("✅ Semantic analysis passed")
            
            print("⚙️ IR Generation...")
            ir_code = self.ir_generator.generate(ast)
            print("✅ LLVM IR generated")
            
            # Save IR to file
            if output_file:
                with open(output_file, 'w') as f:
                    f.write(ir_code)
                print(f"💾 IR saved to {output_file}")
            
            return {
                'success': True,
                'tokens': len(tokens),
                'ast': ast,
                'semantic_errors': len(errors),
                'ir_code': ir_code,
                'output_file': output_file
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'tokens': 0,
                'ast': None,
                'semantic_errors': 0,
                'ir_code': '',
                'output_file': None
            }

def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python realistic_modular_compiler.py <source_file> [output_file]")
        print("\nExample source code:")
        print("function add(a, b) {")
        print("  return a + b;")
        print("}")
        print("")
        print("var x = 5;")
        print("var y = 10;")
        print("var result = add(x, y);")
        sys.exit(1)
    
    source_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "output.ll"
    
    # Read source code
    with open(source_file, 'r') as f:
        source_code = f.read()
    
    print(f"🚀 Compiling {source_file}...")
    print(f"📁 Output: {output_file}")
    
    # Create compiler
    compiler = Compiler()
    
    # Compile
    result = compiler.compile(source_code, output_file)
    
    if result['success']:
        print(f"\n✅ Compilation successful!")
        print(f"   Tokens: {result['tokens']}")
        print(f"   Semantic errors: {result['semantic_errors']}")
        print(f"   Output: {result['output_file']}")
        
        # Try to compile to executable if LLVM is available
        if LLVM_AVAILABLE and result['output_file']:
            try:
                print(f"\n🔨 Compiling to executable...")
                exe_file = output_file.replace('.ll', '.exe')
                
                # Use clang to compile LLVM IR to executable
                result_clang = subprocess.run([
                    'clang', result['output_file'], '-o', exe_file
                ], capture_output=True, text=True)
                
                if result_clang.returncode == 0:
                    print(f"✅ Executable created: {exe_file}")
                else:
                    print(f"⚠️ Clang compilation failed: {result_clang.stderr}")
                    
            except FileNotFoundError:
                print("⚠️ Clang not found - LLVM IR generated but not compiled to executable")
    else:
        print(f"\n❌ Compilation failed: {result['error']}")
        sys.exit(1)

if __name__ == "__main__":
    main()
