; ═══════════════════════════════════════════════════════════════════
; ast_indexer.asm — Monolithic AST context bootstrap seam
;
; This is the first monolithic-lane integration point for the
; persistent AST indexer. It keeps a stable context contract for
; the bridge and startup path while the full scanner/hash table
; implementation is filled in behind the same entry points.
; ═══════════════════════════════════════════════════════════════════

PUBLIC ASTIndexer_Initialize
PUBLIC ASTIndexer_GetContextForAgent
PUBLIC ASTIndexer_GetStatus

AST_MAX_CONTEXT_BYTES equ 4096
AST_QUERY_PREVIEW_MAX equ 256

.data
align 8
g_astIndexerReady    dd 0
g_astIndexedFiles    dd 0
g_astTotalFiles      dd 0
g_astContextLen      dd 0
g_astLastFlags       dd 0
szASTBaseContext     db "# RawrXD AST Context",13,10
                     db "- lane: monolithic",13,10
                     db "- status: bootstrap-ready",13,10
                     db "- files: main.asm, bridge.asm, ui.asm, ast_indexer.asm",13,10
                     db "- hook: RXD_MSG_GET_CONTEXT",13,10,0
szASTQueryPrefix     db "- query: ",0
szASTNewline         db 13,10,0
g_astContextBuf      db AST_MAX_CONTEXT_BYTES dup(0)

.code

ASTI_ClearBuffer PROC
    lea     rdi, g_astContextBuf
    mov     ecx, AST_MAX_CONTEXT_BYTES
    xor     eax, eax
    rep stosb
    mov     g_astContextLen, 0
    ret
ASTI_ClearBuffer ENDP


ASTI_AppendZ PROC
    ; RCX = zero-terminated source string
    test    rcx, rcx
    jz      @done

    mov     rsi, rcx
    lea     rdi, g_astContextBuf
    mov     eax, g_astContextLen
    add     rdi, rax

@copy_loop:
    mov     eax, g_astContextLen
    cmp     eax, AST_MAX_CONTEXT_BYTES - 1
    jae     @terminate
    mov     al, byte ptr [rsi]
    test    al, al
    jz      @terminate
    mov     byte ptr [rdi], al
    inc     rsi
    inc     rdi
    inc     g_astContextLen
    jmp     @copy_loop

@terminate:
    mov     eax, g_astContextLen
    lea     rdx, g_astContextBuf
    mov     byte ptr [rdx + rax], 0
@done:
    ret
ASTI_AppendZ ENDP


ASTI_AppendLimited PROC
    ; RCX = source string, EDX = max bytes to copy
    test    rcx, rcx
    jz      @done
    test    edx, edx
    jle     @done

    mov     rsi, rcx
    mov     r10d, edx
    lea     rdi, g_astContextBuf
    mov     eax, g_astContextLen
    add     rdi, rax

@copy_loop:
    test    r10d, r10d
    jle     @terminate
    mov     eax, g_astContextLen
    cmp     eax, AST_MAX_CONTEXT_BYTES - 1
    jae     @terminate
    mov     al, byte ptr [rsi]
    test    al, al
    jz      @terminate
    mov     byte ptr [rdi], al
    inc     rsi
    inc     rdi
    dec     r10d
    inc     g_astContextLen
    jmp     @copy_loop

@terminate:
    mov     eax, g_astContextLen
    lea     rdx, g_astContextBuf
    mov     byte ptr [rdx + rax], 0
@done:
    ret
ASTI_AppendLimited ENDP


ASTIndexer_Initialize PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_astIndexerReady, 1
    mov     g_astIndexedFiles, 1
    mov     g_astTotalFiles, 1
    call    ASTI_ClearBuffer
    lea     rcx, szASTBaseContext
    call    ASTI_AppendZ
    mov     eax, 1

    add     rsp, 28h
    ret
ASTIndexer_Initialize ENDP


ASTIndexer_GetContextForAgent PROC FRAME
    ; RCX = optional UTF-8 query pointer
    ; EDX = reserved flags
    ; Returns: RAX = pointer to internal UTF-8 markdown buffer
    ;          EDX = byte length
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rbx, rcx
    mov     g_astLastFlags, edx
    cmp     g_astIndexerReady, 1
    je      @ready
    call    ASTIndexer_Initialize

@ready:
    call    ASTI_ClearBuffer
    lea     rcx, szASTBaseContext
    call    ASTI_AppendZ

    test    rbx, rbx
    jz      @return
    lea     rcx, szASTQueryPrefix
    call    ASTI_AppendZ
    mov     rcx, rbx
    mov     edx, AST_QUERY_PREVIEW_MAX
    call    ASTI_AppendLimited
    lea     rcx, szASTNewline
    call    ASTI_AppendZ

@return:
    lea     rax, g_astContextBuf
    mov     edx, g_astContextLen
    add     rsp, 20h
    pop     rbx
    ret
ASTIndexer_GetContextForAgent ENDP


ASTIndexer_GetStatus PROC
    ; Returns packed status: low32=indexed, high32=total
    mov     eax, g_astIndexedFiles
    mov     edx, g_astTotalFiles
    shl     rdx, 32
    and     rax, 00000000FFFFFFFFh
    or      rax, rdx
    ret
ASTIndexer_GetStatus ENDP

END