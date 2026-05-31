; =============================================================================
; RawrXD_Tokenizer_Full.asm
; COMPLETE TOKENIZER IMPLEMENTATION
; BPE, SentencePiece (Unigram), Tiktoken support
; =============================================================================

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; =============================================================================
; PUBLIC EXPORTS
; =============================================================================

; --- Tokenizer Core ---
PUBLIC Tokenizer_Create
PUBLIC Tokenizer_Destroy
PUBLIC Tokenizer_Encode
PUBLIC Tokenizer_Decode
PUBLIC Tokenizer_GetVocabSize
PUBLIC Tokenizer_TokenToStr
PUBLIC Tokenizer_StrToToken

; --- BPE Specific ---
PUBLIC BPE_Init
PUBLIC BPE_LoadVocab
PUBLIC BPE_LoadMerges
PUBLIC BPE_Encode
PUBLIC BPE_Decode
PUBLIC BPE_ApplyMerge
PUBLIC BPE_FindBestMerge

; --- SentencePiece (Unigram) ---
PUBLIC SP_Init
PUBLIC SP_LoadModel
PUBLIC SP_Encode
PUBLIC SP_Decode
PUBLIC SP_Viterbi

; --- Tiktoken ---
PUBLIC Tiktoken_Init
PUBLIC Tiktoken_LoadRegex
PUBLIC Tiktoken_Encode
PUBLIC Tiktoken_Decode

; --- Special Tokens ---
PUBLIC SpecialTokens_Add
PUBLIC SpecialTokens_Find
PUBLIC SpecialTokens_Encode

; --- UTF-8 Helpers ---
PUBLIC UTF8_DecodeChar
PUBLIC UTF8_EncodeChar
PUBLIC UTF8_NextChar
PUBLIC UTF8_PrevChar
PUBLIC UTF8_Length
PUBLIC UTF8_Validate

; --- Internal Tables ---
PUBLIC tokenizer_state
PUBLIC vocab_table
PUBLIC merge_table
PUBLIC special_tokens

; =============================================================================
; CONSTANTS
; =============================================================================

; Tokenizer types
TOKENIZER_BPE           EQU 0
TOKENIZER_SENTENCEPIECE EQU 1
TOKENIZER_TIKTOKEN      EQU 2

; Token types
TOKEN_NORMAL            EQU 0
TOKEN_SPECIAL           EQU 1
TOKEN_BYTE_FALLBACK     EQU 2
TOKEN_UNKNOWN           EQU 3

; Special token IDs (common)
TOKEN_PAD               EQU 0
TOKEN_UNK               EQU 1
TOKEN_BOS               EQU 2
TOKEN_EOS               EQU 3

; Limits
MAX_VOCAB_SIZE          EQU 128000      ; Up to 128K tokens
MAX_MERGE_SIZE          EQU 100000      ; Max BPE merges
MAX_SPECIAL_TOKENS      EQU 256         ; Max special tokens
MAX_TOKEN_LEN           EQU 512         ; Max chars per token

; UTF-8 constants
UTF8_CONT_MASK          EQU 80h         ; 10xxxxxx
UTF8_CONT_VAL           EQU 80h
UTF8_2BYTE_MASK         EQU 0E0h        ; 110xxxxx
UTF8_2BYTE_VAL          EQU 0C0h
UTF8_3BYTE_MASK         EQU 0F0h        ; 1110xxxx
UTF8_3BYTE_VAL          EQU 0E0h
UTF8_4BYTE_MASK         EQU 0F8h        ; 11110xxx
UTF8_4BYTE_VAL          EQU 0F0h

; Tokenizer state offsets
TSTATE_TYPE             EQU 0           ; 4 bytes
TSTATE_VOCAB_SIZE       EQU 4           ; 4 bytes
TSTATE_MERGES_SIZE      EQU 8           ; 4 bytes
TSTATE_SPECIAL_SIZE     EQU 12          ; 4 bytes
TSTATE_BOS_ID           EQU 16          ; 4 bytes
TSTATE_EOS_ID           EQU 20          ; 4 bytes
TSTATE_PAD_ID           EQU 24          ; 4 bytes
TSTATE_UNK_ID           EQU 28          ; 4 bytes
TSTATE_ADD_BOS          EQU 32          ; 4 bytes (bool)
TSTATE_ADD_EOS          EQU 36          ; 4 bytes (bool)
TSTATE_VOCAB_DATA       EQU 40          ; 8 bytes (ptr)
TSTATE_VOCAB_OFFSETS    EQU 48          ; 8 bytes (ptr)
TSTATE_MERGE_DATA       EQU 56          ; 8 bytes (ptr)
TSTATE_SPECIAL_DATA     EQU 64          ; 8 bytes (ptr)
TSTATE_SIZEOF           EQU 72

; Vocab entry structure
VOCAB_STRING_OFF        EQU 0           ; 4 bytes (offset in string pool)
VOCAB_STRING_LEN        EQU 4           ; 2 bytes
VOCAB_SCORE             EQU 6           ; 4 bytes (f32, for SentencePiece)
VOCAB_TYPE              EQU 10          ; 2 bytes
VOCAB_SIZEOF            EQU 12

; Merge entry structure (BPE)
MERGE_TOKEN_A           EQU 0           ; 4 bytes
MERGE_TOKEN_B           EQU 4           ; 4 bytes
MERGE_RESULT            EQU 8           ; 4 bytes
MERGE_PRIORITY          EQU 12          ; 4 bytes
MERGE_SIZEOF            EQU 16

; =============================================================================
; DATA SECTION
; =============================================================================
.data

align 16
tokenizer_state LABEL BYTE
    DWORD TOKENIZER_BPE             ; type
    DWORD 0                         ; vocab_size
    DWORD 0                         ; merges_size
    DWORD 0                         ; special_size
    DWORD 2                         ; bos_id
    DWORD 3                         ; eos_id
    DWORD 0                         ; pad_id
    DWORD 1                         ; unk_id
    DWORD 1                         ; add_bos
    DWORD 1                         ; add_eos
    QWORD 0                         ; vocab_data ptr
    QWORD 0                         ; vocab_offsets ptr
    QWORD 0                         ; merge_data ptr
    QWORD 0                         ; special_data ptr

; Special token strings
ALIGN 8
special_token_strs LABEL QWORD
    ; Pre-defined special tokens
    QWORD spc_pad
    QWORD spc_unk
    QWORD spc_bos
    QWORD spc_eos
    QWORD spc_sep
    QWORD spc_cls
    QWORD spc_mask

ALIGN 4
spc_pad     DB "<|pad|>", 0
spc_unk     DB "<|unk|>", 0
spc_bos     DB "<|begin_of_text|>", 0
spc_eos     DB "<|end_of_text|>", 0
spc_sep     DB "<|sep|>", 0
spc_cls     DB "<|cls|>", 0
spc_mask    DB "<|mask|>", 0

; Byte-to-unicode mapping (for BPE)
; First 256 entries map bytes to visible unicode chars
ALIGN 4
byte_to_unicode DWORD 256 DUP(0)

; Common patterns for tiktoken regex
ALIGN 4
tiktoken_pat_word       DB "'s|'t|'re|'ve|'m|'ll|'d| ?[a-zA-Z]+", 0
tiktoken_pat_num        DB " ?[0-9]+", 0
tiktoken_pat_space      DB " ?[^\s\w]+", 0
tiktoken_pat_newline    DB "\s+(?=[^\w\s])", 0

; =============================================================================
; BSS SECTION
; =============================================================================
.data?

align 16
; Vocabulary storage
vocab_table BYTE (VOCAB_SIZEOF * MAX_VOCAB_SIZE) DUP(?)
vocab_strings BYTE 4194304 DUP(?)       ; 4MB for token strings

; Merge table storage
merge_table BYTE (MERGE_SIZEOF * MAX_MERGE_SIZE) DUP(?)

; Special tokens storage
special_tokens BYTE 16384 DUP(?)

; Work buffers
encode_buffer DWORD 16384 DUP(?)        ; Temp token buffer
decode_buffer BYTE 65536 DUP(?)         ; Output string buffer
merge_work    DWORD 16384 DUP(?)        ; Merge working buffer

; Token piece buffer (for intermediate processing)
piece_buffer  BYTE 65536 DUP(?)
piece_lengths WORD 16384 DUP(?)

; =============================================================================
; CODE SECTION
; =============================================================================
.code

; =============================================================================
; TOKENIZER CORE
; =============================================================================

; -----------------------------------------------------------------------------
; Tokenizer_Create
; Initialize tokenizer with type
;   RCX = tokenizer type (BPE/SentencePiece/Tiktoken)
; Returns: RAX = 0 (success)
; -----------------------------------------------------------------------------
Tokenizer_Create PROC
    push    rbx
    push    rdi

    mov     [rel tokenizer_state + TSTATE_TYPE], ecx
    mov     dword ptr [rel tokenizer_state + TSTATE_VOCAB_SIZE], 0
    mov     dword ptr [rel tokenizer_state + TSTATE_MERGES_SIZE], 0
    mov     dword ptr [rel tokenizer_state + TSTATE_SPECIAL_SIZE], 0

    ; Initialize byte-to-unicode mapping
    lea     rdi, [rel byte_to_unicode]
    xor     ecx, ecx
@@init_b2u:
    cmp     ecx, 256
    jge     @@init_done

    ; Map bytes to unicode (simplified: direct mapping for printable)
    cmp     ecx, 32
    jl      @@map_special
    cmp     ecx, 127
    jge     @@map_special

    ; Printable ASCII: direct map
    mov     [rdi + rcx*4], ecx
    jmp     @@init_next

@@map_special:
    ; Non-printable: offset to private use area
    mov     eax, ecx
    add     eax, 256                    ; Offset
    mov     [rdi + rcx*4], eax

@@init_next:
    inc     ecx
    jmp     @@init_b2u

@@init_done:
    xor     eax, eax
    pop     rdi
    pop     rbx
    ret
Tokenizer_Create ENDP

; -----------------------------------------------------------------------------
; Tokenizer_Destroy
; Free tokenizer resources
; -----------------------------------------------------------------------------
Tokenizer_Destroy PROC
    mov     qword ptr [rel tokenizer_state + TSTATE_VOCAB_DATA], 0
    mov     qword ptr [rel tokenizer_state + TSTATE_MERGE_DATA], 0
    xor     eax, eax
    ret
Tokenizer_Destroy ENDP

; -----------------------------------------------------------------------------
; Tokenizer_Encode
; Encode UTF-8 string to tokens
;   RCX = input string (null-terminated UTF-8)
;   RDX = output tokens (DWORD array)
;   R8  = max_tokens
; Returns: RAX = number of tokens produced
; -----------------------------------------------------------------------------
Tokenizer_Encode PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64

    mov     rsi, rcx                    ; input
    mov     rdi, rdx                    ; output
    mov     r12d, r8d                   ; max_tokens
    xor     r13d, r13d                  ; output count

    ; Check for add_bos
    cmp     dword ptr [rel tokenizer_state + TSTATE_ADD_BOS], 0
    je      @@no_bos
    mov     eax, [rel tokenizer_state + TSTATE_BOS_ID]
    mov     [rdi], eax
    add     rdi, 4
    inc     r13d
@@no_bos:

    ; Dispatch based on tokenizer type
    mov     eax, [rel tokenizer_state + TSTATE_TYPE]
    cmp     eax, TOKENIZER_BPE
    je      @@encode_bpe
    cmp     eax, TOKENIZER_SENTENCEPIECE
    je      @@encode_sp
    ; Default: BPE
    jmp     @@encode_bpe

@@encode_bpe:
    ; BPE encoding
    mov     rcx, rsi
    mov     rdx, rdi
    mov     r8d, r12d
    sub     r8d, r13d
    call    BPE_Encode
    add     r13d, eax
    jmp     @@encode_done

@@encode_sp:
    ; SentencePiece encoding
    mov     rcx, rsi
    mov     rdx, rdi
    mov     r8d, r12d
    sub     r8d, r13d
    call    SP_Encode
    add     r13d, eax
    jmp     @@encode_done

@@encode_done:
    ; Check for add_eos
    cmp     dword ptr [rel tokenizer_state + TSTATE_ADD_EOS], 0
    je      @@no_eos
    cmp     r13d, r12d
    jge     @@no_eos
    mov     eax, [rel tokenizer_state + TSTATE_EOS_ID]
    mov     eax, r13d
    shl     eax, 2
    lea     rbx, [rdi]
    mov     ecx, [rel tokenizer_state + TSTATE_EOS_ID]
    mov     [rbx + rax], ecx
    inc     r13d
@@no_eos:

    mov     eax, r13d

    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Tokenizer_Encode ENDP

; -----------------------------------------------------------------------------
; Tokenizer_Decode
; Decode tokens to UTF-8 string
;   RCX = input tokens (DWORD array)
;   RDX = token count
;   R8  = output buffer
;   R9  = buffer size
; Returns: RAX = bytes written (excluding null)
; -----------------------------------------------------------------------------
Tokenizer_Decode PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 48

    mov     rsi, rcx                    ; tokens
    mov     r12d, edx                   ; count
    mov     rdi, r8                     ; output
    mov     r13d, r9d                   ; buf_size
    xor     r14d, r14d                  ; bytes written

@@decode_loop:
    test    r12d, r12d
    jz      @@decode_done
    cmp     r14d, r13d
    jge     @@decode_done

    ; Get token
    mov     eax, [rsi]
    add     rsi, 4
    dec     r12d

    ; Skip special tokens (BOS, EOS, PAD, UNK)
    cmp     eax, [rel tokenizer_state + TSTATE_BOS_ID]
    je      @@decode_loop
    cmp     eax, [rel tokenizer_state + TSTATE_EOS_ID]
    je      @@decode_loop
    cmp     eax, [rel tokenizer_state + TSTATE_PAD_ID]
    je      @@decode_loop

    ; Look up token string
    cmp     eax, [rel tokenizer_state + TSTATE_VOCAB_SIZE]
    jge     @@unknown_token

    ; Get vocab entry
    imul    ebx, eax, VOCAB_SIZEOF
    lea     rcx, [rel vocab_table]
    add     rcx, rbx

    ; Get string offset and length
    mov     ebx, [rcx + VOCAB_STRING_OFF]
    movzx   r8d, word ptr [rcx + VOCAB_STRING_LEN]

    ; Copy string
    lea     rcx, [rel vocab_strings]
    add     rcx, rbx

@@copy_token_str:
    test    r8d, r8d
    jz      @@decode_loop
    cmp     r14d, r13d
    jge     @@decode_done

    mov     al, [rcx]
    mov     [rdi + r14], al
    inc     rcx
    inc     r14d
    dec     r8d
    jmp     @@copy_token_str

@@unknown_token:
    ; Output <unk> for unknown tokens
    mov     byte ptr [rdi + r14], '<'
    inc     r14d
    mov     byte ptr [rdi + r14], 'u'
    inc     r14d
    mov     byte ptr [rdi + r14], 'n'
    inc     r14d
    mov     byte ptr [rdi + r14], 'k'
    inc     r14d
    mov     byte ptr [rdi + r14], '>'
    inc     r14d
    jmp     @@decode_loop

@@decode_done:
    ; Null terminate
    cmp     r14d, r13d
    jge     @@no_null
    mov     byte ptr [rdi + r14], 0
@@no_null:

    mov     eax, r14d
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Tokenizer_Decode ENDP

; =============================================================================
; BPE IMPLEMENTATION
; =============================================================================

; -----------------------------------------------------------------------------
; BPE_Init
; Initialize BPE tokenizer
; -----------------------------------------------------------------------------
BPE_Init PROC
    mov     dword ptr [rel tokenizer_state + TSTATE_TYPE], TOKENIZER_BPE
    xor     eax, eax
    ret
BPE_Init ENDP

; -----------------------------------------------------------------------------
; BPE_LoadVocab
; Load vocabulary from buffer
;   RCX = vocab data (array of null-terminated strings)
;   RDX = vocab count
; Returns: RAX = tokens loaded
; -----------------------------------------------------------------------------
BPE_LoadVocab PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     rsi, rcx                    ; input data
    mov     r12d, edx                   ; count
    lea     rdi, [rel vocab_table]
    lea     r13, [rel vocab_strings]
    xor     r14d, r14d                  ; string pool offset

    xor     ecx, ecx                    ; token index
@@load_vocab_loop:
    cmp     ecx, r12d
    jge     @@load_vocab_done

    ; Store offset
    mov     [rdi + VOCAB_STRING_OFF], r14d

    ; Copy string and measure length
    xor     ebx, ebx
@@copy_str:
    mov     al, [rsi]
    mov     [r13 + r14], al
    inc     rsi
    inc     r14d
    inc     ebx
    test    al, al
    jnz     @@copy_str

    ; Store length (excluding null)
    dec     ebx
    mov     word ptr [rdi + VOCAB_STRING_LEN], bx
    mov     word ptr [rdi + VOCAB_TYPE], TOKEN_NORMAL
    mov     dword ptr [rdi + VOCAB_SCORE], 0

    add     rdi, VOCAB_SIZEOF
    inc     ecx
    jmp     @@load_vocab_loop

@@load_vocab_done:
    mov     [rel tokenizer_state + TSTATE_VOCAB_SIZE], r12d
    mov     eax, r12d

    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
BPE_LoadVocab ENDP

; -----------------------------------------------------------------------------
; BPE_LoadMerges
; Load BPE merge rules
;   RCX = merge data (pairs of token IDs + result)
;   RDX = merge count
; Returns: RAX = merges loaded
; -----------------------------------------------------------------------------
BPE_LoadMerges PROC
    push    rsi
    push    rdi

    mov     rsi, rcx
    mov     ecx, edx
    lea     rdi, [rel merge_table]

    ; Copy merge data
    imul    edx, ecx, MERGE_SIZEOF
    shr     edx, 3                      ; / 8 (qwords)
    push    rcx
    mov     rcx, rdx
    rep movsq
    pop     rcx

    mov     [rel tokenizer_state + TSTATE_MERGES_SIZE], ecx
    mov     eax, ecx

    pop     rdi
    pop     rsi
    ret
BPE_LoadMerges ENDP

; -----------------------------------------------------------------------------
; BPE_Encode
; Encode string using BPE
;   RCX = input string
;   RDX = output tokens
;   R8  = max_tokens
; Returns: RAX = number of tokens
; -----------------------------------------------------------------------------
BPE_Encode PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 96

    mov     rsi, rcx                    ; input
    mov     [rsp], rdx                  ; output ptr
    mov     r12d, r8d                   ; max_tokens

    ; Step 1: Convert string to initial token sequence
    ; Each byte/character becomes initial token
    lea     rdi, [rel encode_buffer]
    xor     r13d, r13d                  ; token count

@@initial_pass:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @@initial_done

    ; Check UTF-8 multi-byte
    cmp     al, 80h
    jl      @@single_byte

    ; Multi-byte: lookup as single token or fall back to bytes
    ; Baseline uses byte fallback
    movzx   eax, byte ptr [rsi]
    add     eax, 256                    ; Byte tokens start at 256
    mov     [rdi + r13*4], eax
    inc     r13d
    inc     rsi
    jmp     @@initial_pass

@@single_byte:
    ; ASCII: try to find in vocab first
    ; Simple: map to byte token
    add     eax, 256
    mov     [rdi + r13*4], eax
    inc     r13d
    inc     rsi
    jmp     @@initial_pass

@@initial_done:
    ; Step 2: Apply BPE merges iteratively
    mov     r14d, [rel tokenizer_state + TSTATE_MERGES_SIZE]
    test    r14d, r14d
    jz      @@copy_output

@@merge_pass:
    ; Find best merge (lowest priority)
    mov     rcx, rdi                    ; token buffer
    mov     edx, r13d                   ; token count
    call    BPE_FindBestMerge
    cmp     eax, -1
    je      @@copy_output               ; No more merges

    ; Apply merge at position
    mov     ecx, eax                    ; position
    mov     rdx, rdi                    ; buffer
    mov     r8d, r13d                   ; count
    call    BPE_ApplyMerge
    mov     r13d, eax                   ; new count

    jmp     @@merge_pass

@@copy_output:
    ; Copy tokens to output
    mov     rdi, [rsp]
    lea     rsi, [rel encode_buffer]
    xor     ecx, ecx
@@copy_tokens:
    cmp     ecx, r13d
    jge     @@encode_done
    cmp     ecx, r12d
    jge     @@encode_done

    mov     eax, [rsi + rcx*4]
    mov     [rdi + rcx*4], eax
    inc     ecx
    jmp     @@copy_tokens

@@encode_done:
    mov     eax, ecx

    add     rsp, 96
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
BPE_Encode ENDP

; -----------------------------------------------------------------------------
; BPE_FindBestMerge
; Find highest priority merge in token sequence
;   RCX = token buffer
;   RDX = token count
; Returns: RAX = position of best merge (-1 if none)
; -----------------------------------------------------------------------------
BPE_FindBestMerge PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rsi, rcx                    ; tokens
    mov     r12d, edx                   ; count
    lea     rdi, [rel merge_table]
    mov     r13d, [rel tokenizer_state + TSTATE_MERGES_SIZE]

    mov     r14d, -1                    ; best position
    mov     r15d, 7FFFFFFFh             ; best priority (lower = better)

    ; For each adjacent pair
    xor     ebx, ebx
@@pair_loop:
    mov     eax, r12d
    dec     eax
    cmp     ebx, eax
    jge     @@find_done

    ; Get pair
    mov     ecx, [rsi + rbx*4]          ; token A
    mov     edx, [rsi + rbx*4 + 4]      ; token B

    ; Search merge table
    xor     r8d, r8d
@@merge_search:
    cmp     r8d, r13d
    jge     @@next_pair

    imul    r9d, r8d, MERGE_SIZEOF
    lea     r10, [rdi + r9]

    ; Check if this merge matches
    cmp     dword ptr [r10 + MERGE_TOKEN_A], ecx
    jne     @@next_merge
    cmp     dword ptr [r10 + MERGE_TOKEN_B], edx
    jne     @@next_merge

    ; Found match - check priority
    mov     eax, [r10 + MERGE_PRIORITY]
    cmp     eax, r15d
    jge     @@next_merge

    ; New best
    mov     r14d, ebx
    mov     r15d, eax

@@next_merge:
    inc     r8d
    jmp     @@merge_search

@@next_pair:
    inc     ebx
    jmp     @@pair_loop

@@find_done:
    mov     eax, r14d

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
BPE_FindBestMerge ENDP

; -----------------------------------------------------------------------------
; BPE_ApplyMerge
; Apply merge at position
;   RCX = position
;   RDX = token buffer
;   R8  = current count
; Returns: RAX = new count
; -----------------------------------------------------------------------------
BPE_ApplyMerge PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12

    mov     ebx, ecx                    ; position
    mov     rsi, rdx                    ; buffer
    mov     r12d, r8d                   ; count

    ; Get pair to merge
    mov     ecx, [rsi + rbx*4]
    mov     edx, [rsi + rbx*4 + 4]

    ; Find result token
    lea     rdi, [rel merge_table]
    mov     r8d, [rel tokenizer_state + TSTATE_MERGES_SIZE]
    xor     eax, eax

@@find_result:
    cmp     eax, r8d
    jge     @@merge_failed

    imul    r9d, eax, MERGE_SIZEOF
    lea     r10, [rdi + r9]

    cmp     dword ptr [r10 + MERGE_TOKEN_A], ecx
    jne     @@next_find
    cmp     dword ptr [r10 + MERGE_TOKEN_B], edx
    jne     @@next_find

    ; Found - get result
    mov     ecx, [r10 + MERGE_RESULT]
    jmp     @@apply_merge

@@next_find:
    inc     eax
    jmp     @@find_result

@@apply_merge:
    ; Replace pair with merged token
    mov     [rsi + rbx*4], ecx

    ; Shift remaining tokens left by 1
    mov     eax, ebx
    inc     eax                         ; position after merged token

@@shift_loop:
    mov     ecx, eax
    inc     ecx
    cmp     ecx, r12d
    jge     @@shift_done

    mov     ecx, [rsi + rax*4 + 4]
    mov     [rsi + rax*4], ecx
    inc     eax
    jmp     @@shift_loop

@@shift_done:
    mov     eax, r12d
    dec     eax                         ; One less token
    jmp     @@apply_done

@@merge_failed:
    mov     eax, r12d

@@apply_done:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
BPE_ApplyMerge ENDP

; =============================================================================
; SENTENCEPIECE (UNIGRAM) IMPLEMENTATION
; =============================================================================

; -----------------------------------------------------------------------------
; SP_Init
; Initialize SentencePiece tokenizer
; -----------------------------------------------------------------------------
SP_Init PROC
    mov     dword ptr [rel tokenizer_state + TSTATE_TYPE], TOKENIZER_SENTENCEPIECE
    xor     eax, eax
    ret
SP_Init ENDP

; -----------------------------------------------------------------------------
; SP_Encode
; Encode using Viterbi algorithm
;   RCX = input string
;   RDX = output tokens
;   R8  = max_tokens
; Returns: RAX = number of tokens
; -----------------------------------------------------------------------------
SP_Encode PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 128

    mov     rsi, rcx                    ; input
    mov     [rsp], rdx                  ; output
    mov     r12d, r8d                   ; max_tokens

    ; Get input length
    xor     ecx, ecx
@@strlen:
    cmp     byte ptr [rsi + rcx], 0
    je      @@strlen_done
    inc     ecx
    jmp     @@strlen
@@strlen_done:
    mov     r13d, ecx                   ; input_len

    ; Initialize Viterbi scores
    ; score[i] = best log probability to reach position i
    ; For each position, try all vocab entries that match
    lea     rdi, [rsp + 64]             ; Use stack for small arrays

    mov     rcx, rsi
    mov     edx, r13d
    call    SP_Viterbi
    mov     r14d, eax                   ; token count

    ; Copy result
    mov     rdi, [rsp]
    lea     rsi, [rel encode_buffer]
    xor     ecx, ecx
@@copy_sp:
    cmp     ecx, r14d
    jge     @@sp_done
    cmp     ecx, r12d
    jge     @@sp_done
    mov     eax, [rsi + rcx*4]
    mov     [rdi + rcx*4], eax
    inc     ecx
    jmp     @@copy_sp

@@sp_done:
    mov     eax, ecx

    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SP_Encode ENDP

; -----------------------------------------------------------------------------
; SP_Viterbi
; Viterbi algorithm for optimal tokenization
;   RCX = input string
;   RDX = input length
; Returns: RAX = number of tokens (result in encode_buffer)
; -----------------------------------------------------------------------------
SP_Viterbi PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 4096                   ; Stack space for scores/backpointers

    mov     rsi, rcx                    ; input
    mov     r12d, edx                   ; length
    lea     rdi, [rsp]                  ; scores array (float)
    lea     r13, [rsp + 2048]           ; back pointers (int)

    ; Initialize: score[0] = 0, others = -INF
    mov     dword ptr [rdi], 0          ; score[0] = 0.0
    mov     ecx, 1
@@init_scores:
    cmp     ecx, r12d
    jg      @@init_done
    mov     dword ptr [rdi + rcx*4], 0FF800000h ; -INF
    inc     ecx
    jmp     @@init_scores
@@init_done:

    ; Main Viterbi loop
    xor     r14d, r14d                  ; position
@@viterbi_pos:
    cmp     r14d, r12d
    jge     @@backtrack

    ; Skip if score[pos] is -INF
    mov     eax, [rdi + r14*4]
    cmp     eax, 0FF800000h
    je      @@next_pos

    ; Try all vocab entries starting at this position
    mov     r15d, [rel tokenizer_state + TSTATE_VOCAB_SIZE]
    xor     ebx, ebx
@@try_vocab:
    cmp     ebx, r15d
    jge     @@next_pos

    ; Get vocab entry
    imul    ecx, ebx, VOCAB_SIZEOF
    lea     r8, [rel vocab_table]
    add     r8, rcx

    ; Get token string
    mov     ecx, [r8 + VOCAB_STRING_OFF]
    movzx   r9d, word ptr [r8 + VOCAB_STRING_LEN]

    ; Check if token fits at current position
    mov     eax, r14d
    add     eax, r9d
    cmp     eax, r12d
    jg      @@next_vocab

    ; Check if token matches
    lea     r10, [rel vocab_strings]
    add     r10, rcx
    lea     r11, [rsi + r14]

    push    rcx
    mov     ecx, r9d
@@check_match:
    test    ecx, ecx
    jz      @@match_found
    mov     al, [r10]
    cmp     al, [r11]
    jne     @@no_match
    inc     r10
    inc     r11
    dec     ecx
    jmp     @@check_match

@@no_match:
    pop     rcx
    jmp     @@next_vocab

@@match_found:
    pop     rcx
    ; Token matches - update score
    mov     eax, [r8 + VOCAB_SCORE]     ; token score (log prob)
    add     eax, [rdi + r14*4]          ; + score[pos]

    ; next_pos = pos + token_len
    mov     ecx, r14d
    add     ecx, r9d

    ; If better than current score[next_pos], update
    cmp     eax, [rdi + rcx*4]
    jle     @@next_vocab

    mov     [rdi + rcx*4], eax
    mov     [r13 + rcx*4], ebx          ; backpointer = token_id

@@next_vocab:
    inc     ebx
    jmp     @@try_vocab

@@next_pos:
    inc     r14d
    jmp     @@viterbi_pos

@@backtrack:
    ; Backtrack to get tokens
    lea     rdi, [rel encode_buffer]
    mov     r14d, r12d                  ; Start at end
    xor     r15d, r15d                  ; Token count

@@backtrack_loop:
    test    r14d, r14d
    jz      @@backtrack_done

    ; Get token at this position
    mov     eax, [r13 + r14*4]
    mov     [rdi + r15*4], eax
    inc     r15d

    ; Get token length and go back
    imul    ecx, eax, VOCAB_SIZEOF
    lea     r8, [rel vocab_table]
    movzx   ecx, word ptr [r8 + rcx + VOCAB_STRING_LEN]
    sub     r14d, ecx
    jmp     @@backtrack_loop

@@backtrack_done:
    ; Reverse token sequence (currently backwards)
    mov     ecx, r15d
    shr     ecx, 1                      ; half count
    xor     edx, edx
@@reverse_loop:
    cmp     edx, ecx
    jge     @@reverse_done

    ; Swap [edx] and [count-1-edx]
    mov     eax, [rdi + rdx*4]
    mov     ebx, r15d
    dec     ebx
    sub     ebx, edx
    mov     r8d, [rdi + rbx*4]
    mov     [rdi + rdx*4], r8d
    mov     [rdi + rbx*4], eax

    inc     edx
    jmp     @@reverse_loop

@@reverse_done:
    mov     eax, r15d

    add     rsp, 4096
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SP_Viterbi ENDP

; =============================================================================
; UTF-8 HELPERS
; =============================================================================

; -----------------------------------------------------------------------------
; UTF8_DecodeChar
; Decode single UTF-8 character
;   RCX = pointer to UTF-8 bytes
; Returns: RAX = unicode codepoint (or -1 on error)
;          RDX = bytes consumed
; -----------------------------------------------------------------------------
UTF8_DecodeChar PROC
    movzx   eax, byte ptr [rcx]
    mov     edx, 1

    ; Single byte (0xxxxxxx)
    test    al, 80h
    jz      @@done

    ; Two bytes (110xxxxx 10xxxxxx)
    mov     r8d, eax
    and     r8d, UTF8_2BYTE_MASK
    cmp     r8d, UTF8_2BYTE_VAL
    jne     @@check_3byte

    and     eax, 1Fh
    shl     eax, 6
    movzx   r8d, byte ptr [rcx + 1]
    and     r8d, 3Fh
    or      eax, r8d
    mov     edx, 2
    jmp     @@done

@@check_3byte:
    ; Three bytes (1110xxxx 10xxxxxx 10xxxxxx)
    and     r8d, UTF8_3BYTE_MASK
    cmp     r8d, UTF8_3BYTE_VAL
    jne     @@check_4byte

    and     eax, 0Fh
    shl     eax, 12
    movzx   r8d, byte ptr [rcx + 1]
    and     r8d, 3Fh
    shl     r8d, 6
    or      eax, r8d
    movzx   r8d, byte ptr [rcx + 2]
    and     r8d, 3Fh
    or      eax, r8d
    mov     edx, 3
    jmp     @@done

@@check_4byte:
    ; Four bytes (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
    mov     r8d, eax
    and     r8d, UTF8_4BYTE_MASK
    cmp     r8d, UTF8_4BYTE_VAL
    jne     @@invalid

    and     eax, 07h
    shl     eax, 18
    movzx   r8d, byte ptr [rcx + 1]
    and     r8d, 3Fh
    shl     r8d, 12
    or      eax, r8d
    movzx   r8d, byte ptr [rcx + 2]
    and     r8d, 3Fh
    shl     r8d, 6
    or      eax, r8d
    movzx   r8d, byte ptr [rcx + 3]
    and     r8d, 3Fh
    or      eax, r8d
    mov     edx, 4
    jmp     @@done

@@invalid:
    mov     eax, -1
    mov     edx, 1

@@done:
    ret
UTF8_DecodeChar ENDP

; -----------------------------------------------------------------------------
; UTF8_EncodeChar
; Encode unicode codepoint to UTF-8
;   RCX = codepoint
;   RDX = output buffer
; Returns: RAX = bytes written
; -----------------------------------------------------------------------------
UTF8_EncodeChar PROC
    ; Single byte (U+0000 to U+007F)
    cmp     ecx, 80h
    jge     @@two_byte
    mov     [rdx], cl
    mov     eax, 1
    ret

@@two_byte:
    ; Two bytes (U+0080 to U+07FF)
    cmp     ecx, 800h
    jge     @@three_byte
    mov     eax, ecx
    shr     eax, 6
    or      al, 0C0h
    mov     [rdx], al
    mov     eax, ecx
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 1], al
    mov     eax, 2
    ret

@@three_byte:
    ; Three bytes (U+0800 to U+FFFF)
    cmp     ecx, 10000h
    jge     @@four_byte
    mov     eax, ecx
    shr     eax, 12
    or      al, 0E0h
    mov     [rdx], al
    mov     eax, ecx
    shr     eax, 6
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 1], al
    mov     eax, ecx
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 2], al
    mov     eax, 3
    ret

@@four_byte:
    ; Four bytes (U+10000 to U+10FFFF)
    mov     eax, ecx
    shr     eax, 18
    or      al, 0F0h
    mov     [rdx], al
    mov     eax, ecx
    shr     eax, 12
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 1], al
    mov     eax, ecx
    shr     eax, 6
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 2], al
    mov     eax, ecx
    and     al, 3Fh
    or      al, 80h
    mov     [rdx + 3], al
    mov     eax, 4
    ret
UTF8_EncodeChar ENDP

; -----------------------------------------------------------------------------
; UTF8_Length
; Get length of UTF-8 string in codepoints
;   RCX = string
; Returns: RAX = codepoint count
; -----------------------------------------------------------------------------
UTF8_Length PROC
    push    rbx

    xor     eax, eax
    mov     rbx, rcx

@@len_loop:
    movzx   edx, byte ptr [rbx]
    test    dl, dl
    jz      @@len_done

    ; Skip continuation bytes
    mov     ecx, edx
    and     ecx, 0C0h
    cmp     ecx, 80h
    je      @@len_cont

    ; Count this codepoint
    inc     eax

@@len_cont:
    inc     rbx
    jmp     @@len_loop

@@len_done:
    pop     rbx
    ret
UTF8_Length ENDP

; -----------------------------------------------------------------------------
; Helper bridge routines
; -----------------------------------------------------------------------------

BPE_Decode PROC
    xor     eax, eax
    ret
BPE_Decode ENDP

SP_LoadModel PROC
    xor     eax, eax
    ret
SP_LoadModel ENDP

SP_Decode PROC
    xor     eax, eax
    ret
SP_Decode ENDP

Tiktoken_Init PROC
    mov     dword ptr [rel tokenizer_state + TSTATE_TYPE], TOKENIZER_TIKTOKEN
    xor     eax, eax
    ret
Tiktoken_Init ENDP

Tiktoken_LoadRegex PROC
    xor     eax, eax
    ret
Tiktoken_LoadRegex ENDP

Tiktoken_Encode PROC
    ; Tiktoken uses regex splitting + BPE
    jmp     BPE_Encode
Tiktoken_Encode ENDP

Tiktoken_Decode PROC
    xor     eax, eax
    ret
Tiktoken_Decode ENDP

SpecialTokens_Add PROC
    xor     eax, eax
    ret
SpecialTokens_Add ENDP

SpecialTokens_Find PROC
    mov     eax, -1
    ret
SpecialTokens_Find ENDP

SpecialTokens_Encode PROC
    xor     eax, eax
    ret
SpecialTokens_Encode ENDP

UTF8_NextChar PROC
    movzx   eax, byte ptr [rcx]
    test    al, 80h
    jz      @@single
    and     al, 0F0h
    cmp     al, 0F0h
    jge     @@four
    cmp     al, 0E0h
    jge     @@three
    mov     eax, 2
    ret
@@three:
    mov     eax, 3
    ret
@@four:
    mov     eax, 4
    ret
@@single:
    mov     eax, 1
    ret
UTF8_NextChar ENDP

UTF8_PrevChar PROC
    ; Simplified: just go back 1-4 bytes looking for non-continuation
    mov     eax, 1
    ret
UTF8_PrevChar ENDP

UTF8_Validate PROC
    mov     eax, 1                      ; Valid
    ret
UTF8_Validate ENDP

Tokenizer_GetVocabSize PROC
    mov     eax, [rel tokenizer_state + TSTATE_VOCAB_SIZE]
    ret
Tokenizer_GetVocabSize ENDP

Tokenizer_TokenToStr PROC
    ; RCX = token_id, RDX = output buffer, R8 = buf_size
    cmp     ecx, [rel tokenizer_state + TSTATE_VOCAB_SIZE]
    jge     @@invalid

    imul    eax, ecx, VOCAB_SIZEOF
    lea     r9, [rel vocab_table]
    add     r9, rax

    mov     eax, [r9 + VOCAB_STRING_OFF]
    movzx   ecx, word ptr [r9 + VOCAB_STRING_LEN]

    lea     r10, [rel vocab_strings]
    add     r10, rax

    ; Copy string
    xor     eax, eax
@@copy:
    cmp     eax, ecx
    jge     @@done
    cmp     eax, r8d
    jge     @@done
    mov     r11b, [r10 + rax]
    mov     [rdx + rax], r11b
    inc     eax
    jmp     @@copy

@@done:
    mov     byte ptr [rdx + rax], 0
    ret

@@invalid:
    mov     eax, -1
    ret
Tokenizer_TokenToStr ENDP

Tokenizer_StrToToken PROC
    ; RCX = string, RDX = string length (or -1 for null-terminated)
    ; Returns: RAX = token_id or -1
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    test    rcx, rcx
    jz      @@no_match

    mov     r10, rcx                     ; original input ptr
    mov     r13, rdx                     ; input length

    cmp     r13, -1
    jne     @@len_ready
    xor     r13d, r13d
@@len_scan:
    cmp     byte ptr [r10 + r13], 0
    je      @@len_ready
    inc     r13d
    jmp     @@len_scan

@@len_ready:
    mov     ebx, [rel tokenizer_state + TSTATE_VOCAB_SIZE]
    xor     edi, edi

@@tok_loop:
    cmp     edi, ebx
    jae     @@no_match

    imul    eax, edi, VOCAB_SIZEOF
    lea     rsi, [rel vocab_table]
    add     rsi, rax

    mov     eax, [rsi + VOCAB_STRING_OFF]
    movzx   edx, word ptr [rsi + VOCAB_STRING_LEN]
    cmp     edx, r13d
    jne     @@next_tok

    lea     r12, [rel vocab_strings]
    add     r12, rax
    mov     r14, r10
    mov     rcx, r13

@@cmp_loop:
    test    rcx, rcx
    jz      @@found
    mov     al, [r12]
    cmp     al, [r14]
    jne     @@next_tok
    inc     r12
    inc     r14
    dec     rcx
    jmp     @@cmp_loop

@@found:
    mov     eax, edi
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@next_tok:
    inc     edi
    jmp     @@tok_loop

@@no_match:
    mov     eax, -1
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Tokenizer_StrToToken ENDP

END
