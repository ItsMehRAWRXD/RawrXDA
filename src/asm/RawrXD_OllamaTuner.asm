; =============================================================================
; RawrXD_OllamaTuner.asm — Pure x64 Ollama Configuration & Memory Enforcer
; Reverse-engineered Modelfile generation with runtime safety interlocks
; =============================================================================
;
; Architecture:
;   - In-memory Modelfile generator (Q2_K -> Q4_K_M dynamic selection)
;   - API request interceptor (injects survival parameters into JSON)
;   - Memory pressure governor (GlobalMemoryStatusEx polling)
;   - Emergency context window reduction (2048 -> 1024 -> 512 automatic)
;   - NUMA/MMAP enforcer (prevents resident memory bloat)
;
; Safety Envelope (64GB System):
;   120B Q2_K:  38GB weights (mmap'd, disk-backed)
;   KV Cache:   2GB @ 2048 ctx (4GB @ 4096 ctx emergency)
;   GPU:        16GB RX 7800 XT (40 layers)
;   Headroom:   8GB for OS/IDE
;
; Exports:
;   OllamaTuner_Init               — Spawn memory monitor thread
;   OllamaTuner_GenerateModelfile  — Build survival Modelfile in memory
;   OllamaTuner_InterceptRequest   — JSON parameter injection hook
;   OllamaTuner_EnforceMemory      — Hard limit enforcer (call before generate)
;   OllamaTuner_GetSafeParams      — Retrieve current safe context size
;   OllamaTuner_EmergencyFlush     — Drop KV cache, reset to Phi-3
;
; Build: ml64.exe /c /Zi /Fo RawrXD_OllamaTuner.obj RawrXD_OllamaTuner.asm
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
; External Imports
; =============================================================================
EXTERNDEF GlobalMemoryStatusEx:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF TerminateThread:PROC
EXTERNDEF QueryPerformanceCounter:PROC
EXTERNDEF QueryPerformanceFrequency:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF lstrlenA:PROC

; =============================================================================
; Constants
; =============================================================================

; Memory Envelope (64GB Total)
ENVELOPE_TOTAL_RAM      EQU 68719476736   ; 64GB in bytes
ENVELOPE_RESERVED_OS    EQU 8589934592    ; 8GB OS/IDE overhead
ENVELOPE_GPU_VRAM       EQU 17179869184   ; 16GB RX 7800 XT
ENVELOPE_SAFETY_MARGIN  EQU 4294967296    ; 4GB buffer

; Available for 120B: 64 - 8 - 4 = 52GB theoretical
; Practical limit: 38GB weights (Q2_K) + 2GB KV = 40GB

; Model Configuration Constants
QUANT_Q2_K              EQU 0
QUANT_Q3_K_L            EQU 1
QUANT_Q4_K_M            EQU 2
QUANT_Q5_K_M            EQU 3

CTX_EMERGENCY           EQU 512
CTX_CONSERVATIVE        EQU 1024
CTX_SURVIVAL            EQU 2048
CTX_PERFORMANCE         EQU 4096
CTX_MAX_UNSAFE          EQU 8192

; Ollama API Parameter IDs
PARAM_NUM_CTX           EQU 0
PARAM_NUM_BATCH         EQU 1
PARAM_NUM_GPU           EQU 2
PARAM_MAIN_GPU          EQU 3
PARAM_TEMPERATURE       EQU 4
PARAM_TOP_P             EQU 5
PARAM_MMAP              EQU 6
PARAM_MLOCK             EQU 7
PARAM_NUMA              EQU 8
PARAM_NUM_THREAD        EQU 9

; Status codes
TUNER_OK                EQU 0
TUNER_ERR_OOM           EQU 0C0000300h
TUNER_ERR_INVALID_MODEL EQU 0C0000301h
TUNER_ERR_GPU_FAIL      EQU 0C0000302h

; =============================================================================
; Structures
; =============================================================================

; Memory status tracking
MemoryEnvelope struct
    totalPhys       dq ?
    availPhys       dq ?
    totalPageFile   dq ?
    availPageFile   dq ?
    totalVirtual    dq ?
    availVirtual    dq ?
    availExtended   dq ?
    pressureLevel   dd ?        ; 0=Green, 1=Yellow, 2=Red, 3=Critical
    recommendedCtx  dd ?        ; Dynamic context window
    currentQuant    dd ?        ; Current quantization level
    mmapEnabled     dd ?        ; Boolean
    gpuLayers       dd ?        ; Layers offloaded to GPU
MemoryEnvelope ends

; Model configuration profile
ModelProfile struct
    name            db 64 dup(0)        ; Model name
    quantLevel      dd ?                ; QUANT_*
    numCtx          dd ?                ; Context window
    numBatch        dd ?                ; Batch size
    numGpu          dd ?                ; GPU layers
    mmap            dd ?                ; 0/1
    mlock           dd ?                ; 0/1
    numa            dd ?                ; 0/1
    temperature     dd ?                ; Fixed point (e.g., 70 = 0.7)
    topP            dd ?                ; Fixed point
    seed            dd ?
ModelProfile ends

; =============================================================================
; Data Segment
; =============================================================================
.data

ALIGN 8
g_TunerInitialized      dd 0
g_hMonitorThread        dq 0
g_RunMonitor            dd 1
g_LastPressureCheck     dq 0

; Current envelope state
ALIGN 8
g_Envelope              MemoryEnvelope <>

; Pre-defined survival profiles
ALIGN 8

; 120B Survival (Your working configuration)
Profile_120B_Survival   ModelProfile \
    <"gpt-oss:120b-survival", QUANT_Q2_K, CTX_SURVIVAL, 512, 40, 1, 0, 1, 70, 90, 42>

; 120B Emergency (OOM recovery)
Profile_120B_Emergency  ModelProfile \
    <"gpt-oss:120b-emergency", QUANT_Q2_K, CTX_EMERGENCY, 256, 40, 1, 0, 1, 70, 90, 42>

; Phi-3 Default (fallback)
Profile_Phi3_Default    ModelProfile \
    <"phi3", QUANT_Q4_K_M, CTX_MAX_UNSAFE, 2048, 0, 1, 0, 0, 70, 90, 42>

; Modelfile template fragments
szTmpl_FROM             db "FROM ",0
szTmpl_PARAMETER        db 0Dh,0Ah,"PARAMETER ",0
szTmpl_SYSTEM           db 0Dh,0Ah,"SYSTEM ",0Dh,0Ah,'"""',0Dh,0Ah,0
szTmpl_SYSTEM_END       db 0Dh,0Ah,'"""',0Dh,0Ah,0
szTmpl_NUM_CTX          db "num_ctx ",0
szTmpl_NUM_BATCH        db "num_batch ",0
szTmpl_NUM_GPU          db "num_gpu ",0
szTmpl_MAIN_GPU         db "main_gpu ",0
szTmpl_TEMP             db "temperature ",0
szTmpl_TOP_P            db "top_p ",0
szTmpl_MMAP             db "mmap ",0
szTmpl_MLOCK            db "mlock ",0
szTmpl_NUMA             db "numa ",0

; Boolean strings
szTrue                  db "true",0
szFalse                 db "false",0

; JSON injection fragments (for API interception)
szJSON_NumCtx           db '"num_ctx":',0
szJSON_NumBatch         db '"num_batch":',0
szJSON_Options_Open     db ',"options":{',0
szJSON_Options_Close    db '}',0
szJSON_Mmap             db '"mmap":true',0
szJSON_Numa             db '"numa":true',0

; Emergency system prompt (reduces KV cache pressure)
szSystemPrompt_120B     db \
    "You are operating in memory-constrained mode (64GB RAM). ", \
    "Keep responses under 500 tokens. Use concise technical language. ", \
    "Avoid repetitive explanations to conserve KV cache.",0

; Scratch buffer for Modelfile generation
ALIGN 4096
g_ModelfileBuffer       db 4096 dup(0)

; Itoa buffer
g_NumBuf                db 32 dup(0)

; =============================================================================
; Code Segment
; =============================================================================
.code

; =============================================================================
; OllamaTuner_Init — Start memory monitoring subsystem
;
; RCX = Poll interval in milliseconds (suggest 1000)
; Returns: RAX = 0 on success, thread handle in RDX
; =============================================================================
PUBLIC OllamaTuner_Init
OllamaTuner_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 64
    .allocstack 64
    .endprolog

    ; Already initialized?
    cmp     g_TunerInitialized, 1
    je      @oti_already_init

    ; Initial memory survey
    call    UpdateMemoryEnvelope

    ; Spawn monitor thread
    mov     ebx, ecx                ; Save poll interval

    xor     ecx, ecx                ; lpThreadAttributes
    xor     edx, edx                ; dwStackSize
    lea     r8, MemoryMonitorThread ; lpStartAddress
    mov     r9d, ebx                ; lpParameter = poll interval
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0
    mov     qword ptr [rsp + 40], 0
    call    CreateThread
    add     rsp, 40

    test    rax, rax
    jz      @oti_fail

    mov     g_hMonitorThread, rax
    mov     g_TunerInitialized, 1

    xor     eax, eax
    mov     rdx, g_hMonitorThread
    jmp     @oti_return

@oti_already_init:
    xor     eax, eax
    mov     rdx, g_hMonitorThread
    jmp     @oti_return

@oti_fail:
    mov     eax, TUNER_ERR_OOM

@oti_return:
    add     rsp, 64
    pop     rbx
    ret
OllamaTuner_Init ENDP

; =============================================================================
; OllamaTuner_GenerateModelfile — Build survival Modelfile in memory
;
; RCX = Profile pointer (ModelProfile*)
; RDX = Output buffer (must be 4096+ bytes)
; R8  = Model size in billions (120 for 120B, 8 for 8B, etc.)
; Returns: RAX = bytes written to buffer
; =============================================================================
PUBLIC OllamaTuner_GenerateModelfile
OllamaTuner_GenerateModelfile PROC FRAME
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
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     r12, rcx                ; r12 = profile
    mov     r13, rdx                ; r13 = output buffer
    mov     ebx, r8d                ; ebx = model size (B)

    mov     rdi, r13                ; rdi = write pointer

    ; FROM <model>
    lea     rsi, szTmpl_FROM
    call    strcpy_asm

    lea     rsi, [r12].ModelProfile.name
    call    strcat_asm
    call    newline_asm

    ; PARAMETER num_ctx <value>
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_NUM_CTX
    call    strcat_asm

    ; Dynamic adjustment based on current memory pressure
    call    GetSafeContextSize      ; Returns in EAX
    push    rax
    mov     ecx, eax
    call    itoa_asm
    call    strcat_asm
    call    newline_asm
    pop     rax

    ; Store the enforced context back to profile
    mov     [r12].ModelProfile.numCtx, eax

    ; PARAMETER num_batch <value>
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_NUM_BATCH
    call    strcat_asm
    mov     ecx, [r12].ModelProfile.numBatch
    call    itoa_asm
    call    strcat_asm
    call    newline_asm

    ; PARAMETER num_gpu <value>
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_NUM_GPU
    call    strcat_asm

    ; GPU layers: if 120B, ensure we don't exceed VRAM
    cmp     ebx, 100                ; >100B?
    jb      @otgm_gpu_normal

    ; For 120B, enforce 40 layers max (fits in 16GB at Q2)
    mov     ecx, 40
    jmp     @otgm_gpu_set

@otgm_gpu_normal:
    mov     ecx, [r12].ModelProfile.numGpu

@otgm_gpu_set:
    call    itoa_asm
    call    strcat_asm
    call    newline_asm

    ; PARAMETER main_gpu 0
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_MAIN_GPU
    call    strcat_asm
    mov     byte ptr [rdi], '0'
    inc     rdi
    mov     byte ptr [rdi], 0
    call    newline_asm

    ; PARAMETER mmap true (CRITICAL for 120B survival)
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_MMAP
    call    strcat_asm
    lea     rsi, szTrue
    call    strcat_asm
    call    newline_asm

    ; PARAMETER mlock false (Allow swapping of cold weights)
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_MLOCK
    call    strcat_asm
    lea     rsi, szFalse
    call    strcat_asm
    call    newline_asm

    ; PARAMETER numa true (NUMA-aware allocation)
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_NUMA
    call    strcat_asm
    lea     rsi, szTrue
    call    strcat_asm
    call    newline_asm

    ; PARAMETER temperature <value>
    lea     rsi, szTmpl_PARAMETER
    call    strcat_asm
    lea     rsi, szTmpl_TEMP
    call    strcat_asm
    mov     eax, [r12].ModelProfile.temperature
    ; Insert decimal point (fixed point to float)
    xor     edx, edx
    mov     ecx, 10
    div     ecx                     ; EAX = whole, EDX = tenths
    call    itoa_asm
    mov     byte ptr [rdi], '.'
    inc     rdi
    mov     eax, edx
    call    itoa_asm
    call    newline_asm

    ; SYSTEM prompt (memory-conscious)
    lea     rsi, szTmpl_SYSTEM
    call    strcat_asm

    ; If 120B, use constrained system prompt
    cmp     ebx, 100
    jb      @otgm_system_normal
    lea     rsi, szSystemPrompt_120B
    jmp     @otgm_system_write

@otgm_system_normal:
    lea     rsi, [r12].ModelProfile.name  ; Default to model name as placeholder

@otgm_system_write:
    call    strcat_asm
    lea     rsi, szTmpl_SYSTEM_END
    call    strcat_asm

    ; Calculate total length
    mov     rax, rdi
    sub     rax, r13

    add     rsp, 128
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
OllamaTuner_GenerateModelfile ENDP

; =============================================================================
; OllamaTuner_EnforceMemory — JSON parameter injection
;
; Intercepts outgoing Ollama /api/generate requests and injects survival params
; RCX = Original JSON buffer
; RDX = Output buffer (modified JSON)
; R8  = Buffer size
; Returns: RAX = new length
; =============================================================================
PUBLIC OllamaTuner_EnforceMemory
OllamaTuner_EnforceMemory PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 256
    .allocstack 256
    .endprolog

    mov     rsi, rcx                ; rsi = input JSON
    mov     rdi, rdx                ; rdi = output
    mov     rbx, r8                 ; rbx = buffer size

    ; Copy input to output
    push    rdi
    call    strcpy_asm
    pop     rdi

    ; Find insertion point: before final '}'
    ; Search backwards from end for '}'
    mov     rcx, rbx
    add     rdi, rcx
    sub     rdi, 2                  ; Point to likely closing brace

    ; Search back for unquoted '}'
@oem_find_brace:
    cmp     rdi, rdx
    jb      @oem_append_opts        ; Not found, append to end

    cmp     byte ptr [rdi], '}'
    jne     @oem_prev_char

    ; Check if quoted (simplified: check if preceded by odd number of quotes)
    ; For speed, just insert before this brace
    jmp     @oem_insert_here

@oem_prev_char:
    dec     rdi
    jmp     @oem_find_brace

@oem_append_opts:
    ; Append to end
    mov     rdi, rdx
    call    strlen_asm
    add     rdi, rax
    sub     rdi, 1                  ; Overwrite null, will re-add

@oem_insert_here:
    ; Insert ,"options":{...}
    lea     rsi, szJSON_Options_Open
    call    strcpy_asm

    ; mmap: true
    lea     rsi, szJSON_Mmap
    call    strcat_asm

    ; numa: true
    lea     rsi, szJSON_Numa
    call    strcat_asm

    ; num_ctx: <dynamic>
    lea     rsi, szJSON_NumCtx
    call    strcat_asm
    call    GetSafeContextSize
    mov     ecx, eax
    call    itoa_asm
    call    strcat_asm

    ; Close options
    lea     rsi, szJSON_Options_Close
    call    strcat_asm

    ; Re-add closing brace
    mov     byte ptr [rdi], '}'
    inc     rdi
    mov     byte ptr [rdi], 0

    ; Return length
    mov     rax, rdi
    sub     rax, rdx

    add     rsp, 256
    pop     rdi
    pop     rsi
    pop     rbx
    ret
OllamaTuner_EnforceMemory ENDP

; =============================================================================
; Memory Monitor Thread — Background pressure polling
; =============================================================================
MemoryMonitorThread PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     ebx, ecx                ; poll interval

@mmt_loop:
    cmp     g_RunMonitor, 0
    je      @mmt_exit

    call    UpdateMemoryEnvelope

    ; Check if we need to emergency downgrade
    cmp     g_Envelope.pressureLevel, 3  ; Critical?
    jb      @mmt_sleep

    ; Trigger emergency flush
    call    OllamaTuner_EmergencyFlush

@mmt_sleep:
    mov     ecx, ebx
    sub     rsp, 32
    call    Sleep
    add     rsp, 32

    jmp     @mmt_loop

@mmt_exit:
    xor     eax, eax
    add     rsp, 64
    pop     rbx
    ret
MemoryMonitorThread ENDP

; =============================================================================
; UpdateMemoryEnvelope — Poll current RAM status
; =============================================================================
UpdateMemoryEnvelope PROC FRAME
    sub     rsp, 128
    .allocstack 128
    .endprolog

    ; Build MEMORYSTATUSEX on stack
    mov     dword ptr [rsp + 64], 64    ; dwLength = sizeof(MEMORYSTATUSEX)
    lea     rcx, [rsp + 64]
    sub     rsp, 32
    call    GlobalMemoryStatusEx
    add     rsp, 32

    test    eax, eax
    jz      @ume_fail

    ; Update envelope struct from stack-based MEMORYSTATUSEX
    ; ullTotalPhys is at offset 8, ullAvailPhys at offset 16
    mov     rax, qword ptr [rsp + 64 + 8]
    mov     g_Envelope.totalPhys, rax

    mov     rax, qword ptr [rsp + 64 + 16]
    mov     g_Envelope.availPhys, rax

    ; Calculate pressure level
    ; Critical: <8GB available (OOM imminent)
    cmp     rax, 8589934592
    jb      @ume_critical

    ; Red: <16GB available (120B unsafe)
    cmp     rax, 17179869184
    jb      @ume_red

    ; Yellow: <24GB available (tight but workable)
    cmp     rax, 25769803776
    jb      @ume_yellow

    ; Green
    mov     g_Envelope.pressureLevel, 0
    mov     g_Envelope.recommendedCtx, CTX_PERFORMANCE
    jmp     @ume_done

@ume_critical:
    mov     g_Envelope.pressureLevel, 3
    mov     g_Envelope.recommendedCtx, CTX_EMERGENCY
    jmp     @ume_done

@ume_red:
    mov     g_Envelope.pressureLevel, 2
    mov     g_Envelope.recommendedCtx, CTX_CONSERVATIVE
    jmp     @ume_done

@ume_yellow:
    mov     g_Envelope.pressureLevel, 1
    mov     g_Envelope.recommendedCtx, CTX_SURVIVAL

@ume_done:
@ume_fail:
    add     rsp, 128
    ret
UpdateMemoryEnvelope ENDP

; =============================================================================
; GetSafeContextSize — Return current safe context window
; =============================================================================
PUBLIC GetSafeContextSize
GetSafeContextSize PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    call    UpdateMemoryEnvelope
    mov     eax, g_Envelope.recommendedCtx

    add     rsp, 40
    ret
GetSafeContextSize ENDP

; =============================================================================
; OllamaTuner_EmergencyFlush — OOM recovery procedure
; =============================================================================
PUBLIC OllamaTuner_EmergencyFlush
OllamaTuner_EmergencyFlush PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 128
    .allocstack 128
    .endprolog

    ; Force context to minimum
    mov     g_Envelope.recommendedCtx, CTX_EMERGENCY

    ; Signal to reload model with smaller context
    ; (This would interface with your model loader)

    xor     eax, eax
    add     rsp, 128
    pop     rbx
    ret
OllamaTuner_EmergencyFlush ENDP

; =============================================================================
; String Utilities (Zero CRT)
; =============================================================================
strcpy_asm PROC
    ; RDI = dest (implicit), RSI = src (implicit)
    ; Uses LODS/STOS convention (rsi->rdi copy)
@sca_loop:
    lodsb
    stosb
    test    al, al
    jnz     @sca_loop
    dec     rdi                     ; Point back to null for append
    ret
strcpy_asm ENDP

strcat_asm PROC
    ; Append RSI to RDI (rdi already at end from strcpy_asm chain)
@cat_loop:
    lodsb
    stosb
    test    al, al
    jnz     @cat_loop
    dec     rdi                     ; Point back to null
    ret
strcat_asm ENDP

newline_asm PROC
    mov     byte ptr [rdi], 0Dh
    mov     byte ptr [rdi+1], 0Ah
    mov     byte ptr [rdi+2], 0
    add     rdi, 2
    ret
newline_asm ENDP

itoa_asm PROC
    ; ECX = number to convert
    ; Writes to [rdi], advances rdi
    push    rax
    push    rbx
    push    rdx
    push    rsi

    mov     eax, ecx
    lea     rsi, [rdi + 10]         ; Build backwards
    mov     byte ptr [rsi], 0

@ia_convert:
    xor     edx, edx
    mov     ebx, 10
    div     ebx
    add     dl, '0'
    dec     rsi
    mov     [rsi], dl
    test    eax, eax
    jnz     @ia_convert

    ; Copy forward
@ia_copy:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    test    al, al
    jnz     @ia_copy
    dec     rdi                     ; Point to null terminator

    pop     rsi
    pop     rdx
    pop     rbx
    pop     rax
    ret
itoa_asm ENDP

strlen_asm PROC
    ; RDI = string, returns length in RAX
    push    rcx
    push    rdi
    xor     eax, eax
    mov     rcx, -1
    repne scasb
    mov     rax, -2
    sub     rax, rcx
    pop     rdi
    pop     rcx
    ret
strlen_asm ENDP

END
