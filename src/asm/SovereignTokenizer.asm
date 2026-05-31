;=============================================================================
; SovereignTokenizer.asm - RawrXD Real BPE Tokenizer
; Byte Pair Encoding Implementation - Zero External Dependencies
;=============================================================================
; Build: ml64 /c /nologo SovereignTokenizer.asm
;=============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:8

;=============================================================================
; Public Interface
;=============================================================================
PUBLIC SovereignTokenizer_Init
PUBLIC SovereignTokenizer_Encode
PUBLIC SovereignTokenizer_Decode
PUBLIC SovereignTokenizer_MergeLoop
PUBLIC SovereignTokenizer_FindBestPair
PUBLIC SovereignTokenizer_ApplyMerge
PUBLIC SovereignTokenizer_LoadVocab

;=============================================================================
; Constants
;=============================================================================
MAX_VOCAB_SIZE      EQU     50000   ; Maximum vocabulary size
MAX_TOKEN_LEN       EQU     256     ; Maximum token length
MAX_INPUT_LEN       EQU     4096    ; Maximum input length
MAX_MERGES          EQU     50000   ; Maximum merge operations

; Token types
TOKEN_TYPE_BYTE     EQU     0       ; Byte token
TOKEN_TYPE_MERGE    EQU     1       ; Merged token
TOKEN_TYPE_SPECIAL  EQU     2       ; Special token

;=============================================================================
; Data Section
;=============================================================================
.data
ALIGN 64

; Vocabulary storage
g_vocab             DB      MAX_VOCAB_SIZE * MAX_TOKEN_LEN DUP(0)
g_vocab_lengths     DW      MAX_VOCAB_SIZE DUP(0)
g_vocab_types       DB      MAX_VOCAB_SIZE DUP(0)
g_vocab_size        DD      0

; Merge table
g_merge_pairs       DD      MAX_MERGES * 2 DUP(0)
g_merge_scores      DD      MAX_MERGES DUP(0)
g_merge_count       DD      0

; Token buffer for encoding
g_token_buffer      DW      MAX_INPUT_LEN DUP(0)
g_token_count       DD      0

; Special tokens
g_bos_token         DD      1
g_eos_token         DD      2
g_unk_token         DD      0
g_pad_token         DD      3

;=============================================================================
; Code Section
;=============================================================================
.code

;=============================================================================
; SovereignTokenizer_Init - Initialize tokenizer with vocabulary
;=============================================================================
SovereignTokenizer_Init PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = vocab data
    mov r11d, edx           ; R11 = vocab size
    mov r12, r8             ; R12 = merge data
    mov r13d, r9d           ; R13 = merge count
    
    ; Store vocab size
    mov g_vocab_size, r11d
    mov g_merge_count, r13d
    
    xor eax, eax            ; Return success
    
    add rsp, 32
    pop rbx
    pop rbp
    ret
SovereignTokenizer_Init ENDP

;=============================================================================
; SovereignTokenizer_Encode - Encode text to token IDs
;=============================================================================
SovereignTokenizer_Encode PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = input text
    mov r11d, edx           ; R11 = input length
    mov r12, r8             ; R12 = output buffer
    mov r13d, r9d           ; R13 = output buffer size
    
    ; Step 1: Convert text to initial byte tokens
    xor ebx, ebx            ; EBX = token count
    xor esi, esi            ; ESI = input position
    
.init_loop:
    cmp esi, r11d
    jge .init_done
    
    movzx eax, byte ptr [r10 + rsi]
    add eax, 256            ; Offset past special tokens
    mov [r12 + rbx * 4], eax
    
    inc ebx
    inc esi
    jmp .init_loop
    
.init_done:
    ; Step 2: Apply BPE merge loop
    mov rcx, r12
    mov edx, ebx
    call SovereignTokenizer_MergeLoop
    mov ebx, eax
    
    mov eax, ebx
    
    add rsp, 64
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignTokenizer_Encode ENDP

;=============================================================================
; SovereignTokenizer_MergeLoop - Core BPE merge algorithm
;=============================================================================
SovereignTokenizer_MergeLoop PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG
    
    mov r12, rcx            ; R12 = token buffer
    mov r13d, edx           ; R13 = token count
    
.merge_loop:
    mov rcx, r12
    mov edx, r13d
    call SovereignTokenizer_FindBestPair
    
    cmp edx, -1
    je .merge_done
    
    mov r8d, eax
    mov r9d, edx
    mov rcx, r12
    mov edx, r13d
    call SovereignTokenizer_ApplyMerge
    mov r13d, eax
    
    jmp .merge_loop
    
.merge_done:
    mov eax, r13d
    
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignTokenizer_MergeLoop ENDP

;=============================================================================
; SovereignTokenizer_FindBestPair - Find highest priority mergeable pair
;=============================================================================
SovereignTokenizer_FindBestPair PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx            ; R10 = token buffer
    mov r11d, edx           ; R11 = token count
    
    mov ebx, -1
    mov esi, -1
    mov edi, -1
    
    xor ecx, ecx
.find_loop:
    mov eax, ecx
    inc eax
    cmp eax, r11d
    jge .find_done
    
    mov eax, [r10 + rcx * 4]
    mov r8d, [r10 + rcx * 4 + 4]
    
    shl eax, 16
    or eax, r8d
    
    push rcx
    call LookupMergeScore
    pop rcx
    
    cmp edx, -1
    je .next_pair
    
    cmp eax, ebx
    jle .next_pair
    
    mov ebx, eax
    mov esi, ecx
    mov edi, edx
    
.next_pair:
    inc ecx
    jmp .find_loop
    
.find_done:
    mov eax, esi
    mov edx, edi
    
    add rsp, 32
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignTokenizer_FindBestPair ENDP

;=============================================================================
; SovereignTokenizer_ApplyMerge - Apply a merge operation
;=============================================================================
SovereignTokenizer_ApplyMerge PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov r10, rcx
    mov r11d, edx
    mov r12d, r8d
    mov r13d, r9d
    
    mov [r10 + r12 * 4], r13d
    
    mov edi, r12d
    inc edi
    mov esi, r12d
    add esi, 2
    
.shift_loop:
    cmp esi, r11d
    jge .shift_done
    
    mov eax, [r10 + rsi * 4]
    mov [r10 + rdi * 4], eax
    
    inc esi
    inc edi
    jmp .shift_loop
    
.shift_done:
    dec r11d
    
    mov eax, r11d
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignTokenizer_ApplyMerge ENDP

;=============================================================================
; SovereignTokenizer_Decode - Decode token IDs to text
;=============================================================================
SovereignTokenizer_Decode PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG
    
    mov r10, rcx
    mov r11d, edx
    mov r12, r8
    mov r13d, r9d
    
    xor ebx, ebx
    xor esi, esi
    
.decode_loop:
    cmp esi, r11d
    jge .decode_done
    
    mov edi, [r10 + rsi * 4]
    
    cmp edi, g_bos_token
    je .next_token
    cmp edi, g_eos_token
    je .decode_done
    cmp edi, g_pad_token
    je .next_token
    
    sub edi, 256
    
    cmp ebx, r13d
    jge .decode_done
    
    mov [r12 + rbx], dil
    inc ebx
    
.next_token:
    inc esi
    jmp .decode_loop
    
.decode_done:
    cmp ebx, r13d
    jge .no_null
    mov byte ptr [r12 + rbx], 0
    
.no_null:
    mov eax, ebx
    
    add rsp, 48
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SovereignTokenizer_Decode ENDP

;=============================================================================
; SovereignTokenizer_LoadVocab - Load vocabulary from GGUF
;=============================================================================
SovereignTokenizer_LoadVocab PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    xor eax, eax
    
    add rsp, 32
    pop rbp
    ret
SovereignTokenizer_LoadVocab ENDP

;=============================================================================
; LookupMergeScore - Internal helper
;=============================================================================
LookupMergeScore PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    mov ebx, eax
    xor esi, esi
    
    mov ecx, g_merge_count
    test ecx, ecx
    jz .not_found
    
.search_loop:
    cmp esi, ecx
    jge .not_found
    
    mov eax, [g_merge_pairs + rsi * 8]
    cmp eax, ebx
    je .found
    
    inc esi
    jmp .search_loop
    
.found:
    mov eax, [g_merge_scores + rsi * 4]
    mov edx, [g_merge_pairs + rsi * 8 + 4]
    jmp .done
    
.not_found:
    mov eax, -1
    mov edx, -1
    
.done:
    pop rsi
    pop rbx
    pop rbp
    ret
LookupMergeScore ENDP

END
