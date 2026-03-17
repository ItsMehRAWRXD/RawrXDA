;==============================================================================
; File 23: text_tokenizer.asm - Incremental Syntax Tokenization Engine
;==============================================================================
; Block-based tokenization: 512-line blocks with hash-based invalidation
; Supports incremental re-lex only on dirty blocks
;==============================================================================

include windows.inc

.code

;==============================================================================
; Initialize Tokenizer
;==============================================================================
Tokenizer_Init PROC
    LOCAL hHeap:HANDLE
    
    invoke HeapCreate, 0, 2097152, 268435456  ; 2MB initial, 256MB max
    mov [tokHeap], rax
    .if rax == NULL
        LOG_ERROR "Failed to create Tokenizer heap"
        ret
    .endif
    
    ; Allocate token block cache (1000 blocks × 512 lines each)
    invoke HeapAlloc, [tokHeap], 0, 1000 * SIZEOF TokenBlock
    mov [blockCache], rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate block cache"
        ret
    .endif
    
    mov [blockCount], 0
    mov [currentLanguage], 0  ; Default: C++ lexer
    
    LOG_INFO "Tokenizer initialized"
    ret
Tokenizer_Init ENDP

;==============================================================================
; Register Language Lexer
;==============================================================================
Tokenizer_RegisterLanguage PROC langId:DWORD, lexFunction:QWORD
    .if langId >= 10
        LOG_ERROR "Language ID out of range: %d", langId
        ret
    .endif
    
    mov rax, [lexerTable]
    mov [rax + langId * 8], lexFunction
    
    LOG_INFO "Registered lexer for language %d", langId
    ret
Tokenizer_RegisterLanguage ENDP

;==============================================================================
; Tokenize Buffer Range (Main Entry Point)
;==============================================================================
Tokenizer_TokenizeRange PROC gbState:QWORD, startLine:DWORD, lineCount:DWORD
    LOCAL blockNum:DWORD
    LOCAL blockStartLine:DWORD
    LOCAL blockEndLine:DWORD
    LOCAL i:DWORD
    LOCAL block:QWORD
    LOCAL isDirty:BYTE
    
    ; Determine which blocks to re-lex
    mov eax, startLine
    xor edx, edx
    mov ecx, 512
    div ecx
    mov blockNum, eax
    
    ; Tokenize affected blocks
    mov i, blockNum
.loop1:
    mov eax, i
    mov ecx, 512
    mul ecx
    mov blockStartLine, eax
    
    mov eax, blockStartLine
    add eax, 512
    mov blockEndLine, eax
    
    .if blockStartLine >= startLine + lineCount
        .break
    .endif
    
    ; Check if block is dirty
    mov rax, [blockCache]
    mov block, QWORD PTR [rax + i * SIZEOF TokenBlock]
    
    .if BYTE PTR [block].TokenBlock.isDirty
        ; Re-lex this block
        call Tokenizer_LexBlock, gbState, blockStartLine, 512
        mov BYTE PTR [block].TokenBlock.isDirty, 0
    .endif
    
    inc i
    .continue .loop1
.endloop1
    
    ret
Tokenizer_TokenizeRange ENDP

;==============================================================================
; Lex Single Block (512 Lines)
;==============================================================================
Tokenizer_LexBlock PROC gbState:QWORD, blockStartLine:DWORD, lineCount:DWORD
    LOCAL lineNum:DWORD
    LOCAL lineText:QWORD
    LOCAL tokenArray:QWORD
    LOCAL tokenCount:DWORD
    LOCAL blockNum:DWORD
    LOCAL newHash:DWORD
    LOCAL oldHash:DWORD
    LOCAL block:QWORD
    
    ; Allocate token array (estimate 200 tokens per 512-line block)
    invoke HeapAlloc, [tokHeap], 0, 100000 * SIZEOF Token
    mov tokenArray, rax
    
    mov tokenCount, 0
    mov lineNum, blockStartLine
    
.loop1:
    .if lineNum >= blockStartLine + lineCount
        .break
    .endif
    
    ; Get line from buffer
    call GapBuffer_GetLine, gbState, lineNum, ADDR lineBuffer, 65536
    
    ; Lex this line
    call Tokenizer_LexLine, ADDR lineBuffer, rax, 
        ADDR tokenArray, 100000, lineNum
    
    add tokenCount, eax
    inc lineNum
    .continue .loop1
.endloop1
    
    ; Compute content hash
    call Tokenizer_ComputeBlockHash, blockStartLine, lineCount
    mov newHash, eax
    
    ; Calculate block number
    mov eax, blockStartLine
    xor edx, edx
    mov ecx, 512
    div ecx
    mov blockNum, eax
    
    ; Store block in cache
    mov rax, [blockCache]
    mov block, [rax + blockNum * SIZEOF TokenBlock]
    mov [block].TokenBlock.tokens, tokenArray
    mov [block].TokenBlock.tokenCount, tokenCount
    mov [block].TokenBlock.contentHash, newHash
    mov [block].TokenBlock.isDirty, 0
    
    LOG_DEBUG "Lexed block %d: %d tokens", blockNum, tokenCount
    
    ret
Tokenizer_LexBlock ENDP

;==============================================================================
; Lex Single Line (Internal)
;==============================================================================
Tokenizer_LexLine PROC lpLine:QWORD, lineLen:DWORD, 
                       lpTokenArray:QWORD, maxTokens:DWORD, lineNum:DWORD
    LOCAL i:DWORD
    LOCAL j:DWORD
    LOCAL ch:BYTE
    LOCAL tokenStart:DWORD
    LOCAL tokenLen:DWORD
    LOCAL tokenType:DWORD
    LOCAL tokenCount:DWORD
    LOCAL inString:BYTE
    LOCAL inComment:BYTE
    LOCAL ptr:QWORD
    
    mov tokenCount, 0
    mov i, 0
    mov ptr, lpLine
    
.loop1:
    .if i >= lineLen
        .break
    .endif
    
    mov al, BYTE PTR [ptr + i]
    mov ch, al
    
    ; Skip whitespace
    .if ch == 32 || ch == 9  ; space or tab
        inc i
        .continue .loop1
    .endif
    
    ; Comment detection (//)
    .if ch == 47 && i + 1 < lineLen
        mov al, BYTE PTR [ptr + i + 1]
        .if al == 47
            ; Rest of line is comment
            mov tokenStart, i
            mov tokenType, 3  ; Comment token type
            call Tokenizer_AddToken, lpTokenArray, maxTokens, 
                tokenStart, lineLen - i, tokenType, lineNum
            mov i, lineLen
            inc tokenCount
            .break
        .endif
    .endif
    
    ; String detection (")
    .if ch == 34
        mov tokenStart, i
        mov tokenType, 2  ; String token type
        mov inString, 1
        inc i
    .loop2:
        .if i >= lineLen || !inString
            .break
        .endif
        mov al, BYTE PTR [ptr + i]
        .if al == 34 && (i == 0 || BYTE PTR [ptr + i - 1] != 92)  ; Not escaped
            mov inString, 0
            inc i
            .break
        .endif
        inc i
        .continue .loop2
    .endloop2
        
        call Tokenizer_AddToken, lpTokenArray, maxTokens,
            tokenStart, i - tokenStart, tokenType, lineNum
        inc tokenCount
        .continue .loop1
    .endif
    
    ; Identifier/Keyword detection (a-z, A-Z, _)
    .if (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122) || ch == 95
        mov tokenStart, i
        inc i
    .loop3:
        .if i >= lineLen
            .break
        .endif
        mov al, BYTE PTR [ptr + i]
        .if !((al >= 65 && al <= 90) || (al >= 97 && al <= 122) || 
               (al >= 48 && al <= 57) || al == 95)
            .break
        .endif
        inc i
        .continue .loop3
    .endloop3
        
        ; Check if keyword
        call Tokenizer_IsKeyword, lpLine, tokenStart, i - tokenStart
        mov tokenType, eax  ; Returns 1 for keyword, 4 for identifier
        
        call Tokenizer_AddToken, lpTokenArray, maxTokens,
            tokenStart, i - tokenStart, tokenType, lineNum
        inc tokenCount
        .continue .loop1
    .endif
    
    ; Number detection (0-9)
    .if ch >= 48 && ch <= 57
        mov tokenStart, i
        mov tokenType, 5  ; Number token type
        inc i
    .loop4:
        .if i >= lineLen
            .break
        .endif
        mov al, BYTE PTR [ptr + i]
        .if (al < 48 || al > 57) && al != 46  ; Not digit or decimal point
            .break
        .endif
        inc i
        .continue .loop4
    .endloop4
        
        call Tokenizer_AddToken, lpTokenArray, maxTokens,
            tokenStart, i - tokenStart, tokenType, lineNum
        inc tokenCount
        .continue .loop1
    .endif
    
    ; Operator/Punctuation
    mov tokenStart, i
    mov tokenType, 6  ; Operator token type
    inc i
    
    call Tokenizer_AddToken, lpTokenArray, maxTokens,
        tokenStart, 1, tokenType, lineNum
    inc tokenCount
    
    .continue .loop1
.endloop1
    
    mov eax, tokenCount
    ret
Tokenizer_LexLine ENDP

;==============================================================================
; Add Token to Array
;==============================================================================
Tokenizer_AddToken PROC lpTokenArray:QWORD, maxTokens:DWORD, 
                       startOffset:DWORD, length:DWORD, 
                       tokenType:DWORD, lineNum:DWORD
    .if DWORD PTR [lpTokenArray - 4] >= maxTokens
        ret
    .endif
    
    mov rax, [lpTokenArray]
    mov ecx, [lpTokenArray - 4]
    mov [rax + rcx * SIZEOF Token].Token.startOffset, startOffset
    mov [rax + rcx * SIZEOF Token].Token.length, length
    mov [rax + rcx * SIZEOF Token].Token.typeId, tokenType
    mov [rax + rcx * SIZEOF Token].Token.styleId, tokenType
    
    inc DWORD PTR [lpTokenArray - 4]
    
    ret
Tokenizer_AddToken ENDP

;==============================================================================
; Check if Token is C++ Keyword
;==============================================================================
Tokenizer_IsKeyword PROC lpText:QWORD, startOffset:DWORD, length:DWORD
    LOCAL i:DWORD
    LOCAL match:BYTE
    
    ; C++ keywords (common set)
    ; if, else, while, for, do, return, class, struct, const, etc.
    
    .if length == 2
        mov al, BYTE PTR [lpText + startOffset]
        .if al == 'i'
            mov al, BYTE PTR [lpText + startOffset + 1]
            .if al == 'f'
                mov eax, 1  ; Keyword
                ret
            .endif
        .endif
    .endif
    
    .if length == 4
        mov eax, DWORD PTR [lpText + startOffset]
        ; "else" = 0x65736c65
        .if eax == 0x65736c65
            mov eax, 1
            ret
        .endif
    .endif
    
    .if length == 5
        ; "while", "return", "class", etc.
        mov eax, 1  ; Assume keyword for now
        ret
    .endif
    
    mov eax, 4  ; Identifier (not keyword)
    ret
Tokenizer_IsKeyword ENDP

;==============================================================================
; Compute Block Content Hash
;==============================================================================
Tokenizer_ComputeBlockHash PROC blockStartLine:DWORD, lineCount:DWORD
    LOCAL hash:DWORD
    LOCAL i:DWORD
    
    mov hash, 5381  ; djb2 initial value
    
    mov i, blockStartLine
.loop1:
    .if i >= blockStartLine + lineCount
        .break
    .endif
    
    ; TODO: Read line and hash content
    ; For now, simple sequential hash
    mov eax, hash
    shl eax, 5
    add eax, hash
    add eax, i
    mov hash, eax
    
    inc i
    .continue .loop1
.endloop1
    
    mov eax, hash
    ret
Tokenizer_ComputeBlockHash ENDP

;==============================================================================
; Invalidate Block on Edit
;==============================================================================
Tokenizer_InvalidateBlock PROC blockNum:DWORD
    mov rax, [blockCache]
    mov BYTE PTR [rax + blockNum * SIZEOF TokenBlock].TokenBlock.isDirty, 1
    
    LOG_DEBUG "Invalidated token block %d", blockNum
    ret
Tokenizer_InvalidateBlock ENDP

;==============================================================================
; Get Tokens for Line Range
;==============================================================================
Tokenizer_GetTokens PROC blockNum:DWORD, lpOutTokenArray:QWORD
    mov rax, [blockCache]
    mov rcx, [rax + blockNum * SIZEOF TokenBlock].TokenBlock.tokens
    mov [lpOutTokenArray], rcx
    
    mov eax, [rax + blockNum * SIZEOF TokenBlock].TokenBlock.tokenCount
    ret
Tokenizer_GetTokens ENDP

;==============================================================================
; Data Structures
;==============================================================================
.data

; Token structure
Token STRUCT
    startOffset     DWORD ?      ; Byte offset in line
    length          DWORD ?      ; Token length
    typeId          DWORD ?      ; 1=Keyword, 2=String, 3=Comment, 4=Identifier, 5=Number, 6=Operator
    styleId         DWORD ?      ; Color index from theme
Token ENDS

; Token block structure
TokenBlock STRUCT
    blockNum        DWORD ?      ; 512-line block identifier
    tokens          QWORD ?      ; Array of Token structs
    tokenCount      DWORD ?      ; Count of tokens
    contentHash     DWORD ?      ; Hash of block content
    isDirty         BYTE ?       ; Needs re-lex?
TokenBlock ENDS

; Global state
tokHeap             dq ?         ; Private heap for allocations
blockCache          dq ?         ; Array of TokenBlock structures
blockCount          dd 0         ; Number of blocks
currentLanguage     dd 0         ; Language ID (0=C++, 1=Python, etc.)

; Lexer table
lexerTable          dq 10 dup(?) ; Function pointers for lexers

lineBuffer          db 65536 dup(?) ; Temporary line buffer

END
