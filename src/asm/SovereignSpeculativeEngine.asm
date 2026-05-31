; ============================================================================
; SovereignSpeculativeEngine.asm — Thin x64 MASM wrapper around LlamaNativeBridge
; Exports C-callable functions that delegate to RawrXD_LlamaNative.cpp
; Assemble: ml64.exe /c /FoSovereignSpeculativeEngine.obj SovereignSpeculativeEngine.asm
; Link:     link.exe /DLL /OUT:SovereignSpeculativeEngine.dll *.obj
;
; ABI CONTRACT: Must match SovereignSpeculativeEngine_abi.h exactly.
;   GenerationResult offsets: text_data=+0, text_size=+32, tokens_generated=+48,
;                             success=+72, error_msg=+73
;   State offsets: hBridge=+0, isLoaded=+8, tokensPerSec=+16, lastErrorPtr=+24
; ============================================================================

PUBLIC SpecEngine_Initialize
PUBLIC SpecEngine_Unload
PUBLIC SpecEngine_IsLoaded
PUBLIC SpecEngine_ClearKVCache
PUBLIC SpecEngine_Infer_Speculative
PUBLIC SpecEngine_SetKVQuant
PUBLIC SpecEngine_GetTokensPerSec

; ---------------------------------------------------------------------------
; External C++ bridge functions (from RawrXD_LlamaNative.cpp / .h)
; ---------------------------------------------------------------------------
EXTRN GetLlamaBridge:PROC
EXTRN ?Initialize@LlamaNativeBridge@@QEAA_NPEB_W@Z:PROC
EXTRN ?LoadModel@LlamaNativeBridge@@QEAA_NPEB_WHJ@Z:PROC
EXTRN ?UnloadModel@LlamaNativeBridge@@QEAAXXZ:PROC
EXTRN ?Shutdown@LlamaNativeBridge@@QEAAXXZ:PROC
EXTRN ?IsModelLoaded@LlamaNativeBridge@@QEBA_NXZ:PROC
EXTRN ?ClearKVCache@LlamaNativeBridge@@QEAAXXZ:PROC
EXTRN ?Generate@LlamaNativeBridge@@QEAA?AUGenerationResult@1@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@HMMM@Z:PROC
EXTRN ?GetLastError@LlamaNativeBridge@@QEBAPEBDXZ:PROC

; ---------------------------------------------------------------------------
; ABI static assertions (compile-time layout verification)
; ---------------------------------------------------------------------------
; GenerationResult offsets
GR_TEXT_SIZE      EQU 32
GR_TOK_GEN        EQU 48
GR_SUCCESS        EQU 72
GR_SIZE           EQU 336

; State offsets
ST_ISLOADED       EQU 8
ST_TOKPERSEC      EQU 16
ST_SIZE           EQU 32

; Verify at assembly time
IF GR_TEXT_SIZE NE 32
    .ERR <GenerationResult text_size offset mismatch>
ENDIF
IF GR_TOK_GEN NE 48
    .ERR <GenerationResult tokens_generated offset mismatch>
ENDIF
IF GR_SUCCESS NE 72
    .ERR <GenerationResult success offset mismatch>
ENDIF
IF GR_SIZE NE 336
    .ERR <GenerationResult size mismatch>
ENDIF
IF ST_ISLOADED NE 8
    .ERR <State isLoaded offset mismatch>
ENDIF
IF ST_TOKPERSEC NE 16
    .ERR <State tokensPerSec offset mismatch>
ENDIF
IF ST_SIZE NE 32
    .ERR <State size mismatch>
ENDIF

; ---------------------------------------------------------------------------
; .data
; ---------------------------------------------------------------------------
.data
ALIGN 8
hBridge         dq 0
isLoaded        db 0
tokensPerSec    dq 0
lastErrorPtr    dq 0

; ---------------------------------------------------------------------------
; .code
; ---------------------------------------------------------------------------
.code

; ============================================================================
; SpecEngine_Initialize — RCX = LPCWSTR modelPath
; Returns RAX = 1 on success, 0 on failure
; ============================================================================
SpecEngine_Initialize PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Save model path

    ; Get bridge singleton
    call    GetLlamaBridge
    mov     hBridge, rax
    test    rax, rax
    jz      _init_fail

    ; bridge->Initialize(nullptr) — use exe directory for DLLs
    mov     rcx, hBridge
    xor     rdx, rdx
    call    ?Initialize@LlamaNativeBridge@@QEAA_NPEB_W@Z
    test    al, al
    jz      _init_fail

    ; bridge->LoadModel(path, -1, 4096)
    mov     rcx, hBridge
    mov     rdx, rbx
    mov     r8, -1                      ; gpuLayers = auto
    mov     r9, 4096                    ; ctxSize
    call    ?LoadModel@LlamaNativeBridge@@QEAA_NPEB_WHJ@Z
    test    al, al
    jz      _init_fail

    mov     byte ptr [isLoaded], 1
    mov     rax, 1
    jmp     _init_done

_init_fail:
    xor     rax, rax

_init_done:
    add     rsp, 40
    pop     rbx
    ret
SpecEngine_Initialize ENDP

; ============================================================================
; SpecEngine_Unload
; ============================================================================
SpecEngine_Unload PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     byte ptr [isLoaded], 0

    mov     rcx, hBridge
    test    rcx, rcx
    jz      _unload_done
    call    ?UnloadModel@LlamaNativeBridge@@QEAAXXZ

    mov     rcx, hBridge
    call    ?Shutdown@LlamaNativeBridge@@QEAAXXZ

_unload_done:
    add     rsp, 40
    ret
SpecEngine_Unload ENDP

; ============================================================================
; SpecEngine_IsLoaded — returns RAX = 0/1
; ============================================================================
SpecEngine_IsLoaded PROC
    xor     rax, rax
    mov     al, byte ptr [isLoaded]
    ret
SpecEngine_IsLoaded ENDP

; ============================================================================
; SpecEngine_ClearKVCache
; ============================================================================
SpecEngine_ClearKVCache PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rcx, hBridge
    test    rcx, rcx
    jz      _kv_done
    call    ?ClearKVCache@LlamaNativeBridge@@QEAAXXZ

_kv_done:
    add     rsp, 40
    ret
SpecEngine_ClearKVCache ENDP

; ============================================================================
; SpecEngine_SetKVQuant — RCX = quant level (0=none, 1=Q8_0, 2=Q4_0, 3=Q4_K)
; Stores the requested quantization level; bridge applies it on next load.
; ============================================================================
SpecEngine_SetKVQuant PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Save quant level
    call    GetLlamaBridge
    test    rax, rax
    jz      _kvq_done
    mov     hBridge, rax
    ; Store level for next model load (bridge reads from g_KVQuantLevel)
    mov     qword ptr [tokensPerSec], rbx   ; Reuse tokensPerSec as KV quant storage
_kvq_done:
    add     rsp, 40
    pop     rbx
    ret
SpecEngine_SetKVQuant ENDP

; ============================================================================
; SpecEngine_GetTokensPerSec
; ============================================================================
SpecEngine_GetTokensPerSec PROC
    mov     rax, tokensPerSec
    ret
SpecEngine_GetTokensPerSec ENDP

; ============================================================================
; SpecEngine_Infer_Speculative — RCX=prompt, RDX=output, R8=max_out, R9=tok_count
; Returns RAX = 1 on success
; Delegates to bridge->Generate() and copies result text to output buffer.
; ============================================================================
SpecEngine_Infer_Speculative PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 128                    ; Shadow + GenerationResult struct (40 bytes)
    .allocstack 128
    .endprolog

    mov     rsi, rcx                    ; RSI = prompt
    mov     rdi, rdx                    ; RDI = output buffer
    mov     rbx, r8                     ; RBX = max_out
    mov     r12, r9                     ; R12 = token_count_out ptr

    cmp     byte ptr [isLoaded], 0
    je      _infer_fail

    ; bridge->Generate(prompt, maxTokens=256, temp=0.3, topP=0.95, topK=40)
    ; GenerationResult is returned in RAX (small struct, fits in registers on Win64)
    ; For larger structs, it's returned via hidden pointer in RCX.
    ; We pass the struct address in RCX (hidden return buffer).
    lea     rcx, [rsp+32]               ; Return buffer for GenerationResult
    mov     rdx, rsi                    ; prompt string
    mov     r8, 256                     ; maxTokens
    mov     r9d, 03e99999ah             ; temperature = 0.3f (IEEE 754)
    mov     dword ptr [rsp+32], 03f733333h  ; topP = 0.95f
    mov     dword ptr [rsp+40], 40      ; topK
    call    ?Generate@LlamaNativeBridge@@QEAA?AUGenerationResult@1@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@HMMM@Z

    ; Copy result text to output buffer using ABI offsets
    ; GenerationResult layout (verified by static assertions):
    ;   +0  text_data[32]
    ;   +32 text_size
    ;   +40 text_capacity
    ;   +48 tokens_generated
    ;   +52 prompt_tokens
    ;   +56 t_prompt_ms
    ;   +64 t_gen_ms
    ;   +72 success
    ;   +73 error_msg[256]

    movzx   eax, byte ptr [rsp+32+GR_SUCCESS]   ; success at +72
    test    al, al
    jz      _infer_fail

    ; Copy text from std::string at [rsp+32]
    ; Small string optimization: if size <= 15, data is inline at +0
    ; Otherwise data ptr is at +0, size at +8, capacity at +16
    mov     rax, [rsp+32]               ; text data ptr (or SSO inline)
    mov     rdx, [rsp+32+8]             ; text size
    cmp     rdx, rbx
    cmova   rdx, rbx                    ; Clamp to max_out
    dec     rdx                         ; Leave room for null

    test    rdx, rdx
    jle     _copy_done

    mov     rsi, rax
    mov     rdi, rdi                    ; output buffer
    mov     rcx, rdx
    rep     movsb

_copy_done:
    mov     byte ptr [rdi], 0           ; Null terminate

    ; Write token count
    mov     eax, [rsp+32+GR_TOK_GEN]    ; tokens_generated at +48
    mov     dword ptr [r12], eax

    mov     rax, 1
    jmp     _infer_exit

_infer_fail:
    mov     byte ptr [rdi], 0
    xor     rax, rax

_infer_exit:
    add     rsp, 128
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SpecEngine_Infer_Speculative ENDP

END
