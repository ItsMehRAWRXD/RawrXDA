; ═══════════════════════════════════════════════════════════════════
; inference_router.asm — Phase A: Sovereign Inference Router
;
; Unified text→text inference gateway. Routes between:
;   BACKEND_LOCAL  (0) — Native RunInference + TokenGenerate
;   BACKEND_OLLAMA (1) — OllamaClient_Generate2 (HTTP REST)
;   BACKEND_AUTO   (2) — Try LOCAL first, fallback to OLLAMA
;
; Closes the critical loop:
;   type → tokenize → infer → display
;
; Exports:
;   InferenceRouter_Init       — probe backends, populate vocab
;   InferenceRouter_Generate   — text in → text out (same ABI as Generate2)
;   InferenceRouter_Abort      — cancel pending inference
;   InferenceRouter_SetBackend — select backend (0/1/2)
;   InferenceRouter_GetStats   — telemetry (reqs, tokens, latency)
;   InferenceRouter_LoadVocab  — populate vocab table from GGUF base
;   g_routerBackend            — current backend selector
;   g_routerAbort              — abort flag (set by UI on keystroke)
;
; ABI: All procs use Windows x64 calling convention.
;      Shadow space + FRAME directives on every leaf that calls.
; ═══════════════════════════════════════════════════════════════════

PUBLIC InferenceRouter_Init
PUBLIC InferenceRouter_Generate
PUBLIC InferenceRouter_Abort
PUBLIC InferenceRouter_SetBackend
PUBLIC InferenceRouter_GetStats
PUBLIC InferenceRouter_LoadVocab
PUBLIC g_routerBackend
PUBLIC g_routerReady
PUBLIC g_routerAbort

; ── Inference engine imports (inference.asm) ─────────────────────
EXTERN RunInference:PROC
EXTERN TokenGenerate:PROC
EXTERN g_modelbase:QWORD
EXTERN g_kv_len:QWORD
EXTERN g_hasAVX512:DWORD
EXTERN ClearKVCache:PROC

; ── Ollama client imports (ollama_client.asm) ────────────────────
EXTERN OllamaClient_Generate2:PROC
EXTERN OllamaClient_IsConnected:PROC
EXTERN OllamaClient_Abort:PROC

; ── Win32 imports ────────────────────────────────────────────────
EXTERN GetTickCount64:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

; ── Beacon telemetry ─────────────────────────────────────────────
EXTERN BeaconSend:PROC

; ── Constants ────────────────────────────────────────────────────
BACKEND_LOCAL       equ 0
BACKEND_OLLAMA      equ 1
BACKEND_AUTO        equ 2

ROUTER_BEACON_SLOT  equ 15
ROUTER_EVT_LOCAL    equ 0C1h
ROUTER_EVT_OLLAMA   equ 0C2h
ROUTER_EVT_FALLBACK equ 0C3h
ROUTER_EVT_ABORT    equ 0C4h
ROUTER_EVT_BACKEND_CHANGED equ 0C5h

; SetBackend API-level IDs (external interface)
SETBE_AUTO          equ 0            ; Auto-detect best backend
SETBE_LOCAL_GGUF    equ 1            ; Force local GGUF model
SETBE_OLLAMA        equ 2            ; Force Ollama HTTP backend
SETBE_CPU_FALLBACK  equ 3            ; CPU-only pattern matcher
SETBE_MAX           equ 3            ; Highest valid API ID

; Internal backend constant (extends BACKEND_LOCAL/OLLAMA/AUTO)
BACKEND_CPU_FALLBACK equ 3

; Capability flags (bitfield in g_routerCapFlags)
CAP_KV_CACHE        equ 1            ; Backend supports KV cache
CAP_STREAMING       equ 2            ; Backend supports streaming
CAP_LIMITED         equ 4            ; Backend has limited capability

; Stats output structure size (bytes)
STATS_STRUCT_SIZE   equ 88

; Token generation limits
MAX_GEN_TOKENS      equ 128          ; Max tokens per suggestion
MIN_GEN_TOKENS      equ 4            ; Minimum useful suggestion length
MAX_OUT_BYTES       equ 2048         ; Max UTF-8 output bytes

; Vocab table: maps token_id → UTF-8 string (up to 16 bytes each)
VOCAB_ENTRY_SIZE    equ 16           ; 15 bytes text + 1 byte length
VOCAB_MAX_ENTRIES   equ 32000        ; Match g_modelVocab
VOCAB_TABLE_SIZE    equ (VOCAB_MAX_ENTRIES * VOCAB_ENTRY_SIZE) ; 512KB

; VirtualAlloc constants
MEM_COMMIT_R        equ 1000h
MEM_RESERVE_R       equ 2000h
MEM_RELEASE_R       equ 8000h
PAGE_RW_R           equ 4

; Inference context struct for RunInference
; +0  numLayers   DWORD
; +4  pad         DWORD
; +8  reserved    QWORD
INFER_CTX_SIZE      equ 16

.data
align 8
; Backend selection
g_routerBackend     dd  BACKEND_AUTO  ; Default: auto-detect
g_routerReady       dd  0            ; 1 = router initialized
g_routerAbort       dd  0            ; 1 = abort current generation

; Capabilities detected at init
g_hasLocalModel     dd  0            ; 1 = GGUF model loaded in g_modelbase
g_hasOllama         dd  0            ; 1 = Ollama server reachable
g_vocabLoaded       dd  0            ; 1 = vocab table populated

; Telemetry
g_routerReqs        dq  0            ; Total requests
g_routerTokensGen   dq  0            ; Total tokens generated
g_routerLastMs      dq  0            ; Last request latency (ms)
g_routerLocalHits   dq  0            ; Times local engine was used
g_routerOllamaHits  dq  0            ; Times Ollama was used
g_routerAborts      dq  0            ; Times aborted

; Extended telemetry / backend state
g_routerInitTick    dq  0            ; Tick64 when backend was set/initialized
g_routerErrorCount  dd  0            ; Total error count
g_routerLastError   dd  0            ; Last error code (0=none)
g_routerQueueDepth  dd  0            ; Current request queue depth
g_routerActiveReq   dd  0            ; 1 = inference request in flight
g_routerCapFlags    dd  0            ; Capability bitfield for current backend
g_routerPeakTPS     dd  0            ; Peak tokens/sec (16.16 fixed-point)
g_routerMemUsed     dq  0            ; Memory used by inference (bytes)
g_routerModelSize   dq  0            ; Model file size in bytes
g_routerPatternInit dd  0            ; CPU fallback pattern matcher initialized

; Vocab table pointer (VirtualAlloc'd — 512KB)
g_vocabTable        dq  0            ; Pointer to vocab entries
g_vocabCount        dd  0            ; Number of populated entries

.data?
align 16
; Token buffer for local inference (RunInference writes token IDs here)
g_tokenOutBuf       dd  MAX_GEN_TOKENS dup(?)

; Inference context struct
g_inferCtx          db  INFER_CTX_SIZE dup(?)

; Scratch for prompt tokenization (byte-level: 1 byte = 1 token)
g_promptTokens      dd  4096 dup(?)  ; Token ID buffer for input prompt


.code

; ════════════════════════════════════════════════════════════════════
; InferenceRouter_Init — Probe available backends, allocate vocab
;   No args. Returns: EAX = 0 success, -1 failure
;
;   Called after InferenceEngineInit + OllamaClient_Init in bootstrap.
;   Detects:
;     1. Is a model mapped in g_modelbase? → g_hasLocalModel = 1
;     2. Is Ollama reachable? → g_hasOllama = 1
;     3. Allocates vocab table (512KB) for token→text decode
; ════════════════════════════════════════════════════════════════════
InferenceRouter_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; 1. Check local model
    mov     rax, g_modelbase
    test    rax, rax
    jz      @iri_no_local
    ; Verify it's not just the KV cache base — check if it points beyond cache
    ; (Simple heuristic: if g_modelbase != 0 and points to valid mapped memory)
    mov     g_hasLocalModel, 1
    jmp     @iri_check_ollama
@iri_no_local:
    mov     g_hasLocalModel, 0

@iri_check_ollama:
    ; 2. Check Ollama connection
    call    OllamaClient_IsConnected
    mov     g_hasOllama, eax

    ; 3. Allocate vocab table (512KB via VirtualAlloc)
    xor     ecx, ecx                     ; lpAddress = NULL
    mov     edx, VOCAB_TABLE_SIZE        ; 512KB
    mov     r8d, MEM_COMMIT_R or MEM_RESERVE_R
    mov     r9d, PAGE_RW_R
    call    VirtualAlloc
    test    rax, rax
    jz      @iri_no_vocab
    mov     g_vocabTable, rax

    ; If local model available, try loading vocab from GGUF
    cmp     g_hasLocalModel, 1
    jne     @iri_build_ascii
    call    InferenceRouter_LoadVocab
    test    eax, eax
    jz      @iri_vocab_done

@iri_build_ascii:
    ; Fallback: build ASCII byte-level vocab (token 0-255 → single byte)
    mov     rdi, g_vocabTable
    xor     ecx, ecx                     ; token ID = 0
@iri_ascii_loop:
    cmp     ecx, 256
    jge     @iri_ascii_done
    ; Entry layout: [0..14] = UTF-8 text, [15] = length
    imul    eax, ecx, VOCAB_ENTRY_SIZE
    mov     byte ptr [rdi + rax], cl     ; single byte = token ID
    mov     byte ptr [rdi + rax + 15], 1 ; length = 1
    inc     ecx
    jmp     @iri_ascii_loop
@iri_ascii_done:
    mov     g_vocabCount, 256
    mov     g_vocabLoaded, 1

@iri_vocab_done:
@iri_no_vocab:
    ; 4. Set up inference context struct
    lea     rax, g_inferCtx
    mov     dword ptr [rax], 0           ; numLayers = 0 (use model default)
    mov     dword ptr [rax+4], 0         ; pad
    mov     qword ptr [rax+8], 0         ; reserved

    mov     g_routerReady, 1
    mov     g_routerAbort, 0

    ; Beacon: report initialization
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_LOCAL
    mov     r8d, g_hasLocalModel
    call    BeaconSend

    xor     eax, eax
    add     rsp, 30h
    pop     rbx
    ret
InferenceRouter_Init ENDP


; ════════════════════════════════════════════════════════════════════
; InferenceRouter_LoadVocab — Extract vocab from GGUF model metadata
;   No args. Returns: EAX = 0 on success, -1 if no vocab found
;
;   Scans GGUF metadata for `tokenizer.ggml.tokens` string array.
;   GGUF format:
;     [magic(4)][version(4)][tensor_count(8)][metadata_kv_count(8)]
;     Then metadata_kv_count entries:
;       [key_len(8)][key_data(key_len)][value_type(4)][value_data(...)]
;   We scan for the key "tokenizer.ggml.tokens" (type=8=ARRAY, subtype=8=STRING)
; ════════════════════════════════════════════════════════════════════
InferenceRouter_LoadVocab PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rsi, g_modelbase
    test    rsi, rsi
    jz      @lv_fail

    ; Validate GGUF magic
    cmp     dword ptr [rsi], 046554747h  ; 'GGUF'
    jne     @lv_fail

    ; Read header
    mov     ebx, dword ptr [rsi + 4]     ; version
    mov     r12, qword ptr [rsi + 8]     ; tensor_count
    mov     r13, qword ptr [rsi + 10h]   ; metadata_kv_count

    ; Start scanning metadata at offset 0x18
    lea     r14, [rsi + 18h]             ; cursor into metadata
    xor     r15d, r15d                   ; metadata entry index

@lv_scan:
    cmp     r15, r13
    jge     @lv_fail                     ; Scanned all, not found

    ; Read key_len (u64)
    mov     rcx, qword ptr [r14]
    add     r14, 8

    ; Check if key matches "tokenizer.ggml.tokens" (23 chars)
    cmp     rcx, 23
    jne     @lv_skip_key

    ; Compare key data
    ; "tokenizer.ggml.tokens"
    cmp     byte ptr [r14],    't'
    jne     @lv_skip_key
    cmp     byte ptr [r14+1],  'o'
    jne     @lv_skip_key
    cmp     byte ptr [r14+2],  'k'
    jne     @lv_skip_key
    cmp     byte ptr [r14+3],  'e'
    jne     @lv_skip_key
    cmp     byte ptr [r14+4],  'n'
    jne     @lv_skip_key
    cmp     byte ptr [r14+9],  '.'
    jne     @lv_skip_key
    cmp     byte ptr [r14+14], '.'
    jne     @lv_skip_key
    cmp     byte ptr [r14+15], 't'
    jne     @lv_skip_key
    cmp     byte ptr [r14+22], 's'
    jne     @lv_skip_key

    ; Found the key! Advance past key data
    add     r14, rcx

    ; Read value_type (u32) — should be 9 (GGUF_TYPE_ARRAY)
    mov     eax, dword ptr [r14]
    add     r14, 4
    cmp     eax, 9
    jne     @lv_fail

    ; Array header: [subtype(u32)][count(u64)]
    mov     eax, dword ptr [r14]         ; subtype — should be 8 (STRING)
    add     r14, 4
    cmp     eax, 8
    jne     @lv_fail

    mov     r12, qword ptr [r14]         ; count = number of vocab entries
    add     r14, 8

    ; Clamp to VOCAB_MAX_ENTRIES
    cmp     r12, VOCAB_MAX_ENTRIES
    jbe     @lv_count_ok
    mov     r12, VOCAB_MAX_ENTRIES
@lv_count_ok:

    ; Now read r12 string entries into vocab table
    mov     rdi, g_vocabTable
    xor     ecx, ecx                     ; entry index

@lv_read_entry:
    cmp     rcx, r12
    jge     @lv_read_done

    ; Check abort
    cmp     g_routerAbort, 1
    je      @lv_fail

    ; Each STRING in GGUF: [len(u64)][data(len bytes)]
    mov     rax, qword ptr [r14]         ; string length
    add     r14, 8

    ; Compute output offset
    push    rcx
    imul    ecx, ecx, VOCAB_ENTRY_SIZE
    add     rcx, rdi                     ; dest = vocabTable + idx*16

    ; Copy min(len, 15) bytes
    mov     rdx, rax
    cmp     rdx, 15
    jbe     @lv_len_ok
    mov     rdx, 15
@lv_len_ok:
    mov     byte ptr [rcx + 15], dl      ; store length

    ; Copy string data
    push    rsi
    mov     rsi, r14
    push    rcx
    mov     rcx, rdx
    rep     movsb
    pop     rcx
    pop     rsi

    ; Advance past full string in GGUF
    add     r14, rax

    pop     rcx
    inc     rcx
    jmp     @lv_read_entry

@lv_read_done:
    mov     eax, r12d
    mov     g_vocabCount, eax
    mov     g_vocabLoaded, 1
    xor     eax, eax
    jmp     @lv_ret

@lv_skip_key:
    ; Advance past key data
    add     r14, rcx

    ; Read and skip value
    mov     eax, dword ptr [r14]         ; value_type
    add     r14, 4
    ; Skip value based on type (simplified: handle common types)
    cmp     eax, 0                       ; UINT8
    je      @lv_skip_1
    cmp     eax, 1                       ; INT8
    je      @lv_skip_1
    cmp     eax, 2                       ; UINT16
    je      @lv_skip_2
    cmp     eax, 3                       ; INT16
    je      @lv_skip_2
    cmp     eax, 4                       ; UINT32
    je      @lv_skip_4
    cmp     eax, 5                       ; INT32
    je      @lv_skip_4
    cmp     eax, 6                       ; FLOAT32
    je      @lv_skip_4
    cmp     eax, 7                       ; BOOL
    je      @lv_skip_1
    cmp     eax, 8                       ; STRING
    je      @lv_skip_string
    cmp     eax, 9                       ; ARRAY
    je      @lv_skip_array
    cmp     eax, 10                      ; UINT64
    je      @lv_skip_8
    cmp     eax, 11                      ; INT64
    je      @lv_skip_8
    cmp     eax, 12                      ; FLOAT64
    je      @lv_skip_8
    ; Unknown type — bail
    jmp     @lv_fail

@lv_skip_1:
    add     r14, 1
    jmp     @lv_skip_next
@lv_skip_2:
    add     r14, 2
    jmp     @lv_skip_next
@lv_skip_4:
    add     r14, 4
    jmp     @lv_skip_next
@lv_skip_8:
    add     r14, 8
    jmp     @lv_skip_next
@lv_skip_string:
    ; String: [len(u64)][data(len)]
    mov     rax, qword ptr [r14]
    add     r14, 8
    add     r14, rax
    jmp     @lv_skip_next
@lv_skip_array:
    ; Array: [subtype(u32)][count(u64)][data...]
    ; Skip over the array by reading subtype and count, then iterating elements.
    mov     eax, dword ptr [r14]         ; subtype
    add     r14, 4
    mov     rbx, qword ptr [r14]         ; element count
    add     r14, 8
    test    rbx, rbx
    jz      @lv_skip_next

    ; Determine element size from subtype
    cmp     eax, 0                       ; UINT8
    je      @lv_skip_arr_1
    cmp     eax, 1                       ; INT8
    je      @lv_skip_arr_1
    cmp     eax, 7                       ; BOOL
    je      @lv_skip_arr_1
    cmp     eax, 2                       ; UINT16
    je      @lv_skip_arr_2
    cmp     eax, 3                       ; INT16
    je      @lv_skip_arr_2
    cmp     eax, 4                       ; UINT32
    je      @lv_skip_arr_4
    cmp     eax, 5                       ; INT32
    je      @lv_skip_arr_4
    cmp     eax, 6                       ; FLOAT32
    je      @lv_skip_arr_4
    cmp     eax, 10                      ; UINT64
    je      @lv_skip_arr_8
    cmp     eax, 11                      ; INT64
    je      @lv_skip_arr_8
    cmp     eax, 12                      ; FLOAT64
    je      @lv_skip_arr_8
    cmp     eax, 8                       ; STRING array
    je      @lv_skip_arr_string
    ; Nested or unknown array subtype — bail
    jmp     @lv_fail

@lv_skip_arr_1:
    ; Fixed-size 1-byte elements: advance by count * 1
    add     r14, rbx
    jmp     @lv_skip_next

@lv_skip_arr_2:
    ; Fixed-size 2-byte elements: advance by count * 2
    shl     rbx, 1
    add     r14, rbx
    jmp     @lv_skip_next

@lv_skip_arr_4:
    ; Fixed-size 4-byte elements: advance by count * 4
    shl     rbx, 2
    add     r14, rbx
    jmp     @lv_skip_next

@lv_skip_arr_8:
    ; Fixed-size 8-byte elements: advance by count * 8
    shl     rbx, 3
    add     r14, rbx
    jmp     @lv_skip_next

@lv_skip_arr_string:
    ; String array: each element is [len(u64)][data(len)]
    test    rbx, rbx
    jz      @lv_skip_next
@lv_skip_arr_string_loop:
    mov     rax, qword ptr [r14]         ; string length
    add     r14, 8
    add     r14, rax                     ; skip string data
    dec     rbx
    jnz     @lv_skip_arr_string_loop
    jmp     @lv_skip_next

@lv_skip_next:
    inc     r15
    jmp     @lv_scan

@lv_fail:
    mov     eax, -1
@lv_ret:
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
InferenceRouter_LoadVocab ENDP


; ════════════════════════════════════════════════════════════════════
; InferenceRouter_Generate — Text in → text out, routed to best backend
;   RCX = pPrompt    (char* UTF-8 prompt, null-terminated)
;   RDX = pOutBuf    (char* output buffer for generated text)
;   R8D = outBufSize (max bytes to write)
;   Returns: EAX = bytes written, or -1 on error / abort
;
;   Same ABI as OllamaClient_Generate2 — drop-in replacement.
; ════════════════════════════════════════════════════════════════════
InferenceRouter_Generate PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 58h
    .allocstack 58h
    .endprolog

    ; Save args
    mov     [rsp+50h], rcx              ; pPrompt
    mov     [rsp+48h], rdx              ; pOutBuf
    mov     [rsp+40h], r8               ; outBufSize (as qword)

    ; Clear abort flag
    mov     g_routerAbort, 0

    ; Record start time
    call    GetTickCount64
    mov     r15, rax                     ; startTick

    ; Increment request counter
    inc     g_routerReqs

    ; Route based on backend selection
    mov     eax, g_routerBackend
    cmp     eax, BACKEND_LOCAL
    je      @rg_try_local
    cmp     eax, BACKEND_OLLAMA
    je      @rg_try_ollama

    ; BACKEND_AUTO: try local first, then Ollama
@rg_auto:
    cmp     g_hasLocalModel, 1
    je      @rg_try_local
    cmp     g_hasOllama, 1
    je      @rg_try_ollama
    jmp     @rg_fail                     ; No backend available

; ── Local Inference Path ──────────────────────────────────────────
@rg_try_local:
    cmp     g_hasLocalModel, 0
    je      @rg_local_fallback           ; Model not loaded

    ; Check abort before starting
    cmp     g_routerAbort, 1
    je      @rg_aborted

    ; Beacon: local inference starting
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_LOCAL
    xor     r8d, r8d
    call    BeaconSend

    ; ── Tokenize input prompt (byte-level encoding) ──
    ; For MVP: each byte of UTF-8 prompt becomes one token ID
    ; Real implementation would use BPE merges from vocab table
    mov     rsi, [rsp+50h]               ; pPrompt
    lea     rdi, g_promptTokens
    xor     ecx, ecx                     ; token count

@rg_tokenize:
    cmp     g_routerAbort, 1
    je      @rg_aborted
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @rg_tokenize_done
    cmp     ecx, 4096                    ; max prompt tokens
    jge     @rg_tokenize_done
    mov     dword ptr [rdi + rcx*4], eax ; token_id = byte value
    inc     ecx
    inc     rsi
    jmp     @rg_tokenize

@rg_tokenize_done:
    mov     r12d, ecx                    ; r12d = prompt token count

    ; ── Feed tokens through RunInference ──
    ; Clear KV cache for fresh context
    call    ClearKVCache

    ; Set up inference context
    lea     rcx, g_inferCtx              ; pContext
    mov     edx, MAX_GEN_TOKENS          ; tokenCount
    lea     r8, g_tokenOutBuf            ; pOutputBuf
    call    RunInference

    ; Check abort after inference
    cmp     g_routerAbort, 1
    je      @rg_aborted

    ; ── Detokenize output tokens → UTF-8 text ──
    mov     rdi, [rsp+48h]               ; pOutBuf
    mov     r13d, dword ptr [rsp+40h]    ; outBufSize
    lea     rsi, g_tokenOutBuf
    xor     ebx, ebx                     ; output byte offset
    xor     r14d, r14d                   ; token index

@rg_detokenize:
    cmp     r14d, MAX_GEN_TOKENS
    jge     @rg_detok_done
    cmp     ebx, r13d
    jge     @rg_detok_done               ; output buffer full
    cmp     g_routerAbort, 1
    je      @rg_aborted

    ; Get token ID
    mov     eax, dword ptr [rsi + r14*4]
    test    eax, eax
    jz      @rg_detok_done               ; Stop on token 0 (typically EOS/PAD)

    ; BOS/EOS check: skip token IDs 1-2 (common special tokens)
    cmp     eax, 3
    jl      @rg_detok_next

    ; Look up vocab table
    cmp     g_vocabLoaded, 0
    je      @rg_detok_byte               ; No vocab → byte fallback

    mov     rcx, g_vocabTable
    test    rcx, rcx
    jz      @rg_detok_byte

    ; Bounds check
    cmp     eax, g_vocabCount
    jge     @rg_detok_byte

    ; Entry = vocabTable + tokenID * VOCAB_ENTRY_SIZE
    imul    eax, eax, VOCAB_ENTRY_SIZE
    add     rcx, rax
    movzx   edx, byte ptr [rcx + 15]    ; string length
    test    edx, edx
    jz      @rg_detok_next               ; Empty entry

    ; Check remaining space
    lea     eax, [ebx + edx]
    cmp     eax, r13d
    jg      @rg_detok_done               ; Would overflow

    ; Copy vocab entry bytes to output
    push    rsi
    push    rdi
    mov     rsi, rcx                     ; source = vocab entry text
    add     rdi, rbx                     ; dest = outBuf + offset
    movzx   ecx, dl                      ; count
    rep     movsb
    pop     rdi
    pop     rsi

    add     ebx, edx
    jmp     @rg_detok_next

@rg_detok_byte:
    ; Byte-level fallback: token ID → single ASCII byte
    cmp     eax, 256
    jge     @rg_detok_next               ; Skip OOV tokens
    mov     byte ptr [rdi + rbx], al
    inc     ebx

@rg_detok_next:
    inc     r14d
    ; Increment global token counter
    inc     g_routerTokensGen
    jmp     @rg_detokenize

@rg_detok_done:
    ; Null-terminate output
    mov     byte ptr [rdi + rbx], 0

    inc     g_routerLocalHits

    ; Record latency
    call    GetTickCount64
    sub     rax, r15
    mov     g_routerLastMs, rax

    mov     eax, ebx                     ; Return bytes written
    jmp     @rg_ret

; ── Local fallback → try Ollama ───────────────────────────────────
@rg_local_fallback:
    cmp     g_routerBackend, BACKEND_LOCAL
    je      @rg_fail                     ; Explicitly local but model not loaded
    ; Fall through to Ollama for AUTO mode

; ── Ollama Path ───────────────────────────────────────────────────
@rg_try_ollama:
    cmp     g_hasOllama, 0
    je      @rg_fail                     ; Ollama not available

    cmp     g_routerAbort, 1
    je      @rg_aborted

    ; Beacon: Ollama dispatch
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_OLLAMA
    xor     r8d, r8d
    call    BeaconSend

    ; Forward directly to OllamaClient_Generate2 (same calling convention)
    mov     rcx, [rsp+50h]               ; pPrompt
    mov     rdx, [rsp+48h]               ; pOutBuf
    mov     r8d, dword ptr [rsp+40h]     ; outBufSize
    call    OllamaClient_Generate2

    ; EAX already has bytes written or -1
    cmp     eax, -1
    je      @rg_ollama_fail

    inc     g_routerOllamaHits

    ; Record latency
    push    rax
    call    GetTickCount64
    sub     rax, r15
    mov     g_routerLastMs, rax
    pop     rax

    jmp     @rg_ret

@rg_ollama_fail:
    ; Ollama failed — if we're in AUTO mode and haven't tried local...
    cmp     g_routerBackend, BACKEND_AUTO
    jne     @rg_fail
    cmp     g_hasLocalModel, 1
    jne     @rg_fail

    ; Beacon: fallback event
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_FALLBACK
    xor     r8d, r8d
    call    BeaconSend
    jmp     @rg_try_local

; ── Abort ─────────────────────────────────────────────────────────
@rg_aborted:
    inc     g_routerAborts
    ; Beacon: abort
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_ABORT
    xor     r8d, r8d
    call    BeaconSend
    mov     eax, -1
    jmp     @rg_ret

; ── Failure ───────────────────────────────────────────────────────
@rg_fail:
    mov     eax, -1

@rg_ret:
    add     rsp, 58h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
InferenceRouter_Generate ENDP


; ════════════════════════════════════════════════════════════════════
; InferenceRouter_Abort — Signal cancellation of current inference
;   No args. Returns: nothing (void)
;   Thread-safe: UI thread sets this, worker thread checks it.
; ════════════════════════════════════════════════════════════════════
InferenceRouter_Abort PROC
    mov     g_routerAbort, 1
    ; Also forward to Ollama abort if it's running
    call    OllamaClient_Abort
    ret
InferenceRouter_Abort ENDP


; ════════════════════════════════════════════════════════════════════
; InferenceRouter_SetBackend — Select inference backend (full impl)
;   ECX = API backend ID:
;         0 = AUTO        — probe local → Ollama → CPU fallback
;         1 = LOCAL_GGUF  — force native GGUF model inference
;         2 = OLLAMA      — force Ollama HTTP REST backend
;         3 = CPU_FALLBACK — CPU-only pattern matcher (limited)
;   Returns: EAX = 0 on success, -1 if requested backend unavailable
;
;   Maps API IDs to internal BACKEND_xxx constants.
;   Updates g_routerCapFlags, initializes backend-specific state,
;   logs transition timestamp, fires BACKEND_CHANGED beacon.
; ════════════════════════════════════════════════════════════════════
InferenceRouter_SetBackend PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     ebx, ecx                     ; save API backend ID

    ; ── Validate range [0..SETBE_MAX] ──
    cmp     ecx, SETBE_MAX
    ja      @rsb_invalid

    ; ── Dispatch by API backend ID ──
    cmp     ebx, SETBE_AUTO
    je      @rsb_do_auto
    cmp     ebx, SETBE_LOCAL_GGUF
    je      @rsb_do_local
    cmp     ebx, SETBE_OLLAMA
    je      @rsb_do_ollama
    cmp     ebx, SETBE_CPU_FALLBACK
    je      @rsb_do_cpu
    jmp     @rsb_invalid                 ; defensive

    ; ────────────────────────────────────────────────────────────────
    ; AUTO: probe local model → Ollama → CPU fallback
    ; ────────────────────────────────────────────────────────────────
@rsb_do_auto:
    ; 1) Probe local: check g_modelbase != 0
    mov     rax, g_modelbase
    test    rax, rax
    jnz     @rsb_auto_local

    ; 2) Check Ollama connectivity
    call    OllamaClient_IsConnected
    test    eax, eax
    jnz     @rsb_auto_ollama

    ; 3) Fall back to CPU
    jmp     @rsb_auto_cpu

@rsb_auto_local:
    mov     g_routerBackend, BACKEND_LOCAL
    mov     g_routerCapFlags, CAP_KV_CACHE
    mov     g_hasLocalModel, 1
    jmp     @rsb_finalize

@rsb_auto_ollama:
    mov     g_routerBackend, BACKEND_OLLAMA
    mov     g_routerCapFlags, CAP_STREAMING
    mov     g_hasOllama, 1
    jmp     @rsb_finalize

@rsb_auto_cpu:
    mov     g_routerBackend, BACKEND_CPU_FALLBACK
    mov     g_routerCapFlags, CAP_LIMITED
    mov     g_routerPatternInit, 1       ; init pattern matcher
    jmp     @rsb_finalize

    ; ────────────────────────────────────────────────────────────────
    ; LOCAL_GGUF: validate model is loaded in g_modelbase
    ; ────────────────────────────────────────────────────────────────
@rsb_do_local:
    mov     rax, g_modelbase
    test    rax, rax
    jz      @rsb_unavail                 ; model not mapped → unavailable
    mov     g_routerBackend, BACKEND_LOCAL
    mov     g_routerCapFlags, CAP_KV_CACHE
    mov     g_hasLocalModel, 1
    jmp     @rsb_finalize

    ; ────────────────────────────────────────────────────────────────
    ; OLLAMA: verify server connectivity via OllamaClient_IsConnected
    ; ────────────────────────────────────────────────────────────────
@rsb_do_ollama:
    call    OllamaClient_IsConnected
    test    eax, eax
    jz      @rsb_unavail                 ; not connected → unavailable
    mov     g_routerBackend, BACKEND_OLLAMA
    mov     g_routerCapFlags, CAP_STREAMING
    mov     g_hasOllama, 1
    jmp     @rsb_finalize

    ; ────────────────────────────────────────────────────────────────
    ; CPU_FALLBACK: always available, init pattern matcher state
    ; ────────────────────────────────────────────────────────────────
@rsb_do_cpu:
    mov     g_routerBackend, BACKEND_CPU_FALLBACK
    mov     g_routerCapFlags, CAP_LIMITED
    mov     g_routerPatternInit, 1       ; mark pattern matcher active
    jmp     @rsb_finalize

    ; ────────────────────────────────────────────────────────────────
    ; Finalize: timestamp + beacon
    ; ────────────────────────────────────────────────────────────────
@rsb_finalize:
    ; Log backend transition timestamp
    call    GetTickCount64
    mov     g_routerInitTick, rax

    ; Fire BACKEND_CHANGED beacon event (slot=ROUTER, evt=CHANGED, data=backendID)
    mov     ecx, ROUTER_BEACON_SLOT
    mov     edx, ROUTER_EVT_BACKEND_CHANGED
    mov     r8d, g_routerBackend
    call    BeaconSend

    xor     eax, eax                     ; return 0 = success
    jmp     @rsb_ret

    ; ────────────────────────────────────────────────────────────────
    ; Error paths
    ; ────────────────────────────────────────────────────────────────
@rsb_unavail:
    ; Requested backend exists but is not currently available
    mov     g_routerLastError, 1         ; ERR_BACKEND_UNAVAILABLE
    mov     eax, g_routerErrorCount
    inc     eax
    mov     g_routerErrorCount, eax
    mov     eax, -1
    jmp     @rsb_ret

@rsb_invalid:
    ; Backend ID out of valid range
    mov     g_routerLastError, 2         ; ERR_INVALID_BACKEND_ID
    mov     eax, g_routerErrorCount
    inc     eax
    mov     g_routerErrorCount, eax
    mov     eax, -1

@rsb_ret:
    add     rsp, 30h
    pop     rbx
    ret
InferenceRouter_SetBackend ENDP


; ════════════════════════════════════════════════════════════════════
; InferenceRouter_GetStats — Collect full inference statistics (full impl)
;   RCX = pointer to output buffer (at least 128 bytes / STATS_STRUCT_SIZE)
;   Returns: EAX = number of bytes written (88), or 0 on NULL ptr
;
;   Output structure layout (88 bytes, naturally aligned):
;     +00h  totalRequests     QWORD   Total requests processed
;     +08h  totalTokens       QWORD   Total tokens generated
;     +10h  backendId         DWORD   Current internal backend ID
;     +14h  _pad0             DWORD   (alignment)
;     +18h  uptimeMs          QWORD   Backend uptime (ms since SetBackend)
;     +20h  avgTokensSec      DWORD   Average tokens/sec (16.16 fixed-point)
;     +24h  peakTokensSec     DWORD   Peak tokens/sec (16.16 fixed-point)
;     +28h  cacheHitRate      DWORD   Local-hit percentage 0-100
;     +2Ch  errorCount        DWORD   Total error count
;     +30h  lastErrorCode     DWORD   Last error code
;     +34h  queueDepth        DWORD   Current queue depth
;     +38h  activeRequest     DWORD   1 if request in flight
;     +3Ch  _pad1             DWORD   (alignment)
;     +40h  memUsed           QWORD   Memory used by inference (est.)
;     +48h  kvCacheEntries    DWORD   Live KV cache entry count
;     +4Ch  _pad2             DWORD   (alignment)
;     +50h  modelSize         QWORD   Model file size in bytes
;
;   Derived stats computed on the fly:
;     avgTokensSec = totalTokens * 1000 * 65536 / uptimeMs  (16.16)
;     cacheHitRate = localHits * 100 / totalReqs
;     memUsed      = g_kv_len * 128 + VOCAB_TABLE_SIZE (if vocab alloc'd)
; ════════════════════════════════════════════════════════════════════
InferenceRouter_GetStats PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; ── Validate output buffer pointer ──
    test    rcx, rcx
    jz      @rgs_fail

    mov     rsi, rcx                     ; rsi = output buffer (non-volatile)

    ; ── +00h: Total requests processed (QWORD) ──
    mov     rax, g_routerReqs
    mov     qword ptr [rsi], rax

    ; ── +08h: Total tokens generated (QWORD) ──
    mov     rax, g_routerTokensGen
    mov     qword ptr [rsi+08h], rax

    ; ── +10h: Current backend ID (DWORD) ──
    mov     eax, g_routerBackend
    mov     dword ptr [rsi+10h], eax

    ; ── +14h: Padding ──
    mov     dword ptr [rsi+14h], 0

    ; ── +18h: Backend uptime in ms (QWORD) ──
    ;   uptime = GetTickCount64() - g_routerInitTick
    call    GetTickCount64
    sub     rax, g_routerInitTick
    mov     qword ptr [rsi+18h], rax
    mov     rcx, rax                     ; rcx = uptimeMs (kept for division)

    ; ── +20h: Average tokens/sec (DWORD, 16.16 fixed-point) ──
    ;   formula: (totalTokens * 1000 * 65536) / uptimeMs
    ;   Uses 128-bit intermediate via MUL to handle large token counts.
    test    rcx, rcx
    jz      @rgs_zero_tps

    mov     rax, g_routerTokensGen
    test    rax, rax
    jz      @rgs_zero_tps

    mov     r8, 1000
    mul     r8                           ; RDX:RAX = totalTokens * 1000
    shld    rdx, rax, 16                 ; 128-bit shift left 16
    shl     rax, 16                      ; RDX:RAX <<= 16 (fixed-point scale)
    div     rcx                          ; RAX = 16.16 tokens/sec
    mov     dword ptr [rsi+20h], eax

    ; Update peak if current average exceeds stored peak
    cmp     eax, g_routerPeakTPS
    jbe     @rgs_peak_done
    mov     g_routerPeakTPS, eax
@rgs_peak_done:
    jmp     @rgs_tps_done

@rgs_zero_tps:
    mov     dword ptr [rsi+20h], 0

@rgs_tps_done:
    ; ── +24h: Peak tokens/sec (DWORD, 16.16 fixed-point) ──
    mov     eax, g_routerPeakTPS
    mov     dword ptr [rsi+24h], eax

    ; ── +28h: Cache hit rate (DWORD, percentage 0-100) ──
    ;   Approximated as: localHits * 100 / totalReqs
    mov     rax, g_routerLocalHits
    test    rax, rax
    jz      @rgs_zero_cache
    mov     rcx, g_routerReqs
    test    rcx, rcx
    jz      @rgs_zero_cache
    imul    rax, rax, 100                ; localHits * 100
    xor     edx, edx
    div     rcx                          ; / totalReqs
    cmp     eax, 100                     ; clamp to 100
    jbe     @rgs_cache_ok
    mov     eax, 100
@rgs_cache_ok:
    mov     dword ptr [rsi+28h], eax
    jmp     @rgs_cache_done

@rgs_zero_cache:
    mov     dword ptr [rsi+28h], 0

@rgs_cache_done:
    ; ── +2Ch: Error count (DWORD) ──
    mov     eax, g_routerErrorCount
    mov     dword ptr [rsi+2Ch], eax

    ; ── +30h: Last error code (DWORD) ──
    mov     eax, g_routerLastError
    mov     dword ptr [rsi+30h], eax

    ; ── +34h: Queue depth (DWORD) ──
    mov     eax, g_routerQueueDepth
    mov     dword ptr [rsi+34h], eax

    ; ── +38h: Active request flag (DWORD) ──
    mov     eax, g_routerActiveReq
    mov     dword ptr [rsi+38h], eax

    ; ── +3Ch: Padding ──
    mov     dword ptr [rsi+3Ch], 0

    ; ── +40h: Memory used by inference (QWORD, estimated) ──
    ;   Estimate: KV cache entries * 128 bytes/entry + vocab table size
    mov     rax, g_kv_len                ; live KV entry count (QWORD extern)
    shl     rax, 7                       ; * 128 bytes per KV entry
    mov     rdx, g_vocabTable
    test    rdx, rdx
    jz      @rgs_no_vocab_mem
    add     rax, VOCAB_TABLE_SIZE        ; + 512KB vocab table
@rgs_no_vocab_mem:
    mov     qword ptr [rsi+40h], rax

    ; ── +48h: KV cache entries (DWORD) ──
    mov     rax, g_kv_len                ; QWORD → take low 32 bits
    mov     dword ptr [rsi+48h], eax

    ; ── +4Ch: Padding ──
    mov     dword ptr [rsi+4Ch], 0

    ; ── +50h: Model size in bytes (QWORD) ──
    mov     rax, g_routerModelSize
    mov     qword ptr [rsi+50h], rax

    ; Return bytes written
    mov     eax, STATS_STRUCT_SIZE       ; 88
    jmp     @rgs_ret

@rgs_fail:
    xor     eax, eax                     ; NULL pointer → 0 bytes

@rgs_ret:
    add     rsp, 20h
    pop     rsi
    ret
InferenceRouter_GetStats ENDP

END
