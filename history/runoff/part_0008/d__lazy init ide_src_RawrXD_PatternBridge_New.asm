; RawrXD_PatternBridge.asm
; Production AVX-512 Pattern Engine for RawrXD IDE
; Assemble: ml64 /c /FoRawrXD_PatternBridge.obj RawrXD_PatternBridge.asm
; Link: link /DLL /OUT:RawrXD_PatternBridge.dll RawrXD_PatternBridge.obj

EXTERN calloc:PROC
EXTERN free:PROC

; ============================================
; EXPORTS
; ============================================
PUBLIC InitializePatternEngine
PUBLIC ClassifyPattern
PUBLIC ShutdownPatternEngine
PUBLIC GetPatternStats
PUBLIC DllMain

; ============================================
; CONSTANTS
; ============================================
AVX512F_BIT     EQU 10000h
XCR0_ZMM        EQU 0E0h       ; ZMM_HI16 | ZMM_LO16 | YMM | XMM

; Pattern type indices
PAT_TODO        EQU 0
PAT_FIXME       EQU 1
PAT_XXX         EQU 2
PAT_HACK        EQU 3
PAT_BUG         EQU 4
PAT_NOTE        EQU 5
PAT_IDEA        EQU 6
PAT_REVIEW      EQU 7
PAT_UNKNOWN     EQU 8

; ============================================
; DATA SECTION
; ============================================
.const

; 8 patterns packed into single ZMM register (512 bits)
ALIGN 16
PatternTable QWORD \
    0544F444F444F444Fh,  ; "OTOD" (little-endian TODO)
    0454D4946454D4946h,  ; "EMIF" (little-endian FIXME)  
    05858585858585858h,  ; "XXXX"
    04B4341484B434148h,  ; "KCAH" (HACK)
    04755424755424755h,  ; "UBUG" (BUG)
    045544F4E45544F4Eh,  ; "NOTE"
    04145444941454449h,  ; "IDEA"
    05749564552574956h   ; "VIER" (REVIEW)

PatternLengths DWORD 4, 5, 3, 4, 3, 4, 4, 6
PatternPrios   DWORD 1, 2, 2, 1, 3, 0, 0, 1

; ASCII lookup for scalar fallback
PatternASCII BYTE "TODO",0,0,0,0
             BYTE "FIXME",0,0,0
             BYTE "XXX",0,0,0,0,0
             BYTE "HACK",0,0,0,0
             BYTE "BUG",0,0,0,0,0
             BYTE "NOTE",0,0,0,0
             BYTE "IDEA",0,0,0,0
             BYTE "REVIEW",0,0

.data
ALIGN 16

; Engine state
g_Initialized   DWORD 0
g_HasAVX512     DWORD 0
g_TotalScanned  QWORD 0
g_TotalMatched  QWORD 0
g_ByType        QWORD 8 DUP(0)

; Result structure (C# interop layout)
ResultStruct STRUCT
    s_Type      QWORD ?     ; +0
    s_Conf      REAL8 ?     ; +8  
    s_Line      DWORD ?     ; +16
    s_Prio      DWORD ?     ; +20
ResultStruct ENDS

; ============================================
; CODE SECTION
; ============================================
.code

; --------------------------------------------
; DllMain - DLL entry point
; --------------------------------------------
DllMain PROC hInst:QWORD, reason:DWORD, reserved:QWORD
    cmp reason, 1 ; DLL_PROCESS_ATTACH
    je @@init
    cmp reason, 0 ; DLL_PROCESS_DETACH
    je @@cleanup
    mov eax, 1
    ret
    
@@init:
    mov g_Initialized, 0
    mov g_HasAVX512, 0
    mov eax, 1
    ret
    
@@cleanup:
    mov g_Initialized, 0
    mov eax, 1
    ret
DllMain ENDP

; --------------------------------------------
; CheckAVX512Support
; Returns: EAX=1 if AVX-512F supported, 0 otherwise
; --------------------------------------------
CheckAVX512Support PROC
    push rbx
    
    ; CPUID leaf 7, subleaf 0
    mov eax, 7
    xor ecx, ecx
    cpuid
    test ebx, AVX512F_BIT
    jz @@no_avx512
    
    ; XGETBV check
    xor ecx, ecx
    xgetbv
    and eax, XCR0_ZMM
    cmp eax, XCR0_ZMM
    jne @@no_avx512
    
    pop rbx
    mov eax, 1
    ret
    
@@no_avx512:
    pop rbx
    xor eax, eax
    ret
CheckAVX512Support ENDP

; --------------------------------------------
; InitializePatternEngine
; Returns: 0=success, -1=no AVX-512 (scalar mode)
; --------------------------------------------
InitializePatternEngine PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call CheckAVX512Support
    mov g_HasAVX512, eax
    
    ; Preload patterns into ZMM15 if AVX-512 available
    test eax, eax
    jz @@scalar_init
    
    vmovdqa64 zmm15, zmmword ptr [PatternTable]
    
@@scalar_init:
    mov g_Initialized, 1
    mov g_TotalScanned, 0
    mov g_TotalMatched, 0
    
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
InitializePatternEngine ENDP

; --------------------------------------------
; ClassifyPattern - Main classification
; RCX=buffer, EDX=length, R8=result ptr
; Returns: 0=success, -1=error
; --------------------------------------------
ClassifyPattern PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 72
    .allocstack 72
    .endprolog
    
    ; Validate inputs
    test rcx, rcx
    jz @@invalid
    test r8, r8
    jz @@invalid
    test edx, edx
    jz @@invalid
    
    ; Save parameters
    mov rsi, rcx          ; RSI = buffer
    mov r12d, edx         ; R12D = remaining length
    mov rbx, r8           ; RBX = result struct
    
    ; Init result to Unknown
    mov qword ptr [rbx].ResultStruct.s_Type, PAT_UNKNOWN
    mov qword ptr [rbx].ResultStruct.s_Conf, 0
    mov dword ptr [rbx].ResultStruct.s_Line, 1
    mov dword ptr [rbx].ResultStruct.s_Prio, 0
    
    ; Working registers
    xor r13d, r13d        ; R13D = line number
    xor r14d, r14d        ; R14D = best pattern index
    xorpd xmm6, xmm6      ; XMM6 = best confidence
    
    ; Check AVX-512 availability
    cmp g_HasAVX512, 1
    jne @@scalar_main

; ============================================
; AVX-512 FAST PATH
; ============================================
@@avx512_loop:
    cmp r12d, 64
    jb @@scalar_tail
    
    ; Load 64 bytes into ZMM0 (handle alignment)
    mov rax, rsi
    test al, 63
    jnz @@unaligned_load
    
    vmovdqa64 zmm0, zmmword ptr [rsi]
    jmp @@compare
    
@@unaligned_load:
    vmovdqu64 zmm0, zmmword ptr [rsi]
    
@@compare:
    ; Compare all 8 patterns simultaneously
    vpcmpeqq k1, zmm0, zmm15
    
    ; Check for any match
    kortestw k1, k1
    jz @@no_match
    
    ; Extract match index from mask
    kmovw eax, k1
    tzcnt ecx, eax
    cmp ecx, 8
    jae @@no_match
    
    ; Valid match found
    mov r14d, ecx
    mov rax, 03FF0000000000000h  ; 1.0 double
    movq xmm6, rax
    mov [rbx].ResultStruct.s_Line, r13d
    
@@no_match:
    add rsi, 64
    sub r12d, 64
    inc r13d
    jmp @@avx512_loop

; ============================================
; SCALAR FALLBACK
; ============================================
@@scalar_tail:
@@scalar_main:
    cmp r12d, 3
    jb @@store_result
    
    ; Check each pattern
    xor ecx, ecx          ; Pattern index
    
@@check_pattern:
    cmp ecx, 8
    jae @@next_byte
    
    ; Use explicit indexed addressing for arrays to be safe in 64-bit MASM
    lea rax, PatternLengths
    mov eax, [rax + rcx*4]
    cmp r12d, eax
    jb @@next_pat
    
    ; Compare pattern
    lea rdx, PatternASCII
    mov r8, rsi
    ; We need to offset into PatternASCII.
    ; PatternASCII is just bytes, but not fixed width.
    ; Wait, the definition was:
    ; BYTE "TODO",0,0,0,0 (8 bytes)
    ; BYTE "FIXME",0,0,0 (8 bytes)
    ; Each is effectively 8 bytes in storage
    shl ecx, 3
    add rdx, rcx
    shr ecx, 3
    
    push rcx
    lea rax, PatternLengths
    mov r9d, [rax + rcx*4]
    call @@memcmp
    pop rcx
    test eax, eax
    jnz @@next_pat
    
    ; Match found
    mov r14d, ecx
    mov rax, 03FF0000000000000h
    movq xmm6, rax
    mov [rbx].ResultStruct.s_Line, r13d
    
@@next_pat:
    inc ecx
    jmp @@check_pattern
    
@@next_byte:
    mov al, [rsi]
    cmp al, 0Ah
    je @@newline
    cmp al, 0Dh
    jne @@not_newline
@@newline:
    inc r13d
@@not_newline:
    inc rsi
    dec r12d
    jmp @@scalar_main

; ============================================
; Store result
; ============================================
@@store_result:
    mov [rbx].ResultStruct.s_Type, r14
    movsd [rbx].ResultStruct.s_Conf, xmm6
    lea rax, PatternPrios
    mov eax, [rax + r14*4]
    mov [rbx].ResultStruct.s_Prio, eax
    
    inc g_TotalScanned
    lea rax, g_ByType
    inc qword ptr [rax + r14*8]
    
    xor eax, eax
    jmp @@exit
    
@@invalid:
    mov eax, 0FFFFFFFFh
    
@@exit:
    vzeroupper
    add rsp, 72
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret

; --------------------------------------------
; Memcmp helper - returns 0 if equal
; --------------------------------------------
@@memcmp:
    push rdi
    push rsi
    mov rdi, rdx
    mov rsi, r8
    mov ecx, r9d
    xor eax, eax
@@cmp_loop:
    test ecx, ecx
    jz @@cmp_done
    mov dl, [rdi]
    cmp dl, [rsi]
    jne @@cmp_ne
    inc rdi
    inc rsi
    dec ecx
    jmp @@cmp_loop
@@cmp_ne:
    inc eax
@@cmp_done:
    pop rsi
    pop rdi
    ret

ClassifyPattern ENDP

; --------------------------------------------
; ShutdownPatternEngine
; --------------------------------------------
ShutdownPatternEngine PROC
    mov g_Initialized, 0
    vpxor xmm15, xmm15, xmm15
    vzeroupper
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

; --------------------------------------------
; GetPatternStats
; --------------------------------------------
GetPatternStats PROC
    test rcx, rcx
    jz @@invalid
    
    mov rax, g_TotalScanned
    mov [rcx], rax
    mov rax, g_TotalMatched
    mov [rcx+8], rax
    
    lea rax, g_ByType
    mov rdx, rcx
    add rdx, 16
    
    mov r8, [rax]
    mov [rdx], r8
    mov r8, [rax+8]
    mov [rdx+8], r8
    mov r8, [rax+16]
    mov [rdx+16], r8
    mov r8, [rax+24]
    mov [rdx+24], r8
    mov r8, [rax+32]
    mov [rdx+32], r8
    mov r8, [rax+40]
    mov [rdx+40], r8
    mov r8, [rax+48]
    mov [rdx+48], r8
    mov r8, [rax+56]
    mov [rdx+56], r8
    
    xor eax, eax
    ret
    
@@invalid:
    mov eax, 0FFFFFFFFh
    ret
GetPatternStats ENDP

END
