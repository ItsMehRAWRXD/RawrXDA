; Minimal MASM kernel for monolithic build
; Stub implementations for core DMA/compute operations

OPTION CASEMAP:NONE
INCLUDE ssot_beacon.inc

; ============================================================
; EXTERNAL IMPORTS
; ============================================================
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN RtlCopyMemory : PROC
EXTERN RtlZeroMemory : PROC

; ============================================================
; PUBLIC EXPORTS
; ============================================================
PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_InitializeDMA
PUBLIC Titan_ShutdownDMA
PUBLIC Titan_GetDMAStats
PUBLIC SSOT_HasCppOwner
PUBLIC SSOT_GetActiveOwner

; ============================================================
; CONSTANTS
; ============================================================
TITAN_SUCCESS               EQU 0
TITAN_ERROR_INVALID_PARAM   EQU 80000001h

; ============================================================
; DATA SECTION
; ============================================================
.data

g_Initialized           BYTE  0
g_TotalBytesCopied      QWORD 0
g_TotalDMATransfers     QWORD 0
g_FailedTransfers       QWORD 0
g_PeakBandwidthGbps     REAL8 0.0
g_QPCFrequency          QWORD 0

; ============================================================
; CODE SECTION
; ============================================================
.code

; ============================================================
; Titan_ExecuteComputeKernel
; RCX = kernelType, RDX = srcAddr, R8 = destAddr, R9 = size
; Returns EAX = result code
; ============================================================
Titan_ExecuteComputeKernel PROC FRAME
    .ENDPROLOG
    mov eax, TITAN_SUCCESS
    ret
Titan_ExecuteComputeKernel ENDP

; ============================================================
; Titan_PerformCopy
; RCX = src, RDX = dst, R8 = size, R9 = copyType
; Returns RAX = bytes copied
; ============================================================
Titan_PerformCopy PROC FRAME
    .ENDPROLOG
    mov rax, r8          ; Return size
    ret
Titan_PerformCopy ENDP

; ============================================================
; Titan_PerformDMA
; RCX = src, RDX = dst, R8 = size, R9 = dmaType
; Returns EAX = result code
; ============================================================
Titan_PerformDMA PROC FRAME
    .ENDPROLOG
    mov eax, TITAN_SUCCESS
    ret
Titan_PerformDMA ENDP

; ============================================================
; Titan_InitializeDMA
; Returns EAX = result code
; ============================================================
Titan_InitializeDMA PROC FRAME
    .ENDPROLOG
    mov eax, TITAN_SUCCESS
    ret
Titan_InitializeDMA ENDP

; ============================================================
; Titan_ShutdownDMA
; Returns EAX = result code
; ============================================================
Titan_ShutdownDMA PROC FRAME
    .ENDPROLOG
    mov eax, TITAN_SUCCESS
    ret
Titan_ShutdownDMA ENDP

; ============================================================
; Titan_GetDMAStats
; RCX = pointer to stats structure
; Returns EAX = result code
; ============================================================
Titan_GetDMAStats PROC FRAME
    .ENDPROLOG
    mov eax, TITAN_SUCCESS
    ret
Titan_GetDMAStats ENDP

; ============================================================
; SSOT_HasCppOwner
; ECX = FNV1a-32 hash of handler symbol
; Returns EAX = 1 if C++ provider is active for that symbol, else 0
; ============================================================
SSOT_HasCppOwner PROC
    sub rsp, 28h
    call rawr_ssot_owner_for_hash
    cmp eax, RAWR_SSOT_OWNER_NONE
    setne al
    movzx eax, al
    add rsp, 28h
    ret
SSOT_HasCppOwner ENDP

; ============================================================
; SSOT_GetActiveOwner
; Returns EAX = active SSOT owner enum
; ============================================================
SSOT_GetActiveOwner PROC
    sub rsp, 28h
    call rawr_ssot_active_owner
    add rsp, 28h
    ret
SSOT_GetActiveOwner ENDP

END
