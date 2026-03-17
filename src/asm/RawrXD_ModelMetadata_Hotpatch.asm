; =============================================================================
; RawrXD_ModelMetadata_Hotpatch.asm — Model Metadata Injection Hotpatch Kernel
; =============================================================================
; Forces Ollama model metadata to populate when the standard API response
; lacks fields that GitHub/cloud models provide (family, parameter_size,
; quantization_level, capabilities). The kernel intercepts registration
; buffers and injects synthetic metadata so the agent tab displays them
; identically to GitHub models.
;
; Exports:
;   asm_metadata_hotpatch_init        — Initialize patch tables + default metadata
;   asm_metadata_hotpatch_shutdown    — Tear down, free code caves
;   asm_metadata_inject_defaults      — Write default metadata into a ModelConfig buffer
;   asm_metadata_scan_and_patch       — Scan loaded model list, patch empty fields
;   asm_metadata_force_agent_capable  — Force "agent" capability flag on a model entry
;   asm_metadata_get_stats            — Return patch statistics
;   asm_metadata_set_field            — Set a specific metadata field by ID
;   asm_metadata_validate_buffer      — Validate a metadata buffer is well-formed
;
; Architecture: x64 MASM | Windows ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_ModelMetadata_Hotpatch.obj RawrXD_ModelMetadata_Hotpatch.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC asm_metadata_hotpatch_init
PUBLIC asm_metadata_hotpatch_shutdown
PUBLIC asm_metadata_inject_defaults
PUBLIC asm_metadata_scan_and_patch
PUBLIC asm_metadata_force_agent_capable
PUBLIC asm_metadata_get_stats
PUBLIC asm_metadata_set_field
PUBLIC asm_metadata_validate_buffer

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN VirtualProtect: PROC
EXTERN VirtualAlloc: PROC
EXTERN VirtualFree: PROC
EXTERN FlushInstructionCache: PROC
EXTERN GetCurrentProcess: PROC

; =============================================================================
;                            CONSTANTS
; =============================================================================
PAGE_EXECUTE_READWRITE  EQU     040h
PAGE_READWRITE          EQU     004h
MEM_COMMIT              EQU     01000h
MEM_RESERVE             EQU     02000h
MEM_RELEASE             EQU     08000h

; Metadata field IDs (matching C++ enum MetadataFieldID)
FIELD_FAMILY            EQU     0
FIELD_PARAMETER_SIZE    EQU     1
FIELD_QUANTIZATION      EQU     2
FIELD_CAPABILITIES      EQU     3
FIELD_DESCRIPTION       EQU     4
FIELD_AGENT_CAPABLE     EQU     5
FIELD_CONTEXT_LENGTH    EQU     6
FIELD_MAX_TOKENS        EQU     7

; Buffer layout offsets (C++ struct ModelMetadataBuffer)
; Must match the C++ side exactly
BUF_MAGIC               EQU     0       ; QWORD: 'RAWRMETA'
BUF_VERSION             EQU     8       ; DWORD: version (1)
BUF_FLAGS               EQU     12      ; DWORD: flags
BUF_NAME_PTR            EQU     16      ; QWORD: pointer to model name string
BUF_NAME_LEN            EQU     24      ; QWORD: name length
BUF_FAMILY_PTR          EQU     32      ; QWORD: pointer to family string
BUF_FAMILY_LEN          EQU     40      ; QWORD: family length
BUF_PARAM_SIZE_PTR      EQU     48      ; QWORD: pointer to parameter_size string
BUF_PARAM_SIZE_LEN      EQU     56      ; QWORD: parameter_size length
BUF_QUANT_PTR           EQU     64      ; QWORD: pointer to quantization string
BUF_QUANT_LEN           EQU     72      ; QWORD: quantization length
BUF_CAPS_PTR            EQU     80      ; QWORD: pointer to capabilities string
BUF_CAPS_LEN            EQU     88      ; QWORD: capabilities length
BUF_DESC_PTR            EQU     96      ; QWORD: pointer to description string
BUF_DESC_LEN            EQU     104     ; QWORD: description length
BUF_AGENT_FLAG          EQU     112     ; BYTE:  1 = agent-capable
BUF_CTX_LEN             EQU     116     ; DWORD: context length
BUF_MAX_TOKENS          EQU     120     ; DWORD: max output tokens
BUF_TOTAL_SIZE          EQU     128     ; Total struct size

; Flags
FLAG_HAS_FAMILY         EQU     00000001h
FLAG_HAS_PARAMS         EQU     00000002h
FLAG_HAS_QUANT          EQU     00000004h
FLAG_HAS_CAPS           EQU     00000008h
FLAG_HAS_DESC           EQU     00000010h
FLAG_AGENT_CAPABLE      EQU     00000020h
FLAG_METADATA_COMPLETE  EQU     0000003Fh   ; All flags set

MAGIC_VALUE             EQU     4154454D52574152h   ; 'RAWRMETA' in little-endian

; Code cave size for default strings
CODE_CAVE_SIZE          EQU     4096

; Max models in patch table
MAX_PATCH_ENTRIES       EQU     256

; =============================================================================
;                            DATA SECTION
; =============================================================================
.data

ALIGN 16
; Default metadata strings (used when Ollama provides nothing)
szDefaultFamily         DB      "transformer", 0
szDefaultParamSize      DB      "unknown", 0
szDefaultQuantization   DB      "auto", 0
szDefaultCapabilities   DB      "chat,completion,agent,tool_use", 0
szDefaultDescription    DB      "Ollama local model (metadata auto-injected)", 0

; Lengths of default strings (not counting null terminator)
dwDefaultFamilyLen      QWORD   11      ; "transformer"
dwDefaultParamLen       QWORD   7       ; "unknown"
dwDefaultQuantLen       QWORD   4       ; "auto"
dwDefaultCapsLen        QWORD   30      ; "chat,completion,agent,tool_use"
dwDefaultDescLen        QWORD   44      ; "Ollama local model (metadata auto-injected)"

ALIGN 8
; Statistics
g_ModelsPatched         QWORD   0
g_FieldsInjected        QWORD   0
g_PatchErrors           QWORD   0
g_AgentForced           QWORD   0
g_ValidationsPassed     QWORD   0
g_ValidationsFailed     QWORD   0

; State
g_Initialized           BYTE    0
g_CodeCavePtr           QWORD   0       ; RWX code cave for dynamic strings

; Spinlock for thread safety
g_MetadataLock          QWORD   0

; =============================================================================
;                            CODE SECTION
; =============================================================================
.code

; =============================================================================
; AcquireMetadataLock / ReleaseMetadataLock — Spinlock primitives
; =============================================================================
AcquireMetadataLock PROC
    push    rax
@@spin:
    mov     rax, 1
    xchg    QWORD PTR [g_MetadataLock], rax
    test    rax, rax
    jnz     @@spin
    pop     rax
    ret
AcquireMetadataLock ENDP

ReleaseMetadataLock PROC
    mov     QWORD PTR [g_MetadataLock], 0
    mfence
    ret
ReleaseMetadataLock ENDP

; =============================================================================
; asm_metadata_hotpatch_init
; Initializes the metadata hotpatch engine:
;   - Allocates a code cave for dynamic string storage
;   - Zeros statistics
;   - Sets initialized flag
;
; Parameters: none
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_metadata_hotpatch_init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Already initialized?
    cmp     BYTE PTR [g_Initialized], 1
    je      @@already_init

    ; Zero statistics
    xor     rax, rax
    mov     QWORD PTR [g_ModelsPatched], rax
    mov     QWORD PTR [g_FieldsInjected], rax
    mov     QWORD PTR [g_PatchErrors], rax
    mov     QWORD PTR [g_AgentForced], rax
    mov     QWORD PTR [g_ValidationsPassed], rax
    mov     QWORD PTR [g_ValidationsFailed], rax

    ; Allocate code cave for dynamic string storage
    ; VirtualAlloc(NULL, CODE_CAVE_SIZE, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor     rcx, rcx                    ; lpAddress = NULL
    mov     rdx, CODE_CAVE_SIZE         ; dwSize
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@alloc_fail

    mov     QWORD PTR [g_CodeCavePtr], rax

    ; Mark initialized
    mov     BYTE PTR [g_Initialized], 1
    xor     eax, eax                    ; return 0 = success
    jmp     @@init_done

@@already_init:
    xor     eax, eax                    ; Already init = success
    jmp     @@init_done

@@alloc_fail:
    lock inc QWORD PTR [g_PatchErrors]
    mov     eax, -1

@@init_done:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_metadata_hotpatch_init ENDP

; =============================================================================
; asm_metadata_hotpatch_shutdown
; Frees code cave, zeros state
;
; Parameters: none
; Returns: EAX = 0
; =============================================================================
asm_metadata_hotpatch_shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     BYTE PTR [g_Initialized], 0
    je      @@shut_done

    ; Free code cave
    mov     rcx, QWORD PTR [g_CodeCavePtr]
    test    rcx, rcx
    jz      @@no_cave
    xor     rdx, rdx                    ; dwSize = 0
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@no_cave:
    mov     QWORD PTR [g_CodeCavePtr], 0
    mov     BYTE PTR [g_Initialized], 0

@@shut_done:
    xor     eax, eax
    add     rsp, 48
    pop     rbx
    ret
asm_metadata_hotpatch_shutdown ENDP

; =============================================================================
; asm_metadata_inject_defaults
; Fills empty metadata fields in a ModelMetadataBuffer with defaults.
; Only writes fields that are currently empty (length == 0).
;
; RCX = pointer to ModelMetadataBuffer
; Returns: EAX = number of fields injected
; =============================================================================
asm_metadata_inject_defaults PROC FRAME
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
    sub     rsp, 32
    .allocstack 32
    .endprolog

    call    AcquireMetadataLock

    mov     r12, rcx                    ; r12 = buffer pointer
    xor     r13d, r13d                  ; r13 = fields injected count

    ; Validate magic
    mov     rax, QWORD PTR [r12 + BUF_MAGIC]
    mov     rbx, MAGIC_VALUE
    cmp     rax, rbx
    jne     @@inject_done               ; Invalid buffer, skip

    ; --- Family field ---
    cmp     QWORD PTR [r12 + BUF_FAMILY_LEN], 0
    jne     @@skip_family
    lea     rax, [szDefaultFamily]
    mov     QWORD PTR [r12 + BUF_FAMILY_PTR], rax
    mov     rax, QWORD PTR [dwDefaultFamilyLen]
    mov     QWORD PTR [r12 + BUF_FAMILY_LEN], rax
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_HAS_FAMILY
    inc     r13d
@@skip_family:

    ; --- Parameter size field ---
    cmp     QWORD PTR [r12 + BUF_PARAM_SIZE_LEN], 0
    jne     @@skip_params
    lea     rax, [szDefaultParamSize]
    mov     QWORD PTR [r12 + BUF_PARAM_SIZE_PTR], rax
    mov     rax, QWORD PTR [dwDefaultParamLen]
    mov     QWORD PTR [r12 + BUF_PARAM_SIZE_LEN], rax
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_HAS_PARAMS
    inc     r13d
@@skip_params:

    ; --- Quantization field ---
    cmp     QWORD PTR [r12 + BUF_QUANT_LEN], 0
    jne     @@skip_quant
    lea     rax, [szDefaultQuantization]
    mov     QWORD PTR [r12 + BUF_QUANT_PTR], rax
    mov     rax, QWORD PTR [dwDefaultQuantLen]
    mov     QWORD PTR [r12 + BUF_QUANT_LEN], rax
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_HAS_QUANT
    inc     r13d
@@skip_quant:

    ; --- Capabilities field ---
    cmp     QWORD PTR [r12 + BUF_CAPS_LEN], 0
    jne     @@skip_caps
    lea     rax, [szDefaultCapabilities]
    mov     QWORD PTR [r12 + BUF_CAPS_PTR], rax
    mov     rax, QWORD PTR [dwDefaultCapsLen]
    mov     QWORD PTR [r12 + BUF_CAPS_LEN], rax
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_HAS_CAPS
    inc     r13d
@@skip_caps:

    ; --- Description field ---
    cmp     QWORD PTR [r12 + BUF_DESC_LEN], 0
    jne     @@skip_desc
    lea     rax, [szDefaultDescription]
    mov     QWORD PTR [r12 + BUF_DESC_PTR], rax
    mov     rax, QWORD PTR [dwDefaultDescLen]
    mov     QWORD PTR [r12 + BUF_DESC_LEN], rax
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_HAS_DESC
    inc     r13d
@@skip_desc:

    ; --- Force agent capability ---
    cmp     BYTE PTR [r12 + BUF_AGENT_FLAG], 0
    jne     @@skip_agent
    mov     BYTE PTR [r12 + BUF_AGENT_FLAG], 1
    or      DWORD PTR [r12 + BUF_FLAGS], FLAG_AGENT_CAPABLE
    inc     r13d
@@skip_agent:

    ; --- Default context length (4096 if zero) ---
    cmp     DWORD PTR [r12 + BUF_CTX_LEN], 0
    jne     @@skip_ctx
    mov     DWORD PTR [r12 + BUF_CTX_LEN], 4096
    inc     r13d
@@skip_ctx:

    ; --- Default max tokens (2048 if zero) ---
    cmp     DWORD PTR [r12 + BUF_MAX_TOKENS], 0
    jne     @@skip_maxtok
    mov     DWORD PTR [r12 + BUF_MAX_TOKENS], 2048
    inc     r13d
@@skip_maxtok:

    ; Update global stats
    test    r13d, r13d
    jz      @@inject_done
    lock inc QWORD PTR [g_ModelsPatched]
    mov     eax, r13d
    lock add QWORD PTR [g_FieldsInjected], rax

@@inject_done:
    mov     eax, r13d
    call    ReleaseMetadataLock

    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_metadata_inject_defaults ENDP

; =============================================================================
; asm_metadata_scan_and_patch
; Scans an array of ModelMetadataBuffer entries and patches each one.
;
; RCX = pointer to array of ModelMetadataBuffer
; RDX = count of entries
; Returns: EAX = total fields injected across all entries
; =============================================================================
asm_metadata_scan_and_patch PROC FRAME
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
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     r12, rcx                    ; r12 = array base
    mov     r13, rdx                    ; r13 = count
    xor     r14d, r14d                  ; r14 = total fields injected

    test    r13, r13
    jz      @@scan_done

    xor     rbx, rbx                    ; rbx = index
@@scan_loop:
    cmp     rbx, r13
    jge     @@scan_done

    ; Calculate pointer to current entry
    mov     rax, rbx
    imul    rax, BUF_TOTAL_SIZE
    lea     rcx, [r12 + rax]

    ; Check magic before calling inject
    mov     rax, QWORD PTR [rcx + BUF_MAGIC]
    mov     rsi, MAGIC_VALUE
    cmp     rax, rsi
    jne     @@scan_skip                 ; Not a valid buffer, skip

    ; Call inject_defaults for this entry
    call    asm_metadata_inject_defaults
    add     r14d, eax

@@scan_skip:
    inc     rbx
    jmp     @@scan_loop

@@scan_done:
    mov     eax, r14d
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_metadata_scan_and_patch ENDP

; =============================================================================
; asm_metadata_force_agent_capable
; Forces the agent-capable flag on a single ModelMetadataBuffer.
;
; RCX = pointer to ModelMetadataBuffer
; Returns: EAX = 1 if flag was set, 0 if already set or invalid buffer
; =============================================================================
asm_metadata_force_agent_capable PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Validate magic
    mov     rax, QWORD PTR [rcx + BUF_MAGIC]
    mov     rbx, MAGIC_VALUE
    cmp     rax, rbx
    jne     @@force_invalid

    ; Already agent-capable?
    cmp     BYTE PTR [rcx + BUF_AGENT_FLAG], 1
    je      @@force_already

    ; Set the flag
    mov     BYTE PTR [rcx + BUF_AGENT_FLAG], 1
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_AGENT_CAPABLE

    ; If capabilities string is empty, inject default
    cmp     QWORD PTR [rcx + BUF_CAPS_LEN], 0
    jne     @@force_caps_exists
    lea     rax, [szDefaultCapabilities]
    mov     QWORD PTR [rcx + BUF_CAPS_PTR], rax
    mov     rax, QWORD PTR [dwDefaultCapsLen]
    mov     QWORD PTR [rcx + BUF_CAPS_LEN], rax
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_CAPS
@@force_caps_exists:

    lock inc QWORD PTR [g_AgentForced]
    mov     eax, 1
    jmp     @@force_done

@@force_already:
    xor     eax, eax
    jmp     @@force_done

@@force_invalid:
    lock inc QWORD PTR [g_PatchErrors]
    xor     eax, eax

@@force_done:
    add     rsp, 32
    pop     rbx
    ret
asm_metadata_force_agent_capable ENDP

; =============================================================================
; asm_metadata_get_stats
; Copies patch statistics into a caller-provided buffer.
;
; RCX = pointer to output buffer (6 QWORDs = 48 bytes minimum)
; Returns: EAX = 0
; Layout:
;   [+0]  ModelsPatched
;   [+8]  FieldsInjected
;   [+16] PatchErrors
;   [+24] AgentForced
;   [+32] ValidationsPassed
;   [+40] ValidationsFailed
; =============================================================================
asm_metadata_get_stats PROC FRAME
    .endprolog

    mov     rax, QWORD PTR [g_ModelsPatched]
    mov     QWORD PTR [rcx + 0], rax

    mov     rax, QWORD PTR [g_FieldsInjected]
    mov     QWORD PTR [rcx + 8], rax

    mov     rax, QWORD PTR [g_PatchErrors]
    mov     QWORD PTR [rcx + 16], rax

    mov     rax, QWORD PTR [g_AgentForced]
    mov     QWORD PTR [rcx + 24], rax

    mov     rax, QWORD PTR [g_ValidationsPassed]
    mov     QWORD PTR [rcx + 32], rax

    mov     rax, QWORD PTR [g_ValidationsFailed]
    mov     QWORD PTR [rcx + 40], rax

    xor     eax, eax
    ret
asm_metadata_get_stats ENDP

; =============================================================================
; asm_metadata_set_field
; Sets a specific metadata field in a ModelMetadataBuffer.
;
; RCX = pointer to ModelMetadataBuffer
; RDX = field ID (FIELD_xxx constant)
; R8  = pointer to new value string
; R9  = length of new value string
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_metadata_set_field PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Validate magic
    mov     rax, QWORD PTR [rcx + BUF_MAGIC]
    mov     rbx, MAGIC_VALUE
    cmp     rax, rbx
    jne     @@sf_invalid

    ; Dispatch by field ID
    cmp     edx, FIELD_FAMILY
    je      @@sf_family
    cmp     edx, FIELD_PARAMETER_SIZE
    je      @@sf_params
    cmp     edx, FIELD_QUANTIZATION
    je      @@sf_quant
    cmp     edx, FIELD_CAPABILITIES
    je      @@sf_caps
    cmp     edx, FIELD_DESCRIPTION
    je      @@sf_desc
    cmp     edx, FIELD_AGENT_CAPABLE
    je      @@sf_agent
    cmp     edx, FIELD_CONTEXT_LENGTH
    je      @@sf_ctx
    cmp     edx, FIELD_MAX_TOKENS
    je      @@sf_maxtok
    jmp     @@sf_invalid

@@sf_family:
    mov     QWORD PTR [rcx + BUF_FAMILY_PTR], r8
    mov     QWORD PTR [rcx + BUF_FAMILY_LEN], r9
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_FAMILY
    jmp     @@sf_ok

@@sf_params:
    mov     QWORD PTR [rcx + BUF_PARAM_SIZE_PTR], r8
    mov     QWORD PTR [rcx + BUF_PARAM_SIZE_LEN], r9
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_PARAMS
    jmp     @@sf_ok

@@sf_quant:
    mov     QWORD PTR [rcx + BUF_QUANT_PTR], r8
    mov     QWORD PTR [rcx + BUF_QUANT_LEN], r9
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_QUANT
    jmp     @@sf_ok

@@sf_caps:
    mov     QWORD PTR [rcx + BUF_CAPS_PTR], r8
    mov     QWORD PTR [rcx + BUF_CAPS_LEN], r9
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_CAPS
    jmp     @@sf_ok

@@sf_desc:
    mov     QWORD PTR [rcx + BUF_DESC_PTR], r8
    mov     QWORD PTR [rcx + BUF_DESC_LEN], r9
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_HAS_DESC
    jmp     @@sf_ok

@@sf_agent:
    ; R8 is treated as a boolean (0 or 1), R9 unused
    mov     BYTE PTR [rcx + BUF_AGENT_FLAG], r8b
    test    r8b, r8b
    jz      @@sf_agent_off
    or      DWORD PTR [rcx + BUF_FLAGS], FLAG_AGENT_CAPABLE
    jmp     @@sf_ok
@@sf_agent_off:
    mov     eax, DWORD PTR [rcx + BUF_FLAGS]
    and     eax, NOT FLAG_AGENT_CAPABLE
    mov     DWORD PTR [rcx + BUF_FLAGS], eax
    jmp     @@sf_ok

@@sf_ctx:
    ; R8 is context length as DWORD, R9 unused
    mov     DWORD PTR [rcx + BUF_CTX_LEN], r8d
    jmp     @@sf_ok

@@sf_maxtok:
    ; R8 is max_tokens as DWORD, R9 unused
    mov     DWORD PTR [rcx + BUF_MAX_TOKENS], r8d
    jmp     @@sf_ok

@@sf_ok:
    lock inc QWORD PTR [g_FieldsInjected]
    xor     eax, eax
    jmp     @@sf_done

@@sf_invalid:
    lock inc QWORD PTR [g_PatchErrors]
    mov     eax, -1

@@sf_done:
    add     rsp, 32
    pop     rbx
    ret
asm_metadata_set_field ENDP

; =============================================================================
; asm_metadata_validate_buffer
; Validates a ModelMetadataBuffer is well-formed.
;
; RCX = pointer to ModelMetadataBuffer
; Returns: EAX = 1 if valid, 0 if invalid
; =============================================================================
asm_metadata_validate_buffer PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    test    rcx, rcx
    jz      @@val_fail

    ; Check magic
    mov     rax, QWORD PTR [rcx + BUF_MAGIC]
    mov     rbx, MAGIC_VALUE
    cmp     rax, rbx
    jne     @@val_fail

    ; Check version
    cmp     DWORD PTR [rcx + BUF_VERSION], 1
    jne     @@val_fail

    ; Check that string pointers are non-null if lengths > 0
    cmp     QWORD PTR [rcx + BUF_FAMILY_LEN], 0
    je      @@val_check_params
    cmp     QWORD PTR [rcx + BUF_FAMILY_PTR], 0
    je      @@val_fail

@@val_check_params:
    cmp     QWORD PTR [rcx + BUF_PARAM_SIZE_LEN], 0
    je      @@val_check_quant
    cmp     QWORD PTR [rcx + BUF_PARAM_SIZE_PTR], 0
    je      @@val_fail

@@val_check_quant:
    cmp     QWORD PTR [rcx + BUF_QUANT_LEN], 0
    je      @@val_check_caps
    cmp     QWORD PTR [rcx + BUF_QUANT_PTR], 0
    je      @@val_fail

@@val_check_caps:
    cmp     QWORD PTR [rcx + BUF_CAPS_LEN], 0
    je      @@val_check_desc
    cmp     QWORD PTR [rcx + BUF_CAPS_PTR], 0
    je      @@val_fail

@@val_check_desc:
    cmp     QWORD PTR [rcx + BUF_DESC_LEN], 0
    je      @@val_pass
    cmp     QWORD PTR [rcx + BUF_DESC_PTR], 0
    je      @@val_fail

@@val_pass:
    lock inc QWORD PTR [g_ValidationsPassed]
    mov     eax, 1
    jmp     @@val_done

@@val_fail:
    lock inc QWORD PTR [g_ValidationsFailed]
    xor     eax, eax

@@val_done:
    add     rsp, 32
    pop     rbx
    ret
asm_metadata_validate_buffer ENDP

END
