; ============================================================================
; File 23: text_tokenizer.asm - Block-based incremental syntax highlighting
; ============================================================================
; Purpose: Lex source code into tokens with block caching and hash invalidation
; Uses: Token arrays (1000 blocks × 512 lines), language registry, hash-based dirty tracking
; Functions: Init, TokenizeRange, LexBlock, LexLine, IsKeyword, RegisterLanguage, GetTokens
; ============================================================================

.code

; CONSTANTS
; ============================================================================

TOKENIZER_BLOCK_SIZE    equ 512      ; lines per block
TOKENIZER_MAX_BLOCKS    equ 1000     ; max 1000 blocks = 512K lines
TOKENIZER_TOKEN_TYPE_KEYWORD equ 1
TOKENIZER_TOKEN_TYPE_STRING  equ 2
TOKENIZER_TOKEN_TYPE_COMMENT equ 3
TOKENIZER_TOKEN_TYPE_IDENT   equ 4
TOKENIZER_TOKEN_TYPE_NUMBER  equ 5
TOKENIZER_TOKEN_TYPE_OPERATOR equ 6

; INITIALIZATION
; ============================================================================

Tokenizer_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: Tokenizer* in rax
    ; Allocate Tokenizer struct (120 bytes)
    ; { blockCache, blockHashes, languageRegistry, currentLanguage, mutex, stats... }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax  ; process heap
    
    ; Allocate main struct
    mov rdx, 0    ; flags
    mov r8, 120   ; size
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = Tokenizer*
    
    ; Allocate block cache (1000 blocks)
    mov rcx, rbx
    mov rdx, 0
    mov r8, 1048576  ; 1000 * sizeof(Token*) array
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rcx + 0], rax  ; blockCache
    
    ; Allocate block hash array
    mov r8, 8000  ; 1000 * 8 bytes
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rcx + 8], rax  ; blockHashes
    
    ; Allocate language registry (10 languages × 64 bytes)
    mov r8, 640
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rcx + 16], rax  ; languageRegistry
    mov qword ptr [rcx + 24], 0  ; languageCount = 0
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rcx + 40]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    ; Initialize stats
    mov rcx, rbx
    mov qword ptr [rcx + 80], 0   ; totalTokens
    mov qword ptr [rcx + 88], 0   ; cacheHits
    mov qword ptr [rcx + 96], 0   ; cacheMisses
    
    ; Register default languages
    mov rcx, rbx
    lea rdx, [rel cpp_lang_name]
    sub rsp, 40
    mov rcx, rbx
    mov rdx, offset cpp_lang_name
    mov r8, offset cpp_keywords
    call Tokenizer_RegisterLanguage
    add rsp, 40
    
    mov rax, rbx
    ret
Tokenizer_Init ENDP

; REGISTER LANGUAGE
; ============================================================================

Tokenizer_RegisterLanguage PROC USES rbx rcx rdx rsi rdi r8 r9 tokenizer:PTR DWORD, langName:PTR BYTE, keywords:PTR DWORD
    ; tokenizer = Tokenizer*
    ; langName = "C++", "Python", "PowerShell"
    ; keywords = keyword array pointer
    
    mov rcx, tokenizer
    mov rax, [rcx + 24]  ; languageCount
    cmp rax, 10
    jge @register_fail
    
    mov rsi, [rcx + 16]  ; languageRegistry
    lea rdi, [rsi + rax*64]  ; next language slot
    
    ; Copy language name
    mov rsi, langName
    mov rdx, 32  ; max 32 bytes
@copy_name:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    cmp al, 0
    je @name_done
    inc rsi
    inc rdi
    dec rdx
    jnz @copy_name
    
@name_done:
    ; Store keywords pointer
    mov [rdi + 32], keywords
    
    mov rcx, tokenizer
    mov rax, [rcx + 24]
    inc rax
    mov [rcx + 24], rax
    
    mov rax, 1
    ret
    
@register_fail:
    mov rax, 0
    ret
Tokenizer_RegisterLanguage ENDP

; TOKENIZE RANGE (Main Entry Point)
; ============================================================================

Tokenizer_TokenizeRange PROC USES rbx rcx rdx rsi rdi r8 r9 tokenizer:PTR DWORD, startLine:QWORD, endLine:QWORD
    ; tokenizer = Tokenizer*
    ; startLine = first dirty line
    ; endLine = last dirty line
    ; Returns: token count in rax
    
    mov rcx, tokenizer
    lea rdx, [rcx + 40]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, tokenizer
    mov rdx, startLine
    mov r8, endLine
    
    ; Calculate which blocks are affected
    mov rax, rdx
    mov r9, TOKENIZER_BLOCK_SIZE
    xor edx, edx
    div r9  ; rax = startBlock
    
    mov rbx, r8
    mov r9, TOKENIZER_BLOCK_SIZE
    xor edx, edx
    div r9  ; rax = endBlock
    
    mov r9, rax  ; r9 = endBlock
    mov rax, rdx ; rax = startBlock (recalc)
    
    xor r10, r10  ; r10 = total tokens
    
@block_loop:
    cmp rax, r9
    jg @tokenize_done
    
    ; Tokenize this block
    mov rdx, rax
    mov rcx, tokenizer
    call Tokenizer_TokenizeBlock
    
    add r10, rax  ; accumulate tokens
    
    inc rax
    jmp @block_loop
    
@tokenize_done:
    mov rax, r10
    
    mov rcx, tokenizer
    lea rdx, [rcx + 40]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
Tokenizer_TokenizeRange ENDP

; TOKENIZE SINGLE BLOCK
; ============================================================================

Tokenizer_TokenizeBlock PROC USES rbx rcx rdx rsi rdi r8 r9 tokenizer:PTR DWORD, blockNum:QWORD
    ; tokenizer = Tokenizer*
    ; blockNum = block index
    ; Returns: token count in rax
    
    mov rcx, tokenizer
    mov rdx, blockNum
    
    ; Allocate token array for this block (512 lines × ~10 tokens/line = 5KB typical)
    mov r8, 65536  ; allocate 64KB per block
    sub rsp, 40
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 0
    call HeapAlloc
    add rsp, 40
    mov r8, rax  ; r8 = token array
    
    ; Store in cache
    mov rsi, [rcx + 0]  ; blockCache
    mov [rsi + rdx*8], r8
    
    ; Compute block hash
    call Tokenizer_ComputeBlockHash
    mov r9, rax  ; r9 = block hash
    
    mov rsi, [rcx + 8]  ; blockHashes
    mov [rsi + rdx*8], r9
    
    ; Lex each line in block
    mov r10, 0  ; line counter
    xor r11, r11  ; token counter
    
@line_loop:
    cmp r10, TOKENIZER_BLOCK_SIZE
    jge @block_done
    
    ; Calculate absolute line number
    mov rax, rdx
    mov r9, TOKENIZER_BLOCK_SIZE
    imul rax, r9
    add rax, r10
    
    ; Lex this line
    mov rcx, tokenizer
    mov rsi, r8
    mov rdi, r11  ; token offset
    call Tokenizer_LexLine
    
    add r11, rax  ; token count += tokens on this line
    
    inc r10
    jmp @line_loop
    
@block_done:
    mov rax, r11  ; return total tokens
    ret
Tokenizer_TokenizeBlock ENDP

; LEX SINGLE LINE
; ============================================================================

Tokenizer_LexLine PROC USES rbx rcx rdx rsi rdi r8 r9 tokenizer:PTR DWORD, tokenArray:PTR DWORD, tokenOffset:QWORD
    ; tokenizer = Tokenizer*
    ; tokenArray = Token* buffer
    ; tokenOffset = where to store tokens
    ; Returns: token count in rax
    
    ; This is the core lexer that processes a single line
    ; Returns token count
    
    xor rax, rax  ; token count = 0
    mov rcx, 0    ; position in line
    
@lex_char_loop:
    cmp rcx, 256  ; max line length
    jge @lex_done
    
    ; Get character (would need to actually read from buffer)
    ; For now, stub return
    mov rax, 0
    ret
    
@lex_done:
    ret
Tokenizer_LexLine ENDP

; CHECK IF KEYWORD
; ============================================================================

Tokenizer_IsKeyword PROC USES rbx rcx rdx rsi rdi text:PTR BYTE, length:QWORD
    ; text = potential keyword string
    ; length = string length
    ; Returns: 1 if keyword, 0 otherwise
    
    mov rcx, length
    mov rsi, text
    
    ; Fast paths for common keywords
    cmp rcx, 2
    je @check_if
    cmp rcx, 3
    je @check_for
    cmp rcx, 4
    je @check_bool
    cmp rcx, 5
    je @check_const
    cmp rcx, 6
    je @check_return
    cmp rcx, 8
    je @check_unsigned
    
    mov rax, 0  ; not a keyword
    ret
    
@check_if:
    cmp word ptr [rsi], 0x6669  ; "if"
    je @is_keyword
    mov rax, 0
    ret
    
@check_for:
    cmp dword ptr [rsi], 0x726F66  ; "for"
    je @is_keyword
    mov rax, 0
    ret
    
@check_bool:
    mov eax, dword ptr [rsi]
    cmp eax, 0x656B616F  ; likely "bool" or "void"
    je @is_keyword
    mov rax, 0
    ret
    
@check_const:
    mov eax, dword ptr [rsi]
    mov r8d, dword ptr [rsi + 1]
    ; check "const", "while", etc
    mov rax, 0
    ret
    
@check_return:
    mov eax, dword ptr [rsi]
    mov r8w, word ptr [rsi + 4]
    ; check "return"
    mov rax, 0
    ret
    
@check_unsigned:
    mov eax, dword ptr [rsi]
    mov r8d, dword ptr [rsi + 4]
    ; check "unsigned"
    mov rax, 0
    ret
    
@is_keyword:
    mov rax, 1
    ret
Tokenizer_IsKeyword ENDP

; COMPUTE BLOCK HASH
; ============================================================================

Tokenizer_ComputeBlockHash PROC USES rbx rcx rdx rsi rdi r8 r9 tokenizer:PTR DWORD, blockNum:QWORD
    ; Compute djb2 hash of block to detect changes
    ; Returns: hash in rax
    
    mov rcx, tokenizer
    mov rdx, blockNum
    mov rsi, 5381  ; djb2 init
    
    ; In real implementation, would read block from GapBuffer
    ; and compute hash over its content
    
    mov rax, rsi  ; return hash
    ret
Tokenizer_ComputeBlockHash ENDP

; INVALIDATE BLOCK
; ============================================================================

Tokenizer_InvalidateBlock PROC USES rbx rcx rdx rsi rdi tokenizer:PTR DWORD, blockNum:QWORD
    ; Mark block as dirty (hash mismatch)
    ; This is called when edit affects a block
    
    mov rcx, tokenizer
    mov rdx, blockNum
    
    ; Clear cached tokens
    mov rsi, [rcx + 0]  ; blockCache
    mov qword ptr [rsi + rdx*8], 0
    
    ; Zero hash to force re-lex
    mov rsi, [rcx + 8]  ; blockHashes
    mov qword ptr [rsi + rdx*8], 0
    
    mov rax, 1
    ret
Tokenizer_InvalidateBlock ENDP

; GET TOKENS FOR RENDERING
; ============================================================================

Tokenizer_GetTokens PROC USES rbx rcx rdx rsi rdi r8 tokenizer:PTR DWORD, blockNum:QWORD
    ; tokenizer = Tokenizer*
    ; blockNum = which block
    ; Returns: Token* in rax (or NULL if not cached)
    
    mov rcx, tokenizer
    mov rdx, blockNum
    
    mov rsi, [rcx + 0]  ; blockCache
    mov rax, [rsi + rdx*8]  ; get cached token array
    
    cmp rax, 0
    je @tokens_not_cached
    
    mov r8, [rcx + 88]  ; cacheHits
    inc r8
    mov [rcx + 88], r8
    
    ret
    
@tokens_not_cached:
    ; Trigger re-lex
    mov r8, [rcx + 96]  ; cacheMisses
    inc r8
    mov [rcx + 96], r8
    
    mov rax, 0
    ret
Tokenizer_GetTokens ENDP

; LANGUAGE DEFINITIONS (C++ Keywords)
; ============================================================================

cpp_lang_name BYTE "C++", 0
cpp_keywords QWORD 0  ; TODO: keyword array

end
