#!/usr/bin/env python3
"""
Java Language Components for Transpilation System
Lexer, Parser, and AST generation for Java source code
"""

from extensible_compiler_system import BaseLexer, BaseParser, LanguageInfo, LanguageType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall
from lexer_util import tokenize_with_comments, get_simple_token_specs
from typing import List, Any

LANGUAGE_INFO = {
    'java': LanguageInfo(
        name='Java',
        extension='.java',
        language_type=LanguageType.COMPILED,
        description='Java programming language',
        keywords=['class', 'public', 'static', 'void', 'String', 'int', 'double', 'return', 'System', 'out', 'println'],
        operators=['+', '-', '*', '/', '='],
        delimiters=[';', ',', '(', ')', '{', '}'],
        parser_class='JavaParser',
        lexer_class='JavaLexer'
    )
}

class JavaLexer(BaseLexer):
    """Java language lexer"""
    
    def __init__(self):
        super().__init__()
        self.specs = get_simple_token_specs(
            LANGUAGE_INFO['java'].keywords,
            LANGUAGE_INFO['java'].operators,
            LANGUAGE_INFO['java'].delimiters
        )
        self.comment_patterns = [
            (r'//.*$', 'SINGLE_LINE_COMMENT'), 
            (r'/\*.*?\*/', 'MULTI_LINE_COMMENT')
        ]

    def tokenize(self, source: str) -> List[Any]:
        """Tokenize Java source code"""
        return tokenize_with_comments(source, self.specs, self.comment_patterns)

    def get_keywords(self) -> List[str]:
        """Get Java keywords"""
        return LANGUAGE_INFO['java'].keywords

    def get_operators(self) -> List[str]:
        """Get Java operators"""
        return LANGUAGE_INFO['java'].operators

class JavaParser(BaseParser):
    """Java language parser"""
    
    def __init__(self):
        self.tokens = []
        self.pos = 0

    def parse(self, tokens: List[Any]) -> Program:
        """Parse Java tokens into AST"""
        self.tokens = tokens
        self.pos = 0
        program = Program()
        self._parse_class_definition(program)
        return program
    
    def _parse_class_definition(self, program):
        """Parse Java class definition"""
        self._consume('KEYWORD_PUBLIC', 'Expect \'public\' keyword for class.')
        self._consume('KEYWORD_CLASS', 'Expect \'class\' keyword.')
        class_name = self._consume('IDENTIFIER', 'Expect class name.').get('value')
        self._consume('DELIMITER_{', 'Expect \'{\' for class body.')
        
        while not self._check('DELIMITER_}'):
            program.statements.append(self._parse_method_definition())
        self._consume('DELIMITER_}', 'Expect \'}\' for class body.')

    def _parse_method_definition(self):
        """Parse Java method definition"""
        # Simplified: assumes a 'main' method or similar
        self._consume('KEYWORD_PUBLIC', 'Expect public keyword.')
        self._consume('KEYWORD_STATIC', 'Expect static keyword.')
        self._consume('KEYWORD_VOID', 'Expect void keyword.')
        method_name = self._consume('IDENTIFIER', 'Expect method name.').get('value')
        self._consume('DELIMITER_(', 'Expect \'(\' after method name.')
        self._consume('KEYWORD_STRING', 'Expect String parameter.')
        self._consume('IDENTIFIER', 'Expect arguments identifier.')
        self._consume('DELIMITER_)', 'Expect \')\' after parameters.')
        self._consume('DELIMITER_{', 'Expect \'{\' for method body.')

        body = []
        while not self._check('DELIMITER_}'):
            body.append(self._parse_statement())
        self._consume('DELIMITER_}', 'Expect \'}\' for method body.')
        
        return FunctionCall(Identifier(method_name), body)

    def _parse_statement(self):
        """Parse Java statement"""
        if self._check('IDENTIFIER'):
            name = self._advance().get('value')
            if self._match('OPERATOR_='):
                value = self._parse_expression()
                self._consume('DELIMITER_;', 'Expect \';\' after assignment.')
                return Assignment(name, value)
        
        if self._match('KEYWORD_SYSTEM'):  # System.out.println
            self._consume('DELIMITER_.', 'Expect \'.\'')
            self._consume('KEYWORD_OUT', 'Expect \'out\'')
            self._consume('DELIMITER_.', 'Expect \'.\'')
            self._consume('IDENTIFIER', 'Expect \'println\'').get('value')
            self._consume('DELIMITER_(', 'Expect \'(\'')
            arg = self._parse_expression()
            self._consume('DELIMITER_)', 'Expect \')\'')
            self._consume('DELIMITER_;', 'Expect \';\' after print statement.')
            return FunctionCall(Identifier('System.out.println'), [arg])
        
        raise Exception(f'Unexpected token: {self._peek()}')

    def _parse_expression(self):
        """Parse Java expression"""
        return self._parse_addition()

    def _parse_addition(self):
        """Parse addition/subtraction expressions"""
        expr = self._parse_multiplication()
        while self._match('OPERATOR_+', 'OPERATOR_-'):
            op = self._previous().get('value')
            right = self._parse_multiplication()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_multiplication(self):
        """Parse multiplication/division expressions"""
        expr = self._parse_primary()
        while self._match('OPERATOR_*', 'OPERATOR_/'):
            op = self._previous().get('value')
            right = self._parse_primary()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_primary(self):
        """Parse primary expressions"""
        if self._match('NUMBER'):
            return Literal(float(self._previous().get('value')))
        elif self._match('IDENTIFIER'):
            return Identifier(self._previous().get('value'))
        elif self._match('STRING'):
            return Literal(self._previous().get('value'))
        elif self._match('DELIMITER_('):
            expr = self._parse_expression()
            self._consume('DELIMITER_)', 'Expect \')\' after expression.')
            return expr
        raise Exception(f'Expected expression, but got {self._peek()}')

    def _match(self, *types):
        """Match one of the given token types"""
        for type_ in types:
            if self._check(type_):
                self._advance()
                return True
        return False

    def _consume(self, type_, message):
        """Consume a token of the expected type"""
        if self._check(type_):
            return self._advance()
        raise Exception(f"{message} at line {self._peek().get('line')}, column {self._peek().get('column')}. Got {self._peek().get('value')}.")

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
            return None
        return self.tokens[self.pos]

    def _previous(self):
        """Get previous token"""
        return self.tokens[self.pos - 1]

    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        return [node.__name__ for node in [
            Program, VariableDeclaration, Assignment, 
            BinaryOperation, Literal, Identifier, FunctionCall
        ]]
