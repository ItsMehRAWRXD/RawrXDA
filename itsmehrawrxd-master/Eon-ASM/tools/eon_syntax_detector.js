/**
 * EON Syntax Detection Template
 * Comprehensive syntax validator for EON language
 * Based on eon_full_compiler.eon and eon_v1_compiler.eon patterns
 */

class EONSyntaxDetector {
    constructor() {
        this.errors = [];
        this.warnings = [];
        this.tokens = [];
        this.currentToken = 0;
        
        // EON keywords based on compiler analysis
        this.keywords = new Set([
            'def', 'model', 'func', 'let', 'const', 'ret', 'if', 'else', 'loop', 
            'for', 'while', 'break', 'continue', 'import', 'export', 'match', 
            'case', 'default', 'try', 'catch', 'finally', 'throw', 'int', 
            'float', 'bool', 'void', 'auto', 'string', 'char'
        ]);
        
        // Valid operators
        this.operators = new Set([
            '+', '-', '*', '/', '%', '=', '==', '!=', '<', '>', '<=', '>=',
            '&&', '||', '!', '&', '|', '^', '~', '<<', '>>', '++', '--',
            '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='
        ]);
        
        // Valid punctuation
        this.punctuation = new Set([
            '(', ')', '{', '}', '[', ']', ';', ',', ':', '->', '.', '::'
        ]);
    }

    /**
     * Main syntax validation function
     * @param {string} source - EON source code
     * @returns {Object} - Validation result with errors, warnings, and AST
     */
    validate(source) {
        this.errors = [];
        this.warnings = [];
        this.tokens = [];
        this.currentToken = 0;
        
        try {
            // Step 1: Tokenize
            this.tokens = this.tokenize(source);
            
            // Step 2: Parse and validate syntax
            const ast = this.parseProgram();
            
            // Step 3: Additional semantic checks
            this.performSemanticChecks(ast);
            
            return {
                valid: this.errors.length === 0,
                errors: this.errors,
                warnings: this.warnings,
                ast: ast,
                tokens: this.tokens
            };
            
        } catch (error) {
            this.errors.push({
                type: 'CRITICAL_ERROR',
                message: error.message,
                line: this.getCurrentLine(),
                column: this.getCurrentColumn()
            });
            
            return {
                valid: false,
                errors: this.errors,
                warnings: this.warnings,
                ast: null,
                tokens: this.tokens
            };
        }
    }

    /**
     * Tokenize EON source code
     * @param {string} source - Source code to tokenize
     * @returns {Array} - Array of tokens
     */
    tokenize(source) {
        const tokens = [];
        let i = 0;
        let line = 1;
        let column = 1;
        
        while (i < source.length) {
            const char = source[i];
            
            // Skip whitespace but track line/column
            if (/\s/.test(char)) {
                if (char === '\n') {
                    line++;
                    column = 1;
                } else {
                    column++;
                }
                i++;
                continue;
            }
            
            // Skip comments
            if (char === '/' && source[i + 1] === '/') {
                while (i < source.length && source[i] !== '\n') {
                    i++;
                }
                continue;
            }
            
            // Multi-character operators
            if (i + 1 < source.length) {
                const twoChar = source.substr(i, 2);
                if (this.operators.has(twoChar) || this.punctuation.has(twoChar)) {
                    tokens.push({
                        type: this.getTokenType(twoChar),
                        value: twoChar,
                        line: line,
                        column: column
                    });
                    i += 2;
                    column += 2;
                    continue;
                }
            }
            
            // Single character tokens
            if (this.operators.has(char) || this.punctuation.has(char)) {
                tokens.push({
                    type: this.getTokenType(char),
                    value: char,
                    line: line,
                    column: column
                });
                i++;
                column++;
                continue;
            }
            
            // Numbers
            if (/\d/.test(char)) {
                const numberToken = this.parseNumber(source, i, line, column);
                tokens.push(numberToken);
                i = numberToken.end;
                column += (numberToken.end - i);
                continue;
            }
            
            // Strings
            if (char === '"') {
                const stringToken = this.parseString(source, i, line, column);
                tokens.push(stringToken);
                i = stringToken.end;
                column += (stringToken.end - i);
                continue;
            }
            
            // Identifiers and keywords
            if (/[a-zA-Z_]/.test(char)) {
                const identifierToken = this.parseIdentifier(source, i, line, column);
                tokens.push(identifierToken);
                i = identifierToken.end;
                column += (identifierToken.end - i);
                continue;
            }
            
            // Unknown character
            this.addError(`Unexpected character: ${char}`, line, column);
            i++;
            column++;
        }
        
        // Add EOF token
        tokens.push({
            type: 'EOF',
            value: '',
            line: line,
            column: column
        });
        
        return tokens;
    }

    /**
     * Parse a number literal
     */
    parseNumber(source, start, line, column) {
        let i = start;
        let value = '';
        
        // Handle hex numbers
        if (source[i] === '0' && (source[i + 1] === 'x' || source[i + 1] === 'X')) {
            value += source[i++];
            value += source[i++];
            while (i < source.length && /[0-9a-fA-F]/.test(source[i])) {
                value += source[i++];
            }
        }
        // Handle binary numbers
        else if (source[i] === '0' && (source[i + 1] === 'b' || source[i + 1] === 'B')) {
            value += source[i++];
            value += source[i++];
            while (i < source.length && /[01]/.test(source[i])) {
                value += source[i++];
            }
        }
        // Handle decimal numbers
        else {
            while (i < source.length && /\d/.test(source[i])) {
                value += source[i++];
            }
            
            // Handle floating point
            if (i < source.length && source[i] === '.') {
                value += source[i++];
                while (i < source.length && /\d/.test(source[i])) {
                    value += source[i++];
                }
            }
        }
        
        return {
            type: 'NUMBER',
            value: value,
            line: line,
            column: column,
            end: i
        };
    }

    /**
     * Parse a string literal
     */
    parseString(source, start, line, column) {
        let i = start + 1; // Skip opening quote
        let value = '';
        
        while (i < source.length) {
            const char = source[i];
            
            if (char === '"') {
                return {
                    type: 'STRING',
                    value: value,
                    line: line,
                    column: column,
                    end: i + 1
                };
            } else if (char === '\\') {
                i++; // Skip escape character
                if (i < source.length) {
                    const nextChar = source[i];
                    switch (nextChar) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case 'r': value += '\r'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        default: value += nextChar; break;
                    }
                }
            } else {
                value += char;
            }
            i++;
        }
        
        this.addError('Unterminated string literal', line, column);
        return {
            type: 'STRING',
            value: value,
            line: line,
            column: column,
            end: i
        };
    }

    /**
     * Parse an identifier or keyword
     */
    parseIdentifier(source, start, line, column) {
        let i = start;
        let value = '';
        
        while (i < source.length && /[a-zA-Z0-9_]/.test(source[i])) {
            value += source[i++];
        }
        
        const type = this.keywords.has(value) ? value.toUpperCase() : 'IDENTIFIER';
        
        return {
            type: type,
            value: value,
            line: line,
            column: column,
            end: i
        };
    }

    /**
     * Get token type for operators and punctuation
     */
    getTokenType(value) {
        const typeMap = {
            '+': 'PLUS', '-': 'MINUS', '*': 'MULTIPLY', '/': 'DIVIDE', '%': 'MODULO',
            '=': 'ASSIGN', '==': 'EQUALS', '!=': 'NOT_EQUALS', '<': 'LESS', '>': 'GREATER',
            '<=': 'LESS_EQUAL', '>=': 'GREATER_EQUAL', '&&': 'AND', '||': 'OR', '!': 'NOT',
            '(': 'LPAREN', ')': 'RPAREN', '{': 'LBRACE', '}': 'RBRACE',
            '[': 'LBRACKET', ']': 'RBRACKET', ';': 'SEMICOLON', ',': 'COMMA',
            ':': 'COLON', '->': 'ARROW', '.': 'DOT', '::': 'SCOPE'
        };
        
        return typeMap[value] || 'UNKNOWN';
    }

    /**
     * Parse the main program structure
     */
    parseProgram() {
        const program = {
            type: 'PROGRAM',
            declarations: []
        };
        
        while (this.currentToken < this.tokens.length && this.tokens[this.currentToken].type !== 'EOF') {
            const declaration = this.parseTopLevelDeclaration();
            if (declaration) {
                program.declarations.push(declaration);
            }
        }
        
        return program;
    }

    /**
     * Parse top-level declarations (functions, models, imports)
     */
    parseTopLevelDeclaration() {
        const token = this.tokens[this.currentToken];
        
        switch (token.type) {
            case 'DEF':
                return this.parseDefinition();
            case 'IMPORT':
                return this.parseImport();
            case 'EXPORT':
                return this.parseExport();
            default:
                this.addError(`Expected top-level declaration, got ${token.type}`, token.line, token.column);
                this.currentToken++;
                return null;
        }
    }

    /**
     * Parse function or model definitions
     */
    parseDefinition() {
        this.currentToken++; // Skip 'def'
        
        const token = this.tokens[this.currentToken];
        if (token.type === 'FUNC') {
            return this.parseFunctionDefinition();
        } else if (token.type === 'MODEL') {
            return this.parseModelDefinition();
        } else {
            this.addError(`Expected 'func' or 'model' after 'def', got ${token.type}`, token.line, token.column);
            return null;
        }
    }

    /**
     * Parse function definition
     */
    parseFunctionDefinition() {
        this.currentToken++; // Skip 'func'
        
        // Parse function name
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected function name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        // Parse parameters
        const params = this.parseParameterList();
        
        // Parse return type
        let returnType = null;
        if (this.tokens[this.currentToken].type === 'ARROW') {
            this.currentToken++; // Skip '->'
            returnType = this.parseType();
        }
        
        // Parse function body
        const body = this.parseBlock();
        
        return {
            type: 'FUNCTION_DEFINITION',
            name: nameToken.value,
            parameters: params,
            returnType: returnType,
            body: body
        };
    }

    /**
     * Parse model definition
     */
    parseModelDefinition() {
        this.currentToken++; // Skip 'model'
        
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected model name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        const fields = this.parseModelFields();
        
        return {
            type: 'MODEL_DEFINITION',
            name: nameToken.value,
            fields: fields
        };
    }

    /**
     * Parse parameter list
     */
    parseParameterList() {
        const params = [];
        
        if (this.tokens[this.currentToken].type !== 'LPAREN') {
            this.addError(`Expected '(', got ${this.tokens[this.currentToken].type}`, 
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return params;
        }
        this.currentToken++; // Skip '('
        
        while (this.tokens[this.currentToken].type !== 'RPAREN') {
            const param = this.parseParameter();
            if (param) {
                params.push(param);
            }
            
            if (this.tokens[this.currentToken].type === 'COMMA') {
                this.currentToken++;
            } else if (this.tokens[this.currentToken].type !== 'RPAREN') {
                this.addError(`Expected ',' or ')', got ${this.tokens[this.currentToken].type}`,
                             this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
                break;
            }
        }
        
        this.currentToken++; // Skip ')'
        return params;
    }

    /**
     * Parse single parameter
     */
    parseParameter() {
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected parameter name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        if (this.tokens[this.currentToken].type !== 'COLON') {
            this.addError(`Expected ':', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip ':'
        
        const type = this.parseType();
        
        return {
            name: nameToken.value,
            type: type
        };
    }

    /**
     * Parse type annotation
     */
    parseType() {
        const token = this.tokens[this.currentToken];
        
        // Handle pointer types
        if (token.type === 'MULTIPLY') {
            this.currentToken++;
            const baseType = this.parseType();
            return {
                type: 'POINTER_TYPE',
                baseType: baseType
            };
        }
        
        // Handle basic types
        if (['INT', 'FLOAT', 'BOOL', 'VOID', 'STRING', 'CHAR', 'AUTO'].includes(token.type)) {
            this.currentToken++;
            return {
                type: 'BASIC_TYPE',
                name: token.value
            };
        }
        
        // Handle identifier types
        if (token.type === 'IDENTIFIER') {
            this.currentToken++;
            return {
                type: 'IDENTIFIER_TYPE',
                name: token.value
            };
        }
        
        this.addError(`Expected type, got ${token.type}`, token.line, token.column);
        return null;
    }

    /**
     * Parse block statement
     */
    parseBlock() {
        if (this.tokens[this.currentToken].type !== 'LBRACE') {
            this.addError(`Expected '{', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip '{'
        
        const statements = [];
        
        while (this.tokens[this.currentToken].type !== 'RBRACE') {
            const statement = this.parseStatement();
            if (statement) {
                statements.push(statement);
            }
        }
        
        this.currentToken++; // Skip '}'
        
        return {
            type: 'BLOCK',
            statements: statements
        };
    }

    /**
     * Parse individual statements
     */
    parseStatement() {
        const token = this.tokens[this.currentToken];
        
        switch (token.type) {
            case 'LET':
                return this.parseVariableDeclaration();
            case 'CONST':
                return this.parseConstantDeclaration();
            case 'RET':
                return this.parseReturnStatement();
            case 'IF':
                return this.parseIfStatement();
            case 'LOOP':
                return this.parseLoopStatement();
            case 'LBRACE':
                return this.parseBlock();
            default:
                return this.parseExpressionStatement();
        }
    }

    /**
     * Parse variable declaration
     */
    parseVariableDeclaration() {
        this.currentToken++; // Skip 'let'
        
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected variable name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        let type = null;
        if (this.tokens[this.currentToken].type === 'COLON') {
            this.currentToken++; // Skip ':'
            type = this.parseType();
        }
        
        let value = null;
        if (this.tokens[this.currentToken].type === 'ASSIGN') {
            this.currentToken++; // Skip '='
            value = this.parseExpression();
        }
        
        // Expect semicolon
        if (this.tokens[this.currentToken].type === 'SEMICOLON') {
            this.currentToken++;
        } else {
            this.addWarning(`Expected ';' after variable declaration`, 
                           this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
        }
        
        return {
            type: 'VARIABLE_DECLARATION',
            name: nameToken.value,
            variableType: type,
            value: value
        };
    }

    /**
     * Parse return statement
     */
    parseReturnStatement() {
        this.currentToken++; // Skip 'ret'
        
        let value = null;
        if (this.tokens[this.currentToken].type !== 'SEMICOLON') {
            value = this.parseExpression();
        }
        
        // Expect semicolon
        if (this.tokens[this.currentToken].type === 'SEMICOLON') {
            this.currentToken++;
        } else {
            this.addWarning(`Expected ';' after return statement`, 
                           this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
        }
        
        return {
            type: 'RETURN_STATEMENT',
            value: value
        };
    }

    /**
     * Parse expression statement
     */
    parseExpressionStatement() {
        const expression = this.parseExpression();
        
        // Expect semicolon
        if (this.tokens[this.currentToken].type === 'SEMICOLON') {
            this.currentToken++;
        } else {
            this.addWarning(`Expected ';' after expression`, 
                           this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
        }
        
        return {
            type: 'EXPRESSION_STATEMENT',
            expression: expression
        };
    }

    /**
     * Parse expressions (simplified version)
     */
    parseExpression() {
        return this.parseAssignmentExpression();
    }

    parseAssignmentExpression() {
        const left = this.parseLogicalOrExpression();
        
        if (this.tokens[this.currentToken].type === 'ASSIGN') {
            this.currentToken++;
            const right = this.parseAssignmentExpression();
            return {
                type: 'ASSIGNMENT',
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseLogicalOrExpression() {
        let left = this.parseLogicalAndExpression();
        
        while (this.tokens[this.currentToken].type === 'OR') {
            this.currentToken++;
            const right = this.parseLogicalAndExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: '||',
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseLogicalAndExpression() {
        let left = this.parseEqualityExpression();
        
        while (this.tokens[this.currentToken].type === 'AND') {
            this.currentToken++;
            const right = this.parseEqualityExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: '&&',
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseEqualityExpression() {
        let left = this.parseRelationalExpression();
        
        while (['EQUALS', 'NOT_EQUALS'].includes(this.tokens[this.currentToken].type)) {
            const operator = this.tokens[this.currentToken].value;
            this.currentToken++;
            const right = this.parseRelationalExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: operator,
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseRelationalExpression() {
        let left = this.parseAdditiveExpression();
        
        while (['LESS', 'GREATER', 'LESS_EQUAL', 'GREATER_EQUAL'].includes(this.tokens[this.currentToken].type)) {
            const operator = this.tokens[this.currentToken].value;
            this.currentToken++;
            const right = this.parseAdditiveExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: operator,
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseAdditiveExpression() {
        let left = this.parseMultiplicativeExpression();
        
        while (['PLUS', 'MINUS'].includes(this.tokens[this.currentToken].type)) {
            const operator = this.tokens[this.currentToken].value;
            this.currentToken++;
            const right = this.parseMultiplicativeExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: operator,
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parseMultiplicativeExpression() {
        let left = this.parsePrimaryExpression();
        
        while (['MULTIPLY', 'DIVIDE', 'MODULO'].includes(this.tokens[this.currentToken].type)) {
            const operator = this.tokens[this.currentToken].value;
            this.currentToken++;
            const right = this.parsePrimaryExpression();
            left = {
                type: 'BINARY_OPERATION',
                operator: operator,
                left: left,
                right: right
            };
        }
        
        return left;
    }

    parsePrimaryExpression() {
        const token = this.tokens[this.currentToken];
        
        switch (token.type) {
            case 'IDENTIFIER':
                this.currentToken++;
                return {
                    type: 'IDENTIFIER',
                    name: token.value
                };
                
            case 'NUMBER':
                this.currentToken++;
                return {
                    type: 'NUMBER_LITERAL',
                    value: token.value
                };
                
            case 'STRING':
                this.currentToken++;
                return {
                    type: 'STRING_LITERAL',
                    value: token.value
                };
                
            case 'LPAREN':
                this.currentToken++; // Skip '('
                const expr = this.parseExpression();
                if (this.tokens[this.currentToken].type !== 'RPAREN') {
                    this.addError(`Expected ')', got ${this.tokens[this.currentToken].type}`,
                                 this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
                } else {
                    this.currentToken++; // Skip ')'
                }
                return expr;
                
            default:
                this.addError(`Unexpected token in primary expression: ${token.type}`, 
                             token.line, token.column);
                this.currentToken++;
                return null;
        }
    }

    /**
     * Parse import statement
     */
    parseImport() {
        this.currentToken++; // Skip 'import'
        
        const moduleToken = this.tokens[this.currentToken];
        if (moduleToken.type !== 'STRING' && moduleToken.type !== 'IDENTIFIER') {
            this.addError(`Expected module name, got ${moduleToken.type}`, moduleToken.line, moduleToken.column);
            return null;
        }
        this.currentToken++;
        
        return {
            type: 'IMPORT_STATEMENT',
            module: moduleToken.value
        };
    }

    /**
     * Parse export statement
     */
    parseExport() {
        this.currentToken++; // Skip 'export'
        
        const declaration = this.parseTopLevelDeclaration();
        
        return {
            type: 'EXPORT_STATEMENT',
            declaration: declaration
        };
    }

    /**
     * Parse model fields
     */
    parseModelFields() {
        const fields = [];
        
        if (this.tokens[this.currentToken].type !== 'LBRACE') {
            this.addError(`Expected '{', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return fields;
        }
        this.currentToken++; // Skip '{'
        
        while (this.tokens[this.currentToken].type !== 'RBRACE') {
            const field = this.parseModelField();
            if (field) {
                fields.push(field);
            }
            
            if (this.tokens[this.currentToken].type === 'COMMA') {
                this.currentToken++;
            } else if (this.tokens[this.currentToken].type !== 'RBRACE') {
                this.addError(`Expected ',' or '}', got ${this.tokens[this.currentToken].type}`,
                             this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
                break;
            }
        }
        
        this.currentToken++; // Skip '}'
        return fields;
    }

    /**
     * Parse model field
     */
    parseModelField() {
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected field name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        if (this.tokens[this.currentToken].type !== 'COLON') {
            this.addError(`Expected ':', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip ':'
        
        const type = this.parseType();
        
        return {
            name: nameToken.value,
            type: type
        };
    }

    /**
     * Parse if statement
     */
    parseIfStatement() {
        this.currentToken++; // Skip 'if'
        
        if (this.tokens[this.currentToken].type !== 'LPAREN') {
            this.addError(`Expected '(', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip '('
        
        const condition = this.parseExpression();
        
        if (this.tokens[this.currentToken].type !== 'RPAREN') {
            this.addError(`Expected ')', got ${this.tokens[this.currentToken].type}`,
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip ')'
        
        const thenBranch = this.parseStatement();
        
        let elseBranch = null;
        if (this.tokens[this.currentToken].type === 'ELSE') {
            this.currentToken++; // Skip 'else'
            elseBranch = this.parseStatement();
        }
        
        return {
            type: 'IF_STATEMENT',
            condition: condition,
            thenBranch: thenBranch,
            elseBranch: elseBranch
        };
    }

    /**
     * Parse loop statement
     */
    parseLoopStatement() {
        this.currentToken++; // Skip 'loop'
        
        const body = this.parseStatement();
        
        return {
            type: 'LOOP_STATEMENT',
            body: body
        };
    }

    /**
     * Parse constant declaration
     */
    parseConstantDeclaration() {
        this.currentToken++; // Skip 'const'
        
        const nameToken = this.tokens[this.currentToken];
        if (nameToken.type !== 'IDENTIFIER') {
            this.addError(`Expected constant name, got ${nameToken.type}`, nameToken.line, nameToken.column);
            return null;
        }
        this.currentToken++;
        
        let type = null;
        if (this.tokens[this.currentToken].type === 'COLON') {
            this.currentToken++; // Skip ':'
            type = this.parseType();
        }
        
        if (this.tokens[this.currentToken].type !== 'ASSIGN') {
            this.addError(`Expected '=' after constant declaration`, 
                         this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
            return null;
        }
        this.currentToken++; // Skip '='
        
        const value = this.parseExpression();
        
        // Expect semicolon
        if (this.tokens[this.currentToken].type === 'SEMICOLON') {
            this.currentToken++;
        } else {
            this.addWarning(`Expected ';' after constant declaration`, 
                           this.tokens[this.currentToken].line, this.tokens[this.currentToken].column);
        }
        
        return {
            type: 'CONSTANT_DECLARATION',
            name: nameToken.value,
            constantType: type,
            value: value
        };
    }

    /**
     * Perform additional semantic checks
     */
    performSemanticChecks(ast) {
        // Check for main function
        const hasMain = ast.declarations.some(decl => 
            decl.type === 'FUNCTION_DEFINITION' && decl.name === 'main'
        );
        
        if (!hasMain) {
            this.addWarning('No main function found - program may not be executable');
        }
        
        // Check for unused variables, etc.
        // This is a simplified version - a full implementation would track symbol tables
    }

    /**
     * Add error to the error list
     */
    addError(message, line, column) {
        this.errors.push({
            type: 'SYNTAX_ERROR',
            message: message,
            line: line,
            column: column
        });
    }

    /**
     * Add warning to the warning list
     */
    addWarning(message, line, column) {
        this.warnings.push({
            type: 'WARNING',
            message: message,
            line: line,
            column: column
        });
    }

    /**
     * Get current line number
     */
    getCurrentLine() {
        return this.currentToken < this.tokens.length ? 
               this.tokens[this.currentToken].line : 1;
    }

    /**
     * Get current column number
     */
    getCurrentColumn() {
        return this.currentToken < this.tokens.length ? 
               this.tokens[this.currentToken].column : 1;
    }
}

/**
 * EON Syntax Template Generator
 * Creates example EON code with correct syntax
 */
class EONSyntaxTemplate {
    constructor() {
        this.templates = {
            basic_function: `def func main() {
    let x: int = 42;
    ret x;
}`,
            
            function_with_params: `def func add(a: int, b: int) -> int {
    let result: int = a + b;
    ret result;
}`,
            
            model_definition: `def model Point {
    x: int,
    y: int
}`,
            
            conditional_logic: `def func check_value(x: int) -> bool {
    if (x > 0) {
        ret true;
    } else {
        ret false;
    }
}`,
            
            loop_example: `def func count_to_ten() -> void {
    let i: int = 0;
    loop {
        if (i >= 10) {
            break;
        }
        i = i + 1;
    }
}`,
            
            import_example: `import std.io
import std.string

def func main() {
    let message: string = "Hello, EON!";
    ret 0;
}`
        };
    }

    /**
     * Get template by name
     */
    getTemplate(name) {
        return this.templates[name] || this.templates.basic_function;
    }

    /**
     * Get all available templates
     */
    getAllTemplates() {
        return Object.keys(this.templates);
    }

    /**
     * Generate custom template based on requirements
     */
    generateCustomTemplate(requirements) {
        let template = '';
        
        if (requirements.imports) {
            requirements.imports.forEach(imp => {
                template += `import ${imp}\n`;
            });
            template += '\n';
        }
        
        if (requirements.models) {
            requirements.models.forEach(model => {
                template += `def model ${model.name} {\n`;
                model.fields.forEach(field => {
                    template += `    ${field.name}: ${field.type},\n`;
                });
                template += '}\n\n';
            });
        }
        
        if (requirements.functions) {
            requirements.functions.forEach(func => {
                template += `def func ${func.name}(`;
                if (func.parameters) {
                    template += func.parameters.map(p => `${p.name}: ${p.type}`).join(', ');
                }
                template += ')';
                if (func.returnType) {
                    template += ` -> ${func.returnType}`;
                }
                template += ' {\n';
                if (func.body) {
                    template += func.body;
                } else {
                    template += '    // TODO: Implement function body\n';
                }
                template += '}\n\n';
            });
        }
        
        return template;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { EONSyntaxDetector, EONSyntaxTemplate };
}

// Global access for browser usage
if (typeof window !== 'undefined') {
    window.EONSyntaxDetector = EONSyntaxDetector;
    window.EONSyntaxTemplate = EONSyntaxTemplate;
}
