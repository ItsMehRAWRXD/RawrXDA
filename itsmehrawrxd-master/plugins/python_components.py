#!/usr/bin/env python3
"""
Python Language Components for Extensible Compiler System
Implements lexer and parser for Python subset
"""

from typing import List, Any, Dict
import re
from extensible_compiler_system import BaseLexer, BaseParser, LanguageInfo, LanguageType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionDefinition
from lexer_util import tokenize_with_comments, get_simple_token_specs

LANGUAGE_INFO = {
    'python': LanguageInfo(
        name='Python',
        extension='.py',
        language_type=LanguageType.INTERPRETED,
        description='Python programming language',
        keywords=['def', 'if', 'else', 'return', 'print', 'input', 'len', 'range', 'for', 'while', 'in', 'and', 'or', 'not', 'True', 'False', 'None'],
        operators=['+', '-', '*', '/', '//', '%', '**', '=', '==', '!=', '<', '>', '<=', '>=', 'and', 'or', 'not'],
        delimiters=[':', '(', ')', '[', ']', '{', '}', ',', '.'],
        parser_class='PythonParser',
        lexer_class='PythonLexer'
    )
}

class PythonLexer(BaseLexer):
    """Python lexer implementation"""
    
    def __init__(self):
        super().__init__()
        self.specs = get_simple_token_specs(
            LANGUAGE_INFO['python'].keywords,
            LANGUAGE_INFO['python'].operators,
            LANGUAGE_INFO['python'].delimiters
        )
        self.comment_patterns = [(r'#.*$', 'HASH_COMMENT')]
        self.string_patterns = [
            (r'""".*?"""', 'TRIPLE_STRING'),
            (r"'''.*?'''", 'TRIPLE_STRING'),
            (r'"[^"]*"', 'STRING'),
            (r"'[^']*'", 'STRING')
        ]

    def tokenize(self, source: str) -> List[Dict[str, Any]]:
        """Tokenize Python source code"""
        tokens = tokenize_with_comments(source, self.specs, self.comment_patterns)
        
        # Handle Python-specific patterns
        processed_tokens = []
        i = 0
        line = 1
        column = 1
        
        while i < len(source):
            char = source[i]
            
            # Handle strings
            string_matched = False
            for pattern, token_type in self.string_patterns:
                match = re.match(pattern, source[i:], re.DOTALL)
                if match:
                    value = match.group(0)
                    processed_tokens.append({
                        'type': token_type,
                        'value': value,
                        'line': line,
                        'column': column
                    })
                    i += len(value)
                    column += len(value)
                    string_matched = True
                    break
            
            if string_matched:
                continue
            
            # Handle indentation (Python-specific)
            if char == '\n':
                line += 1
                column = 1
                i += 1
                continue
            elif char == ' ' or char == '\t':
                # Count indentation
                indent_level = 0
                while i < len(source) and (source[i] == ' ' or source[i] == '\t'):
                    if source[i] == ' ':
                        indent_level += 1
                    else:  # tab
                        indent_level += 4
                    i += 1
                    column += 1
                
                if indent_level > 0:
                    processed_tokens.append({
                        'type': 'INDENT',
                        'value': indent_level,
                        'line': line,
                        'column': column - indent_level
                    })
                continue
            
            # Handle other tokens
            if i < len(source):
                processed_tokens.append({
                    'type': 'CHAR',
                    'value': char,
                    'line': line,
                    'column': column
                })
                i += 1
                column += 1
        
        return processed_tokens

    def get_keywords(self) -> List[str]:
        """Get Python keywords"""
        return LANGUAGE_INFO['python'].keywords

    def get_operators(self) -> List[str]:
        """Get Python operators"""
        return LANGUAGE_INFO['python'].operators

class PythonParser(BaseParser):
    """Python parser implementation"""
    
    def __init__(self):
        self.tokens = []
        self.pos = 0

    def parse(self, tokens: List[Dict[str, Any]]) -> Program:
        """Parse Python tokens into AST"""
        self.tokens = tokens
        self.pos = 0
        program = Program()
        
        while not self._is_at_end():
            stmt = self._parse_statement()
            if stmt:
                program.statements.append(stmt)
        
        return program

    def _parse_statement(self):
        """Parse a Python statement"""
        if self._check('KEYWORD_DEF'):
            return self._parse_function_definition()
        elif self._check('KEYWORD_IF'):
            return self._parse_if_statement()
        elif self._check('KEYWORD_FOR'):
            return self._parse_for_statement()
        elif self._check('KEYWORD_WHILE'):
            return self._parse_while_statement()
        elif self._check('KEYWORD_PRINT'):
            return self._parse_print_statement()
        else:
            return self._parse_assignment_statement()

    def _parse_function_definition(self):
        """Parse function definition"""
        self._consume('KEYWORD_DEF', 'Expect \'def\' keyword.')
        name = self._consume('IDENTIFIER', 'Expect function name.').get('value')
        self._consume('DELIMITER_(', 'Expect \'(\' after function name.')
        
        # Parse parameters
        parameters = []
        if not self._check('DELIMITER_)'):
            parameters.append(self._consume('IDENTIFIER', 'Expect parameter name.').get('value'))
            while self._match('DELIMITER_,'):
                parameters.append(self._consume('IDENTIFIER', 'Expect parameter name.').get('value'))
        
        self._consume('DELIMITER_)', 'Expect \')\' after parameters.')
        self._consume('DELIMITER_:', 'Expect \':\' before function body.')
        
        # Parse function body (simplified - just statements until dedent)
        body = []
        while not self._is_at_end() and not self._check('DEDENT'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        return FunctionDefinition(name, parameters, body)

    def _parse_if_statement(self):
        """Parse if statement"""
        self._consume('KEYWORD_IF', 'Expect \'if\' keyword.')
        condition = self._parse_expression()
        self._consume('DELIMITER_:', 'Expect \':\' after condition.')
        
        # Parse if body
        if_body = []
        while not self._is_at_end() and not self._check('KEYWORD_ELSE') and not self._check('DEDENT'):
            stmt = self._parse_statement()
            if stmt:
                if_body.append(stmt)
        
        # Parse else clause if present
        else_body = []
        if self._match('KEYWORD_ELSE'):
            self._consume('DELIMITER_:', 'Expect \':\' after else.')
            while not self._is_at_end() and not self._check('DEDENT'):
                stmt = self._parse_statement()
                if stmt:
                    else_body.append(stmt)
        
        return {'type': 'IfStatement', 'condition': condition, 'if_body': if_body, 'else_body': else_body}

    def _parse_for_statement(self):
        """Parse for statement"""
        self._consume('KEYWORD_FOR', 'Expect \'for\' keyword.')
        var = self._consume('IDENTIFIER', 'Expect variable name.').get('value')
        self._consume('KEYWORD_IN', 'Expect \'in\' keyword.')
        iterable = self._parse_expression()
        self._consume('DELIMITER_:', 'Expect \':\' after for statement.')
        
        # Parse for body
        body = []
        while not self._is_at_end() and not self._check('DEDENT'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        return {'type': 'ForStatement', 'variable': var, 'iterable': iterable, 'body': body}

    def _parse_while_statement(self):
        """Parse while statement"""
        self._consume('KEYWORD_WHILE', 'Expect \'while\' keyword.')
        condition = self._parse_expression()
        self._consume('DELIMITER_:', 'Expect \':\' after condition.')
        
        # Parse while body
        body = []
        while not self._is_at_end() and not self._check('DEDENT'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        return {'type': 'WhileStatement', 'condition': condition, 'body': body}

    def _parse_print_statement(self):
        """Parse print statement"""
        self._consume('KEYWORD_PRINT', 'Expect \'print\' keyword.')
        self._consume('DELIMITER_(', 'Expect \'(\' after print.')
        args = []
        
        if not self._check('DELIMITER_)'):
            args.append(self._parse_expression())
            while self._match('DELIMITER_,'):
                args.append(self._parse_expression())
        
        self._consume('DELIMITER_)', 'Expect \')\' after print arguments.')
        return {'type': 'PrintStatement', 'args': args}

    def _parse_assignment_statement(self):
        """Parse assignment statement"""
        name = self._consume('IDENTIFIER', 'Expect identifier for assignment.').get('value')
        self._consume('OPERATOR_=', 'Expect \'=\' for assignment.')
        value = self._parse_expression()
        return Assignment(name, value)

    def _parse_expression(self):
        """Parse Python expression"""
        return self._parse_logical_or()

    def _parse_logical_or(self):
        """Parse logical OR expression"""
        expr = self._parse_logical_and()
        while self._match('KEYWORD_OR'):
            right = self._parse_logical_and()
            expr = BinaryOperation('or', expr, right)
        return expr

    def _parse_logical_and(self):
        """Parse logical AND expression"""
        expr = self._parse_comparison()
        while self._match('KEYWORD_AND'):
            right = self._parse_comparison()
            expr = BinaryOperation('and', expr, right)
        return expr

    def _parse_comparison(self):
        """Parse comparison expression"""
        expr = self._parse_addition()
        while self._match('OPERATOR_==', 'OPERATOR_!=', 'OPERATOR_<', 'OPERATOR_>', 'OPERATOR_<=', 'OPERATOR_>='):
            op = self._previous().get('value')
            right = self._parse_addition()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_addition(self):
        """Parse addition expression"""
        expr = self._parse_multiplication()
        while self._match('OPERATOR_+', 'OPERATOR_-'):
            op = self._previous().get('value')
            right = self._parse_multiplication()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_multiplication(self):
        """Parse multiplication expression"""
        expr = self._parse_primary()
        while self._match('OPERATOR_*', 'OPERATOR_/', 'OPERATOR_//', 'OPERATOR_%', 'OPERATOR_**'):
            op = self._previous().get('value')
            right = self._parse_primary()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_primary(self):
        """Parse primary expression"""
        if self._match('NUMBER'):
            value = self._previous().get('value')
            try:
                return Literal(int(value))
            except ValueError:
                return Literal(float(value))
        elif self._match('STRING', 'TRIPLE_STRING'):
            return Literal(self._previous().get('value'))
        elif self._match('KEYWORD_TRUE'):
            return Literal(True)
        elif self._match('KEYWORD_FALSE'):
            return Literal(False)
        elif self._match('KEYWORD_NONE'):
            return Literal(None)
        elif self._match('IDENTIFIER'):
            return Identifier(self._previous().get('value'))
        elif self._match('DELIMITER_('):
            expr = self._parse_expression()
            self._consume('DELIMITER_)', 'Expect \')\' after expression.')
            return expr
        elif self._match('DELIMITER_['):
            # List literal
            elements = []
            if not self._check('DELIMITER_]'):
                elements.append(self._parse_expression())
                while self._match('DELIMITER_,'):
                    elements.append(self._parse_expression())
            self._consume('DELIMITER_]', 'Expect \']\' after list elements.')
            return {'type': 'ListLiteral', 'elements': elements}
        
        raise Exception(f'Unexpected token: {self._peek()}')

    def _match(self, *types):
        """Match one of the given token types"""
        for type_ in types:
            if self._check(type_):
                self._advance()
                return True
        return False

    def _consume(self, type_, message):
        """Consume a token of the given type"""
        if self._check(type_):
            return self._advance()
        raise Exception(f"{message} at line {self._peek().get('line', '?')}, column {self._peek().get('column', '?')}. Got {self._peek().get('value', '?')}.")

    def _check(self, type_):
        """Check if current token matches type"""
        if self._is_at_end():
            return False
        return self._peek().get('type') == type_

    def _advance(self):
        """Advance to next token"""
        if not self._is_at_end():
            self.pos += 1
        return self._previous()

    def _is_at_end(self):
        """Check if at end of tokens"""
        return self.pos >= len(self.tokens)

    def _peek(self):
        """Peek at current token"""
        if self._is_at_end():
            return {'type': 'EOF', 'value': '', 'line': 0, 'column': 0}
        return self.tokens[self.pos]

    def _previous(self):
        """Get previous token"""
        return self.tokens[self.pos - 1]

    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        return [node.__name__ for node in [Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionDefinition]]

# Export language info
def get_language_info():
    """Get Python language information"""
    return LANGUAGE_INFO['python']
