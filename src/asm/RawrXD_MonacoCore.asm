; ============================================================================
; RawrXD_MonacoCore.asm — Native Monaco-Equivalent Editor Engine
; ============================================================================
;
; Phase 28: MonacoCore — Pure x64 MASM editor kernel
;
; Architecture:
;   Gap Buffer      — O(1) insert/delete at cursor
;   ASM Tokenizer   — Branch-predicted state machine (no regex)
;   Direct2D Bridge — Exports called by C++ MonacoCoreEngine for rendering
;
; Assemble: ml64.exe /c /Fo RawrXD_MonacoCore.obj RawrXD_MonacoCore.asm
; Link:     Linked as object into RawrXD-Win32IDE (not a DLL)
;
; Exports:
;   MC_GapBuffer_Init       — Initialize gap buffer with capacity
;   MC_GapBuffer_Destroy    — Free gap buffer memory
;   MC_GapBuffer_MoveGap    — Reposition gap for insertion
;   MC_GapBuffer_Insert     — Insert text at position
;   MC_GapBuffer_Delete     — Delete range from buffer
;   MC_GapBuffer_GetLine    — Extract line content
;   MC_GapBuffer_Length     — Get logical content length
;   MC_GapBuffer_LineCount  — Count newlines in buffer
;   MC_TokenizeLine         — Tokenize a single line (ASM-optimized)
;   MC_IsRegister           — Check if token is CPU register
;   MC_IsInstruction        — Check if token is CPU instruction
;   MC_IsDirective          — Check if token is ASM directive
;
; ABI:      Windows x64 (RCX, RDX, R8, R9 + shadow space)
; Callee-save: RBX, RBP, RSI, RDI, R12-R15, XMM6-XMM15
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; ============================================================================

; Use basic includes only — no masm64rt dependency for portability
option casemap:none

; ============================================================================
; Constants
; ============================================================================
MC_GAP_MIN_SIZE       equ 256         ; Minimum gap size after grow
MC_MAX_LINE_LENGTH    equ 4096        ; Max extractable line
MC_TAB_SIZE           equ 4

; Token types (must match include/RawrXD_MonacoCore.h MC_TokenType enum)
TOKEN_NONE            equ 0
TOKEN_COMMENT         equ 1
TOKEN_KEYWORD         equ 2
TOKEN_IDENTIFIER      equ 3
TOKEN_STRING          equ 4
TOKEN_NUMBER          equ 5
TOKEN_OPERATOR        equ 6
TOKEN_REGISTER        equ 7
TOKEN_DIRECTIVE       equ 8
TOKEN_INSTRUCTION     equ 9
TOKEN_PREPROCESSOR    equ 10
TOKEN_WHITESPACE      equ 11

; ============================================================================
; Structures (must match C++ layout in include/RawrXD_MonacoCore.h)
; ============================================================================

; MC_GapBuffer — 40 bytes
; struct MC_GapBuffer {
;     uint8_t* pBuffer;    // +0   (8 bytes)
;     uint32_t gapStart;   // +8   (4 bytes)
;     uint32_t gapEnd;     // +12  (4 bytes)
;     uint32_t capacity;   // +16  (4 bytes)
;     uint32_t used;       // +20  (4 bytes)
;     uint32_t lineCount;  // +24  (4 bytes)
;     uint32_t reserved;   // +28  (4 bytes, alignment)
; };
; Total: 32 bytes

GB_pBuffer      equ 0
GB_gapStart     equ 8
GB_gapEnd       equ 12
GB_capacity     equ 16
GB_used         equ 20
GB_lineCount    equ 24

; MC_Token — 16 bytes
; struct MC_Token {
;     uint32_t startCol;   // +0
;     uint32_t length;     // +4
;     uint32_t tokenType;  // +8
;     uint32_t color;      // +12 (BGRA cached)
; };

TK_startCol     equ 0
TK_length       equ 4
TK_tokenType    equ 8
TK_color        equ 12
TK_SIZE         equ 16

; ============================================================================
; External Win32 API imports
; ============================================================================
extern HeapAlloc:proc
extern HeapReAlloc:proc
extern HeapFree:proc
extern GetProcessHeap:proc

; ============================================================================
; Data Section
; ============================================================================
.data
align 16

; x86/x64 register names for tokenizer classification
; Sorted by length for quick matching. Double-null terminated.
szRegisters_2 db 'ax',0,'bx',0,'cx',0,'dx',0,'si',0,'di',0,'bp',0,'sp',0
              db 'r8',0,'r9',0, 0

szRegisters_3 db 'rax',0,'rbx',0,'rcx',0,'rdx',0,'rsi',0,'rdi',0,'rbp',0,'rsp',0
              db 'eax',0,'ebx',0,'ecx',0,'edx',0,'esi',0,'edi',0,'ebp',0,'esp',0
              db 'r10',0,'r11',0,'r12',0,'r13',0,'r14',0,'r15',0
              db 'xmm',0,'ymm',0,'zmm',0, 0

; Common x86/x64 instruction mnemonics (double-null terminated)
szInstructions db 'mov',0,'push',0,'pop',0,'call',0,'ret',0,'jmp',0
               db 'je',0,'jne',0,'jz',0,'jnz',0,'jg',0,'jl',0,'jge',0,'jle',0
               db 'ja',0,'jb',0,'jae',0,'jbe',0,'jc',0,'jnc',0
               db 'cmp',0,'test',0,'add',0,'sub',0,'mul',0,'imul',0
               db 'div',0,'idiv',0,'xor',0,'and',0,'or',0,'not',0,'neg',0
               db 'shl',0,'shr',0,'sar',0,'sal',0,'rol',0,'ror',0
               db 'lea',0,'inc',0,'dec',0,'nop',0,'int',0,'syscall',0
               db 'rep',0,'lock',0,'xchg',0,'bswap',0
               db 'movzx',0,'movsx',0,'movsb',0,'movsw',0,'movsd',0,'movsq',0
               db 'cmova',0,'cmovb',0,'cmove',0,'cmovg',0,'cmovl',0
               db 'sete',0,'setne',0,'setg',0,'setl',0,'setge',0,'setle',0
               db 'loop',0,'loope',0,'loopne',0
               db 'enter',0,'leave',0
               db 'stosb',0,'stosw',0,'stosd',0,'stosq',0
               db 'lodsb',0,'lodsw',0,'lodsd',0,'lodsq',0
               db 'scasb',0,'scasw',0,'scasd',0,'scasq',0
               db 'cld',0,'std',0,'clc',0,'stc',0,'cmc',0
               db 'cbw',0,'cwd',0,'cdq',0,'cqo',0
               db 'movaps',0,'movups',0,'movntps',0,'movdqa',0,'movdqu',0
               db 'addps',0,'subps',0,'mulps',0,'divps',0
               db 'addss',0,'subss',0,'mulss',0,'divss',0
               db 'vmovaps',0,'vmovups',0,'vaddps',0,'vmulps',0
               db 'vfmadd132ps',0,'vfmadd231ps',0,'vfmadd213ps',0
               db 'vzeroupper',0,'vzeroall',0
               db 'pxor',0,'por',0,'pand',0,'pandn',0
               db 'pcmpeqb',0,'pcmpeqw',0,'pcmpeqd',0
               db 'pmovmskb',0,'pshufb',0,'pshufd',0
               db 'cpuid',0,'rdtsc',0,'rdtscp',0
               db 'mfence',0,'sfence',0,'lfence',0
               db 'prefetcht0',0,'prefetcht1',0,'prefetchnta',0
               db 0

; ASM directives (double-null terminated)
szDirectives  db 'proc',0,'endp',0,'struct',0,'ends',0,'macro',0,'endm',0
              db 'include',0,'includelib',0,'extern',0,'public',0,'proto',0
              db 'invoke',0,'local',0,'uses',0
              db '.code',0,'.data',0,'.const',0,'.stack',0
              db '.model',0,'.386',0,'.486',0,'.586',0,'.686',0,'.x64',0
              db 'byte',0,'word',0,'dword',0,'qword',0,'real4',0,'real8',0
              db 'db',0,'dw',0,'dd',0,'dq',0
              db 'equ',0,'textequ',0,'typedef',0,'union',0
              db 'if',0,'ifdef',0,'ifndef',0,'else',0,'elseif',0,'endif',0
              db 'option',0,'segment',0,'align',0,'org',0,'end',0
              db 0

; C/C++ keywords for multi-language support (double-null terminated)
szCKeywords   db 'auto',0,'break',0,'case',0,'char',0,'const',0,'continue',0
              db 'default',0,'do',0,'double',0,'else',0,'enum',0,'extern',0
              db 'float',0,'for',0,'goto',0,'if',0,'inline',0,'int',0
              db 'long',0,'register',0,'restrict',0,'return',0,'short',0
              db 'signed',0,'sizeof',0,'static',0,'struct',0,'switch',0
              db 'typedef',0,'union',0,'unsigned',0,'void',0,'volatile',0
              db 'while',0,'class',0,'namespace',0,'template',0,'virtual',0
              db 'public',0,'private',0,'protected',0,'new',0,'delete',0
              db 'try',0,'catch',0,'throw',0,'nullptr',0,'constexpr',0
              db 'override',0,'final',0,'noexcept',0,'decltype',0,'auto',0
              db 'using',0,'typename',0,'concept',0,'requires',0,'co_await',0
              db 'co_yield',0,'co_return',0
              db 0

; ============================================================================
; Code Section
; ============================================================================
.code

; ============================================================================
; MC_GapBuffer_Init
; ============================================================================
; bool MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity)
; RCX = pGB, EDX = initialCapacity
; Returns: 1 on success, 0 on failure
;
; Allocates the backing buffer. Gap spans the entire capacity initially.
; ============================================================================
MC_GapBuffer_Init proc
    ; Preserve callee-saved
    push rbx
    push rsi
    sub rsp, 40                     ; Shadow space + alignment

    mov rbx, rcx                    ; RBX = pGB
    mov esi, edx                    ; ESI = initialCapacity

    ; Clamp minimum capacity
    cmp esi, MC_GAP_MIN_SIZE
    jae @init_alloc
    mov esi, MC_GAP_MIN_SIZE

@init_alloc:
    ; HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, capacity)
    call GetProcessHeap
    mov rcx, rax                    ; hHeap
    mov edx, 8                      ; HEAP_ZERO_MEMORY
    mov r8d, esi                    ; dwBytes
    call HeapAlloc

    test rax, rax
    jz @init_fail

    ; Store in struct
    mov [rbx + GB_pBuffer], rax
    mov dword ptr [rbx + GB_gapStart], 0
    mov [rbx + GB_gapEnd], esi
    mov [rbx + GB_capacity], esi
    mov dword ptr [rbx + GB_used], 0
    mov dword ptr [rbx + GB_lineCount], 0

    mov eax, 1
    jmp @init_done

@init_fail:
    xor eax, eax

@init_done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
MC_GapBuffer_Init endp

; ============================================================================
; MC_GapBuffer_Destroy
; ============================================================================
; void MC_GapBuffer_Destroy(MC_GapBuffer* pGB)
; RCX = pGB
; ============================================================================
MC_GapBuffer_Destroy proc
    push rbx
    sub rsp, 32

    mov rbx, rcx
    mov rcx, [rbx + GB_pBuffer]
    test rcx, rcx
    jz @destroy_done

    ; HeapFree(GetProcessHeap(), 0, pBuffer)
    push rcx                        ; Save buffer ptr
    call GetProcessHeap
    mov rcx, rax                    ; hHeap
    xor edx, edx                    ; dwFlags = 0
    pop r8                          ; lpMem = pBuffer
    call HeapFree

    ; Zero the struct
    mov qword ptr [rbx + GB_pBuffer], 0
    mov dword ptr [rbx + GB_gapStart], 0
    mov dword ptr [rbx + GB_gapEnd], 0
    mov dword ptr [rbx + GB_capacity], 0
    mov dword ptr [rbx + GB_used], 0
    mov dword ptr [rbx + GB_lineCount], 0

@destroy_done:
    add rsp, 32
    pop rbx
    ret
MC_GapBuffer_Destroy endp

; ============================================================================
; MC_GapBuffer_MoveGap
; ============================================================================
; void MC_GapBuffer_MoveGap(MC_GapBuffer* pGB, uint32_t pos)
; RCX = pGB, EDX = target position (in logical content space)
;
; Moves the gap so that gapStart == pos, enabling O(1) insert at pos.
; ============================================================================
MC_GapBuffer_MoveGap proc
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    mov rbx, rcx                    ; RBX = pGB
    mov esi, edx                    ; ESI = target position

    mov edi, [rbx + GB_gapStart]    ; EDI = current gapStart

    cmp esi, edi
    je @movegap_done                ; Already at target

    mov r8, [rbx + GB_pBuffer]      ; R8 = base pointer

    ja @movegap_right

@movegap_left:
    ; Moving gap left: copy [pos, gapStart) to end of gap
    ; Bytes to move = gapStart - pos
    mov ecx, edi
    sub ecx, esi                    ; ECX = byte count to move

    ; Source: pBuffer + pos
    ; Dest:   pBuffer + (gapEnd - count)
    mov eax, [rbx + GB_gapEnd]
    sub eax, ecx                    ; EAX = new gapEnd

    ; Use backward copy (memmove-safe, though regions don't overlap here)
    lea rdi, [r8 + rax]             ; Destination
    lea rsi, [r8 + rsi]             ; Source (original ESI = pos, now in RSI for rep)
    ; ECX already set
    rep movsb

    ; Update struct
    mov esi, edx                    ; Restore original pos from EDX shadow? No, use stack
    ; Actually ESI was clobbered by rep movsb. Recalculate.
    mov eax, [rbx + GB_gapEnd]
    mov ecx, [rbx + GB_gapStart]
    sub ecx, edx                    ; count = old gapStart - pos
    sub eax, ecx                    ; new gapEnd = old gapEnd - count
    mov [rbx + GB_gapEnd], eax
    mov [rbx + GB_gapStart], edx
    jmp @movegap_done

@movegap_right:
    ; Moving gap right: copy [gapEnd, gapEnd + (pos - gapStart)) to gapStart
    ; Bytes to move = pos - gapStart
    mov ecx, esi
    sub ecx, edi                    ; ECX = byte count

    ; Source: pBuffer + gapEnd
    ; Dest:   pBuffer + gapStart
    mov eax, [rbx + GB_gapEnd]
    lea rsi, [r8 + rax]             ; Source = gapEnd
    lea rdi, [r8 + rdi]             ; Dest = gapStart (EDI was old gapStart)
    ; ECX = count
    rep movsb

    ; Update struct
    mov eax, [rbx + GB_gapStart]
    mov ecx, edx                    ; pos
    sub ecx, eax                    ; count = pos - old gapStart
    add eax, ecx
    mov [rbx + GB_gapStart], eax    ; new gapStart = pos
    mov eax, [rbx + GB_gapEnd]
    add eax, ecx
    mov [rbx + GB_gapEnd], eax      ; new gapEnd += count

@movegap_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
MC_GapBuffer_MoveGap endp

; ============================================================================
; MC_GapBuffer_EnsureGap
; ============================================================================
; Internal: Grow the buffer if gap < needed bytes
; RCX = pGB, EDX = needed gap bytes
; Returns: 1 success, 0 failure (allocation error)
; ============================================================================
MC_GapBuffer_EnsureGap proc
    push rbx
    push rsi
    push rdi
    sub rsp, 48

    mov rbx, rcx                    ; RBX = pGB
    mov esi, edx                    ; ESI = needed

    ; Current gap size
    mov eax, [rbx + GB_gapEnd]
    sub eax, [rbx + GB_gapStart]
    cmp eax, esi
    jae @ensure_ok                  ; Gap is big enough

    ; Need to grow: new capacity = max(capacity * 2, used + needed + MIN_GAP)
    mov eax, [rbx + GB_capacity]
    shl eax, 1                      ; Double
    mov ecx, [rbx + GB_used]
    add ecx, esi
    add ecx, MC_GAP_MIN_SIZE
    cmp eax, ecx
    jae @ensure_realloc
    mov eax, ecx                    ; Use larger value

@ensure_realloc:
    mov edi, eax                    ; EDI = new capacity
    mov [rsp + 32], edi             ; Save newCapacity past shadow space (safe from calls)

    ; HeapReAlloc(GetProcessHeap(), 0, pBuffer, newCapacity)
    call GetProcessHeap
    mov rcx, rax                    ; hHeap
    xor edx, edx                    ; dwFlags = 0
    mov r8, [rbx + GB_pBuffer]      ; lpMem
    mov r9d, [rsp + 32]             ; dwBytes = newCapacity
    call HeapReAlloc

    test rax, rax
    jz @ensure_fail

    mov [rbx + GB_pBuffer], rax
    mov edi, [rsp + 32]             ; Restore newCapacity into EDI

    ; Move data after gap to end of new buffer
    ; Bytes after gap = old_capacity - gapEnd
    mov ecx, [rbx + GB_capacity]
    mov eax, [rbx + GB_gapEnd]
    sub ecx, eax                    ; ECX = bytes after gap

    test ecx, ecx
    jz @ensure_no_move

    ; Compute newGapEnd = newCapacity - bytesAfterGap
    mov r10d, edi                   ; R10D = newCapacity (non-volatile save)
    mov eax, edi
    sub eax, ecx                    ; EAX = newGapEnd
    mov [rsp + 40], eax             ; Save newGapEnd past shadow space

    ; Source: pBuffer + old gapEnd
    mov r8, [rbx + GB_pBuffer]
    mov eax, [rbx + GB_gapEnd]
    lea rsi, [r8 + rax]             ; Source = pBuffer + old gapEnd
    ; Dest: pBuffer + newGapEnd
    mov eax, [rsp + 40]
    lea rdi, [r8 + rax]             ; Dest = pBuffer + newGapEnd (clobbers EDI)

    ; Must use backward copy since dest > source (regions may overlap)
    add rsi, rcx
    add rdi, rcx
    dec rsi
    dec rdi
    std                             ; Set direction flag for backward
    rep movsb
    cld                             ; Clear direction flag

    ; Update gapEnd from saved value
    mov eax, [rsp + 40]             ; Recover newGapEnd past shadow space
    mov [rbx + GB_gapEnd], eax
    jmp @ensure_update_cap

@ensure_no_move:
    ; No data after gap, so gapEnd = newCapacity
    mov eax, [rsp + 32]             ; newCapacity past shadow space
    mov [rbx + GB_gapEnd], eax

@ensure_update_cap:
    mov edi, [rsp + 32]             ; Recover newCapacity past shadow space
    mov [rbx + GB_capacity], edi

@ensure_ok:
    mov eax, 1
    jmp @ensure_done

@ensure_fail:
    xor eax, eax

@ensure_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
MC_GapBuffer_EnsureGap endp

; ============================================================================
; MC_GapBuffer_Insert
; ============================================================================
; bool MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos,
;                          const char* text, uint32_t len)
; RCX = pGB, EDX = pos, R8 = text, R9D = len
; Returns: 1 success, 0 failure
; ============================================================================
MC_GapBuffer_Insert proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48

    mov rbx, rcx                    ; RBX = pGB
    mov r12d, edx                   ; R12D = pos
    mov r13, r8                     ; R13 = text pointer
    mov esi, r9d                    ; ESI = len

    test esi, esi
    jz @insert_ok                   ; Nothing to insert

    ; Ensure gap is big enough
    mov rcx, rbx
    mov edx, esi
    call MC_GapBuffer_EnsureGap
    test eax, eax
    jz @insert_fail

    ; Move gap to insertion position
    mov rcx, rbx
    mov edx, r12d
    call MC_GapBuffer_MoveGap

    ; Copy text into gap
    ; NOTE: len is in ESI (low32 of RSI). We must load ECX from ESI
    ; BEFORE overwriting RSI with the source pointer, because
    ; mov rsi, r13 clobbers ESI.
    mov ecx, esi                    ; Count = len (save BEFORE rsi is overwritten)
    mov rdi, [rbx + GB_pBuffer]
    mov eax, [rbx + GB_gapStart]
    add rdi, rax                    ; Dest = pBuffer + gapStart
    mov rsi, r13                    ; Source = text (clobbers ESI)
    mov [rsp], ecx                  ; Stash len on shadow space for later use
    rep movsb

    ; Update gapStart and used, count newlines
    mov ecx, [rsp]                  ; Recover len from shadow space
    add [rbx + GB_gapStart], ecx
    add [rbx + GB_used], ecx

    ; Count newlines in inserted text
    mov rsi, r13                    ; Restore text ptr
    mov ecx, [rsp]                  ; Recover len from shadow space
    xor edx, edx                   ; Newline counter

@count_nl:
    test ecx, ecx
    jz @count_nl_done
    mov al, [rsi]
    cmp al, 0Ah                     ; LF
    jne @count_nl_next
    inc edx
@count_nl_next:
    inc rsi
    dec ecx
    jmp @count_nl

@count_nl_done:
    add [rbx + GB_lineCount], edx

@insert_ok:
    mov eax, 1
    jmp @insert_done

@insert_fail:
    xor eax, eax

@insert_done:
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MC_GapBuffer_Insert endp

; ============================================================================
; MC_GapBuffer_Delete
; ============================================================================
; bool MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len)
; RCX = pGB, EDX = pos, R8D = len
; Returns: 1 success, 0 failure
; ============================================================================
MC_GapBuffer_Delete proc
    push rbx
    push rsi
    sub rsp, 40

    mov rbx, rcx
    mov esi, r8d                    ; ESI = len

    test esi, esi
    jz @del_ok

    ; Move gap to pos
    call MC_GapBuffer_MoveGap       ; RCX = pGB, EDX = pos already set

    ; Count newlines being deleted (in [gapEnd, gapEnd + len))
    mov r8, [rbx + GB_pBuffer]
    mov eax, [rbx + GB_gapEnd]
    lea r8, [r8 + rax]             ; R8 = start of text to delete
    xor edx, edx                   ; Newline counter
    mov ecx, esi

@del_count:
    test ecx, ecx
    jz @del_count_done
    mov al, [r8]
    cmp al, 0Ah
    jne @del_count_next
    inc edx
@del_count_next:
    inc r8
    dec ecx
    jmp @del_count

@del_count_done:
    sub [rbx + GB_lineCount], edx

    ; Expand gap to cover deleted range
    add [rbx + GB_gapEnd], esi
    sub [rbx + GB_used], esi

@del_ok:
    mov eax, 1
    add rsp, 40
    pop rsi
    pop rbx
    ret
MC_GapBuffer_Delete endp

; ============================================================================
; MC_GapBuffer_GetLine
; ============================================================================
; uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx,
;                                char* outBuffer, uint32_t maxLen)
; RCX = pGB, EDX = lineIdx, R8 = outBuffer, R9D = maxLen
; Returns: length of line written (excluding null), 0 if line not found
;
; Extracts a single line from the gap buffer by scanning for newlines.
; Handles the gap transparently.
; ============================================================================
MC_GapBuffer_GetLine proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40

    mov rbx, rcx                    ; RBX = pGB
    mov r12d, edx                   ; R12D = target line index
    mov r13, r8                     ; R13 = outBuffer
    mov r14d, r9d                   ; R14D = maxLen
    dec r14d                        ; Reserve space for null terminator

    mov r15, [rbx + GB_pBuffer]     ; R15 = base pointer
    mov r8d, [rbx + GB_gapStart]    ; R8D = gapStart
    mov r9d, [rbx + GB_gapEnd]      ; R9D = gapEnd
    mov r10d, [rbx + GB_capacity]   ; R10D = capacity

    ; Scan through logical content to find line start
    xor esi, esi                    ; ESI = current logical position
    xor ecx, ecx                    ; ECX = current line number

    ; Total content = used
    mov r11d, [rbx + GB_used]

    ; Skip to target line
    test r12d, r12d
    jz @getline_found               ; Line 0 starts at pos 0

@getline_scan:
    cmp esi, r11d
    jae @getline_notfound            ; Past end
    
    ; Map logical pos to physical pos
    cmp esi, r8d
    jb @getline_before_gap
    ; Physical = logical + (gapEnd - gapStart)
    mov eax, esi
    add eax, r9d
    sub eax, r8d
    jmp @getline_check_byte

@getline_before_gap:
    mov eax, esi                    ; Physical = logical (before gap)

@getline_check_byte:
    cmp eax, r10d
    jae @getline_notfound
    mov dl, [r15 + rax]
    inc esi
    cmp dl, 0Ah                     ; LF
    jne @getline_scan
    inc ecx
    cmp ecx, r12d
    jb @getline_scan
    ; ECX == target line, ESI = position right after the LF = start of target line

@getline_found:
    ; Copy line content until LF, EOF, or maxLen
    mov rdi, r13                    ; Dest = outBuffer
    xor ecx, ecx                    ; Bytes written

@getline_copy:
    cmp ecx, r14d
    jae @getline_terminate           ; Hit maxLen
    cmp esi, r11d
    jae @getline_terminate           ; Hit EOF

    ; Map logical to physical
    cmp esi, r8d
    jb @getline_copy_before
    mov eax, esi
    add eax, r9d
    sub eax, r8d
    jmp @getline_copy_byte

@getline_copy_before:
    mov eax, esi

@getline_copy_byte:
    mov dl, [r15 + rax]
    cmp dl, 0Ah
    je @getline_terminate            ; Hit newline — end of line
    cmp dl, 0Dh
    je @getline_skip_cr              ; Skip CR in CRLF
    mov [rdi], dl
    inc rdi
    inc ecx
@getline_skip_cr:
    inc esi
    jmp @getline_copy

@getline_terminate:
    mov byte ptr [rdi], 0           ; Null-terminate
    mov eax, ecx                    ; Return length
    jmp @getline_done

@getline_notfound:
    ; Line not found
    test r14d, r14d
    jz @getline_ret_zero
    mov byte ptr [r13], 0
@getline_ret_zero:
    xor eax, eax

@getline_done:
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MC_GapBuffer_GetLine endp

; ============================================================================
; MC_GapBuffer_Length
; ============================================================================
; uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB)
; RCX = pGB
; Returns: logical content length (used bytes)
; ============================================================================
MC_GapBuffer_Length proc
    mov eax, [rcx + GB_used]
    ret
MC_GapBuffer_Length endp

; ============================================================================
; MC_GapBuffer_LineCount
; ============================================================================
; uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB)
; RCX = pGB
; Returns: number of lines (lineCount + 1, since 0 newlines = 1 line)
; ============================================================================
MC_GapBuffer_LineCount proc
    mov eax, [rcx + GB_lineCount]
    inc eax                         ; +1: N newlines = N+1 lines
    ret
MC_GapBuffer_LineCount endp

; ============================================================================
; MC_TokenizeLine
; ============================================================================
; uint32_t MC_TokenizeLine(const char* line, uint32_t len,
;                          MC_Token* outTokens, uint32_t maxTokens)
; RCX = line, EDX = len, R8 = outTokens, R9D = maxTokens
; Returns: number of tokens emitted
;
; Branch-predicted state machine tokenizer. No regex engine.
; Handles: comments (;, //), strings ("", ''), numbers (dec, hex, bin),
;          identifiers (classified as keyword/register/instruction/directive)
; ============================================================================
MC_TokenizeLine proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 56

    mov rsi, rcx                    ; RSI = line
    mov r12d, edx                   ; R12D = len
    mov rdi, r8                     ; RDI = outTokens
    mov r13d, r9d                   ; R13D = maxTokens

    xor ebx, ebx                    ; EBX = token count
    xor r14d, r14d                  ; R14D = position in line

@tok_loop:
    cmp ebx, r13d
    jae @tok_done                   ; Max tokens reached
    cmp r14d, r12d
    jae @tok_done                   ; End of line

    ; Skip whitespace (don't emit tokens for it)
@tok_skip_ws:
    cmp r14d, r12d
    jae @tok_done
    movzx eax, byte ptr [rsi + r14]
    cmp al, ' '
    je @tok_ws_inc
    cmp al, 9                       ; Tab
    je @tok_ws_inc
    cmp al, 0Dh                     ; CR
    je @tok_ws_inc
    jmp @tok_classify

@tok_ws_inc:
    inc r14d
    jmp @tok_skip_ws

@tok_classify:
    ; Check first character to determine token type
    
    ; Comment: ; or //
    cmp al, ';'
    je @tok_comment
    cmp al, '/'
    jne @tok_check_string
    cmp r14d, r12d
    jae @tok_operator                ; / at end = operator
    movzx ecx, byte ptr [rsi + r14 + 1]
    cmp cl, '/'
    je @tok_comment
    cmp cl, '*'
    je @tok_comment                  ; Block comment also goes to end
    jmp @tok_operator

@tok_check_string:
    cmp al, '"'
    je @tok_string
    cmp al, 27h                     ; Single quote '
    je @tok_string

    ; Number: 0-9 or 0x prefix
    cmp al, '0'
    jb @tok_check_hash
    cmp al, '9'
    jbe @tok_number

@tok_check_hash:
    ; Preprocessor: #
    cmp al, '#'
    je @tok_preprocessor

    ; Identifier/keyword: A-Z, a-z, _, ., @
    cmp al, '_'
    je @tok_identifier
    cmp al, '@'
    je @tok_identifier
    cmp al, '.'
    je @tok_identifier
    cmp al, 'A'
    jb @tok_operator
    cmp al, 'Z'
    jbe @tok_identifier
    cmp al, 'a'
    jb @tok_operator
    cmp al, 'z'
    jbe @tok_identifier

    ; Fall through: operator or unknown punctuation
@tok_operator:
    ; Single-character operator token
    mov [rdi + TK_startCol], r14d
    mov dword ptr [rdi + TK_length], 1
    mov dword ptr [rdi + TK_tokenType], TOKEN_OPERATOR
    mov dword ptr [rdi + TK_color], 0
    add rdi, TK_SIZE
    inc ebx
    inc r14d
    jmp @tok_loop

; ---- Comment ----
@tok_comment:
    mov r15d, r14d                  ; Start position
    ; Rest of line is comment
    mov eax, r12d
    sub eax, r14d                   ; Length = remaining
    mov [rdi + TK_startCol], r15d
    mov [rdi + TK_length], eax
    mov dword ptr [rdi + TK_tokenType], TOKEN_COMMENT
    mov dword ptr [rdi + TK_color], 0
    add rdi, TK_SIZE
    inc ebx
    mov r14d, r12d                  ; Skip to end
    jmp @tok_done

; ---- String ----
@tok_string:
    mov r15d, r14d                  ; Start
    movzx ecx, byte ptr [rsi + r14] ; Quote character
    inc r14d                        ; Skip opening quote

@tok_string_loop:
    cmp r14d, r12d
    jae @tok_string_emit
    movzx eax, byte ptr [rsi + r14]
    inc r14d
    cmp al, '\'                     ; Backslash escape
    jne @tok_string_check_close
    cmp r14d, r12d                  ; Skip escaped char
    jae @tok_string_emit
    inc r14d
    jmp @tok_string_loop

@tok_string_check_close:
    cmp al, cl                      ; Matching close quote?
    jne @tok_string_loop

@tok_string_emit:
    mov [rdi + TK_startCol], r15d
    mov eax, r14d
    sub eax, r15d
    mov [rdi + TK_length], eax
    mov dword ptr [rdi + TK_tokenType], TOKEN_STRING
    mov dword ptr [rdi + TK_color], 0
    add rdi, TK_SIZE
    inc ebx
    jmp @tok_loop

; ---- Number ----
@tok_number:
    mov r15d, r14d                  ; Start

@tok_number_loop:
    inc r14d
    cmp r14d, r12d
    jae @tok_number_emit
    movzx eax, byte ptr [rsi + r14]
    ; Decimal digits
    cmp al, '0'
    jb @tok_number_check_hex
    cmp al, '9'
    jbe @tok_number_loop
@tok_number_check_hex:
    ; Hex digits A-F, a-f
    cmp al, 'A'
    jb @tok_number_check_suffix
    cmp al, 'F'
    jbe @tok_number_loop
    cmp al, 'a'
    jb @tok_number_check_suffix
    cmp al, 'f'
    jbe @tok_number_loop
@tok_number_check_suffix:
    ; Hex/bin suffixes: h, H, b, B, x, X, o, O
    cmp al, 'h'
    je @tok_number_inc
    cmp al, 'H'
    je @tok_number_inc
    cmp al, 'b'
    je @tok_number_inc
    cmp al, 'B'
    je @tok_number_inc
    cmp al, 'x'
    je @tok_number_inc
    cmp al, 'X'
    je @tok_number_inc
    cmp al, 'o'
    je @tok_number_inc
    cmp al, 'O'
    je @tok_number_inc
    ; Decimal point
    cmp al, '.'
    je @tok_number_inc
    jmp @tok_number_emit

@tok_number_inc:
    inc r14d
    jmp @tok_number_loop

@tok_number_emit:
    mov [rdi + TK_startCol], r15d
    mov eax, r14d
    sub eax, r15d
    mov [rdi + TK_length], eax
    mov dword ptr [rdi + TK_tokenType], TOKEN_NUMBER
    mov dword ptr [rdi + TK_color], 0
    add rdi, TK_SIZE
    inc ebx
    jmp @tok_loop

; ---- Preprocessor (#include, #define, etc.) ----
@tok_preprocessor:
    mov r15d, r14d

@tok_preproc_loop:
    inc r14d
    cmp r14d, r12d
    jae @tok_preproc_emit
    movzx eax, byte ptr [rsi + r14]
    ; Continue while alphanumeric or _
    cmp al, '_'
    je @tok_preproc_loop
    cmp al, 'A'
    jb @tok_preproc_emit
    cmp al, 'Z'
    jbe @tok_preproc_loop
    cmp al, 'a'
    jb @tok_preproc_emit
    cmp al, 'z'
    jbe @tok_preproc_loop

@tok_preproc_emit:
    mov [rdi + TK_startCol], r15d
    mov eax, r14d
    sub eax, r15d
    mov [rdi + TK_length], eax
    mov dword ptr [rdi + TK_tokenType], TOKEN_PREPROCESSOR
    mov dword ptr [rdi + TK_color], 0
    add rdi, TK_SIZE
    inc ebx
    jmp @tok_loop

; ---- Identifier (needs classification) ----
@tok_identifier:
    mov r15d, r14d                  ; Start

@tok_id_loop:
    inc r14d
    cmp r14d, r12d
    jae @tok_id_classify
    movzx eax, byte ptr [rsi + r14]
    cmp al, '_'
    je @tok_id_loop
    cmp al, '@'
    je @tok_id_loop
    cmp al, '.'
    je @tok_id_loop
    cmp al, '0'
    jb @tok_id_classify
    cmp al, '9'
    jbe @tok_id_loop
    cmp al, 'A'
    jb @tok_id_classify
    cmp al, 'Z'
    jbe @tok_id_loop
    cmp al, 'a'
    jb @tok_id_classify
    cmp al, 'z'
    jbe @tok_id_loop

@tok_id_classify:
    ; Save token start info
    mov [rdi + TK_startCol], r15d
    mov eax, r14d
    sub eax, r15d
    mov [rdi + TK_length], eax

    ; Default to identifier
    mov dword ptr [rdi + TK_tokenType], TOKEN_IDENTIFIER
    mov dword ptr [rdi + TK_color], 0

    ; Try to classify: register > instruction > directive > keyword > identifier
    ; We use the helper functions below
    ; Save state before calls
    mov [rsp + 0], rsi              ; Save line ptr
    mov [rsp + 8], rdi              ; Save token ptr
    mov [rsp + 16], r14             ; Save position

    ; MC_IsRegister(line, offset, len)
    mov rcx, rsi
    mov edx, r15d
    mov r8d, [rdi + TK_length]
    call MC_IsRegister
    test eax, eax
    jnz @tok_id_is_register

    ; MC_IsInstruction(line, offset, len)
    mov rsi, [rsp + 0]
    mov rcx, rsi
    mov rdi, [rsp + 8]
    mov edx, [rdi + TK_startCol]
    mov r8d, [rdi + TK_length]
    call MC_IsInstruction
    test eax, eax
    jnz @tok_id_is_instruction

    ; MC_IsDirective(line, offset, len)
    mov rsi, [rsp + 0]
    mov rcx, rsi
    mov rdi, [rsp + 8]
    mov edx, [rdi + TK_startCol]
    mov r8d, [rdi + TK_length]
    call MC_IsDirective
    test eax, eax
    jnz @tok_id_is_directive

    ; Stays as TOKEN_IDENTIFIER
    mov rsi, [rsp + 0]
    mov rdi, [rsp + 8]
    mov r14, [rsp + 16]
    jmp @tok_id_emit

@tok_id_is_register:
    mov rsi, [rsp + 0]
    mov rdi, [rsp + 8]
    mov r14, [rsp + 16]
    mov dword ptr [rdi + TK_tokenType], TOKEN_REGISTER
    jmp @tok_id_emit

@tok_id_is_instruction:
    mov rsi, [rsp + 0]
    mov rdi, [rsp + 8]
    mov r14, [rsp + 16]
    mov dword ptr [rdi + TK_tokenType], TOKEN_INSTRUCTION
    jmp @tok_id_emit

@tok_id_is_directive:
    mov rsi, [rsp + 0]
    mov rdi, [rsp + 8]
    mov r14, [rsp + 16]
    mov dword ptr [rdi + TK_tokenType], TOKEN_DIRECTIVE
    jmp @tok_id_emit

@tok_id_emit:
    add rdi, TK_SIZE
    inc ebx
    jmp @tok_loop

@tok_done:
    mov eax, ebx                    ; Return token count
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MC_TokenizeLine endp

; ============================================================================
; MC_IsRegister
; ============================================================================
; bool MC_IsRegister(const char* line, uint32_t offset, uint32_t len)
; RCX = line, EDX = offset, R8D = len
; Returns: 1 if token is a CPU register name, 0 otherwise
;
; Case-insensitive comparison against register table.
; ============================================================================
MC_IsRegister proc
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    ; Build pointer to token and length
    lea rsi, [rcx + rdx]           ; RSI = token start
    mov ecx, r8d                   ; ECX = token length

    ; Quick reject: registers are 2-5 chars
    cmp ecx, 2
    jb @isreg_no
    cmp ecx, 5
    ja @isreg_no

    ; Check 3-char registers first (most common: rax, rbx, etc.)
    cmp ecx, 3
    jne @isreg_check_2

    lea rdi, szRegisters_3
    jmp @isreg_scan

@isreg_check_2:
    cmp ecx, 2
    jne @isreg_check_extended
    lea rdi, szRegisters_2
    jmp @isreg_scan

@isreg_check_extended:
    ; 4-5 char registers (xmm0-15, ymm0-15, zmm0-15, r10d, etc.)
    ; Check prefix: xmm, ymm, zmm
    cmp ecx, 4
    jb @isreg_no
    movzx eax, byte ptr [rsi]
    or al, 20h                      ; Lowercase
    cmp al, 'x'
    je @isreg_check_xmm
    cmp al, 'y'
    je @isreg_check_xmm
    cmp al, 'z'
    je @isreg_check_xmm
    ; Check r10-r15 with suffix (r10d, r10w, r10b)
    cmp al, 'r'
    je @isreg_check_rNN
    ; Check e** with suffix (eax, etc. — already handled in 3-char)
    jmp @isreg_no

@isreg_check_xmm:
    ; Must be xmmN or xmmNN (mm at positions 1-2)
    movzx eax, byte ptr [rsi + 1]
    or al, 20h
    cmp al, 'm'
    jne @isreg_no
    movzx eax, byte ptr [rsi + 2]
    or al, 20h
    cmp al, 'm'
    jne @isreg_no
    ; Remaining chars must be digits
    mov eax, 3
@isreg_xmm_digit:
    cmp eax, ecx
    jae @isreg_yes
    movzx edx, byte ptr [rsi + rax]
    cmp dl, '0'
    jb @isreg_no
    cmp dl, '9'
    ja @isreg_no
    inc eax
    jmp @isreg_xmm_digit

@isreg_check_rNN:
    ; r + digit + optional suffix
    movzx eax, byte ptr [rsi + 1]
    cmp al, '0'
    jb @isreg_no
    cmp al, '9'
    ja @isreg_no
    ; Digits + optional d/w/b suffix
    mov eax, 1
@isreg_rnn_loop:
    inc eax
    cmp eax, ecx
    jae @isreg_yes
    movzx edx, byte ptr [rsi + rax]
    cmp dl, '0'
    jb @isreg_rnn_suffix
    cmp dl, '9'
    jbe @isreg_rnn_loop
@isreg_rnn_suffix:
    ; Must be last char and one of d, w, b
    mov edx, eax
    inc edx
    cmp edx, ecx
    jne @isreg_no
    movzx edx, byte ptr [rsi + rax]
    or dl, 20h
    cmp dl, 'd'
    je @isreg_yes
    cmp dl, 'w'
    je @isreg_yes
    cmp dl, 'b'
    je @isreg_yes
    jmp @isreg_no

@isreg_scan:
    ; Linear scan through null-terminated table
    ; RDI = table, RSI = token, ECX = token length
@isreg_scan_loop:
    movzx eax, byte ptr [rdi]
    test al, al
    jz @isreg_no                    ; Double-null = end of table

    ; Get length of current table entry
    push rdi
    xor edx, edx
@isreg_entry_len:
    movzx eax, byte ptr [rdi + rdx]
    test al, al
    jz @isreg_compare
    inc edx
    jmp @isreg_entry_len

@isreg_compare:
    ; EDX = entry length
    cmp edx, ecx
    jne @isreg_next_entry

    ; Case-insensitive compare
    pop rdi
    push rdi
    push rcx
    push rsi
    xor eax, eax
@isreg_cmp_loop:
    cmp eax, ecx
    jae @isreg_match
    movzx r8d, byte ptr [rsi + rax]
    movzx r9d, byte ptr [rdi + rax]
    or r8d, 20h                     ; Lowercase both
    or r9d, 20h
    cmp r8d, r9d
    jne @isreg_cmp_fail
    inc eax
    jmp @isreg_cmp_loop

@isreg_match:
    pop rsi
    pop rcx
    pop rdi
    jmp @isreg_yes

@isreg_cmp_fail:
    pop rsi
    pop rcx

@isreg_next_entry:
    pop rdi
    ; Skip to next entry (past null terminator)
    xor eax, eax
@isreg_skip:
    movzx edx, byte ptr [rdi]
    inc rdi
    test edx, edx
    jnz @isreg_skip
    jmp @isreg_scan_loop

@isreg_yes:
    mov eax, 1
    jmp @isreg_done
@isreg_no:
    xor eax, eax
@isreg_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
MC_IsRegister endp

; ============================================================================
; MC_IsInstruction
; ============================================================================
; bool MC_IsInstruction(const char* line, uint32_t offset, uint32_t len)
; Case-insensitive linear scan against instruction table.
; ============================================================================
MC_IsInstruction proc
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    lea rsi, [rcx + rdx]           ; Token pointer
    mov ecx, r8d                   ; Token length

    ; Quick reject: instructions are 2-16 chars
    cmp ecx, 2
    jb @isinst_no
    cmp ecx, 16
    ja @isinst_no

    lea rdi, szInstructions

@isinst_scan_loop:
    movzx eax, byte ptr [rdi]
    test al, al
    jz @isinst_no                   ; End of table

    ; Get entry length
    push rdi
    xor edx, edx
@isinst_elen:
    movzx eax, byte ptr [rdi + rdx]
    test al, al
    jz @isinst_compare
    inc edx
    jmp @isinst_elen

@isinst_compare:
    cmp edx, ecx
    jne @isinst_next

    ; Case-insensitive compare
    pop rdi
    push rdi
    xor eax, eax
@isinst_cmp:
    cmp eax, ecx
    jae @isinst_yes
    movzx r8d, byte ptr [rsi + rax]
    movzx r9d, byte ptr [rdi + rax]
    or r8d, 20h
    or r9d, 20h
    cmp r8d, r9d
    jne @isinst_next
    inc eax
    jmp @isinst_cmp

@isinst_next:
    pop rdi
@isinst_skip:
    movzx eax, byte ptr [rdi]
    inc rdi
    test eax, eax
    jnz @isinst_skip
    jmp @isinst_scan_loop

@isinst_yes:
    pop rdi
    mov eax, 1
    jmp @isinst_done
@isinst_no:
    xor eax, eax
@isinst_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
MC_IsInstruction endp

; ============================================================================
; MC_IsDirective
; ============================================================================
; bool MC_IsDirective(const char* line, uint32_t offset, uint32_t len)
; Same pattern as IsInstruction but against directives table.
; ============================================================================
MC_IsDirective proc
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    lea rsi, [rcx + rdx]
    mov ecx, r8d

    cmp ecx, 1
    jb @isdir_no
    cmp ecx, 16
    ja @isdir_no

    lea rdi, szDirectives

@isdir_scan_loop:
    movzx eax, byte ptr [rdi]
    test al, al
    jz @isdir_no

    push rdi
    xor edx, edx
@isdir_elen:
    movzx eax, byte ptr [rdi + rdx]
    test al, al
    jz @isdir_compare
    inc edx
    jmp @isdir_elen

@isdir_compare:
    cmp edx, ecx
    jne @isdir_next

    pop rdi
    push rdi
    xor eax, eax
@isdir_cmp:
    cmp eax, ecx
    jae @isdir_yes
    movzx r8d, byte ptr [rsi + rax]
    movzx r9d, byte ptr [rdi + rax]
    or r8d, 20h
    or r9d, 20h
    cmp r8d, r9d
    jne @isdir_next
    inc eax
    jmp @isdir_cmp

@isdir_next:
    pop rdi
@isdir_skip:
    movzx eax, byte ptr [rdi]
    inc rdi
    test eax, eax
    jnz @isdir_skip
    jmp @isdir_scan_loop

@isdir_yes:
    pop rdi
    mov eax, 1
    jmp @isdir_done
@isdir_no:
    xor eax, eax
@isdir_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
MC_IsDirective endp

; ============================================================================
; Export Table
; ============================================================================
public MC_GapBuffer_Init
public MC_GapBuffer_Destroy
public MC_GapBuffer_MoveGap
public MC_GapBuffer_Insert
public MC_GapBuffer_Delete
public MC_GapBuffer_GetLine
public MC_GapBuffer_Length
public MC_GapBuffer_LineCount
public MC_TokenizeLine
public MC_IsRegister
public MC_IsInstruction
public MC_IsDirective

end
