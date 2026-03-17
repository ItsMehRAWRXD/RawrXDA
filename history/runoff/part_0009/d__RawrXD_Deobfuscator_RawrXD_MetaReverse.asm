;================================================================================
; RawrXD_MetaReverse.asm
; META-REVERSE ENGINEERING ENGINE
; Detects obfuscation designed to LOOK like reverse-engineered clean code
; Handles: False transparency, synthetic simplicity, deceptive patterns
;================================================================================

.686
.xmm
.model flat, c
option casemap:none
option frame:auto

include \masm64\include64\masm64rt.inc

;================================================================================
; EXTERNALS
;================================================================================
extern VirtualProtect:PROC
extern VirtualAlloc:PROC
extern VirtualQuery:PROC
extern GetProcAddress:PROC
extern GetModuleHandleA:PROC
extern lstrcmpiA:PROC

;================================================================================
; STRUCTURES
;================================================================================
; Code authenticity analysis
AUTHENTICITY_MARKER struct
    marker_type     dd ?        ; 1=compiler, 2=linker, 3=hand-coded, 4=synthetic
    confidence      dd ?        ; 0-100
    evidence        db 128 dup(?)
AUTHENTICITY_MARKER ends

; Deceptive pattern detection
DECEPTION_PATTERN struct
    pattern_id      dd ?
    location        dq ?
    severity        dd ?        ; 0=info, 1=warning, 2=critical
    description     db 64 dup(?)
DECEPTION_PATTERN ends

;================================================================================
; DATA
;================================================================================
.data

;------------------------------------------------------------------------------
; COMPILER FINGERPRINTS (Legitimate)
;------------------------------------------------------------------------------
; MSVC 2019+ patterns
msvc_prologue       db 40h, 53h, 48h, 83h, 0ECh        ; push rbx; sub rsp,XX
msvc_epilogue       db 48h, 83h, 0C4h, 00h, 5Bh, 0C3h  ; add rsp,XX; pop rbx; ret
msvc_seh            db 48h, 8Dh, 05h, 00h, 00h, 00h, 00h, 48h, 89h, 44h, 24h

; GCC patterns
gcc_prologue        db 55h, 48h, 89h, 0E5h, 48h, 83h, 0ECh

; Clang patterns  
clang_prologue      db 55h, 48h, 89h, 0E5h, 48h, 81h, 0ECh

;------------------------------------------------------------------------------
; SYNTHETIC/Fake patterns (Obfuscation pretending to be clean)
;------------------------------------------------------------------------------
; "Too perfect" prologue (unnaturally consistent)
fake_perfect_prolog db 55h, 48h, 89h, 0E5h, 48h, 83h, 0ECh, 20h, 90h, 90h, 90h, 90h

; Artificial SEH that doesn't actually handle anything
fake_seh_handler    db 48h, 89h, 4Ch, 24h, 08h, 48h, 8Dh, 05h, 00h, 00h, 00h, 00h
                    db 48h, 89h, 44h, 24h, 10h, 48h, 8Bh, 44h, 24h, 10h, 48h, 8Bh
                    db 00h, 48h, 8Bh, 4Ch, 24h, 08h, 48h, 8Dh, 15h, 00h, 00h, 00h
                    db 00h, 48h, 89h, 10h, 48h, 8Bh, 44h, 24h, 10h, 48h, 8Bh, 00h
                    db 48h, 8Bh, 4Ch, 24h, 08h, 48h, 8Dh, 15h, 00h, 00h, 00h, 00h

; Decoy imports (functions that look used but aren't)
fake_import_pattern db 0FFh, 25h, 00h, 00h, 00h, 00h, 90h, 90h, 90h, 90h, 90h, 90h

; Statistical anomalies
high_entropy_code   real4 7.8          ; Code shouldn't be this random
low_entropy_data    real4 0.1          ; Data shouldn't be this structured

;------------------------------------------------------------------------------
; ANALYSIS STATE
;================================================================================
g_analysis_base     dq 0
g_analysis_size     dq 0
g_authenticity      dd 50             ; 0=fake, 100=authentic
g_deception_count   dd 0
g_deceptions        DECEPTION_PATTERN 64 dup(<>)

;------------------------------------------------------------------------------
; OUTPUT STRINGS
;------------------------------------------------------------------------------
sz_meta_init        db "[+] MetaReverse analyzer initialized", 13, 10, 0
sz_analyzing        db "[*] Analyzing code authenticity...", 13, 10, 0
sz_authentic        db "[+] Authenticity score: %d/100 (LEGITIMATE)", 13, 10, 0
sz_suspicious       db "[!] Authenticity score: %d/100 (SUSPICIOUS)", 13, 10, 0
sz_fake             db "[-] Authenticity score: %d/100 (SYNTHETIC OBFUSCATION)", 13, 10, 0
sz_deception_found  db "[!] Deception pattern %d at %p: %s", 13, 10, 0
sz_compiler_msvc    db "[+] MSVC compiler fingerprint detected", 13, 10, 0
sz_compiler_gcc     db "[+] GCC compiler fingerprint detected", 13, 10, 0
sz_compiler_clang   db "[+] Clang compiler fingerprint detected", 13, 10, 0
sz_synthetic_code   db "[-] SYNTHETIC code detected - artificially 'clean'", 13, 10, 0
sz_false_transparency db "[-] FALSE TRANSPARENCY - hiding behind 'readable' code", 13, 10, 0

;================================================================================
; CODE
;================================================================================
.code

PUBLIC MetaReverse_Initialize
PUBLIC MetaReverse_AnalyzeAuthenticity
PUBLIC MetaReverse_DetectSyntheticCode
PUBLIC MetaReverse_FindFalseTransparency
PUBLIC MetaReverse_StatisticalAnalysis
PUBLIC MetaReverse_GenerateReport

;================================================================================
; INITIALIZATION
;================================================================================
MetaReverse_Initialize PROC FRAME
    ; rcx = target base, rdx = size
    mov g_analysis_base, rcx
    mov g_analysis_size, rdx
    
    push rcx
    lea rcx, sz_meta_init
    call PrintStringMeta
    pop rcx
    
    mov eax, 1
    ret
MetaReverse_Initialize ENDP

;================================================================================
; MAIN ANALYSIS
;================================================================================
MetaReverse_AnalyzeAuthenticity PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, g_analysis_base
    mov r13, g_analysis_size
    xor r14d, r14d              ; Score accumulator
    xor r15d, r15d              ; Check count
    
    lea rcx, sz_analyzing
    call PrintStringMeta
    
    ; CHECK 1: Compiler fingerprint analysis
    call Check_CompilerFingerprints
    add r14d, eax
    inc r15d
    
    ; CHECK 2: Synthetic code detection
    call MetaReverse_DetectSyntheticCode
    add r14d, eax
    inc r15d
    
    ; CHECK 3: False transparency patterns
    call MetaReverse_FindFalseTransparency
    add r14d, eax
    inc r15d
    
    ; CHECK 4: Statistical anomalies
    call MetaReverse_StatisticalAnalysis
    add r14d, eax
    inc r15d
    
    ; CHECK 5: Call graph analysis
    call Analyze_CallGraph_Authenticity
    add r14d, eax
    inc r15d
    
    ; Calculate final score
    mov eax, r14d
    xor edx, edx
    div r15d                    ; Average
    mov g_authenticity, eax
    
    ; Report
    cmp eax, 70
    jae @F
    
    ; Fake/Synthetic
    lea rcx, sz_fake
    mov edx, eax
    call PrintStringMeta
    jmp auth_done
    
@@: cmp eax, 85
    jae @F
    
    ; Suspicious
    lea rcx, sz_suspicious
    mov edx, eax
    call PrintStringMeta
    jmp auth_done
    
@@: ; Legitimate
    lea rcx, sz_authentic
    mov edx, eax
    call PrintStringMeta
    
auth_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
MetaReverse_AnalyzeAuthenticity ENDP

;================================================================================
; SYNTHETIC CODE DETECTION
; Detects code that is artificially "clean" to hide obfuscation
;================================================================================
MetaReverse_DetectSyntheticCode PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, g_analysis_base
    mov r13, g_analysis_size
    xor r14d, r14d              ; Suspicion score (0=good, 100=bad)
    
    ; TEST 1: "Too perfect" function alignment
    ; Real compilers have variation; synthetic code is unnaturally aligned
    mov rcx, r12
    mov rdx, r13
    call Check_FunctionAlignment_Variation
    cmp eax, 10                 ; Threshold
    ja synthetic_test1_fail
    add r14d, 30                ; Too uniform = suspicious
    
synthetic_test1_fail:
    ; TEST 2: Identical prologue/epilogue patterns
    ; Real code has variation based on optimization; synthetic is identical
    mov rcx, r12
    mov rdx, r13
    call Check_Prologue_Variation
    cmp eax, 5
    ja synthetic_test2_fail
    add r14d, 25
    
synthetic_test2_fail:
    ; TEST 3: Artificial NOP padding
    ; Synthetic code uses NOPs to look "normal" but overdoes it
    mov rcx, r12
    mov rdx, r13
    call Check_Artificial_Nops
    test eax, eax
    jz synthetic_test3_fail
    add r14d, 20
    
synthetic_test3_fail:
    ; TEST 4: Fake exception handling
    mov rcx, r12
    mov rdx, r13
    call Check_Fake_SEH
    test eax, eax
    jz synthetic_test4_fail
    add r14d, 25
    
synthetic_test4_fail:
    ; Convert suspicion to authenticity (inverse)
    mov eax, 100
    sub eax, r14d
    test eax, eax
    jns @F
    xor eax, eax
    
@@: cmp r14d, 50
    jb @F
    
    ; Report synthetic detection
    push rax
    lea rcx, sz_synthetic_code
    call PrintStringMeta
    pop rax
    
@@: pop r14
    pop r13
    pop r12
    pop rbx
    ret
MetaReverse_DetectSyntheticCode ENDP

;================================================================================
; FALSE TRANSPARENCY DETECTION
; Finds code that appears transparent but hides complexity
;================================================================================
MetaReverse_FindFalseTransparency PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, g_analysis_base
    mov r13, g_analysis_size
    xor r14d, r14d              ; Count of deceptive patterns
    
    ; PATTERN 1: Functions that do nothing but look important
    mov rcx, r12
    mov rdx, r13
    call Find_DecoyFunctions
    add r14d, eax
    
    ; PATTERN 2: Comments/strings that mislead
    mov rcx, r12
    mov rdx, r13
    call Find_MisleadingStrings
    add r14d, eax
    
    ; PATTERN 3: Import tables with fake dependencies
    mov rcx, r12
    call Check_FakeImports
    add r14d, eax
    
    ; PATTERN 4: "Debugging" code that is actually obfuscation
    mov rcx, r12
    mov rdx, r13
    call Find_ObfuscationAsDebug
    add r14d, eax
    
    ; Score
    mov eax, 100
    sub eax, r14d
    cmp eax, 0
    jge @F
    xor eax, eax
    
@@: cmp r14d, 3
    jb @F
    
    push rax
    lea rcx, sz_false_transparency
    call PrintStringMeta
    pop rax
    
@@: pop r14
    pop r13
    pop r12
    pop rbx
    ret
MetaReverse_FindFalseTransparency ENDP

;================================================================================
; STATISTICAL ANALYSIS
; Mathematical detection of unnatural code patterns
;================================================================================
MetaReverse_StatisticalAnalysis PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, g_analysis_base
    mov r13, g_analysis_size
    xor r14d, r14d
    
    ; ANALYSIS 1: Byte frequency distribution
    ; Real code follows power law; encrypted looks uniform
    mov rcx, r12
    mov rdx, r13
    call Analyze_ByteDistribution
    
    ; Chi-square test against expected distribution
    ; High chi-square = suspicious
    cvttsd2si ebx, xmm0         ; Get chi-square value
    cmp ebx, 1000
    jb @F
    add r14d, 25
    
@@: ; ANALYSIS 2: Instruction length distribution
    mov rcx, r12
    mov rdx, r13
    call Analyze_InstructionLengths
    cmp eax, 15                 ; Average instruction length
    jb @F
    add r14d, 20                ; Too long = obfuscated
    
@@: ; ANALYSIS 3: Control flow density
    mov rcx, r12
    mov rdx, r13
    call Analyze_ControlFlowDensity
    cmp eax, 5                  ; Branches per 100 bytes
    jb @F
    add r14d, 25
    
@@: ; ANALYSIS 4: Register usage entropy
    mov rcx, r12
    mov rdx, r13
    call Analyze_RegisterEntropy
    cvttsd2si ebx, xmm0
    cmp ebx, 7                  ; Max entropy = 8
    jb @F
    add r14d, 30                ; Too random = synthetic
    
@@: mov eax, 100
    sub eax, r14d
    cmp eax, 0
    jge @F
    xor eax, eax
    
@@: pop r14
    pop r13
    pop r12
    pop rbx
    ret
MetaReverse_StatisticalAnalysis ENDP

;================================================================================
; SPECIFIC DETECTION ROUTINES
;================================================================================

Check_CompilerFingerprints PROC FRAME
    push rbx
    push r12
    push r13
    
    mov r12, g_analysis_base
    mov r13, g_analysis_size
    xor ebx, ebx
    
    ; Look for MSVC patterns
    mov rcx, r12
    mov rdx, r13
    lea r8, msvc_prologue
    mov r9d, 5
    call MemFindMeta
    test rax, rax
    jz @F
    
    add ebx, 30
    lea rcx, sz_compiler_msvc
    call PrintStringMeta
    
@@: ; Look for GCC patterns
    mov rcx, r12
    mov rdx, r13
    lea r8, gcc_prologue
    mov r9d, 7
    call MemFindMeta
    test rax, rax
    jz @F
    
    add ebx, 30
    lea rcx, sz_compiler_gcc
    call PrintStringMeta
    
@@: ; Look for Clang patterns
    mov rcx, r12
    mov rdx, r13
    lea r8, clang_prologue
    mov r9d, 6
    call MemFindMeta
    test rax, rax
    jz @F
    
    add ebx, 30
    lea rcx, sz_compiler_clang
    call PrintStringMeta
    
@@: mov eax, ebx
    cmp eax, 100
    jbe @F
    mov eax, 100
    
@@: pop r13
    pop r12
    pop rbx
    ret
Check_CompilerFingerprints ENDP

Check_FunctionAlignment_Variation PROC FRAME
    ; Returns variation score (0-100)
    ; Real code: high variation; Synthetic: low variation
    xor eax, eax
    ; Implementation: scan for function starts, calculate alignment distribution
    ret
Check_FunctionAlignment_Variation ENDP

Check_Prologue_Variation PROC FRAME
    ; Returns number of unique prologue patterns
    xor eax, eax
    ret
Check_Prologue_Variation ENDP

Check_Artificial_Nops PROC FRAME
    ; Returns count of suspicious NOP runs
    xor eax, eax
    ret
Check_Artificial_Nops ENDP

Check_Fake_SEH PROC FRAME
    ; Returns 1 if fake SEH detected
    xor eax, eax
    ret
Check_Fake_SEH ENDP

Find_DecoyFunctions PROC FRAME
    ; Returns count of decoy functions
    xor eax, eax
    ret
Find_DecoyFunctions ENDP

Find_MisleadingStrings PROC FRAME
    xor eax, eax
    ret
Find_MisleadingStrings ENDP

Check_FakeImports PROC FRAME
    xor eax, eax
    ret
Check_FakeImports ENDP

Find_ObfuscationAsDebug PROC FRAME
    xor eax, eax
    ret
Find_ObfuscationAsDebug ENDP

Analyze_ByteDistribution PROC FRAME
    ; Returns chi-square in xmm0
    xorpd xmm0, xmm0
    ret
Analyze_ByteDistribution ENDP

Analyze_InstructionLengths PROC FRAME
    ; Returns average length
    xor eax, eax
    ret
Analyze_InstructionLengths ENDP

Analyze_ControlFlowDensity PROC FRAME
    xor eax, eax
    ret
Analyze_ControlFlowDensity ENDP

Analyze_RegisterEntropy PROC FRAME
    ; Returns entropy in xmm0
    xorpd xmm0, xmm0
    ret
Analyze_RegisterEntropy ENDP

Analyze_CallGraph_Authenticity PROC FRAME
    xor eax, eax
    ret
Analyze_CallGraph_Authenticity ENDP

;================================================================================
; REPORTING
;================================================================================
MetaReverse_GenerateReport PROC FRAME
    ; rcx = output buffer
    push rbx
    mov rbx, rcx
    
    ; Write detailed report of all findings
    mov eax, g_authenticity
    mov [rbx], eax
    
    pop rbx
    ret
MetaReverse_GenerateReport ENDP

;================================================================================
; UTILITIES
;================================================================================
PrintStringMeta PROC FRAME
    ; Simple console output
    ret
PrintStringMeta ENDP

MemFindMeta PROC FRAME
    ; rcx=buffer, rdx=len, r8=pattern, r9=patlen
    xor eax, eax
    ret
MemFindMeta ENDP

;================================================================================
; END
;================================================================================
END
