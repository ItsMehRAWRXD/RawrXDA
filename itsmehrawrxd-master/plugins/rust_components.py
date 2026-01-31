#!/usr/bin/env python3
"""
Rust Language Components for Extensible Compiler System
Implements lexer and parser for Rust subset
"""

from typing import List, Any, Dict
import re
from main_compiler_system import BaseLexer, BaseParser, LanguageInfo, LanguageType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionDeclaration, ReturnStatement, IfStatement, WhileStatement, ForStatement, Block
from lexer_util import tokenize_with_comments, get_simple_token_specs

LANGUAGE_INFO = {
    'rust': LanguageInfo(
        name='Rust',
        extension='.rs',
        language_type=LanguageType.COMPILED,
        description='Rust programming language',
        keywords=['fn', 'let', 'mut', 'const', 'if', 'else', 'while', 'for', 'loop', 'return', 'break', 'continue', 'true', 'false', 'i32', 'i64', 'f32', 'f64', 'bool', 'String'],
        operators=['+', '-', '*', '/', '%', '=', '==', '!=', '<', '>', '<=', '>=', '&&', '||', '!', '&', '|', '^', '<<', '>>', '+=', '-=', '*=', '/=', '=>'],
        delimiters=[';', ',', '(', ')', '{', '}', '[', ']', ':', '::'],
        parser_class='RustParser',
        lexer_class='RustLexer'
    )
}

class RustLexer(BaseLexer):
    """Rust lexer implementation"""
    
    def __init__(self):
        super().__init__()
        self.specs = get_simple_token_specs(
            LANGUAGE_INFO['rust'].keywords,
            LANGUAGE_INFO['rust'].operators,
            LANGUAGE_INFO['rust'].delimiters
        )
        self.comment_patterns = [
            (r'//.*$', 'SINGLE_LINE_COMMENT'),
            (r'/\*.*?\*/', 'MULTI_LINE_COMMENT')
        ]

    def tokenize(self, source: str) -> List[Dict[str, Any]]:
        """Tokenize Rust source code"""
        tokens = tokenize_with_comments(source, self.specs, self.comment_patterns)
        
        # Filter out comments
        filtered_tokens = [token for token in tokens if 'COMMENT' not in token['type']]
        
        return filtered_tokens

    def get_keywords(self) -> List[str]:
        return LANGUAGE_INFO['rust'].keywords

    def get_operators(self) -> List[str]:
        return LANGUAGE_INFO['rust'].operators

class RustParser(BaseParser):
    """Rust parser implementation using recursive descent"""
    
    def __init__(self):
        self.tokens = []
        self.current = 0

    def parse(self, tokens: List[Dict[str, Any]]) -> Program:
        """Parse Rust tokens into AST"""
        self.tokens = tokens
        self.current = 0
        
        program = Program()
        
        while not self._is_at_end():
            stmt = self._parse_statement()
            if stmt:
                program.statements.append(stmt)
        
        return program

    def _parse_statement(self):
        """Parse a Rust statement"""
        if self._check('KEYWORD_FN'):
            return self._parse_function_declaration()
        elif self._check('KEYWORD_LET'):
            return self._parse_variable_declaration()
        elif self._check('KEYWORD_IF'):
            return self._parse_if_statement()
        elif self._check('KEYWORD_WHILE'):
            return self._parse_while_statement()
        elif self._check('KEYWORD_FOR'):
            return self._parse_for_statement()
        elif self._check('KEYWORD_LOOP'):
            return self._parse_loop_statement()
        elif self._check('KEYWORD_RETURN'):
            return self._parse_return_statement()
        elif self._check('IDENTIFIER'):
            # Could be assignment or function call
            if self._peek_next() and self._peek_next()['type'] == 'OPERATOR_=':
                return self._parse_assignment()
            else:
                return self._parse_expression()
        else:
            # Skip unknown tokens
            self._advance()
            return None

    def _parse_function_declaration(self):
        """Parse function declaration"""
        self._consume('KEYWORD_FN', 'Expected fn keyword')
        name = self._consume('IDENTIFIER', 'Expected function name')['value']
        
        self._consume('DELIMITER_(', 'Expected opening parenthesis')
        parameters = []
        if not self._check('DELIMITER_)'):
            parameters = self._parse_parameter_list()
        self._consume('DELIMITER_)', 'Expected closing parenthesis')
        
        return_type = None
        if self._match('DELIMITER_:'):
            return_type = self._parse_type()
        
        body = None
        if self._match('DELIMITER_{'):
            body = self._parse_block()
        
        return FunctionDeclaration(name, parameters, return_type, body)

    def _parse_parameter_list(self):
        """Parse function parameter list"""
        parameters = []
        while not self._check('DELIMITER_)'):
            param_name = self._consume('IDENTIFIER', 'Expected parameter name')['value']
            self._consume('DELIMITER_:', 'Expected colon')
            param_type = self._parse_type()
            parameters.append(VariableDeclaration(param_type, param_name, None))
            
            if self._match('DELIMITER_,'):
                continue
            else:
                break
        
        return parameters

    def _parse_type(self):
        """Parse type annotation"""
        if self._match('KEYWORD_I32', 'KEYWORD_I64', 'KEYWORD_F32', 'KEYWORD_F64', 'KEYWORD_BOOL', 'KEYWORD_STRING'):
            return self._previous()['value']
        elif self._match('IDENTIFIER'):
            return self._previous()['value']
        else:
            raise Exception(f"Expected type, found: {self._peek()}")

    def _parse_variable_declaration(self):
        """Parse variable declaration"""
        self._consume('KEYWORD_LET', 'Expected let keyword')
        
        is_mutable = False
        if self._match('KEYWORD_MUT'):
            is_mutable = True
        
        name = self._consume('IDENTIFIER', 'Expected variable name')['value']
        
        var_type = None
        if self._match('DELIMITER_:'):
            var_type = self._parse_type()
        
        initializer = None
        if self._match('OPERATOR_='):
            initializer = self._parse_expression()
        
        self._consume('DELIMITER_;', 'Expected semicolon after declaration')
        return VariableDeclaration(var_type, name, initializer, is_mutable=is_mutable)

    def _parse_assignment(self):
        """Parse assignment statement"""
        name = self._advance()['value']
        self._consume('OPERATOR_=', 'Expected assignment operator')
        value = self._parse_expression()
        self._consume('DELIMITER_;', 'Expected semicolon after assignment')
        return Assignment(name, value)

    def _parse_if_statement(self):
        """Parse if statement"""
        self._consume('KEYWORD_IF', 'Expected if keyword')
        condition = self._parse_expression()
        
        then_branch = self._parse_statement()
        else_branch = None
        
        if self._match('KEYWORD_ELSE'):
            else_branch = self._parse_statement()
        
        return IfStatement(condition, then_branch, else_branch)

    def _parse_while_statement(self):
        """Parse while statement"""
        self._consume('KEYWORD_WHILE', 'Expected while keyword')
        condition = self._parse_expression()
        body = self._parse_statement()
        return WhileStatement(condition, body)

    def _parse_for_statement(self):
        """Parse for statement"""
        self._consume('KEYWORD_FOR', 'Expected for keyword')
        # Simplified for loop parsing
        iterator = self._parse_expression()
        body = self._parse_statement()
        return ForStatement(None, iterator, None, body)

    def _parse_loop_statement(self):
        """Parse loop statement"""
        self._consume('KEYWORD_LOOP', 'Expected loop keyword')
        body = self._parse_statement()
        return WhileStatement(Literal(True, 'bool'), body)

    def _parse_return_statement(self):
        """Parse return statement"""
        self._consume('KEYWORD_RETURN', 'Expected return keyword')
        value = None
        if not self._check('DELIMITER_;'):
            value = self._parse_expression()
        self._consume('DELIMITER_;', 'Expected semicolon after return')
        return ReturnStatement(value)

    def _parse_block(self):
        """Parse block statement"""
        statements = []
        while not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                statements.append(stmt)
        self._consume('DELIMITER_}', 'Expected closing brace')
        return Block(statements)

    def _parse_expression(self):
        """Parse expression using precedence climbing"""
        return self._parse_assignment_expression()

    def _parse_assignment_expression(self):
        """Parse assignment expression"""
        expr = self._parse_logical_or()
        
        if self._match('OPERATOR_='):
            right = self._parse_assignment_expression()
            return BinaryOperation('=', expr, right)
        
        return expr

    def _parse_logical_or(self):
        """Parse logical OR expression"""
        expr = self._parse_logical_and()
        
        while self._match('OPERATOR_||'):
            right = self._parse_logical_and()
            expr = BinaryOperation('||', expr, right)
        
        return expr

    def _parse_logical_and(self):
        """Parse logical AND expression"""
        expr = self._parse_equality()
        
        while self._match('OPERATOR_&&'):
            right = self._parse_equality()
            expr = BinaryOperation('&&', expr, right)
        
        return expr

    def _parse_equality(self):
        """Parse equality expression"""
        expr = self._parse_relational()
        
        while self._match('OPERATOR_==', 'OPERATOR_!='):
            operator = self._previous()['value']
            right = self._parse_relational()
            expr = BinaryOperation(operator, expr, right)
        
        return expr

    def _parse_relational(self):
        """Parse relational expression"""
        expr = self._parse_additive()
        
        while self._match('OPERATOR_<', 'OPERATOR_>', 'OPERATOR_<=', 'OPERATOR_>='):
            operator = self._previous()['value']
            right = self._parse_additive()
            expr = BinaryOperation(operator, expr, right)
        
        return expr

    def _parse_additive(self):
        """Parse additive expression"""
        expr = self._parse_multiplicative()
        
        while self._match('OPERATOR_+', 'OPERATOR_-'):
            operator = self._previous()['value']
            right = self._parse_multiplicative()
            expr = BinaryOperation(operator, expr, right)
        
        return expr

    def _parse_multiplicative(self):
        """Parse multiplicative expression"""
        expr = self._parse_unary()
        
        while self._match('OPERATOR_*', 'OPERATOR_/', 'OPERATOR_%'):
            operator = self._previous()['value']
            right = self._parse_unary()
            expr = BinaryOperation(operator, expr, right)
        
        return expr

    def _parse_unary(self):
        """Parse unary expression"""
        if self._match('OPERATOR_!', 'OPERATOR_-', 'OPERATOR_+'):
            operator = self._previous()['value']
            right = self._parse_unary()
            return BinaryOperation(operator, None, right)
        
        return self._parse_primary()

    def _parse_primary(self):
        """Parse primary expression"""
        if self._match('NUMBER'):
            value = self._previous()['value']
            try:
                # Try to parse as float first, then int
                if '.' in value:
                    return Literal(float(value), 'f64')
                else:
                    return Literal(int(value), 'i32')
            except ValueError:
                return Literal(value, 'number')
        
        if self._match('STRING'):
            value = self._previous()['value']
            # Remove quotes
            if len(value) >= 2:
                value = value[1:-1]
            return Literal(value, 'String')
        
        if self._match('KEYWORD_TRUE'):
            return Literal(True, 'bool')
        
        if self._match('KEYWORD_FALSE'):
            return Literal(False, 'bool')
        
        if self._match('IDENTIFIER'):
            return Identifier(self._previous()['value'])
        
        if self._match('DELIMITER_('):
            expr = self._parse_expression()
            self._consume('DELIMITER_)', 'Expected closing parenthesis')
            return expr
        
        raise Exception(f"Unexpected token: {self._peek()}")

    # Helper methods
    def _match(self, *types):
        """Check if current token matches any of the given types"""
        for token_type in types:
            if self._check(token_type):
                self._advance()
                return True
        return False

    def _check(self, token_type):
        """Check if current token is of given type"""
        if self._is_at_end():
            return False
        return self._peek()['type'] == token_type

    def _consume(self, token_type, message):
        """Consume token of given type or raise error"""
        if self._check(token_type):
            return self._advance()
        raise Exception(f"{message}. Found: {self._peek()}")

    def _advance(self):
        """Advance to next token"""
        if not self._is_at_end():
            self.current += 1
        return self._previous()

    def _previous(self):
        """Get previous token"""
        return self.tokens[self.current - 1]

    def _peek(self):
        """Peek at current token"""
        if self._is_at_end():
            return {'type': 'EOF', 'value': ''}
        return self.tokens[self.current]

    def _peek_next(self):
        """Peek at next token"""
        if self.current + 1 >= len(self.tokens):
            return {'type': 'EOF', 'value': ''}
        return self.tokens[self.current + 1]

    def _is_at_end(self):
        """Check if at end of tokens"""
        return self.current >= len(self.tokens)

    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        return [
            'Program', 'VariableDeclaration', 'Assignment', 'BinaryOperation', 
            'Literal', 'Identifier', 'FunctionDeclaration', 'ReturnStatement',
            'IfStatement', 'WhileStatement', 'ForStatement', 'Block'
        ]