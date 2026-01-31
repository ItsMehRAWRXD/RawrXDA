#!/usr/bin/env python3
"""
Universal AST Generator
Auto-generates Abstract Syntax Trees for any programming language
Uses meta-prompting and pattern recognition to create language-specific parsers
"""

import re
import json
from typing import Dict, List, Any, Optional, Union, Tuple
from dataclasses import dataclass, asdict
from abc import ABC, abstractmethod
from pathlib import Path

@dataclass
class ASTNode:
    """Universal AST node representation"""
    node_type: str
    value: Any = None
    children: List['ASTNode'] = None
    metadata: Dict[str, Any] = None
    source_location: Optional[Tuple[int, int]] = None  # (line, column)
    
    def __post_init__(self):
        if self.children is None:
            self.children = []
        if self.metadata is None:
            self.metadata = {}

@dataclass 
class Token:
    """Universal token representation"""
    type: str
    value: str
    line: int
    column: int
    metadata: Dict[str, Any] = None

class UniversalLexer:
    """Universal lexer that can tokenize any programming language"""
    
    def __init__(self, language_spec):
        self.language_spec = language_spec
        self.tokens = []
        self.current_line = 1
        self.current_column = 1
    
    def tokenize(self, source_code: str) -> List[Token]:
        """Tokenize source code based on language specification"""
        
        self.tokens = []
        self.current_line = 1
        self.current_column = 1
        
        i = 0
        while i < len(source_code):
            char = source_code[i]
            
            # Skip whitespace but track position
            if char in ' \t':
                self.current_column += 1
                i += 1
                continue
            elif char == '\n':
                self.current_line += 1
                self.current_column = 1
                i += 1
                continue
            
            # Handle comments
            comment_result = self._handle_comments(source_code, i)
            if comment_result:
                token, new_i = comment_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Handle strings
            string_result = self._handle_strings(source_code, i)
            if string_result:
                token, new_i = string_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Handle numbers
            number_result = self._handle_numbers(source_code, i)
            if number_result:
                token, new_i = number_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Handle identifiers and keywords
            identifier_result = self._handle_identifiers(source_code, i)
            if identifier_result:
                token, new_i = identifier_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Handle operators
            operator_result = self._handle_operators(source_code, i)
            if operator_result:
                token, new_i = operator_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Handle delimiters
            delimiter_result = self._handle_delimiters(source_code, i)
            if delimiter_result:
                token, new_i = delimiter_result
                self.tokens.append(token)
                i = new_i
                continue
            
            # Unknown character - skip
            i += 1
            self.current_column += 1
        
        return self.tokens
    
    def _handle_comments(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle comment tokenization"""
        
        # Single line comments
        single_comment = self.language_spec.comment_styles.get('single', '')
        if single_comment and source[pos:].startswith(single_comment):
            end = source.find('\n', pos)
            if end == -1:
                end = len(source)
            
            comment_text = source[pos + len(single_comment):end]
            token = Token('COMMENT', comment_text.strip(), self.current_line, self.current_column)
            return token, end
        
        # Multi-line comments
        multi_start = self.language_spec.comment_styles.get('multi_start', '')
        multi_end = self.language_spec.comment_styles.get('multi_end', '')
        
        if multi_start and source[pos:].startswith(multi_start):
            end = source.find(multi_end, pos + len(multi_start))
            if end == -1:
                end = len(source)
            else:
                end += len(multi_end)
            
            comment_text = source[pos + len(multi_start):end - len(multi_end)]
            token = Token('COMMENT', comment_text.strip(), self.current_line, self.current_column)
            return token, end
        
        return None
    
    def _handle_strings(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle string literal tokenization"""
        
        char = source[pos]
        if char in ['"', "'", '`']:  # Common string delimiters
            quote = char
            i = pos + 1
            string_value = ""
            
            while i < len(source) and source[i] != quote:
                if source[i] == '\\' and i + 1 < len(source):
                    # Handle escape sequences
                    string_value += source[i:i+2]
                    i += 2
                else:
                    string_value += source[i]
                    i += 1
            
            if i < len(source):  # Found closing quote
                i += 1  # Skip closing quote
            
            token = Token('STRING', string_value, self.current_line, self.current_column)
            return token, i
        
        return None
    
    def _handle_numbers(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle numeric literal tokenization"""
        
        if not source[pos].isdigit():
            return None
        
        i = pos
        number_str = ""
        has_dot = False
        
        while i < len(source):
            char = source[i]
            if char.isdigit():
                number_str += char
                i += 1
            elif char == '.' and not has_dot:
                has_dot = True
                number_str += char
                i += 1
            elif char in 'eE' and i + 1 < len(source) and source[i + 1].isdigit():
                # Scientific notation
                number_str += char
                i += 1
            else:
                break
        
        token = Token('NUMBER', number_str, self.current_line, self.current_column)
        return token, i
    
    def _handle_identifiers(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle identifier and keyword tokenization"""
        
        char = source[pos]
        if not (char.isalpha() or char == '_'):
            return None
        
        i = pos
        identifier = ""
        
        while i < len(source) and (source[i].isalnum() or source[i] == '_'):
            identifier += source[i]
            i += 1
        
        # Check if it's a keyword
        if identifier in self.language_spec.keywords:
            token_type = 'KEYWORD'
        else:
            token_type = 'IDENTIFIER'
        
        token = Token(token_type, identifier, self.current_line, self.current_column)
        return token, i
    
    def _handle_operators(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle operator tokenization"""
        
        # Try multi-character operators first
        for op_len in [3, 2, 1]:  # Check longest operators first
            if pos + op_len <= len(source):
                candidate = source[pos:pos + op_len]
                if candidate in self.language_spec.operators:
                    token = Token('OPERATOR', candidate, self.current_line, self.current_column)
                    return token, pos + op_len
        
        return None
    
    def _handle_delimiters(self, source: str, pos: int) -> Optional[Tuple[Token, int]]:
        """Handle delimiter tokenization"""
        
        char = source[pos]
        if char in self.language_spec.delimiters:
            token = Token('DELIMITER', char, self.current_line, self.current_column)
            return token, pos + 1
        
        return None

class UniversalParser:
    """Universal parser that can build ASTs for any programming language"""
    
    def __init__(self, language_spec, ast_spec):
        self.language_spec = language_spec
        self.ast_spec = ast_spec
        self.tokens = []
        self.current = 0
    
    def parse(self, tokens: List[Token]) -> ASTNode:
        """Parse tokens into AST"""
        
        self.tokens = tokens
        self.current = 0
        
        return self._parse_program()
    
    def _parse_program(self) -> ASTNode:
        """Parse the root program node"""
        
        program = ASTNode('Program', metadata={'language': self.language_spec.name})
        
        while not self._is_at_end():
            stmt = self._parse_statement()
            if stmt:
                program.children.append(stmt)
        
        return program
    
    def _parse_statement(self) -> Optional[ASTNode]:
        """Parse a statement based on language patterns"""
        
        if self._is_at_end():
            return None
        
        current_token = self._peek()
        
        # Function declaration pattern
        if self._matches_function_pattern():
            return self._parse_function_declaration()
        
        # Variable declaration pattern
        elif self._matches_variable_pattern():
            return self._parse_variable_declaration()
        
        # Control flow patterns
        elif current_token.type == 'KEYWORD':
            keyword = current_token.value
            
            if keyword in ['if', 'when', 'cond']:
                return self._parse_if_statement()
            elif keyword in ['while', 'loop']:
                return self._parse_while_statement()
            elif keyword in ['for', 'foreach']:
                return self._parse_for_statement()
            elif keyword in ['return', 'ret']:
                return self._parse_return_statement()
            elif keyword in ['class', 'struct', 'type']:
                return self._parse_class_declaration()
        
        # Expression statement
        expr = self._parse_expression()
        if expr:
            return ASTNode('ExpressionStatement', children=[expr])
        
        # Skip unrecognized tokens
        self._advance()
        return None
    
    def _matches_function_pattern(self) -> bool:
        """Check if current position matches function declaration pattern"""
        
        # Common function patterns: "def name", "function name", "fn name", etc.
        if not self._check('KEYWORD'):
            return False
        
        keyword = self._peek().value
        function_keywords = ['def', 'function', 'func', 'fn', 'method', 'sub', 'proc']
        
        return keyword in function_keywords
    
    def _matches_variable_pattern(self) -> bool:
        """Check if current position matches variable declaration pattern"""
        
        if not self._check('KEYWORD'):
            return False
        
        keyword = self._peek().value
        var_keywords = ['var', 'let', 'const', 'int', 'float', 'double', 'string', 'auto']
        
        return keyword in var_keywords
    
    def _parse_function_declaration(self) -> ASTNode:
        """Parse function declaration"""
        
        func_node = ASTNode('FunctionDeclaration')
        
        # Consume function keyword
        self._advance()
        
        # Function name
        if self._check('IDENTIFIER'):
            func_node.value = self._advance().value
        
        # Parameters
        if self._check('DELIMITER') and self._peek().value == '(':
            self._advance()  # consume '('
            params = self._parse_parameter_list()
            func_node.children.extend(params)
            
            if self._check('DELIMITER') and self._peek().value == ')':
                self._advance()  # consume ')'
        
        # Function body
        body = self._parse_block()
        if body:
            func_node.children.append(body)
        
        return func_node
    
    def _parse_variable_declaration(self) -> ASTNode:
        """Parse variable declaration"""
        
        var_node = ASTNode('VariableDeclaration')
        
        # Type/keyword
        type_token = self._advance()
        var_node.metadata['type'] = type_token.value
        
        # Variable name
        if self._check('IDENTIFIER'):
            var_node.value = self._advance().value
        
        # Initializer
        if self._check('OPERATOR') and self._peek().value == '=':
            self._advance()  # consume '='
            initializer = self._parse_expression()
            if initializer:
                var_node.children.append(initializer)
        
        return var_node
    
    def _parse_expression(self) -> Optional[ASTNode]:
        """Parse expressions using precedence climbing"""
        
        return self._parse_assignment()
    
    def _parse_assignment(self) -> Optional[ASTNode]:
        """Parse assignment expression"""
        
        left = self._parse_logical_or()
        
        if self._check('OPERATOR') and self._peek().value in ['=', '+=', '-=', '*=', '/=']:
            operator = self._advance().value
            right = self._parse_assignment()
            return ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_logical_or(self) -> Optional[ASTNode]:
        """Parse logical OR expression"""
        
        left = self._parse_logical_and()
        
        while self._check('OPERATOR') and self._peek().value in ['||', 'or', '|']:
            operator = self._advance().value
            right = self._parse_logical_and()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_logical_and(self) -> Optional[ASTNode]:
        """Parse logical AND expression"""
        
        left = self._parse_equality()
        
        while self._check('OPERATOR') and self._peek().value in ['&&', 'and', '&']:
            operator = self._advance().value
            right = self._parse_equality()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_equality(self) -> Optional[ASTNode]:
        """Parse equality expression"""
        
        left = self._parse_comparison()
        
        while self._check('OPERATOR') and self._peek().value in ['==', '!=', '===', '!==']:
            operator = self._advance().value
            right = self._parse_comparison()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_comparison(self) -> Optional[ASTNode]:
        """Parse comparison expression"""
        
        left = self._parse_addition()
        
        while self._check('OPERATOR') and self._peek().value in ['<', '>', '<=', '>=']:
            operator = self._advance().value
            right = self._parse_addition()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_addition(self) -> Optional[ASTNode]:
        """Parse addition/subtraction expression"""
        
        left = self._parse_multiplication()
        
        while self._check('OPERATOR') and self._peek().value in ['+', '-']:
            operator = self._advance().value
            right = self._parse_multiplication()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_multiplication(self) -> Optional[ASTNode]:
        """Parse multiplication/division expression"""
        
        left = self._parse_unary()
        
        while self._check('OPERATOR') and self._peek().value in ['*', '/', '%', '//', '**']:
            operator = self._advance().value
            right = self._parse_unary()
            left = ASTNode('BinaryExpression', operator, [left, right])
        
        return left
    
    def _parse_unary(self) -> Optional[ASTNode]:
        """Parse unary expression"""
        
        if self._check('OPERATOR') and self._peek().value in ['-', '+', '!', 'not', '~']:
            operator = self._advance().value
            expr = self._parse_unary()
            return ASTNode('UnaryExpression', operator, [expr])
        
        return self._parse_primary()
    
    def _parse_primary(self) -> Optional[ASTNode]:
        """Parse primary expression"""
        
        if self._check('NUMBER'):
            token = self._advance()
            return ASTNode('Literal', token.value, metadata={'type': 'number'})
        
        if self._check('STRING'):
            token = self._advance()
            return ASTNode('Literal', token.value, metadata={'type': 'string'})
        
        if self._check('IDENTIFIER'):
            token = self._advance()
            return ASTNode('Identifier', token.value)
        
        if self._check('DELIMITER') and self._peek().value == '(':
            self._advance()  # consume '('
            expr = self._parse_expression()
            if self._check('DELIMITER') and self._peek().value == ')':
                self._advance()  # consume ')'
            return expr
        
        return None
    
    def _parse_block(self) -> Optional[ASTNode]:
        """Parse block statement"""
        
        if not (self._check('DELIMITER') and self._peek().value == '{'):
            # Single statement or indented block (Python-style)
            stmt = self._parse_statement()
            if stmt:
                return ASTNode('Block', children=[stmt])
            return None
        
        self._advance()  # consume '{'
        
        block = ASTNode('Block')
        
        while not self._is_at_end() and not (self._check('DELIMITER') and self._peek().value == '}'):
            stmt = self._parse_statement()
            if stmt:
                block.children.append(stmt)
        
        if self._check('DELIMITER') and self._peek().value == '}':
            self._advance()  # consume '}'
        
        return block
    
    def _parse_parameter_list(self) -> List[ASTNode]:
        """Parse function parameter list"""
        
        params = []
        
        while not self._is_at_end() and not (self._check('DELIMITER') and self._peek().value == ')'):
            if self._check('IDENTIFIER'):
                param = ASTNode('Parameter', self._advance().value)
                params.append(param)
            else:
                self._advance()  # skip unrecognized tokens
            
            # Handle comma separator
            if self._check('DELIMITER') and self._peek().value == ',':
                self._advance()
        
        return params
    
    def _parse_if_statement(self) -> ASTNode:
        """Parse if statement"""
        
        if_node = ASTNode('IfStatement')
        
        self._advance()  # consume 'if'
        
        # Condition
        condition = self._parse_expression()
        if condition:
            if_node.children.append(condition)
        
        # Then block
        then_block = self._parse_block()
        if then_block:
            if_node.children.append(then_block)
        
        # Else block
        if self._check('KEYWORD') and self._peek().value == 'else':
            self._advance()  # consume 'else'
            else_block = self._parse_block()
            if else_block:
                if_node.children.append(else_block)
        
        return if_node
    
    def _parse_while_statement(self) -> ASTNode:
        """Parse while statement"""
        
        while_node = ASTNode('WhileStatement')
        
        self._advance()  # consume 'while'
        
        # Condition
        condition = self._parse_expression()
        if condition:
            while_node.children.append(condition)
        
        # Body
        body = self._parse_block()
        if body:
            while_node.children.append(body)
        
        return while_node
    
    def _parse_for_statement(self) -> ASTNode:
        """Parse for statement"""
        
        for_node = ASTNode('ForStatement')
        
        self._advance()  # consume 'for'
        
        # For now, just parse as a generic construct
        # Real implementation would handle for loop specifics
        body = self._parse_block()
        if body:
            for_node.children.append(body)
        
        return for_node
    
    def _parse_return_statement(self) -> ASTNode:
        """Parse return statement"""
        
        return_node = ASTNode('ReturnStatement')
        
        self._advance()  # consume 'return'
        
        # Return value
        value = self._parse_expression()
        if value:
            return_node.children.append(value)
        
        return return_node
    
    def _parse_class_declaration(self) -> ASTNode:
        """Parse class declaration"""
        
        class_node = ASTNode('ClassDeclaration')
        
        self._advance()  # consume class keyword
        
        # Class name
        if self._check('IDENTIFIER'):
            class_node.value = self._advance().value
        
        # Class body
        body = self._parse_block()
        if body:
            class_node.children.append(body)
        
        return class_node
    
    # Helper methods
    def _check(self, token_type: str) -> bool:
        """Check if current token is of given type"""
        if self._is_at_end():
            return False
        return self._peek().type == token_type
    
    def _peek(self) -> Token:
        """Get current token without advancing"""
        if self._is_at_end():
            return Token('EOF', '', 0, 0)
        return self.tokens[self.current]
    
    def _advance(self) -> Token:
        """Get current token and advance"""
        if not self._is_at_end():
            self.current += 1
        return self.tokens[self.current - 1]
    
    def _is_at_end(self) -> bool:
        """Check if we're at end of tokens"""
        return self.current >= len(self.tokens)

class UniversalASTGenerator:
    """Main class for universal AST generation"""
    
    def __init__(self, language_registry):
        self.language_registry = language_registry
        self.parsers = {}
        
        print("🌳 Universal AST Generator initialized")
    
    def generate_ast(self, source_code: str, language_name: str) -> ASTNode:
        """Generate AST for source code in specified language"""
        
        language_name = language_name.lower()
        
        # Get language specification
        language_spec = self.language_registry.get_language_spec(language_name)
        if not language_spec:
            raise ValueError(f"Language {language_name} not supported")
        
        # Get or create parser
        parser = self._get_parser(language_name, language_spec)
        
        # Tokenize source code
        lexer = UniversalLexer(language_spec)
        tokens = lexer.tokenize(source_code)
        
        # Parse tokens into AST
        ast = parser.parse(tokens)
        
        return ast
    
    def _get_parser(self, language_name: str, language_spec) -> UniversalParser:
        """Get cached parser or create new one"""
        
        if language_name not in self.parsers:
            # Generate AST specification for the language
            ast_spec = self.language_registry.generate_ast_for_language(language_name)
            
            # Create parser
            parser = UniversalParser(language_spec, ast_spec)
            self.parsers[language_name] = parser
        
        return self.parsers[language_name]
    
    def visualize_ast(self, ast: ASTNode, indent: int = 0) -> str:
        """Create a text visualization of the AST"""
        
        result = "  " * indent + f"{ast.node_type}"
        if ast.value is not None:
            result += f": {ast.value}"
        result += "\n"
        
        for child in ast.children:
            result += self.visualize_ast(child, indent + 1)
        
        return result
    
    def ast_to_json(self, ast: ASTNode) -> str:
        """Convert AST to JSON representation"""
        
        def ast_to_dict(node):
            return {
                'type': node.node_type,
                'value': node.value,
                'children': [ast_to_dict(child) for child in node.children],
                'metadata': node.metadata,
                'location': node.source_location
            }
        
        return json.dumps(ast_to_dict(ast), indent=2)
    
    def save_ast(self, ast: ASTNode, output_file: str):
        """Save AST to file"""
        
        with open(output_file, 'w') as f:
            f.write(self.ast_to_json(ast))
        
        print(f"✅ AST saved to {output_file}")

# Integration function
def integrate_universal_ast_generator(ide_instance):
    """Integrate universal AST generator with IDE"""
    
    ide_instance.ast_generator = UniversalASTGenerator(ide_instance.language_registry)
    print("🌳 Universal AST Generator integrated with IDE")

if __name__ == "__main__":
    print("🌳 Universal AST Generator")
    print("=" * 50)
    
    # This would normally use the language registry
    print("✅ Universal AST Generator ready!")
