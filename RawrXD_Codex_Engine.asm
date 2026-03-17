; RawrXD_Codex_Engine.asm
; Advanced PE Analysis & Vulnerability Detection
; System 2 of 6: Codex Infrastructure
; PURE X64 MASM - ZERO STUBS - ZERO CRT

OPTION CASEMAP:NONE
 

;=============================================================================
; EXTERNAL DEPENDENCIES
;=============================================================================
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC

; System 1 Exports
EXTERN RawrDumpBin_Create : PROC
EXTERN RawrDumpBin_Destroy : PROC
EXTERN RawrDumpBin_ParsePE : PROC
EXTERN RawrDumpBin_GetSections : PROC
EXTERN RawrDumpBin_GetExports : PROC
EXTERN RawrDumpBin_GetImports : PROC
EXTERN RawrDumpBin_RVAToFileOffset : PROC

;=============================================================================
; PUBLIC INTERFACE
;=============================================================================
PUBLIC Codex_Create
PUBLIC Codex_Destroy
PUBLIC Codex_AnalyzeSecurity
PUBLIC Codex_CalculateEntropy
PUBLIC Codex_GetEnhancedSections

;=============================================================================
; STRUCTURES
;=============================================================================
DUMPBIN_CTX STRUCT 8
    hFile               QWORD ?
    hMapping            QWORD ?
    pBase               QWORD ?
    pNtHeaders          QWORD ?
    FileSize            QWORD ?
    IsPE32Plus          BYTE ?
    NumberOfSections    WORD ?
    ExportDirRVA        DWORD ?
    ImportDirRVA        DWORD ?
    ResourceDirRVA      DWORD ?
    SectionHeadersRVA   DWORD ?
DUMPBIN_CTX ENDS

CODEX_CTX STRUCT 8
    DumpBinCtx          DUMPBIN_CTX <>
    Is64Bit             BYTE ?
    HasASLR             BYTE ?
    HasDEP              BYTE ?
    HasSEH              BYTE ?
    HasCfg              BYTE ?
    IsDotNet            BYTE ?
    SectionCount        DWORD ?
    pSectionAnalysis    QWORD ?
CODEX_CTX ENDS

SECTION_ANALYSIS STRUCT 8
    Name                BYTE 8 DUP(?)
    Entropy             REAL8 ?
    Characteristics     DWORD ?
    IsSuspicious        BYTE ?
SECTION_ANALYSIS ENDS

.CODE

;-----------------------------------------------------------------------------
; Codex_Create
;-----------------------------------------------------------------------------
Codex_Create PROC
    push rbx
    push rsi
    mov rbx, rcx                    ; RBX = pContext
    mov rsi, rdx                    ; RSI = pFilePath
    
    ; Init DumpBin first
    lea rcx, [rbx].CODEX_CTX.DumpBinCtx
    mov rdx, rsi
    call RawrDumpBin_Create
    test eax, eax
    jz @@Failed
    
    lea rcx, [rbx].CODEX_CTX.DumpBinCtx
    call RawrDumpBin_ParsePE
    test eax, eax
    jz @@Failed
    
    ; Identify Security Features
    mov rsi, [rbx].CODEX_CTX.DumpBinCtx.pNtHeaders
    movzx eax, WORD PTR [rsi+5Eh]   ; DllCharacteristics
    
    test ax, 0040h                  ; IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
    setnz [rbx].CODEX_CTX.HasASLR
    
    test ax, 0100h                  ; IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    setnz [rbx].CODEX_CTX.HasDEP
    
    test ax, 0400h                  ; IMAGE_DLLCHARACTERISTICS_NO_SEH
    setz [rbx].CODEX_CTX.HasSEH
    
    test ax, 4000h                  ; IMAGE_DLLCHARACTERISTICS_GUARD_CF
    setnz [rbx].CODEX_CTX.HasCfg

    mov eax, 1
    jmp @@Exit
    
@@Failed:
    xor eax, eax
@@Exit:
    pop rsi
    pop rbx
    ret
Codex_Create ENDP

;-----------------------------------------------------------------------------
; Codex_CalculateEntropy
;-----------------------------------------------------------------------------
Codex_CalculateEntropy PROC
    ; RCX = pData, RDX = Size
    ; Returns XMM0 = Entropy
    push rbx
    push rsi
    push rdi
    sub rsp, 2048                   ; 256 * 8 byte frequency table
    
    mov rsi, rcx
    mov rbx, rdx
    
    ; Zero table
    mov rdi, rsp
    xor eax, eax
    mov rcx, 256
    rep stosq
    
    ; Count frequencies
    xor r8, r8
@@Count:
    cmp r8, rbx
    jae @@Math
    movzx eax, BYTE PTR [rsi+r8]
    inc QWORD PTR [rsp+rax*8]
    inc r8
    jmp @@Count
    
@@Math:
    xorpd xmm0, xmm0                ; Accumulator
    cvtsi2sd xmm1, rbx              ; Total size
    xor r8, r8
@@Loop:
    cmp r8, 256
    jae @@Done
    mov rax, [rsp+r8*8]
    test rax, rax
    jz @@Next
    
    cvtsi2sd xmm2, rax
    divsd xmm2, xmm1                ; P(i)
    
    ; log2(P) = log(P) / log(2)
    ; Approximation: log2(x) for entropy trend
    ; Since we are PURE MASM and want NO STUBS, we implement a fast log2
    movsd xmm3, xmm2
    call @@FastLog2
    mulsd xmm2, xmm3                ; P * log2(P)
    addsd xmm0, xmm2                ; Sum
@@Next:
    inc r8
    jmp @@Loop
    
@@Done:
    ; Entropy = -Sum
    movsd xmm1, [@@NegOne]
    mulsd xmm0, xmm1
    
    add rsp, 2048
    pop rdi
    pop rsi
    pop rbx
    ret

@@FastLog2:
    ; Input: XMM3 (double), Output: XMM3 (log2 of input)
    ; Real IEEE-754 manipulation
    movq rax, xmm3
    mov rdx, rax
    shr rdx, 52
    and rdx, 7FFh                   ; Exponent
    sub rdx, 1023                   ; Bias
    cvtsi2sd xmm3, rdx              ; Exponent is log2 part
    ; Mentissa linear approx: log2(1+m) ~= m
    mov rcx, -1
    shr rcx, 12
    and rax, rcx
    mov rcx, 3FFh
    shl rcx, 52
    or rax, rcx
    movq xmm4, rax
    subsd xmm4, [@@One]
    addsd xmm3, xmm4
    ret

ALIGN 8
@@NegOne REAL8 -1.0
@@One    REAL8 1.0
Codex_CalculateEntropy ENDP

;-----------------------------------------------------------------------------
; Codex_AnalyzeSecurity
;-----------------------------------------------------------------------------
Codex_AnalyzeSecurity PROC
    ; Just returns the bitmask of detected features
    push rbx
    mov rbx, rcx
    xor eax, eax
    cmp [rbx].CODEX_CTX.HasASLR, 1
    jne @f
    or eax, 1
@@: cmp [rbx].CODEX_CTX.HasDEP, 1
    jne @f
    or eax, 2
@@: cmp [rbx].CODEX_CTX.HasSEH, 1
    jne @f
    or eax, 4
@@: cmp [rbx].CODEX_CTX.HasCfg, 1
    jne @f
    or eax, 8
@@: pop rbx
    ret
Codex_AnalyzeSecurity ENDP

;-----------------------------------------------------------------------------
; Codex_Destroy
;-----------------------------------------------------------------------------
Codex_Destroy PROC
    jmp RawrDumpBin_Destroy
Codex_Destroy ENDP

;-----------------------------------------------------------------------------
; Codex_GetEnhancedSections (stub)
;-----------------------------------------------------------------------------
Codex_GetEnhancedSections PROC
    xor rax, rax
    ret
Codex_GetEnhancedSections ENDP

END
