#!/usr/bin/env python3
"""
JavaScript Language Components for Extensible Compiler System
Implements lexer and parser for JavaScript subset
"""

from typing import List, Any, Dict
import re
from extensible_compiler_system import BaseLexer, BaseParser, LanguageInfo, LanguageType
from ast_nodes import Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall, FunctionDefinition
from lexer_util import tokenize_with_comments, get_simple_token_specs

LANGUAGE_INFO = {
    'javascript': LanguageInfo(
        name='JavaScript',
        extension='.js',
        language_type=LanguageType.INTERPRETED,
        description='JavaScript programming language',
        keywords=['var', 'let', 'const', 'function', 'if', 'else', 'return', 'for', 'while', 'do', 'break', 'continue', 'true', 'false', 'null', 'undefined', 'typeof', 'instanceof'],
        operators=['+', '-', '*', '/', '%', '=', '==', '!=', '===', '!==', '<', '>', '<=', '>=', '&&', '||', '!', '++', '--', '+=', '-=', '*=', '/=', '?', ':'],
        delimiters=[';', ',', '(', ')', '{', '}', '[', ']', '.'],
        parser_class='JavaScriptParser',
        lexer_class='JavaScriptLexer'
    )
}

class JavaScriptLexer(BaseLexer):
    """JavaScript lexer implementation"""
    
    def __init__(self):
        super().__init__()
        self.specs = get_simple_token_specs(
            LANGUAGE_INFO['javascript'].keywords,
            LANGUAGE_INFO['javascript'].operators,
            LANGUAGE_INFO['javascript'].delimiters
        )
        self.comment_patterns = [
            (r'//.*$', 'SINGLE_LINE_COMMENT'),
            (r'/\*.*?\*/', 'MULTI_LINE_COMMENT')
        ]
        self.string_patterns = [
            (r'"[^"]*"', 'STRING'),
            (r"'[^']*'", 'STRING'),
            (r'`[^`]*`', 'TEMPLATE_STRING')
        ]

    def tokenize(self, source: str) -> List[Dict[str, Any]]:
        """Tokenize JavaScript source code"""
        tokens = tokenize_with_comments(source, self.specs, self.comment_patterns)
        
        # Handle JavaScript-specific patterns
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
            
            # Handle numbers (including decimals and scientific notation)
            if char.isdigit() or (char == '.' and i + 1 < len(source) and source[i + 1].isdigit()):
                number_start = i
                while i < len(source) and (source[i].isdigit() or source[i] == '.' or source[i].lower() == 'e' or source[i] == '+' or source[i] == '-'):
                    i += 1
                    column += 1
                
                number = source[number_start:i]
                processed_tokens.append({
                    'type': 'NUMBER',
                    'value': number,
                    'line': line,
                    'column': column - len(number)
                })
                continue
            
            # Handle identifiers and keywords
            if char.isalpha() or char == '_' or char == '$':
                identifier_start = i
                while i < len(source) and (source[i].isalnum() or source[i] == '_' or source[i] == '$'):
                    i += 1
                    column += 1
                
                identifier = source[identifier_start:i]
                
                # Check if it's a keyword
                if identifier in LANGUAGE_INFO['javascript'].keywords:
                    processed_tokens.append({
                        'type': f'KEYWORD_{identifier.upper()}',
                        'value': identifier,
                        'line': line,
                        'column': column - len(identifier)
                    })
                else:
                    processed_tokens.append({
                        'type': 'IDENTIFIER',
                        'value': identifier,
                        'line': line,
                        'column': column - len(identifier)
                    })
                continue
            
            # Handle operators
            if char in LANGUAGE_INFO['javascript'].operators:
                # Check for multi-character operators
                if i + 1 < len(source):
                    two_char = source[i:i + 2]
                    if two_char in LANGUAGE_INFO['javascript'].operators:
                        processed_tokens.append({
                            'type': f'OPERATOR_{two_char}',
                            'value': two_char,
                            'line': line,
                            'column': column
                        })
                        i += 2
                        column += 2
                        continue
                
                processed_tokens.append({
                    'type': f'OPERATOR_{char}',
                    'value': char,
                    'line': line,
                    'column': column
                })
                i += 1
                column += 1
                continue
            
            # Handle delimiters
            if char in LANGUAGE_INFO['javascript'].delimiters:
                processed_tokens.append({
                    'type': f'DELIMITER_{char}',
                    'value': char,
                    'line': line,
                    'column': column
                })
                i += 1
                column += 1
                continue
            
            # Handle whitespace
            if char.isspace():
                if char == '\n':
                    line += 1
                    column = 1
                else:
                    column += 1
                i += 1
                continue
            
            # Unknown character
            i += 1
            column += 1
        
        return processed_tokens

    def get_keywords(self) -> List[str]:
        """Get JavaScript keywords"""
        return LANGUAGE_INFO['javascript'].keywords

    def get_operators(self) -> List[str]:
        """Get JavaScript operators"""
        return LANGUAGE_INFO['javascript'].operators

class JavaScriptParser(BaseParser):
    """JavaScript parser implementation"""
    
    def __init__(self):
        self.tokens = []
        self.pos = 0

    def parse(self, tokens: List[Dict[str, Any]]) -> Program:
        """Parse JavaScript tokens into AST"""
        self.tokens = tokens
        self.pos = 0
        program = Program()
        
        while not self._is_at_end():
            stmt = self._parse_statement()
            if stmt:
                program.statements.append(stmt)
        
        return program

    def _parse_statement(self):
        """Parse a JavaScript statement"""
        if self._check('KEYWORD_VAR', 'KEYWORD_LET', 'KEYWORD_CONST'):
            return self._parse_variable_declaration()
        elif self._check('KEYWORD_FUNCTION'):
            return self._parse_function_declaration()
        elif self._check('KEYWORD_IF'):
            return self._parse_if_statement()
        elif self._check('KEYWORD_FOR'):
            return self._parse_for_statement()
        elif self._check('KEYWORD_WHILE'):
            return self._parse_while_statement()
        elif self._check('KEYWORD_DO'):
            return self._parse_do_while_statement()
        elif self._check('KEYWORD_RETURN'):
            return self._parse_return_statement()
        else:
            return self._parse_expression_statement()

    def _parse_variable_declaration(self):
        """Parse variable declaration"""
        var_type = self._advance().get('value')
        name = self._consume('IDENTIFIER', 'Expect variable name after var/let/const.').get('value')
        initializer = None
        
        if self._match('OPERATOR_='):
            initializer = self._parse_expression()
        
        self._consume('DELIMITER_;', 'Expect \';\' after statement.')
        return VariableDeclaration(var_type, name, initializer)

    def _parse_function_declaration(self):
        """Parse function declaration"""
        self._consume('KEYWORD_FUNCTION', 'Expect \'function\' keyword.')
        name = self._consume('IDENTIFIER', 'Expect function name.').get('value')
        self._consume('DELIMITER_(', 'Expect \'(\' after function name.')
        
        # Parse parameters
        parameters = []
        if not self._check('DELIMITER_)'):
            parameters.append(self._consume('IDENTIFIER', 'Expect parameter name.').get('value'))
            while self._match('DELIMITER_,'):
                parameters.append(self._consume('IDENTIFIER', 'Expect parameter name.').get('value'))
        
        self._consume('DELIMITER_)', 'Expect \')\' after parameters.')
        self._consume('DELIMITER_{', 'Expect \'{\' before function body.')
        
        # Parse function body
        body = []
        while not self._is_at_end() and not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        self._consume('DELIMITER_}', 'Expect \'}\' after function body.')
        return FunctionDefinition(name, parameters, body)

    def _parse_if_statement(self):
        """Parse if statement"""
        self._consume('KEYWORD_IF', 'Expect \'if\' keyword.')
        self._consume('DELIMITER_(', 'Expect \'(\' after if.')
        condition = self._parse_expression()
        self._consume('DELIMITER_)', 'Expect \')\' after condition.')
        self._consume('DELIMITER_{', 'Expect \'{\' before if body.')
        
        # Parse if body
        if_body = []
        while not self._is_at_end() and not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                if_body.append(stmt)
        
        self._consume('DELIMITER_}', 'Expect \'}\' after if body.')
        
        # Parse else clause if present
        else_body = []
        if self._match('KEYWORD_ELSE'):
            self._consume('DELIMITER_{', 'Expect \'{\' before else body.')
            while not self._is_at_end() and not self._check('DELIMITER_}'):
                stmt = self._parse_statement()
                if stmt:
                    else_body.append(stmt)
            self._consume('DELIMITER_}', 'Expect \'}\' after else body.')
        
        return {'type': 'IfStatement', 'condition': condition, 'if_body': if_body, 'else_body': else_body}

    def _parse_for_statement(self):
        """Parse for statement"""
        self._consume('KEYWORD_FOR', 'Expect \'for\' keyword.')
        self._consume('DELIMITER_(', 'Expect \'(\' after for.')
        
        # Parse initialization
        init = None
        if not self._check('DELIMITER_;'):
            init = self._parse_expression()
        self._consume('DELIMITER_;', 'Expect \';\' after for initialization.')
        
        # Parse condition
        condition = None
        if not self._check('DELIMITER_;'):
            condition = self._parse_expression()
        self._consume('DELIMITER_;', 'Expect \';\' after for condition.')
        
        # Parse increment
        increment = None
        if not self._check('DELIMITER_)'):
            increment = self._parse_expression()
        self._consume('DELIMITER_)', 'Expect \')\' after for increment.')
        self._consume('DELIMITER_{', 'Expect \'{\' before for body.')
        
        # Parse for body
        body = []
        while not self._is_at_end() and not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        self._consume('DELIMITER_}', 'Expect \'}\' after for body.')
        return {'type': 'ForStatement', 'init': init, 'condition': condition, 'increment': increment, 'body': body}

    def _parse_while_statement(self):
        """Parse while statement"""
        self._consume('KEYWORD_WHILE', 'Expect \'while\' keyword.')
        self._consume('DELIMITER_(', 'Expect \'(\' after while.')
        condition = self._parse_expression()
        self._consume('DELIMITER_)', 'Expect \')\' after condition.')
        self._consume('DELIMITER_{', 'Expect \'{\' before while body.')
        
        # Parse while body
        body = []
        while not self._is_at_end() and not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        self._consume('DELIMITER_}', 'Expect \'}\' after while body.')
        return {'type': 'WhileStatement', 'condition': condition, 'body': body}

    def _parse_do_while_statement(self):
        """Parse do-while statement"""
        self._consume('KEYWORD_DO', 'Expect \'do\' keyword.')
        self._consume('DELIMITER_{', 'Expect \'{\' before do body.')
        
        # Parse do body
        body = []
        while not self._is_at_end() and not self._check('DELIMITER_}'):
            stmt = self._parse_statement()
            if stmt:
                body.append(stmt)
        
        self._consume('DELIMITER_}', 'Expect \'}\' after do body.')
        self._consume('KEYWORD_WHILE', 'Expect \'while\' after do body.')
        self._consume('DELIMITER_(', 'Expect \'(\' after while.')
        condition = self._parse_expression()
        self._consume('DELIMITER_)', 'Expect \')\' after condition.')
        self._consume('DELIMITER_;', 'Expect \';\' after do-while.')
        
        return {'type': 'DoWhileStatement', 'condition': condition, 'body': body}

    def _parse_return_statement(self):
        """Parse return statement"""
        self._consume('KEYWORD_RETURN', 'Expect \'return\' keyword.')
        value = None
        if not self._check('DELIMITER_;'):
            value = self._parse_expression()
        self._consume('DELIMITER_;', 'Expect \';\' after return.')
        return {'type': 'ReturnStatement', 'value': value}

    def _parse_expression_statement(self):
        """Parse expression statement"""
        expr = self._parse_expression()
        self._consume('DELIMITER_;', 'Expect \';\' after expression.')
        return expr

    def _parse_expression(self):
        """Parse JavaScript expression"""
        return self._parse_assignment()

    def _parse_assignment(self):
        """Parse assignment expression"""
        expr = self._parse_equality()
        
        if self._match('OPERATOR_=', 'OPERATOR_+=', 'OPERATOR_-=', 'OPERATOR_*=', 'OPERATOR_/='):
            op = self._previous().get('value')
            right = self._parse_assignment()
            if isinstance(expr, Identifier):
                return Assignment(expr.name, right)
            else:
                return BinaryOperation(op, expr, right)
        
        return expr

    def _parse_equality(self):
        """Parse equality expression"""
        expr = self._parse_comparison()
        while self._match('OPERATOR_==', 'OPERATOR_!=', 'OPERATOR_===', 'OPERATOR_!=='):
            op = self._previous().get('value')
            right = self._parse_comparison()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_comparison(self):
        """Parse comparison expression"""
        expr = self._parse_addition()
        while self._match('OPERATOR_<', 'OPERATOR_>', 'OPERATOR_<=', 'OPERATOR_>='):
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
        expr = self._parse_unary()
        while self._match('OPERATOR_*', 'OPERATOR_/', 'OPERATOR_%'):
            op = self._previous().get('value')
            right = self._parse_unary()
            expr = BinaryOperation(op, expr, right)
        return expr

    def _parse_unary(self):
        """Parse unary expression"""
        if self._match('OPERATOR_!', 'OPERATOR_-', 'OPERATOR_+'):
            op = self._previous().get('value')
            right = self._parse_unary()
            return BinaryOperation(op, right, None)
        
        return self._parse_call()

    def _parse_call(self):
        """Parse function call"""
        expr = self._parse_primary()
        
        while True:
            if self._match('DELIMITER_('):
                arguments = self._parse_arguments()
                expr = FunctionCall(expr, arguments)
            elif self._match('DELIMITER_['):
                index = self._parse_expression()
                self._consume('DELIMITER_]', 'Expect \']\' after index.')
                expr = {'type': 'MemberAccess', 'object': expr, 'property': index}
            elif self._match('DELIMITER_.'):
                property_name = self._consume('IDENTIFIER', 'Expect property name after \'.\'.').get('value')
                expr = {'type': 'MemberAccess', 'object': expr, 'property': Identifier(property_name)}
            else:
                break
        
        return expr

    def _parse_arguments(self):
        """Parse function arguments"""
        args = []
        if not self._check('DELIMITER_)'):
            args.append(self._parse_expression())
            while self._match('DELIMITER_,'):
                args.append(self._parse_expression())
        self._consume('DELIMITER_)', 'Expect \')\' after arguments.')
        return args

    def _parse_primary(self):
        """Parse primary expression"""
        if self._match('NUMBER'):
            value = self._previous().get('value')
            try:
                return Literal(int(value))
            except ValueError:
                return Literal(float(value))
        elif self._match('STRING', 'TEMPLATE_STRING'):
            return Literal(self._previous().get('value'))
        elif self._match('KEYWORD_TRUE'):
            return Literal(True)
        elif self._match('KEYWORD_FALSE'):
            return Literal(False)
        elif self._match('KEYWORD_NULL'):
            return Literal(None)
        elif self._match('KEYWORD_UNDEFINED'):
            return Literal('undefined')
        elif self._match('IDENTIFIER'):
            return Identifier(self._previous().get('value'))
        elif self._match('DELIMITER_('):
            expr = self._parse_expression()
            self._consume('DELIMITER_)', 'Expect \')\' after expression.')
            return expr
        elif self._match('DELIMITER_['):
            # Array literal
            elements = []
            if not self._check('DELIMITER_]'):
                elements.append(self._parse_expression())
                while self._match('DELIMITER_,'):
                    elements.append(self._parse_expression())
            self._consume('DELIMITER_]', 'Expect \']\' after array elements.')
            return {'type': 'ArrayLiteral', 'elements': elements}
        elif self._match('DELIMITER_{'):
            # Object literal
            properties = []
            if not self._check('DELIMITER_}'):
                key = self._consume('IDENTIFIER', 'Expect property name.').get('value')
                self._consume('DELIMITER_:', 'Expect \':\' after property name.')
                value = self._parse_expression()
                properties.append({'key': key, 'value': value})
                while self._match('DELIMITER_,'):
                    key = self._consume('IDENTIFIER', 'Expect property name.').get('value')
                    self._consume('DELIMITER_:', 'Expect \':\' after property name.')
                    value = self._parse_expression()
                    properties.append({'key': key, 'value': value})
            self._consume('DELIMITER_}', 'Expect \'}\' after object properties.')
            return {'type': 'ObjectLiteral', 'properties': properties}
        
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

    def _check(self, *types):
        """Check if current token matches any of the given types"""
        if self._is_at_end():
            return False
        current_type = self._peek().get('type')
        return any(current_type == type_ for type_ in types)

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
        return [node.__name__ for node in [Program, VariableDeclaration, Assignment, BinaryOperation, Literal, Identifier, FunctionCall, FunctionDefinition]]

# Export language info
def get_language_info():
    """Get JavaScript language information"""
    return LANGUAGE_INFO['javascript']
