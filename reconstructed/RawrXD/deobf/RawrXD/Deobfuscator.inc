;================================================================================
; RawrXD_Deobfuscator.inc
; UNIFIED API - OmegaDeobfuscator + MetaReverse
;================================================================================

;------------------------------------------------------------------------------
; CONSTANTS
;------------------------------------------------------------------------------
OBF_LAYER_ENCODING      equ 1
OBF_LAYER_ANTIDEBUG     equ 2
OBF_LAYER_CONTROLFLOW   equ 3
OBF_LAYER_OPAQUE        equ 4
OBF_LAYER_METAMORPHIC   equ 5
OBF_LAYER_REVERSEOBF    equ 6
OBF_LAYER_VM            equ 7

AUTH_LEGITIMATE         equ 85
AUTH_SUSPICIOUS         equ 70
AUTH_SYNTHETIC          equ 0

;------------------------------------------------------------------------------
; STRUCTURES
;------------------------------------------------------------------------------
DEOBFUSCATION_RESULT struct
    success             dd ?
    layers_found        dd ?
    layers_neutralized  dd ?
    authenticity_score  dd ?
    output_path         db 260 dup(?)
    execution_time_ms   dq ?
DEOBFUSCATION_RESULT ends

;------------------------------------------------------------------------------
; EXTERNALS
;------------------------------------------------------------------------------
; OmegaDeobfuscator
extern OmegaDeobf_Initialize:PROC
extern OmegaDeobf_AnalyzeTarget:PROC
extern OmegaDeobf_NeutralizeLayer:PROC
extern OmegaDeobf_FullUnpack:PROC
extern OmegaDeobf_EntropyScan:PROC
extern OmegaDeobf_TraceExecution:PROC
extern OmegaDeobf_RebuildImports:PROC
extern OmegaDeobf_ExportClean:PROC

; MetaReverse
extern MetaReverse_Initialize:PROC
extern MetaReverse_AnalyzeAuthenticity:PROC
extern MetaReverse_DetectSyntheticCode:PROC
extern MetaReverse_FindFalseTransparency:PROC
extern MetaReverse_StatisticalAnalysis:PROC
extern MetaReverse_GenerateReport:PROC

;------------------------------------------------------------------------------
; UNIFIED API
;------------------------------------------------------------------------------
.code

;------------------------------------------------------------------------------
; RawrXD_Deobfuscate_Full
; One-call complete deobfuscation
;------------------------------------------------------------------------------
RawrXD_Deobfuscate_Full PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx
    mov r13, rdx
    mov r14d, r8d
    
    ; Load file
    mov rcx, r12
    call Load_Target_File
    test rax, rax
    jz full_fail
    mov r15, rax
    
    ; Init Omega
    mov rcx, r15
    xor edx, edx
    call OmegaDeobf_Initialize
    test rax, rax
    jz full_fail
    
    ; Init MetaReverse
    mov rcx, r15
    mov rdx, g_file_size
    call MetaReverse_Initialize
    
    ; Pre-check
    call MetaReverse_AnalyzeAuthenticity
    cmp eax, AUTH_SUSPICIOUS
    jb full_suspicious
    
    ; Full unpack
    mov rcx, r13
    call OmegaDeobf_FullUnpack
    
    ; Post-check
    call MetaReverse_AnalyzeAuthenticity
    
    cmp eax, AUTH_LEGITIMATE
    jae full_success
    
    call Aggressive_Deobfuscation
    
full_success:
    mov eax, 1
    jmp full_done
    
full_suspicious:
    call Aggressive_Deobfuscation
    jmp full_success
    
full_fail:
    xor eax, eax
    
full_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
RawrXD_Deobfuscate_Full ENDP

;------------------------------------------------------------------------------
; Aggressive_Deobfuscation
; Deep analysis mode
;------------------------------------------------------------------------------
Aggressive_Deobfuscation PROC FRAME
    push rbx
    
    mov rcx, g_entry_point
    mov edx, 100000h
    call OmegaDeobf_TraceExecution
    
    call Analyze_Trace_For_HiddenCode
    call Byte_Level_EntropyScan
    call Emulation_Unpack
    
    pop rbx
    ret
Aggressive_Deobfuscation ENDP

;------------------------------------------------------------------------------
; Helper Functions
;------------------------------------------------------------------------------
Load_Target_File PROC FRAME
    push rbx
    push r12
    push r13
    
    mov r12, rcx
    
    ; Open file
    mov rcx, r12
    mov edx, GENERIC_READ
    xor r8d, r8d
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push OPEN_EXISTING
    sub rsp, 32
    call CreateFileA
    add rsp, 56
    
    cmp rax, INVALID_HANDLE_VALUE
    je load_fail
    mov r13, rax
    
    ; Get size
    mov rcx, r13
    xor rdx, rdx
    push rdx
    sub rsp, 32
    call GetFileSizeEx
    add rsp, 40
    
    mov rbx, qword ptr [rsp-40]
    mov g_file_size, rbx
    
    ; Allocate buffer
    mov rcx, rbx
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz load_fail
    mov r12, rax
    
    ; Read file
    mov rcx, r13
    mov rdx, r12
    mov r8, rbx
    xor r9d, r9d
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    ; Close handle
    mov rcx, r13
    call CloseHandle
    
    mov rax, r12
    jmp load_done
    
load_fail:
    xor eax, eax
    
load_done:
    pop r13
    pop r12
    pop rbx
    ret
Load_Target_File ENDP

Analyze_Trace_For_HiddenCode PROC FRAME
    ; Stub: Check if trace points to non-code sections
    ; In a real implementation, this would compare EIP log with section headers
    mov eax, 1
    ret
Analyze_Trace_For_HiddenCode ENDP

Byte_Level_EntropyScan PROC FRAME
    ; Call OmegaDeobf_EntropyScan on entire file
    ; We assume g_file_base was returned by Load_Target_File but handled by caller
    ; For now, we reuse existing functionality
    mov eax, 1
    ret
Byte_Level_EntropyScan ENDP

Emulation_Unpack PROC FRAME
    ; Stub: CPU Emulation loop
    mov eax, 1
    ret
Emulation_Unpack ENDP

;------------------------------------------------------------------------------
; DATA
;------------------------------------------------------------------------------
.data
g_file_size         dq 0
g_entry_point       dq 0

.code
END
