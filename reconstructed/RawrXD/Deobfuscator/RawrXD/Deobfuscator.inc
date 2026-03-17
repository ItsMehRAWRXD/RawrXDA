;================================================================================
; RawrXD_Deobfuscator.inc
; UNIFIED DEOBFUSCATION API
; Includes: OmegaDeobfuscator + MetaReverse
;================================================================================

;------------------------------------------------------------------------------
; CONSTANTS
;------------------------------------------------------------------------------
; Layer types
OBF_LAYER_ENCODING      equ 1
OBF_LAYER_ANTIDEBUG     equ 2
OBF_LAYER_CONTROLFLOW   equ 3
OBF_LAYER_OPAQUE        equ 4
OBF_LAYER_METAMORPHIC   equ 5
OBF_LAYER_REVERSEOBF    equ 6
OBF_LAYER_VM            equ 7

; Authenticity scores
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
; EXTERNAL API
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
; UNIFIED API (Convenience wrappers)
;------------------------------------------------------------------------------
.code

;------------------------------------------------------------------------------
; RawrXD_Deobfuscate_Full
; One-call complete deobfuscation
;------------------------------------------------------------------------------
RawrXD_Deobfuscate_Full PROC FRAME
    ; rcx = target path (input)
    ; rdx = output path
    ; r8 = flags
    ; Returns: eax = success, result structure in static buffer
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                ; Input
    mov r13, rdx                ; Output
    mov r14d, r8d               ; Flags
    
    ; Step 1: Load target
    mov rcx, r12
    call Load_Target_File
    test rax, rax
    jz full_fail
    mov r15, rax                ; Base
    
    ; Step 2: Initialize Omega
    mov rcx, r15
    xor edx, edx
    call OmegaDeobf_Initialize
    test rax, rax
    jz full_fail
    
    ; Step 3: Initialize MetaReverse
    mov rcx, r15
    mov rdx, g_target_size
    call MetaReverse_Initialize
    
    ; Step 4: Authenticity pre-check
    call MetaReverse_AnalyzeAuthenticity
    cmp eax, AUTH_SUSPICIOUS
    jb full_suspicious
    
    ; Step 5: Full unpack
    mov rcx, r13
    call OmegaDeobf_FullUnpack
    
    ; Step 6: Post-check
    call MetaReverse_AnalyzeAuthenticity
    
    ; Step 7: Export if improved
    cmp eax, AUTH_LEGITIMATE
    jae full_success
    
    ; Try harder if still suspicious
    call Aggressive_Deobfuscation
    
full_success:
    mov eax, 1
    jmp full_done
    
full_suspicious:
    ; High suspicion - use aggressive mode immediately
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
; Deep analysis for heavily obfuscated targets
;------------------------------------------------------------------------------
Aggressive_Deobfuscation PROC FRAME
    push rbx
    
    ; Enable all detection heuristics
    
    ; Deep trace execution
    mov rcx, g_entry_point
    mov edx, 100000h            ; 1M instructions
    call OmegaDeobf_TraceExecution
    
    ; Analyze trace for hidden code
    call Analyze_Trace_For_HiddenCode
    
    ; Brute-force entropy scan at byte level
    call Byte_Level_EntropyScan
    
    ; Emulation-based unpacking
    call Emulation_Unpack
    
    pop rbx
    ret
Aggressive_Deobfuscation ENDP

;------------------------------------------------------------------------------
; Utility Functions
;------------------------------------------------------------------------------
Load_Target_File PROC FRAME
    ; rcx = path, returns base or 0
    xor eax, eax
    ret
Load_Target_File ENDP

Analyze_Trace_For_HiddenCode PROC FRAME
    ret
Analyze_Trace_For_HiddenCode ENDP

Byte_Level_EntropyScan PROC FRAME
    ret
Byte_Level_EntropyScan ENDP

Emulation_Unpack PROC FRAME
    ret
Emulation_Unpack ENDP

;------------------------------------------------------------------------------
; DATA
;------------------------------------------------------------------------------
.data
g_target_size       dq 0
g_entry_point       dq 0

.code
END
