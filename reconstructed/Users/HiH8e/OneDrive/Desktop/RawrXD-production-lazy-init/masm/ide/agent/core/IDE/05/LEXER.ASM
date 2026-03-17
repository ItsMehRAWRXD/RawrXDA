;==============================================================================
; RawrXD Private Compiler - Phase 5.1: Lexical Analyzer
; Complete MASM lexer with tokenization, macro preprocessing, and error recovery
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

;==============================================================================
; LEXER CONSTANTS
;==============================================================================
MAX_TOKEN_LENGTH        equ 256
MAX_LINE_LENGTH         equ 1024
MAX_INCLUDE_DEPTH       equ 16
MAX_MACRO_NESTING       equ 32
LEXER_BUFFER_SIZE       equ 65536

; Token categories
TOKEN_CATEGORY_KEYWORD      equ 1
TOKEN_CATEGORY_IDENTIFIER   equ 2
TOKEN_CATEGORY_NUMBER       equ 3
TOKEN_CATEGORY_STRING       equ 4
TOKEN_CATEGORY_OPERATOR     equ 5
TOKEN_CATEGORY_DELIMITER    equ 6
TOKEN_CATEGORY_DIRECTIVE    equ 7
TOKEN_CATEGORY_REGISTER     equ 8
TOKEN_CATEGORY_COMMENT      equ 9
TOKEN_CATEGORY_WHITESPACE   equ 10

; Lexer states
LEXER_STATE_NORMAL          equ 0
LEXER_STATE_STRING          equ 1
LEXER_STATE_COMMENT         equ 2
LEXER_STATE_MACRO           equ 3
LEXER_STATE_DIRECTIVE       equ 4

;==============================================================================
; TOKEN STRUCTURES
;==============================================================================
TOKEN struct
    type            dd ?
    category        dd ?
    value           dd ?
    line            dd ?
    column          dd ?
    length          dd ?
    text            db MAX_TOKEN_LENGTH dup(?)
    literalValue    QWORD ?
    flags           dd ?
TOKEN ends

TOKEN_STREAM struct
    tokens          dd ?
    count           dd ?
    capacity        dd ?
    current         dd ?
    filename        dd MAX_PATH dup(?)
    line            dd ?
    column          dd ?
TOKEN_STREAM ends

LEXER_STATE struct
    buffer          dd ?
    bufferSize      dd ?
    position        dd ?
    line            dd ?
    column          dd ?
    filename        dd MAX_PATH dup(?)
    includeStack    dd MAX_INCLUDE_DEPTH dup(?)
    includeDepth    dd ?
    macroStack      dd MAX_MACRO_NESTING dup(?)
    macroDepth      dd ?
    state           dd ?
    lastToken       dd ?
    errorCount      dd ?
    warningCount    dd ?
LEXER_STATE ends

;==============================================================================
; LEXER TABLES
;==============================================================================
.data
; MASM Keywords
masmKeywords \
    db 'mov',0, 'add',0, 'sub',0, 'mul',0, 'imul',0, 'div',0, 'idiv',0
    db 'inc',0, 'dec',0, 'neg',0, 'not',0, 'and',0, 'or',0, 'xor',0
    db 'shl',0, 'shr',0, 'sal',0, 'sar',0, 'rol',0, 'ror',0, 'rcl',0, 'rcr',0
    db 'push',0, 'pop',0, 'pusha',0, 'popa',0, 'pushad',0, 'popad',0
    db 'call',0, 'ret',0, 'retn',0, 'retf',0, 'jmp',0, 'je',0, 'jz',0
    db 'jne',0, 'jnz',0, 'jg',0, 'jnle',0, 'jge',0, 'jnl',0, 'jl',0, 'jnge',0
    db 'jle',0, 'jng',0, 'ja',0, 'jnbe',0, 'jae',0, 'jnb',0, 'jb',0, 'jnae',0
    db 'jbe',0, 'jna',0, 'jo',0, 'jno',0, 'js',0, 'jns',0, 'jc',0, 'jnc',0
    db 'jp',0, 'jpe',0, 'jnp',0, 'jpo',0, 'loop',0, 'loope',0, 'loopne',0
    db 'cmp',0, 'test',0, 'lea',0, 'lds',0, 'les',0, 'lfs',0, 'lgs',0, 'lss',0
    db 'xchg',0, 'cmpxchg',0, 'cmpxchg8b',0, 'xadd',0, 'daa',0, 'das',0
    db 'aaa',0, 'aas',0, 'aam',0, 'aad',0, 'cbw',0, 'cwd',0, 'cwde',0, 'cdq',0
    db 'in',0, 'out',0, 'ins',0, 'outs',0, 'hlt',0, 'nop',0, 'cli',0, 'sti',0
    db 'cld',0, 'std',0, 'clc',0, 'stc',0, 'cmc',0, 'lahf',0, 'sahf',0
    db 'pushf',0, 'popf',0, 'pushfd',0, 'popfd',0, 'leave',0, 'enter',0
    db 'bound',0, 'arpl',0, 'lds',0, 'les',0, 'lfs',0, 'lgs',0, 'lss',0
    db 0

; MASM Directives
masmDirectives \
    db '.model',0, '.data',0, '.code',0, '.stack',0, '.data?',0, '.const',0
    db '.code',0, '.fardata',0, '.fardata?',0, '.sdata',0, '.sdata?',0
    db '.386',0, '.386p',0, '.486',0, '.486p',0, '.586',0, '.586p',0, '.687',0
    db '.mmx',0, '.xmm',0, '.k3d',0, '.xmm',0, '.codeview',0, '.dosseg',0
    db '.alpha',0, '.seq',0, '.if',0, '.else',0, '.elseif',0, '.endif',0
    db '.while',0, '.endw',0, '.repeat',0, '.until',0, '.break',0, '.continue',0
    db '.switch',0, '.endsw',0, '.case',0, '.default',0, '.for',0, '.endf',0
    db 'proc',0, 'endp',0, 'proto',0, 'struct',0, 'ends',0, 'union',0, 'endu',0
    db 'typedef',0, 'equ',0, 'texequ',0, 'macro',0, 'endm',0, 'exitm',0
    db 'goto',0, 'end',0, 'option',0, 'assume',0, 'segment',0, 'ends',0
    db 'ends',0, 'public',0, 'extern',0, 'extern',0, 'proto',0, 'comm',0
    db 'align',0, 'even',0, 'org',0, '.radix',0, 'include',0, 'includelib',0
    db 0

; x86 Registers
x86Registers \
    db 'eax',0, 'ebx',0, 'ecx',0, 'edx',0, 'esi',0, 'edi',0, 'ebp',0, 'esp',0
    db 'ax',0, 'bx',0, 'cx',0, 'dx',0, 'si',0, 'di',0, 'bp',0, 'sp',0
    db 'al',0, 'bl',0, 'cl',0, 'dl',0, 'ah',0, 'bh',0, 'ch',0, 'dh',0
    db 'r8',0, 'r8d',0, 'r8w',0, 'r8b',0
    db 'r9',0, 'r9d',0, 'r9w',0, 'r9b',0
    db 'r10',0, 'r10d',0, 'r10w',0, 'r10b',0
    db 'r11',0, 'r11d',0, 'r11w',0, 'r11b',0
    db 'r12',0, 'r12d',0, 'r12w',0, 'r12b',0
    db 'r13',0, 'r13d',0, 'r13w',0, 'r13b',0
    db 'r14',0, 'r14d',0, 'r14w',0, 'r14b',0
    db 'r15',0, 'r15d',0, 'r15w',0, 'r15b',0
    db 'rax',0, 'rbx',0, 'rcx',0, 'rdx',0, 'rsi',0, 'rdi',0, 'rbp',0, 'rsp',0
    db 'rip',0, 'eip',0, 'ip',0
    db 'cs',0, 'ds',0, 'es',0, 'fs',0, 'gs',0, 'ss',0
    db 'cr0',0, 'cr2',0, 'cr3',0, 'cr4',0, 'cr8',0
    db 'dr0',0, 'dr1',0, 'dr2',0, 'dr3',0, 'dr6',0, 'dr7',0
    db 'st0',0, 'st1',0, 'st2',0, 'st3',0, 'st4',0, 'st5',0, 'st6',0, 'st7',0
    db 'mm0',0, 'mm1',0, 'mm2',0, 'mm3',0, 'mm4',0, 'mm5',0, 'mm6',0, 'mm7',0
    db 'xmm0',0, 'xmm1',0, 'xmm2',0, 'xmm3',0, 'xmm4',0, 'xmm5',0, 'xmm6',0, 'xmm7',0
    db 'xmm8',0, 'xmm9',0, 'xmm10',0, 'xmm11',0, 'xmm12',0, 'xmm13',0, 'xmm14',0, 'xmm15',0
    db 'ymm0',0, 'ymm1',0, 'ymm2',0, 'ymm3',0, 'ymm4',0, 'ymm5',0, 'ymm6',0, 'ymm7',0
    db 'ymm8',0, 'ymm9',0, 'ymm10',0, 'ymm11',0, 'ymm12',0, 'ymm13',0, 'ymm14',0, 'ymm15',0
    db 'zmm0',0, 'zmm1',0, 'zmm2',0, 'zmm3',0, 'zmm4',0, 'zmm5',0, 'zmm6',0, 'zmm7',0
    db 'zmm8',0, 'zmm9',0, 'zmm10',0, 'zmm11',0, 'zmm12',0, 'zmm13',0, 'zmm14',0, 'zmm15',0
    db 0

; Operators and delimiters
operators db '+', '-', '*', '/', '%', '&', '|', '^', '~', '!', '<', '>', '=', 0
delimiters db '(', ')', '[', ']', '{', '}', ',', ';', ':', '@', '#', '$', 0

.data?
currentLexerState LEXER_STATE <>
currentTokenStream TOKEN_STREAM <>
tokenBuffer TOKEN 4096 dup(<?>)

.code

;==============================================================================
; Phase 5.1.1: Main Lexical Analysis Engine
;==============================================================================
Lexer_Init proc sourceFile:DWORD, buffer:DWORD, bufferSize:DWORD
    ; Initialize lexer state
    invoke ZeroMemory, addr currentLexerState, SIZEOF LEXER_STATE
    
    mov eax, sourceFile
    mov currentLexerState.filename, eax
    mov eax, buffer
    mov currentLexerState.buffer, eax
    mov eax, bufferSize
    mov currentLexerState.bufferSize, eax
    
    mov currentLexerState.position, 0
    mov currentLexerState.line, 1
    mov currentLexerState.column, 1
    mov currentLexerState.state, LEXER_STATE_NORMAL
    mov currentLexerState.includeDepth, 0
    mov currentLexerState.macroDepth, 0
    mov currentLexerState.errorCount, 0
    mov currentLexerState.warningCount, 0
    
    ; Initialize token stream
    invoke TokenStream_Init, addr currentTokenStream, addr tokenBuffer, 4096
    
    ; Preload keyword tables
    invoke BuildKeywordHashTables
    
    return TRUE
Lexer_Init endp

Lexer_Tokenize proc
    local token:TOKEN
    local continueTokenizing:DWORD
    
    mov continueTokenizing, 1
    
    .while continueTokenizing == 1
        ; Get next token
        invoke Lexer_NextToken, addr token
        
        .if token.type == TOKEN_EOF
            mov continueTokenizing, 0
        .else
            ; Add token to stream
            invoke TokenStream_AddToken, addr currentTokenStream, addr token
        .endif
    .endw
    
    return currentTokenStream.count
    
Lexer_Tokenize endp

Lexer_NextToken proc token:DWORD
    local startPos:DWORD
    local startLine:DWORD
    local startColumn:DWORD
    local currentChar:BYTE
    local nextState:DWORD
    
    assume token:ptr TOKEN
    
    ; Skip whitespace and comments
    invoke SkipWhitespaceAndComments
    
    .if currentLexerState.position >= currentLexerState.bufferSize
        ; End of file
        mov [token].type, TOKEN_EOF
        mov [token].category, TOKEN_CATEGORY_DELIMITER
        mov [token].line, currentLexerState.line
        mov [token].column, currentLexerState.column
        return TOKEN_EOF
    .endif
    
    ; Save start position
    mov eax, currentLexerState.position
    mov startPos, eax
    mov eax, currentLexerState.line
    mov startLine, eax
    mov eax, currentLexerState.column
    mov startColumn, eax
    
    ; Get current character
    invoke GetCurrentChar
    mov currentChar, al
    
    ; Determine token type based on first character
    mov nextState, LEXER_STATE_NORMAL
    
    .if currentChar == ';'
        mov nextState, LEXER_STATE_COMMENT
        
    .elseif currentChar == '"'
        mov nextState, LEXER_STATE_STRING
        
    .elseif currentChar == "'" 
        mov nextState, LEXER_STATE_STRING
        
    .elseif currentChar >= '0' && currentChar <= '9'
        ; Number
        invoke LexNumber, token
        return [token].type
        
    .elseif (currentChar >= 'A' && currentChar <= 'Z') || (currentChar >= 'a' && currentChar <= 'z') || currentChar == '_' || currentChar == '.' || currentChar == '%'
        ; Identifier or keyword
        invoke LexIdentifier, token
        return [token].type
        
    .elseif currentChar == '%' && currentLexerState.state != LEXER_STATE_MACRO
        ; Macro operator
        mov nextState, LEXER_STATE_MACRO
        
    .elseif currentChar == '.'
        ; Directive
        invoke LexDirective, token
        return [token].type
        
    .else
        ; Operator or delimiter
        invoke LexOperator, token
        return [token].type
    .endif
    
    ; Handle special states
    .if nextState == LEXER_STATE_COMMENT
        invoke LexComment, token
    .elseif nextState == LEXER_STATE_STRING
        invoke LexString, token
    .elseif nextState == LEXER_STATE_MACRO
        invoke LexMacro, token
    .endif
    
    ; Set token position
    mov [token].line, startLine
    mov [token].column, startColumn
    mov eax, currentLexerState.position
    sub eax, startPos
    mov [token].length, eax
    
    return [token].type
    
Lexer_NextToken endp

;==============================================================================
; Phase 5.1.2: Number Lexical Analysis
;==============================================================================
LexNumber proc token:DWORD
    local startPos:DWORD
    local isHex:DWORD
    local isBinary:DWORD
    local isOctal:DWORD
    local isFloat:DWORD
    local value:QWORD
    
    assume token:ptr TOKEN
    
    mov startPos, currentLexerState.position
    mov isHex, 0
    mov isBinary, 0
    mov isOctal, 0
    mov isFloat, 0
    mov value, 0
    
    ; Check for hex prefix
    invoke GetCurrentChar
    .if al == '0'
        invoke PeekNextChar
        .if al == 'x' || al == 'X'
            mov isHex, 1
            invoke AdvanceChar
            invoke AdvanceChar
        .endif
    .endif
    
    .if isHex == 1
        ; Parse hexadecimal number
        invoke ParseHexNumber, addr value
        mov [token].type, TOKEN_NUMBER
        mov [token].category, TOKEN_CATEGORY_NUMBER
        invoke wsprintf, addr [token].text, CSTR("%llX"), value
        mov [token].literalValue, value
        
    .else
        ; Parse decimal or binary number
        invoke GetCurrentChar
        .if al == '0'
            invoke PeekNextChar
            .if al == 'b' || al == 'B'
                mov isBinary, 1
                invoke AdvanceChar
                invoke AdvanceChar
            .elseif al >= '0' && al <= '7'
                mov isOctal, 1
            .endif
        .endif
        
        .if isBinary == 1
            invoke ParseBinaryNumber, addr value
        .elseif isOctal == 1
            invoke ParseOctalNumber, addr value
        .else
            invoke ParseDecimalNumber, addr value
        .endif
        
        ; Check for float suffix
        invoke GetCurrentChar
        .if al == '.' || al == 'e' || al == 'E'
            mov isFloat, 1
            invoke ParseFloatNumber, addr value
        .endif
        
        mov [token].type, TOKEN_NUMBER
        mov [token].category, TOKEN_CATEGORY_NUMBER
        
        .if isFloat == 1
            invoke wsprintf, addr [token].text, CSTR("%f"), value
        .else
            invoke wsprintf, addr [token].text, CSTR("%llu"), value
        .endif
        
        mov [token].literalValue, value
    .endif
    
    return TOKEN_NUMBER
    
LexNumber endp

ParseHexNumber proc value:DWORD
    local digitValue:DWORD
    local result:QWORD
    
    mov result, 0
    
    .while TRUE
        invoke GetCurrentChar
        .if al >= '0' && al <= '9'
            sub al, '0'
            mov digitValue, eax
        .elseif al >= 'A' && al <= 'F'
            sub al, 'A'
            add al, 10
            mov digitValue, eax
        .elseif al >= 'a' && al <= 'f'
            sub al, 'a'
            add al, 10
            mov digitValue, eax
        .elseif al == 'h' || al == 'H'
            invoke AdvanceChar
            .break
        .else
            .break
        .endif
        
        shl result, 4
        add result, digitValue
        invoke AdvanceChar
    .endw
    
    mov eax, value
    mov [eax], result
    
    return TRUE
    
ParseHexNumber endp

;==============================================================================
; Phase 5.1.3: Identifier and Keyword Analysis
;==============================================================================
LexIdentifier proc token:DWORD
    local startPos:DWORD
    local identifierLength:DWORD
    local isKeyword:DWORD
    local keywordType:DWORD
    
    assume token:ptr TOKEN
    
    mov startPos, currentLexerState.position
    
    ; Collect identifier characters
    .while TRUE
        invoke GetCurrentChar
        .if !(al >= 'A' && al <= 'Z' || al >= 'a' && al <= 'z' || al >= '0' && al <= '9' || al == '_' || al == '@' || al == '$' || al == '%' || al == '?')
            .break
        .endif
        invoke AdvanceChar
    .endw
    
    ; Calculate identifier length
    mov eax, currentLexerState.position
    sub eax, startPos
    mov identifierLength, eax
    
    ; Copy identifier text
    invoke CopyTokenText, startPos, identifierLength, addr [token].text, MAX_TOKEN_LENGTH
    
    ; Check if it's a register
    invoke IsRegister, addr [token].text, identifierLength
    .if eax == TRUE
        mov [token].type, TOKEN_REGISTER
        mov [token].category, TOKEN_CATEGORY_REGISTER
        return TOKEN_REGISTER
    .endif
    
    ; Check if it's a keyword
    invoke IsKeyword, addr [token].text, identifierLength
    mov isKeyword, eax
    
    .if isKeyword == TRUE
        mov [token].type, TOKEN_KEYWORD
        mov [token].category, TOKEN_CATEGORY_KEYWORD
        return TOKEN_KEYWORD
    .else
        mov [token].type, TOKEN_IDENTIFIER
        mov [token].category, TOKEN_CATEGORY_IDENTIFIER
        return TOKEN_IDENTIFIER
    .endif
    
LexIdentifier endp

IsRegister proc text:DWORD, length:DWORD
    local regPtr:DWORD
    
    lea esi, x86Registers
    mov regPtr, esi
    
    .while byte ptr [esi] != 0
        invoke CompareKeyword, text, 0, length, esi
        .if eax == TRUE
            return TRUE
        .endif
        
        ; Move to next register
        .while byte ptr [esi] != 0
            inc esi
        .endw
        inc esi
    .endw
    
    return FALSE
    
IsRegister endp

IsKeyword proc text:DWORD, length:DWORD
    local keywordPtr:DWORD
    
    ; Check instruction keywords
    lea esi, masmKeywords
    mov keywordPtr, esi
    
    .while byte ptr [esi] != 0
        invoke CompareKeyword, text, 0, length, esi
        .if eax == TRUE
            return TRUE
        .endif
        
        ; Move to next keyword
        .while byte ptr [esi] != 0
            inc esi
        .endw
        inc esi
    .endw
    
    ; Check directives
    lea esi, masmDirectives
    .while byte ptr [esi] != 0
        invoke CompareKeyword, text, 0, length, esi
        .if eax == TRUE
            return TRUE
        .endif
        
        .while byte ptr [esi] != 0
            inc esi
        .endw
        inc esi
    .endw
    
    return FALSE
    
IsKeyword endp

;==============================================================================
; Phase 5.1.4: String Literal Analysis
;==============================================================================
LexString proc token:DWORD
    local quoteChar:BYTE
    local startPos:DWORD
    local stringLength:DWORD
    local escapeSequence:DWORD
    
    assume token:ptr TOKEN
    
    ; Get opening quote
    invoke GetCurrentChar
    mov quoteChar, al
    mov [token].text, al
    
    mov startPos, currentLexerState.position
    invoke AdvanceChar  ; Skip opening quote
    
    mov stringLength, 1
    mov escapeSequence, 0
    
    .while TRUE
        invoke GetCurrentChar
        
        .if al == 0 || al == 10 || al == 13
            ; Unterminated string
            invoke Lexer_Error, currentLexerState.line, currentLexerState.column, CSTR("Unterminated string literal")
            .break
            
        .elseif al == quoteChar && escapeSequence == 0
            ; Closing quote
            invoke AdvanceChar
            mov eax, stringLength
            inc eax
            mov byte ptr [token].text[eax], al  ; Add closing quote
            .break
            
        .elseif al == '\' && escapeSequence == 0
            ; Escape sequence
            mov escapeSequence, 1
            invoke AdvanceChar
            
        .else
            ; Regular character
            .if escapeSequence == 1
                ; Handle escape sequence
                invoke HandleEscapeSequence, al, addr stringLength
                mov escapeSequence, 0
            .else
                ; Add character to string
                mov eax, stringLength
                mov byte ptr [token].text[eax], al
                inc stringLength
            .endif
            
            invoke AdvanceChar
        .endif
        
        .if stringLength >= MAX_TOKEN_LENGTH - 1
            invoke Lexer_Error, currentLexerState.line, currentLexerState.column, CSTR("String literal too long")
            .break
        .endif
    .endw
    
    mov [token].type, TOKEN_STRING
    mov [token].category, TOKEN_CATEGORY_STRING
    mov [token].length, stringLength
    
    return TOKEN_STRING
    
LexString endp

HandleEscapeSequence proc escapeChar:BYTE, stringLength:DWORD
    local escapedChar:BYTE
    
    ; Handle escape sequences
    mov al, escapeChar
    
    .if al == 'n'
        mov escapedChar, 10  ; \n
    .elseif al == 't'
        mov escapedChar, 9   ; \t
    .elseif al == 'r'
        mov escapedChar, 13  ; \r
    .elseif al == '\'
        mov escapedChar, '\' ; \\
    .elseif al == '"'
        mov escapedChar, '"' ; \"
    .elseif al == '''
        mov escapedChar, ''' ; \'
    .elseif al == '0'
        mov escapedChar, 0   ; \0
    .elseif al == 'x'
        ; Hex escape sequence
        invoke AdvanceChar
        invoke GetCurrentChar
        invoke ParseHexEscape, al
        mov escapedChar, al
    .else
        ; Unknown escape sequence
        mov escapedChar, al
    .endif
    
    ; Add escaped character
    mov eax, stringLength
    mov ecx, [eax]
    mov byte ptr currentLexerState.buffer[ecx], escapedChar
    inc dword ptr [eax]
    
    ret
HandleEscapeSequence endp

;==============================================================================
; Phase 5.1.5: Comment Analysis
;==============================================================================
LexComment proc token:DWORD
    local startPos:DWORD
    local commentType:DWORD
    
    assume token:ptr TOKEN
    
    mov startPos, currentLexerState.position
    mov commentType, 0  ; 0=single line, 1=multi-line
    
    ; Check comment type
    invoke GetCurrentChar
    .if al == ';'
        ; Single-line comment
        mov commentType, 0
        invoke AdvanceChar  ; Skip ;
        
        .while TRUE
            invoke GetCurrentChar
            .if al == 0 || al == 10 || al == 13
                .break
            .endif
            invoke AdvanceChar
        .endw
        
    .elseif al == '/'
        ; Check for // or /* */
        invoke PeekNextChar
        .if al == '/'
            ; // comment
            mov commentType, 0
            invoke AdvanceChar  ; Skip first /
            invoke AdvanceChar  ; Skip second /
            
            .while TRUE
                invoke GetCurrentChar
                .if al == 0 || al == 10 || al == 13
                    .break
                .endif
                invoke AdvanceChar
            .endw
            
        .elseif al == '*'
            ; /* */ comment
            mov commentType, 1
            invoke AdvanceChar  ; Skip /
            invoke AdvanceChar  ; Skip *
            
            .while TRUE
                invoke GetCurrentChar
                .if al == 0
                    invoke Lexer_Error, currentLexerState.line, currentLexerState.column, CSTR("Unterminated multi-line comment")
                    .break
                .endif
                
                .if al == '*'
                    invoke PeekNextChar
                    .if al == '/'
                        invoke AdvanceChar  ; Skip *
                        invoke AdvanceChar  ; Skip /
                        .break
                    .endif
                .endif
                
                invoke AdvanceChar
            .endw
        .endif
    .endif
    
    ; Copy comment text
    mov eax, currentLexerState.position
    sub eax, startPos
    invoke CopyTokenText, startPos, eax, addr [token].text, MAX_TOKEN_LENGTH
    
    mov [token].type, TOKEN_COMMENT
    mov [token].category, TOKEN_CATEGORY_COMMENT
    mov [token].length, eax
    
    return TOKEN_COMMENT
    
LexComment endp

;==============================================================================
; Phase 5.1.6: Directive Analysis
;==============================================================================
LexDirective proc token:DWORD
    local startPos:DWORD
    local directiveLength:DWORD
    
    assume token:ptr TOKEN
    
    mov startPos, currentLexerState.position
    
    ; Skip the '.' character
    invoke AdvanceChar
    
    ; Collect directive name
    .while TRUE
        invoke GetCurrentChar
        .if !(al >= 'A' && al <= 'Z' || al >= 'a' && al <= 'z' || al >= '0' && al <= '9' || al == '_')
            .break
        .endif
        invoke AdvanceChar
    .endw
    
    ; Calculate directive length
    mov eax, currentLexerState.position
    sub eax, startPos
    mov directiveLength, eax
    
    ; Copy directive text
    invoke CopyTokenText, startPos, directiveLength, addr [token].text, MAX_TOKEN_LENGTH
    
    ; Validate directive
    invoke IsValidDirective, addr [token].text, directiveLength
    .if eax == TRUE
        mov [token].type, TOKEN_DIRECTIVE
        mov [token].category, TOKEN_CATEGORY_DIRECTIVE
    .else
        mov [token].type, TOKEN_IDENTIFIER
        mov [token].category, TOKEN_CATEGORY_IDENTIFIER
        invoke Lexer_Warning, currentLexerState.line, currentLexerState.column, CSTR("Unknown directive")
    .endif
    
    mov [token].length, directiveLength
    
    return [token].type
    
LexDirective endp

;==============================================================================
; Phase 5.1.7: Operator Analysis
;==============================================================================
LexOperator proc token:DWORD
    local startPos:DWORD
    local operatorLength:DWORD
    local currentChar:BYTE
    local nextChar:BYTE
    
    assume token:ptr TOKEN
    
    mov startPos, currentLexerState.position
    
    ; Get operator character
    invoke GetCurrentChar
    mov currentChar, al
    
    ; Check for multi-character operators
    invoke PeekNextChar
    mov nextChar, al
    
    mov operatorLength, 1
    
    ; Two-character operators
    .if currentChar == '<' && nextChar == '<'
        mov operatorLength, 2
    .elseif currentChar == '>' && nextChar == '>'
        mov operatorLength, 2
    .elseif currentChar == '<' && nextChar == '='
        mov operatorLength, 2
    .elseif currentChar == '>' && nextChar == '='
        mov operatorLength, 2
    .elseif currentChar == '=' && nextChar == '='
        mov operatorLength, 2
    .elseif currentChar == '!' && nextChar == '='
        mov operatorLength, 2
    .elseif currentChar == '&' && nextChar == '&'
        mov operatorLength, 2
    .elseif currentChar == '|' && nextChar == '|'
        mov operatorLength, 2
    .endif
    
    ; Copy operator text
    invoke CopyTokenText, startPos, operatorLength, addr [token].text, MAX_TOKEN_LENGTH
    
    ; Advance position
    mov ecx, operatorLength
    .while ecx > 0
        invoke AdvanceChar
        dec ecx
    .endw
    
    mov [token].type, TOKEN_OPERATOR
    mov [token].category, TOKEN_CATEGORY_OPERATOR
    mov [token].length, operatorLength
    
    return TOKEN_OPERATOR
    
LexOperator endp

;==============================================================================
; Phase 5.1.8: Macro Preprocessing
;==============================================================================
LexMacro proc token:DWORD
    local macroName[64]:BYTE
    local macroParams[256]:BYTE
    local paramCount:DWORD
    
    assume token:ptr TOKEN
    
    ; Skip % character
    invoke AdvanceChar
    
    ; Get macro name (delegated to macro preprocessor)
    invoke Macro_GetIdentifier, addr macroName, 64
    
    ; Check for macro parameters
    invoke GetCurrentChar
    .if al == '('
        invoke AdvanceChar  ; Skip (
        invoke Macro_GetMacroParameters, addr macroParams, addr paramCount
    .else
        mov paramCount, 0
    .endif
    
    ; Expand macro
    invoke ExpandMacro, addr macroName, addr macroParams, paramCount
    
    ; Return expanded tokens
    mov [token].type, TOKEN_MACRO_EXPAND
    mov [token].category, TOKEN_CATEGORY_MACRO
    
    return TOKEN_MACRO_EXPAND
    
LexMacro endp

ExpandMacro proc macroName:DWORD, macroParams:DWORD, paramCount:DWORD
    local macroDef:DWORD
    local expandedText[1024]:BYTE
    
    ; Find macro definition (implemented in macro preprocessor module)
    invoke Macro_FindMacroDefinition, macroName
    mov macroDef, eax

    .if macroDef == NULL
        invoke Lexer_Error, currentLexerState.line, currentLexerState.column, CSTR("Undefined macro")
        return FALSE
    .endif

    ; Substitute parameters (delegate to macro module)
    invoke Macro_SubstituteMacroParams, macroDef, macroParams, paramCount, addr expandedText

    ; Push expanded text back into input stream
    invoke Macro_PushMacroExpansion, addr expandedText
    
    return TRUE
    
ExpandMacro endp

;==============================================================================
; Phase 5.1.9: Utility Functions
;==============================================================================
GetCurrentChar proc
    mov eax, currentLexerState.position
    .if eax >= currentLexerState.bufferSize
        return 0
    .endif
    
    mov eax, currentLexerState.buffer
    add eax, currentLexerState.position
    mov al, [eax]
    return al
    
GetCurrentChar endp

PeekNextChar proc
    mov eax, currentLexerState.position
    inc eax
    .if eax >= currentLexerState.bufferSize
        return 0
    .endif
    
    mov eax, currentLexerState.buffer
    add eax, currentLexerState.position
    inc eax
    mov al, [eax]
    return al
    
PeekNextChar endp

AdvanceChar proc
    mov eax, currentLexerState.position
    inc eax
    mov currentLexerState.position, eax
    
    ; Update line and column
    invoke GetCurrentChar
    .if al == 10
        inc currentLexerState.line
        mov currentLexerState.column, 1
    .elseif al == 13
        ; Handle CRLF
        invoke PeekNextChar
        .if al == 10
            invoke AdvanceChar  ; Skip LF
        .endif
        inc currentLexerState.line
        mov currentLexerState.column, 1
    .else
        inc currentLexerState.column
    .endif
    
    return TRUE
    
AdvanceChar endp

SkipWhitespaceAndComments proc
    local continueSkipping:DWORD
    
    mov continueSkipping, 1
    
    .while continueSkipping == 1
        invoke GetCurrentChar
        
        .if al == 0
            mov continueSkipping, 0
            
        .elseif al == ' ' || al == 9 || al == 13
            ; Whitespace
            invoke AdvanceChar
            
        .elseif al == 10
            ; Newline
            invoke AdvanceChar
            
        .elseif al == ';'
            ; Single-line comment
            invoke SkipSingleLineComment
            invoke AdvanceChar  ; Skip newline
            
        .elseif al == '/'
            ; Check for // or /* */ comments
            invoke PeekNextChar
            .if al == '/'
                invoke SkipSingleLineComment
            .elseif al == '*'
                invoke SkipMultiLineComment
            .else
                mov continueSkipping, 0
            .endif
            
        .else
            mov continueSkipping, 0
        .endif
    .endw
    
    return TRUE
    
SkipWhitespaceAndComments endp

CopyTokenText proc startPos:DWORD, length:DWORD, dest:DWORD, maxLen:DWORD
    local copyLength:DWORD
    
    mov eax, length
    .if eax >= maxLen
        mov eax, maxLen
        dec eax
    .endif
    mov copyLength, eax
    
    mov esi, currentLexerState.buffer
    add esi, startPos
    mov edi, dest
    
    mov ecx, copyLength
    rep movsb
    
    mov byte ptr [edi], 0
    
    return copyLength
    
CopyTokenText endp

Lexer_Error proc line:DWORD, column:DWORD, message:DWORD
    inc currentLexerState.errorCount
    
    ; Format error message
    local errorMsg[512]:BYTE
    invoke wsprintf, addr errorMsg, CSTR("Error at %s:%d:%d - %s"), currentLexerState.filename, line, column, message
    
    ; Log error
    invoke OutputDebugString, addr errorMsg
    
    return TRUE
    
Lexer_Error endp

Lexer_Warning proc line:DWORD, column:DWORD, message:DWORD
    inc currentLexerState.warningCount
    
    ; Format warning message
    local warningMsg[512]:BYTE
    invoke wsprintf, addr warningMsg, CSTR("Warning at %s:%d:%d - %s"), currentLexerState.filename, line, column, message
    
    ; Log warning
    invoke OutputDebugString, addr warningMsg
    
    return TRUE
    
Lexer_Warning endp

TokenStream_Init proc stream:DWORD, buffer:DWORD, capacity:DWORD
    assume stream:ptr TOKEN_STREAM
    
    mov eax, buffer
    mov [stream].tokens, eax
    mov [stream].capacity, capacity
    mov [stream].count, 0
    mov [stream].current, 0
    
    return TRUE
    
TokenStream_Init endp

TokenStream_AddToken proc stream:DWORD, token:DWORD
    local streamPtr:DWORD
    local tokenIndex:DWORD
    
    assume stream:ptr TOKEN_STREAM
    assume token:ptr TOKEN
    
    mov streamPtr, stream
    
    .if [streamPtr].count >= [streamPtr].capacity
        ; Token stream full
        return FALSE
    .endif
    
    mov eax, [streamPtr].count
    mov tokenIndex, eax
    mov ebx, SIZEOF TOKEN
    mul ebx
    add eax, [streamPtr].tokens
    
    ; Copy token
    invoke RtlMoveMemory, eax, token, SIZEOF TOKEN
    
    inc [streamPtr].count
    
    return TRUE
    
TokenStream_AddToken endp

BuildKeywordHashTables proc
    ; Build optimized keyword lookup tables
    ; This would create hash tables for O(1) keyword lookup
    
    return TRUE
    
BuildKeywordHashTables endp

IsValidDirective proc text:DWORD, length:DWORD
    ; Check if text is a valid MASM directive
    
    return TRUE
    
IsValidDirective endp

SkipSingleLineComment proc
    ; Skip to end of line
    
    .while TRUE
        invoke GetCurrentChar
        .if al == 0 || al == 10 || al == 13
            .break
        .endif
        invoke AdvanceChar
    .endw
    
    return TRUE
    
SkipSingleLineComment endp

SkipMultiLineComment proc
    ; Skip to end of multi-line comment
    
    .while TRUE
        invoke GetCurrentChar
        .if al == 0
            invoke Lexer_Error, currentLexerState.line, currentLexerState.column, CSTR("Unterminated multi-line comment")
            .break
        .endif
        
        .if al == '*'
            invoke PeekNextChar
            .if al == '/'
                invoke AdvanceChar  ; Skip *
                invoke AdvanceChar  ; Skip /
                .break
            .endif
        .endif
        
        invoke AdvanceChar
    .endw
    
    return TRUE
    
SkipMultiLineComment endp

; Macro preprocessor interface (implemented in IDE_08_MACRO.ASM)
Macro_GetIdentifier PROTO :DWORD, :DWORD
Macro_GetMacroParameters PROTO :DWORD, :DWORD
Macro_FindMacroDefinition PROTO :DWORD
Macro_SubstituteMacroParams PROTO :DWORD, :DWORD, :DWORD, :DWORD
Macro_PushMacroExpansion PROTO :DWORD

ParseBinaryNumber proc value:DWORD
    ; Parse binary number
    
    return TRUE
    
ParseBinaryNumber endp

ParseOctalNumber proc value:DWORD
    ; Parse octal number
    
    return TRUE
    
ParseOctalNumber endp

ParseDecimalNumber proc value:DWORD
    ; Parse decimal number
    
    return TRUE
    
ParseDecimalNumber endp

ParseFloatNumber proc value:DWORD
    ; Parse floating point number
    
    return TRUE
    
ParseFloatNumber endp

ParseHexEscape proc hexChar:BYTE
    ; Parse hex escape sequence
    
    return TRUE
    
ParseHexEscape endp

GetIdentifier proc buffer:DWORD, maxLen:DWORD
    ; Get identifier from current position
    
    return TRUE
    
GetIdentifier endp

; Token types
TOKEN_EOF           equ -1
TOKEN_KEYWORD       equ 1
TOKEN_IDENTIFIER    equ 2
TOKEN_NUMBER        equ 3
TOKEN_STRING        equ 4
TOKEN_OPERATOR      equ 5
TOKEN_DIRECTIVE     equ 6
TOKEN_REGISTER      equ 7
TOKEN_COMMENT       equ 8
TOKEN_MACRO_EXPAND  equ 9
TOKEN_WHITESPACE    equ 10

;==============================================================================
; DATA SEGMENTS
;==============================================================================
.data?
currentLexer LEXER_STATE <>
tokenStream TOKEN_STREAM <>
masmDirectives db 2048 dup(?)

.code
