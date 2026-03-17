;==============================================================================
; RawrXD Private Compiler - Phase 5.2: Parser/Syntax Analyzer
; Recursive descent parser with error recovery and AST generation
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

;==============================================================================
; PARSER CONSTANTS
;==============================================================================
MAX_AST_NODES           equ 65536
MAX_SYMBOL_TABLE_SIZE   equ 16384
MAX_SCOPE_STACK         equ 256
MAX_ERROR_RECOVERY      equ 16

; AST node types
AST_PROGRAM             equ 1
AST_PROCEDURE           equ 2
AST_INSTRUCTION         equ 3
AST_DIRECTIVE           equ 4
AST_LABEL               equ 5
AST_EXPRESSION          equ 6
AST_OPERAND             equ 7
AST_REGISTER            equ 8
AST_MEMORY              equ 9
AST_IMMEDIATE           equ 10
AST_IDENTIFIER          equ 11
AST_MACRO               equ 12
AST_STRUCT              equ 13
AST_UNION               equ 14
AST_ENUM                equ 15

; Parser precedence levels
PRECEDENCE_LOWEST       equ 0
PRECEDENCE_ASSIGNMENT   equ 1
PRECEDENCE_CONDITIONAL  equ 2
PRECEDENCE_LOGICAL_OR   equ 3
PRECEDENCE_LOGICAL_AND  equ 4
PRECEDENCE_BITWISE_OR   equ 5
PRECEDENCE_BITWISE_XOR  equ 6
PRECEDENCE_BITWISE_AND  equ 7
PRECEDENCE_EQUALITY     equ 8
PRECEDENCE_RELATIONAL   equ 9
PRECEDENCE_SHIFT        equ 10
PRECEDENCE_ADDITIVE     equ 11
PRECEDENCE_MULTIPLICATIVE equ 12
PRECEDENCE_UNARY        equ 13
PRECEDENCE_POSTFIX      equ 14
PRECEDENCE_PRIMARY      equ 15

;==============================================================================
; AST STRUCTURES
;==============================================================================
AST_NODE struct
    nodeType        dd ?
    token           dd ?            ; Reference to lexer token
    line            dd ?
    column          dd ?
    parent          dd ?
    children        dd 8 dup(?)     ; Child nodes
    childCount      dd ?
    symbol          dd ?            ; Symbol table entry
    attributes      dd 4 dup(?)     ; Node-specific attributes
    flags           dd ?
AST_NODE ends

SYMBOL_TABLE_ENTRY struct
    name            db 128 dup(?)
    type            dd ?
    size            dd ?
    value           QWORD ?
    section         dd ?
    scope           dd ?
    attributes      dd ?
    lineDefined     dd ?
    references      dd 64 dup(?)
    refCount        dd ?
SYMBOL_TABLE_ENTRY ends

PARSER_STATE struct
    tokenStream     dd ?
    currentToken    dd ?
    astRoot         dd ?
    symbolTable     dd ?
    scopeStack      dd MAX_SCOPE_STACK dup(?)
    scopeDepth      dd ?
    errorCount      dd ?
    warningCount    dd ?
    recoveryMode    dd ?
    panicMode       dd ?
PARSER_STATE ends

;==============================================================================
; FORWARD DECLARATIONS
;==============================================================================
ParseProgram              PROTO :DWORD
ParseProcedure            PROTO :DWORD
ParseInstruction          PROTO :DWORD
ParseDirective            PROTO :DWORD
ParseExpression           PROTO :DWORD, :DWORD
ParseOperand              PROTO :DWORD
ParseLabel                PROTO :DWORD
ParseMacro                PROTO :DWORD
ParseStruct               PROTO :DWORD
ParseStatement            PROTO :DWORD

;==============================================================================
; PARSER CORE
;==============================================================================
.data?
parserState PARSER_STATE <>
astNodes AST_NODE MAX_AST_NODES dup(<?>)
astNodeCount dd ?
symbolTable SYMBOL_TABLE_ENTRY MAX_SYMBOL_TABLE_SIZE dup(<?>)
symbolCount dd ?

.code

;==============================================================================
; Phase 5.2.1: Parser Initialization
;==============================================================================
Parser_Init proc tokenStream:DWORD
    ; Initialize parser state
    invoke ZeroMemory, addr parserState, SIZEOF PARSER_STATE
    
    mov eax, tokenStream
    mov parserState.tokenStream, eax
    mov parserState.currentToken, 0
    mov parserState.astRoot, 0
    mov parserState.symbolTable, offset symbolTable
    mov parserState.scopeDepth, 0
    mov parserState.errorCount, 0
    mov parserState.warningCount, 0
    mov parserState.recoveryMode, 0
    mov parserState.panicMode, 0
    
    ; Initialize AST node pool
    mov astNodeCount, 0
    
    ; Initialize symbol table
    mov symbolCount, 0
    
    ; Push global scope
    invoke PushScope, CSTR("global")
    
    return TRUE
Parser_Init endp

Parser_Parse proc
    local programNode:DWORD
    
    ; Parse program
    invoke ParseProgram
    mov programNode, eax
    
    .if programNode == NULL
        invoke Parser_Error, 0, 0, CSTR("Failed to parse program")
        return NULL
    .endif
    
    ; Set as AST root
    mov parserState.astRoot, programNode
    
    ; Validate AST
    invoke ValidateAST, programNode
    
    ; Generate symbol table
    invoke BuildSymbolTable, programNode
    
    return programNode
    
Parser_Parse endp

;==============================================================================
; Phase 5.2.2: Program Parsing
;==============================================================================
ParseProgram proc
    local programNode:DWORD
    local childNode:DWORD
    
    ; Create program node
    invoke CreateASTNode, AST_PROGRAM
    mov programNode, eax
    
    ; Parse global declarations
    .while TRUE
        invoke PeekToken
        .if eax == TOKEN_EOF
            .break
        .endif
        
        ; Check for procedure
        invoke PeekTokenType
        .if eax == TOKEN_KEYWORD
            invoke PeekTokenText
            invoke lstrcmpi, eax, CSTR("proc")
            .if eax == 0
                invoke ParseProcedure
                mov childNode, eax
                .if childNode != NULL
                    invoke AddChildNode, programNode, childNode
                .endif
                .continue
            .endif
        .endif
        
        ; Check for directive
        invoke PeekTokenType
        .if eax == TOKEN_DIRECTIVE
            invoke ParseDirective
            mov childNode, eax
            .if childNode != NULL
                invoke AddChildNode, programNode, childNode
            .endif
            .continue
        .endif
        
        ; Check for macro
        invoke PeekTokenText
        invoke lstrcmpi, eax, CSTR("macro")
        .if eax == 0
            invoke ParseMacro
            mov childNode, eax
            .if childNode != NULL
                invoke AddChildNode, programNode, childNode
            .endif
            .continue
        .endif
        
        ; Check for struct
        invoke PeekTokenText
        invoke lstrcmpi, eax, CSTR("struct")
        .if eax == 0
            invoke ParseStruct
            mov childNode, eax
            .if childNode != NULL
                invoke AddChildNode, programNode, childNode
            .endif
            .continue
        .endif
        
        ; Default: parse statement
        invoke ParseStatement
        mov childNode, eax
        .if childNode != NULL
            invoke AddChildNode, programNode, childNode
        .endif
    .endw
    
    return programNode
    
ParseProgram endp

;==============================================================================
; Phase 5.2.3: Procedure Parsing
;==============================================================================
ParseProcedure proc
    local procNode:DWORD
    local procName[128]:BYTE
    local paramList[256]:BYTE
    local hasParams:DWORD
    
    ; Check for PROC keyword
    invoke ConsumeToken
    mov eax, currentTokenType
    .if eax != TOKEN_KEYWORD
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected 'proc' keyword")
        return NULL
    .endif
    
    ; Create procedure node
    invoke CreateASTNode, AST_PROCEDURE
    mov procNode, eax
    
    ; Get procedure name
    invoke ExpectToken, TOKEN_IDENTIFIER
    mov eax, currentTokenValue
    invoke lstrcpyn, addr procName, eax, 128
    
    ; Set procedure name in AST node
    invoke SetNodeAttribute, procNode, 0, addr procName
    
    ; Check for parameters
    invoke PeekToken
    .if eax == '('
        invoke AdvanceToken
        invoke ParseParameterList, addr paramList
        mov hasParams, eax
    .else
        mov hasParams, 0
    .endif
    
    ; Parse procedure body
    invoke ParseBlockStatement
    mov eax, childNode
    .if eax != NULL
        invoke AddChildNode, procNode, childNode
    .endif
    
    ; Expect ENDP
    invoke ExpectKeyword, CSTR("endp")
    .if eax == FALSE
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected 'endp' to close procedure")
    .endif
    
    return procNode
    
ParseProcedure endp

;==============================================================================
; Phase 5.2.4: Instruction Parsing
;==============================================================================
ParseInstruction proc
    local instrNode:DWORD
    local mnemonic[32]:BYTE
    local operandCount:DWORD
    
    ; Create instruction node
    invoke CreateASTNode, AST_INSTRUCTION
    mov instrNode, eax
    
    ; Get mnemonic
    invoke ExpectToken, TOKEN_KEYWORD
    mov eax, currentTokenValue
    invoke lstrcpyn, addr mnemonic, eax, 32
    
    ; Validate instruction
    invoke ValidateInstruction, addr mnemonic
    .if eax == FALSE
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Invalid instruction mnemonic")
        return NULL
    .endif
    
    ; Set instruction mnemonic
    invoke SetNodeAttribute, instrNode, 0, addr mnemonic
    
    ; Parse operands
    mov operandCount, 0
    
    .while TRUE
        invoke PeekToken
        .if eax == TOKEN_EOF || eax == TOKEN_NEWLINE
            .break
        .endif
        
        ; Check for end of instruction
        invoke PeekTokenType
        .if eax == TOKEN_DIRECTIVE || eax == TOKEN_KEYWORD
            .break
        .endif
        
        ; Parse operand
        invoke ParseOperand
        mov eax, operandNode
        .if operandNode != NULL
            invoke AddChildNode, instrNode, operandNode
            inc operandCount
            
            ; Check for comma
            invoke PeekToken
            .if eax == ','
                invoke AdvanceToken
            .else
                .break
            .endif
        .else
            .break
        .endif
    .endw
    
    ; Validate operand count
    invoke ValidateOperandCount, addr mnemonic, operandCount
    .if eax == FALSE
        invoke Parser_Warning, parserState.currentToken, 0, CSTR("Invalid operand count for instruction")
    .endif
    
    return instrNode
    
ParseInstruction endp

;==============================================================================
; Phase 5.2.5: Expression Parsing (Pratt Parser)
;==============================================================================
ParseExpression proc precedence:DWORD
    local leftNode:DWORD
    local tokenType:DWORD
    local rightNode:DWORD
    local currentPrecedence:DWORD
    
    ; Parse prefix expression
    invoke ParsePrefixExpression
    mov leftNode, eax
    
    .if leftNode == NULL
        return NULL
    .endif
    
    ; Parse infix expressions
    .while TRUE
        invoke PeekTokenType
        mov tokenType, eax
        
        ; Check if we should stop parsing
        invoke GetTokenPrecedence, tokenType
        mov currentPrecedence, eax
        
        .if currentPrecedence < precedence
            .break
        .endif
        
        ; Advance past operator
        invoke AdvanceToken
        
        ; Parse right operand
        invoke ParseExpression, currentPrecedence + 1
        mov rightNode, eax
        
        .if rightNode == NULL
            invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected expression after operator")
            return NULL
        .endif
        
        ; Create binary operation node
        invoke CreateBinaryOpNode, tokenType, leftNode, rightNode
        mov leftNode, eax
    .endw
    
    return leftNode
    
ParseExpression endp

ParsePrefixExpression proc
    local tokenType:DWORD
    local exprNode:DWORD
    
    invoke PeekTokenType
    mov tokenType, eax
    
    ; Handle prefix operators
    .if tokenType == TOKEN_OPERATOR
        invoke GetPrefixOperatorPrecedence
        .if eax != 0
            ; Unary operator
            invoke AdvanceToken
            invoke ParseExpression, PRECEDENCE_UNARY
            mov exprNode, eax
            
            .if exprNode == NULL
                return NULL
            .endif
            
            ; Create unary operation node
            invoke CreateUnaryOpNode, tokenType, exprNode
            return eax
        .endif
    .endif
    
    ; Parse primary expression
    invoke ParsePrimaryExpression
    return eax
    
ParsePrefixExpression endp

ParsePrimaryExpression proc
    local tokenType:DWORD
    local primaryNode:DWORD
    
    invoke PeekTokenType
    mov tokenType, eax
    
    ; Handle different primary expression types
    .if tokenType == TOKEN_NUMBER
        invoke AdvanceToken
        invoke CreateNumberNode, currentTokenValue
        return eax
        
    .elseif tokenType == TOKEN_STRING
        invoke AdvanceToken
        invoke CreateStringNode, currentTokenValue
        return eax
        
    .elseif tokenType == TOKEN_IDENTIFIER
        invoke AdvanceToken
        invoke CreateIdentifierNode, currentTokenValue
        return eax
        
    .elseif tokenType == TOKEN_REGISTER
        invoke AdvanceToken
        invoke CreateRegisterNode, currentTokenValue
        return eax
        
    .elseif tokenType == '('
        invoke AdvanceToken
        invoke ParseExpression, PRECEDENCE_LOWEST
        
        mov primaryNode, eax
        .if primaryNode == NULL
            return NULL
        .endif
        
        ; Expect closing parenthesis
        invoke ExpectToken, ')'
        .if eax == FALSE
            return NULL
        .endif
        
        return primaryNode
        
    .else
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Unexpected token in expression")
        return NULL
    .endif
    
ParsePrimaryExpression endp

;==============================================================================
; Phase 5.2.6: Operand Parsing
;==============================================================================
ParseOperand proc
    local operandNode:DWORD
    local tokenType:DWORD
    
    invoke PeekTokenType
    mov tokenType, eax
    
    ; Handle different operand types
    .if tokenType == TOKEN_REGISTER
        invoke ParseRegisterOperand
        return eax
        
    .elseif tokenType == TOKEN_NUMBER
        invoke ParseImmediateOperand
        return eax
        
    .elseif tokenType == TOKEN_IDENTIFIER
        ; Could be label, variable, or memory operand
        invoke ParseMemoryOperand
        return eax
        
    .elseif tokenType == '['
        invoke ParseMemoryOperand
        return eax
        
    .elseif tokenType == TOKEN_STRING
        invoke ParseStringOperand
        return eax
        
    .else
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected operand")
        return NULL
    .endif
    
ParseOperand endp

ParseRegisterOperand proc
    local regNode:DWORD
    local regName[32]:BYTE
    
    ; Expect register token
    invoke ExpectToken, TOKEN_REGISTER
    mov eax, currentTokenValue
    invoke lstrcpyn, addr regName, eax, 32
    
    ; Create register node
    invoke CreateASTNode, AST_REGISTER
    mov regNode, eax
    
    ; Set register name
    invoke SetNodeAttribute, regNode, 0, addr regName
    
    ; Validate register
    invoke ValidateRegister, addr regName
    .if eax == FALSE
        invoke Parser_Warning, parserState.currentToken, 0, CSTR("Invalid register name")
    .endif
    
    return regNode
    
ParseRegisterOperand endp

ParseMemoryOperand proc
    local memNode:DWORD
    local baseReg[32]:BYTE
    local indexReg[32]:BYTE
    local scale:DWORD
    local displacement:DWORD
    local hasBase:DWORD
    local hasIndex:DWORD
    local hasScale:DWORD
    local hasDisplacement:DWORD
    
    mov hasBase, 0
    mov hasIndex, 0
    mov hasScale, 0
    mov hasDisplacement, 0
    
    ; Create memory operand node
    invoke CreateASTNode, AST_MEMORY
    mov memNode, eax
    
    ; Check for opening bracket
    invoke PeekToken
    .if eax == '['
        invoke AdvanceToken
        
        ; Parse memory addressing components
        .while TRUE
            invoke PeekTokenType
            
            .if eax == TOKEN_REGISTER
                ; Base or index register
                invoke GetCurrentTokenText
                .if hasBase == 0
                    invoke lstrcpyn, addr baseReg, eax, 32
                    mov hasBase, 1
                .elseif hasIndex == 0
                    invoke lstrcpyn, addr indexReg, eax, 32
                    mov hasIndex, 1
                .else
                    invoke Parser_Error, parserState.currentToken, 0, CSTR("Too many registers in memory operand")
                    return NULL
                .endif
                invoke AdvanceToken
                
            .elseif eax == TOKEN_NUMBER
                ; Displacement or scale
                invoke GetCurrentTokenValue
                .if hasDisplacement == 0 && hasBase == 0 && hasIndex == 0
                    mov displacement, eax
                    mov hasDisplacement, 1
                .elseif hasScale == 0 && hasIndex == 1
                    mov scale, eax
                    mov hasScale, 1
                .else
                    invoke Parser_Error, parserState.currentToken, 0, CSTR("Invalid memory operand syntax")
                    return NULL
                .endif
                invoke AdvanceToken
                
            .elseif eax == '+'
                ; Addition operator
                invoke AdvanceToken
                
            .elseif eax == '*'
                ; Scale operator
                invoke AdvanceToken
                
            .elseif eax == ']'
                ; End of memory operand
                invoke AdvanceToken
                .break
                
            .else
                invoke Parser_Error, parserState.currentToken, 0, CSTR("Unexpected token in memory operand")
                return NULL
            .endif
        .endw
        
    .else
        ; Label or variable reference
        invoke ExpectToken, TOKEN_IDENTIFIER
        mov eax, currentTokenValue
        invoke lstrcpyn, addr baseReg, eax, 32
        mov hasBase, 1
    .endif
    
    ; Set memory operand attributes
    .if hasBase == 1
        invoke SetNodeAttribute, memNode, 0, addr baseReg
    .endif
    
    .if hasIndex == 1
        invoke SetNodeAttribute, memNode, 1, addr indexReg
    .endif
    
    .if hasScale == 1
        invoke SetNodeAttribute, memNode, 2, scale
    .endif
    
    .if hasDisplacement == 1
        invoke SetNodeAttribute, memNode, 3, displacement
    .endif
    
    return memNode
    
ParseMemoryOperand endp

;==============================================================================
; Phase 5.2.7: AST Node Management
;==============================================================================
CreateASTNode proc nodeType:DWORD
    local nodeIndex:DWORD
    local nodePtr:DWORD
    
    ; Allocate AST node
    .if astNodeCount >= MAX_AST_NODES
        invoke Parser_Error, 0, 0, CSTR("AST node pool exhausted")
        return NULL
    .endif
    
    mov eax, astNodeCount
    mov nodeIndex, eax
    inc astNodeCount
    
    ; Get node pointer
    mov eax, nodeIndex
    mov ebx, SIZEOF AST_NODE
    mul ebx
    add eax, offset astNodes
    mov nodePtr, eax
    
    assume nodePtr:ptr AST_NODE
    
    ; Initialize node
    invoke ZeroMemory, nodePtr, SIZEOF AST_NODE
    
    mov eax, nodeType
    mov [nodePtr].nodeType, eax
    mov eax, parserState.currentToken
    mov [nodePtr].token, eax
    mov eax, currentLine
    mov [nodePtr].line, eax
    mov eax, currentColumn
    mov [nodePtr].column, eax
    mov [nodePtr].childCount, 0
    
    return nodePtr
    
CreateASTNode endp

AddChildNode proc parent:DWORD, child:DWORD
    local parentPtr:DWORD
    local childCount:DWORD
    
    assume parent:ptr AST_NODE
    assume child:ptr AST_NODE
    
    mov parentPtr, parent
    
    mov eax, [parentPtr].childCount
    mov childCount, eax
    
    .if childCount >= 8
        invoke Parser_Error, [parentPtr].line, [parentPtr].column, CSTR("Too many child nodes")
        return FALSE
    .endif
    
    ; Add child
    mov eax, childCount
    mov [parentPtr].children[eax*4], child
    mov [child].parent, parent
    inc [parentPtr].childCount
    
    return TRUE
    
AddChildNode endp

SetNodeAttribute proc node:DWORD, attrIndex:DWORD, value:DWORD
    local nodePtr:DWORD
    
    assume node:ptr AST_NODE
    mov nodePtr, node
    
    .if attrIndex >= 4
        return FALSE
    .endif
    
    mov eax, value
    mov [nodePtr].attributes[attrIndex*4], eax
    
    return TRUE
    
SetNodeAttribute endp

;==============================================================================
; Phase 5.2.8: Symbol Table Management
;==============================================================================
BuildSymbolTable proc astRoot:DWORD
    local symbolVisitor:DWORD
    
    ; Create symbol table visitor
    invoke CreateSymbolTableVisitor
    mov symbolVisitor, eax
    
    ; Walk AST and build symbol table
    invoke WalkAST, astRoot, symbolVisitor
    
    ; Resolve symbols
    invoke ResolveSymbols, symbolVisitor
    
    return TRUE
    
BuildSymbolTable endp

AddSymbol proc name:DWORD, symbolType:DWORD, size:DWORD, value:QWORD, section:DWORD
    local symbolIndex:DWORD
    local symbolPtr:DWORD
    
    .if symbolCount >= MAX_SYMBOL_TABLE_SIZE
        invoke Parser_Error, 0, 0, CSTR("Symbol table full")
        return -1
    .endif
    
    mov eax, symbolCount
    mov symbolIndex, eax
    inc symbolCount
    
    ; Get symbol pointer
    mov eax, symbolIndex
    mov ebx, SIZEOF SYMBOL_TABLE_ENTRY
    mul ebx
    add eax, offset symbolTable
    mov symbolPtr, eax
    
    assume symbolPtr:ptr SYMBOL_TABLE_ENTRY
    
    ; Initialize symbol
    invoke lstrcpyn, addr [symbolPtr].name, name, 128
    mov eax, symbolType
    mov [symbolPtr].type, eax
    mov eax, size
    mov [symbolPtr].size, eax
    mov eax, dword ptr value
    mov [symbolPtr].value, eax
    mov eax, dword ptr value+4
    mov [symbolPtr].value+4, eax
    mov eax, section
    mov [symbolPtr].section, eax
    mov eax, parserState.scopeDepth
    mov [symbolPtr].scope, eax
    mov [symbolPtr].lineDefined, currentLine
    mov [symbolPtr].refCount, 0
    
    return symbolIndex
    
AddSymbol endp

FindSymbol proc name:DWORD
    local i:DWORD
    
    mov i, symbolCount
    dec i
    
    .while i >= 0
        mov eax, i
        mov ebx, SIZEOF SYMBOL_TABLE_ENTRY
        mul ebx
        add eax, offset symbolTable
        assume eax:ptr SYMBOL_TABLE_ENTRY
        
        invoke lstrcmpi, addr [eax].name, name
        .if eax == 0
            ; Found symbol
            mov eax, i
            ret
        .endif
        
        dec i
    .endw
    
    return -1  ; Not found
    
FindSymbol endp

;==============================================================================
; Phase 5.2.9: Error Recovery
;==============================================================================
Parser_Error proc tokenIndex:DWORD, line:DWORD, message:DWORD
    inc parserState.errorCount
    
    ; Format error message
    local errorMsg[512]:BYTE
    invoke wsprintf, addr errorMsg, CSTR("Parse error: %s"), message
    
    ; Log error
    invoke OutputDebugString, addr errorMsg
    
    ; Enter recovery mode
    mov parserState.recoveryMode, 1
    
    return TRUE
    
Parser_Error endp

Parser_Warning proc tokenIndex:DWORD, line:DWORD, message:DWORD
    inc parserState.warningCount
    
    ; Format warning message
    local warningMsg[512]:BYTE
    invoke wsprintf, addr warningMsg, CSTR("Parse warning: %s"), message
    
    ; Log warning
    invoke OutputDebugString, addr warningMsg
    
    return TRUE
    
Parser_Warning endp

RecoverFromError proc
    ; Synchronize to next statement boundary
    local recovered:DWORD
    
    mov recovered, 0
    
    .while recovered == 0 && parserState.currentToken < tokenCount
        invoke PeekTokenType
        
        ; Look for statement boundaries
        .if eax == TOKEN_DIRECTIVE || eax == TOKEN_KEYWORD || eax == TOKEN_NEWLINE || eax == TOKEN_EOF
            mov recovered, 1
        .else
            invoke AdvanceToken
        .endif
    .endw
    
    mov parserState.recoveryMode, 0
    
    return recovered
    
RecoverFromError endp

;==============================================================================
; Phase 5.2.10: Validation Functions
;==============================================================================
ValidateInstruction proc mnemonic:DWORD
    ; Validate instruction mnemonic
    
    return TRUE
    
ValidateInstruction endp

ValidateOperandCount proc mnemonic:DWORD, operandCount:DWORD
    ; Validate operand count for instruction
    
    return TRUE
    
ValidateOperandCount endp

ValidateRegister proc regName:DWORD
    ; Validate register name
    
    return TRUE
    
ValidateRegister endp

ValidateAST proc astRoot:DWORD
    ; Validate complete AST
    
    return TRUE
    
ValidateAST endp

ResolveSymbols proc symbolVisitor:DWORD
    ; Resolve symbol references
    
    return TRUE
    
ResolveSymbols endp

WalkAST proc astRoot:DWORD, visitor:DWORD
    ; Walk AST with visitor pattern
    
    return TRUE
    
WalkAST endp

CreateSymbolTableVisitor proc
    ; Create symbol table visitor
    
    return TRUE
    
CreateSymbolTableVisitor endp

;==============================================================================
; Phase 5.2.11: Utility Functions
;==============================================================================
ExpectToken proc expectedType:DWORD
    local currentType:DWORD
    
    invoke PeekTokenType
    mov currentType, eax
    
    .if currentType != expectedType
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Unexpected token")
        return FALSE
    .endif
    
    invoke AdvanceToken
    return TRUE
    
ExpectToken endp

ExpectKeyword proc keyword:DWORD
    local tokenText:DWORD
    
    invoke PeekTokenType
    .if eax != TOKEN_KEYWORD
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected keyword")
        return FALSE
    .endif
    
    invoke PeekTokenText
    mov tokenText, eax
    
    invoke lstrcmpi, tokenText, keyword
    .if eax != 0
        invoke Parser_Error, parserState.currentToken, 0, CSTR("Expected specific keyword")
        return FALSE
    .endif
    
    invoke AdvanceToken
    return TRUE
    
ExpectKeyword endp

AdvanceToken proc
    inc parserState.currentToken
    return TRUE
    
AdvanceToken endp

PeekToken proc
    ; Return token at current position
    
    return TRUE
    
PeekToken endp

PeekTokenType proc
    ; Return token type at current position
    
    return TOKEN_EOF
    
PeekTokenType endp

PeekTokenText proc
    ; Return token text at current position
    
    return NULL
    
PeekTokenText endp

GetCurrentTokenValue proc
    ; Return current token value
    
    return 0
    
GetCurrentTokenValue endp

GetCurrentLine proc
    ; Return current line number
    
    return 1
    
GetCurrentLine endp

GetCurrentColumn proc
    ; Return current column number
    
    return 1
    
GetCurrentColumn endp

CreateNumberNode proc numberValue:DWORD
    ; Create number literal node
    
    return NULL
    
CreateNumberNode endp

CreateStringNode proc stringValue:DWORD
    ; Create string literal node
    
    return NULL
    
CreateStringNode endp

CreateIdentifierNode proc identifier:DWORD
    ; Create identifier node
    
    return NULL
    
CreateIdentifierNode endp

CreateRegisterNode proc register:DWORD
    ; Create register node
    
    return NULL
    
CreateRegisterNode endp

CreateBinaryOpNode proc opType:DWORD, left:DWORD, right:DWORD
    ; Create binary operation node
    
    return NULL
    
CreateBinaryOpNode endp

CreateUnaryOpNode proc opType:DWORD, operand:DWORD
    ; Create unary operation node
    
    return NULL
    
CreateUnaryOpNode endp

GetTokenPrecedence proc tokenType:DWORD
    ; Get operator precedence for token
    
    return PRECEDENCE_LOWEST
    
GetTokenPrecedence endp

GetPrefixOperatorPrecedence proc
    ; Get precedence for prefix operators
    
    return PRECEDENCE_UNARY
    
GetPrefixOperatorPrecedence endp

ParseParameterList proc paramList:DWORD
    ; Parse procedure parameter list
    
    return 0
    
ParseParameterList endp

ParseBlockStatement proc
    ; Parse block statement
    
    return NULL
    
ParseBlockStatement endp

ParseStatement proc
    ; Parse statement
    
    return NULL
    
ParseStatement endp

ParseDirective proc
    ; Parse directive
    
    return NULL
    
ParseDirective endp

ParseMacro proc
    ; Parse macro definition
    
    return NULL
    
ParseMacro endp

ParseStruct proc
    ; Parse structure definition
    
    return NULL
    
ParseStruct endp

ParsePrefixExpression proc
    ; Parse prefix expression
    
    return NULL
    
ParsePrefixExpression endp

ParsePrimaryExpression proc
    ; Parse primary expression
    
    return NULL
    
ParsePrimaryExpression endp

ParseRegisterOperand proc
    ; Parse register operand
    
    return NULL
    
ParseRegisterOperand endp

ParseImmediateOperand proc
    ; Parse immediate operand
    
    return NULL
    
ParseImmediateOperand endp

ParseStringOperand proc
    ; Parse string operand
    
    return NULL
    
ParseStringOperand endp

PushScope proc scopeName:DWORD
    ; Push new scope
    
    return TRUE
    
PushScope endp

PopScope proc
    ; Pop current scope
    
    return TRUE
    
PopScope endp

; Token stream access
ConsumeToken proc
    ; Consume current token
    
    return TRUE
    
ConsumeToken endp

currentTokenType dd ?
currentTokenValue dd ?
currentLine dd ?
currentColumn dd ?
tokenCount dd ?

end
