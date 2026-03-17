; ================================================
; RXUC-IntegrationLayer v7.0 — The Orchestration Core
; Reverse-Engineered Complete Build Pipeline
; Zero-State Reset | Temp Patch Application | CLI Driver
; ================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

option frame:auto

; ================================================
; External Symbols (Link-Time Resolution)
; ================================================
extrn manifest_agent_limits:proc
extrn agent_controller_execute:proc
extrn patch_pe_headers:proc
extrn lex:proc
extrn parse_function:proc
extrn write_pe_file:proc
extrn cur_tok:proc
extrn advance:proc
extrn emit_prolog:proc
extrn emit_epilog:proc
extrn AgentEngine_Init:proc
extrn AgentEngine_CreateAgent:proc

; WinAPI
externdef GetCommandLineA:qword
externdef GetModuleHandleA:qword
externdef GetProcAddress:qword
externdef LoadLibraryA:qword
externdef CreateFileA:qword
externdef ReadFile:qword
externdef WriteFile:qword
externdef CloseHandle:qword
externdef GetFileSizeEx:qword
externdef VirtualAlloc:qword
externdef VirtualFree:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword
externdef ExitProcess:qword
externdef SetConsoleTitleA:qword
externdef GetTickCount64:qword
externdef QueryPerformanceCounter:qword
externdef QueryPerformanceFrequency:qword

; ================================================
; Integration Constants
; ================================================
INTEGRATION_MAGIC equ 0x5241545241434E49  ; 'INTCARTAR' (Integration Marker)
TEMP_PATCH_MAGIC equ 0x5041544348544554   ; 'TEMPTHACT' (Temp Patch Marker)
MAX_COMPILE_UNITS equ 32
MAX_TEMP_PATCHES equ 256

; Compile Unit States
UNIT_IDLE equ 0
UNIT_SCANNING equ 1
UNIT_AGENT_MANIFEST equ 2
UNIT_COMPILING equ 3
UNIT_LINKING equ 4
UNIT_PATCHING equ 5
UNIT_COMPLETE equ 6

; ================================================
; Complex Structures
; ================================================

; Compile Unit (Per Source File)
COMPILE_UNIT struct
    dwState dd ?                  ; Current state
    dwUnitId dd ?                 ; Unique ID
    qwSourceHash dq ?             ; FNV-1a hash of source
    pSourceBuffer dq ?            ; Source code
    cbSourceSize dq ?             ; Source size
    pOutputBuffer dq ?            ; Compiled code
    cbOutputSize dd ?             ; Output size
    pAgentState dq ?              ; Pointer to agent manifest
    dwComplexityScore dd ?        ; 0-1000 complexity
    dwSignalMask dd ?             ; Detected stress signals
    dwStartTick dd ?              ; Compile start time
    dwEndTick dd ?                ; Compile end time
    bLimitPatched db ?            ; PE limits modified
    bAgentInjected db ?           ; Agent stub injected
    szInputPath db 260 dup(?)     ; Input file path
    szOutputPath db 260 dup(?)    ; Output executable path
    align 8
COMPILE_UNIT ends

; Temp Patch Record (Active Patching)
TEMP_PATCH struct
    dwMagic dd ?                  ; TEMP_PATCH_MAGIC
    dwType dd ?                   ; Patch type (STACK, HEAP, etc)
    dwOffset dd ?                 ; File offset
    dwOldValue dd ?               ; Original
    dwNewValue dd ?               ; Modified
    dwSize dd ?                   ; Patch size
    bApplied db ?                 ; Applied flag
    align 7
TEMP_PATCH ends

; Global Orchestration State
ORCHESTRATION_STATE struct
    qwMagic dq ?                  ; INTEGRATION_MAGIC
    dwUnitCount dd ?              ; Total units
    dwActiveUnit dd ?             ; Currently compiling
    dwGlobalComplexity dd ?       ; Aggregate complexity
    hOrchestrationMutex dq ?      ; Thread safety
    pTempPatchTable dq ?          ; Patch table base
    dwPatchCount dd ?             ; Active patches
    bZeroStateLocked db ?         ; Prevent re-entrancy
    align 7
ORCHESTRATION_STATE ends

; ================================================
; Data Segment
; ================================================
.data
align 16

; Global State
g_Orchestration ORCHESTRATION_STATE <INTEGRATION_MAGIC, 0, 0, 0, 0, 0, 0, 0>

; Compile Unit Pool
g_Units COMPILE_UNIT MAX_COMPILE_UNITS dup(<>)

; Temp Patch Pool (1MB)
g_TempPatchPool db MAX_TEMP_PATCHES * sizeof(TEMP_PATCH) dup(0)

; CLI Strings
szTitle db "RXUC-Orchestrator v7.0 [Agentic Compilation]", 0
szBanner db 10, "╔════════════════════════════════════════════════╗", 10
         db "║  RXUC TerraForm + AgentEngine Integration      ║", 10
         db "║  Reverse-Engineered Process Manifestation      ║", 10
         db "╚════════════════════════════════════════════════╝", 10, 0
szScanning db "[*] Phase 1: Zero-State Reset & Heuristic Scan...", 10, 0
szManifest db "[*] Phase 2: Agent Manifestation...", 10, 0
szCompile db "[*] Phase 3: Adaptive Compilation...", 10, 0
szPatch db "[*] Phase 4: Temp Patch Application...", 10, 0
szSuccess db "[+] Compilation Successful: ", 0
szFailed db "[!] Compilation Failed: ", 0
szComplexity db "    Complexity Score: ", 0
szSignal db "    Stress Signals: ", 0
szLimitKey db "    Limit-Key: ", 0
szZeroState db "[*] Zero-State Reset: ", 0
szClean db "CLEAN", 10, 0
szArrow db " -> ", 0

; Patch Type Names
szPatchStack db "STACK_PIVOT", 0
szPatchHeap db "HEAP_EXPAND", 0
szPatchFiber db "FIBER_MODE", 0
szPatchNuma db "NUMA_LOCAL", 0

; ================================================
; BSS Segment (Uninitialized Data)
; ================================================
.data?
align 4096

; Working Buffers
g_ScratchBuffer db 32768 dup(?)
g_HashTable dd 4096 dup(?)          ; String hash table for symbols
g_PerformanceFreq dq ?

; ================================================
; Code: Integration Core
; ================================================
.code
align 16

; ================================================
; Zero-State Reset Protocol
; ================================================

ZeroState_Reset proc
    ; Performs complete memory sanitization between compiles
    ; Prevents "Logic Bleed" between source files
    push rbp
    mov rbp, rsp
    
    ; 1. Clear all registers (except rsp/rbp)
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; 2. Sanitize FPU/MMX/SSE state
    emms
    pxor xmm0, xmm0
    pxor xmm1, xmm1
    pxor xmm2, xmm2
    pxor xmm3, xmm3
    pxor xmm4, xmm4
    pxor xmm5, xmm5
    pxor xmm6, xmm6
    pxor xmm7, xmm7
    
    ; 3. Clear scratch buffer (non-zero pattern to detect use-after-free)
    lea rdi, g_ScratchBuffer
    mov ecx, 32768 / 8
    mov rax, 0xDEADBEEFC0FFEEEE
    rep stosq
    
    ; 4. Reset hash table
    lea rdi, g_HashTable
    mov ecx, 4096
    xor eax, eax
    rep stosd
    
    ; 5. Clear unit state (except paths)
    lea rdi, g_Units
    mov ecx, MAX_COMPILE_UNITS
clear_unit:
    mov dword ptr [rdi], UNIT_IDLE
    mov qword ptr [rdi+8], 0    ; Hash
    mov qword ptr [rdi+16], 0   ; Source buffer
    mov qword ptr [rdi+24], 0   ; Source size
    mov qword ptr [rdi+32], 0   ; Output buffer
    add rdi, sizeof(COMPILE_UNIT)
    dec ecx
    jnz clear_unit
    
    ; 6. Reset patch pool
    lea rdi, g_TempPatchPool
    mov ecx, (MAX_TEMP_PATCHES * sizeof(TEMP_PATCH)) / 8
    xor eax, eax
    rep stosq
    
    ; 7. Reset orchestration (keep magic)
    mov g_Orchestration.dwUnitCount, 0
    mov g_Orchestration.dwActiveUnit, 0
    mov g_Orchestration.dwGlobalComplexity, 0
    mov g_Orchestration.dwPatchCount, 0
    mov g_Orchestration.bZeroStateLocked, 1
    
    leave
    ret
ZeroState_Reset endp

ZeroState_Unlock proc
    mov g_Orchestration.bZeroStateLocked, 0
    ret
ZeroState_Unlock endp

; ================================================
; Heuristic Source Analysis (Pre-Flight)
; ================================================

CalculateSourceHash proc
    ; RCX = Buffer, RDX = Size
    ; Returns: RAX = FNV-1a 64-bit hash
    push rbp
    mov rbp, rsp
    
    mov rsi, rcx
    mov rbx, rdx
    
    ; FNV-1a 64-bit offset basis
    mov rax, 14695981039346656037
    mov r8, 1099511628211         ; FNV prime
    
hash_loop:
    test rbx, rbx
    jz hash_done
    
    movzx r9, byte ptr [rsi]
    xor rax, r9
    mul r8
    
    inc rsi
    dec rbx
    jmp hash_loop
    
hash_done:
    leave
    ret
CalculateSourceHash endp

AnalyzeComplexityPreFlight proc
    ; RCX = Unit Pointer
    ; Performs deep analysis before agent manifestation
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov r12, rcx                  ; Unit
    mov rsi, [r12+COMPILE_UNIT.pSourceBuffer]
    mov rbx, [r12+COMPILE_UNIT.cbSourceSize]
    add rbx, rsi                  ; End pointer
    
    xor r13d, r13d                ; Cyclomatic complexity
    xor r14d, r14d                ; Data complexity
    xor r15d, r15d                ; Nesting depth
    
    mov [rbp-4], esi              ; Current line start
    
line_loop:
    cmp rsi, rbx
    jae analysis_done
    
    ; Check for control flow keywords
    cmp dword ptr [rsi], 20666920h    ; " if"
    je control_flow
    cmp dword ptr [rsi], 656C7768h    ; "whil" (while)
    je control_flow
    cmp dword ptr [rsi], 726F6620h    ; " for"
    je control_flow
    cmp dword ptr [rsi], 636E7566h    ; "func"
    je control_flow
    
    ; Check for data declarations
    cmp dword ptr [rsi], 2074656Ch    ; " let"
    je data_decl
    cmp dword ptr [rsi], 74736964h    ; "dist" (distinct types)
    je data_decl
    
    ; Track nesting
    cmp byte ptr [rsi], '{'
    je nest_inc
    cmp byte ptr [rsi], '}'
    je nest_dec
    
    ; New line check
    cmp byte ptr [rsi], 10
    je new_line
    
next_char:
    inc rsi
    jmp line_loop

new_line:
    cmp r15d, [rbp-8]             ; Max depth tracking
    jle next_char
    mov [rbp-8], r15d
    jmp next_char

nest_inc:
    inc r15d
    jmp next_char

nest_dec:
    dec r15d
    jmp next_char

control_flow:
    inc r13d                      ; Cyclomatic +1
    jmp next_char

data_decl:
    ; Check for large arrays
    cmp byte ptr [rsi+4], '['
    jne next_char
    ; Parse size
    add rsi, 5
    xor eax, eax
parse_arr_size:
    cmp byte ptr [rsi], ']'
    je got_arr_size
    movzx edx, byte ptr [rsi]
    cmp dl, '0'
    jb got_arr_size
    cmp dl, '9'
    ja got_arr_size
    imul eax, 10
    sub dl, '0'
    add eax, edx
    inc rsi
    jmp parse_arr_size
got_arr_size:
    add r14d, eax                 ; Accumulate data size
    jmp next_char

analysis_done:
    ; Calculate aggregate complexity score
    ; Score = (Cyclomatic * 10) + (MaxNesting * 20) + (DataSize / 1024)
    mov eax, r13d
    imul eax, 10
    mov edx, [rbp-8]
    imul edx, 20
    add eax, edx
    shr r14d, 10                  ; Divide by 1024
    add eax, r14d
    
    cmp eax, 1000
    jle score_ok
    mov eax, 1000
score_ok:
    mov [r12+COMPILE_UNIT.dwComplexityScore], eax
    
    leave
    ret
AnalyzeComplexityPreFlight endp

; ================================================
; Temp Patch Table Management
; ================================================

TempPatch_Initialize proc
    lea rax, g_TempPatchPool
    mov g_Orchestration.pTempPatchTable, rax
    mov g_Orchestration.dwPatchCount, 0
    ret
TempPatch_Initialize endp

TempPatch_Record proc
    ; RCX = Type, RDX = Offset, R8 = OldVal, R9 = NewVal, [rsp+40] = Size
    push rbp
    mov rbp, rsp
    
    mov eax, g_Orchestration.dwPatchCount
    cmp eax, MAX_TEMP_PATCHES
    jae patch_overflow
    
    mov r11d, sizeof(TEMP_PATCH)
    mul r11d
    add rax, g_Orchestration.pTempPatchTable
    mov r10, rax
    
    mov [r10+TEMP_PATCH.dwMagic], TEMP_PATCH_MAGIC
    mov [r10+TEMP_PATCH.dwType], ecx
    mov [r10+TEMP_PATCH.dwOffset], edx
    mov [r10+TEMP_PATCH.dwOldValue], r8d
    mov [r10+TEMP_PATCH.dwNewValue], r9d
    mov eax, [rbp+48]
    mov [r10+TEMP_PATCH.dwSize], eax
    mov byte ptr [r10+TEMP_PATCH.bApplied], 0
    
    inc g_Orchestration.dwPatchCount
    
patch_overflow:
    leave
    ret 8
TempPatch_Record endp

TempPatch_ApplyAll proc
    ; Applies all recorded temp patches to the output buffer
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    xor ebx, ebx
    mov r12d, g_Orchestration.dwPatchCount
    test r12d, r12d
    jz no_patches_apply
    
patch_apply_loop:
    cmp ebx, r12d
    jae patches_applied
    
    mov eax, ebx
    imul eax, sizeof(TEMP_PATCH)
    add rax, g_Orchestration.pTempPatchTable
    mov r13, rax
    
    ; Verify magic
    cmp dword ptr [r13], TEMP_PATCH_MAGIC
    jne next_patch_apply
    
    ; Get location in output buffer (current unit)
    mov r14d, g_Orchestration.dwActiveUnit
    imul r14d, sizeof(COMPILE_UNIT)
    lea r15, g_Units
    add r15, r14
    
    mov r8, [r15+COMPILE_UNIT.pOutputBuffer]
    add r8d, [r13+TEMP_PATCH.dwOffset]
    
    ; Change protection
    lea r9, [rbp-4]
    push r9
    push 40h                      ; PAGE_EXECUTE_READWRITE
    push [r13+TEMP_PATCH.dwSize]
    push r8
    sub rsp, 32
    call VirtualProtect
    add rsp, 56
    
    ; Apply patch based on size
    mov eax, [r13+TEMP_PATCH.dwNewValue]
    cmp dword ptr [r13+TEMP_PATCH.dwSize], 1
    je apply_byte
    cmp dword ptr [r13+TEMP_PATCH.dwSize], 4
    je apply_dword
    cmp dword ptr [r13+TEMP_PATCH.dwSize], 8
    je apply_qword
    jmp patch_marked
    
apply_byte:
    mov byte ptr [r8], al
    jmp patch_marked
    
apply_dword:
    mov dword ptr [r8], eax
    jmp patch_marked
    
apply_qword:
    mov rax, [r13+TEMP_PATCH.dwNewValue]
    mov qword ptr [r8], rax
    
patch_marked:
    mov byte ptr [r13+TEMP_PATCH.bApplied], 1
    
next_patch_apply:
    inc ebx
    jmp patch_apply_loop
    
patches_applied:
no_patches_apply:
    leave
    ret
TempPatch_ApplyAll endp

; ================================================
; Main Orchestration Driver
; ================================================

main proc
    sub rsp, 88
    
    ; Set console title
    lea rcx, szTitle
    call SetConsoleTitleA
    
    ; Print banner
    lea rcx, szBanner
    call PrintString
    
    ; Get command line
    call GetCommandLineA
    mov rsi, rax
    
    ; Skip to first argument (source file)
    call SkipExecutableName
    
    ; Check for source file
    cmp byte ptr [rsi], 0
    je ShowUsage
    
    ; Initialize performance counter
    lea rcx, g_PerformanceFreq
    call QueryPerformanceFrequency
    
    ; Phase 0: Global Initialization
    call AgentEngine_Init
    call TempPatch_Initialize
    
    ; Process each file (simplified: single file for now)
    mov r12, rsi              ; Source path
    
    ; Find or create unit slot
    xor ebx, ebx
    lea r13, g_Units
    
find_slot:
    cmp dword ptr [r13+COMPILE_UNIT.dwState], UNIT_IDLE
    je found_slot
    add r13, sizeof(COMPILE_UNIT)
    inc ebx
    cmp ebx, MAX_COMPILE_UNITS
    jb find_slot
    jmp ShowUsage             ; No slots
    
found_slot:
    mov [r13+COMPILE_UNIT.dwUnitId], ebx
    
    ; Copy input path
    lea rdi, [r13+COMPILE_UNIT.szInputPath]
    mov rsi, r12
    mov ecx, 260
    rep movsb
    
    ; Phase 1: Zero-State Reset
    lea rcx, szZeroState
    call PrintString
    call ZeroState_Reset
    lea rcx, szClean
    call PrintString
    
    ; Phase 2: Load & Hash Source
    call LoadSourceFile
    test eax, eax
    jnz load_failed
    
    ; Calculate hash
    mov rcx, [r13+COMPILE_UNIT.pSourceBuffer]
    mov rdx, [r13+COMPILE_UNIT.cbSourceSize]
    call CalculateSourceHash
    mov [r13+COMPILE_UNIT.qwSourceHash], rax
    
    ; Phase 3: Pre-Flight Analysis
    lea rcx, szScanning
    call PrintString
    
    mov rcx, r13
    call AnalyzeComplexityPreFlight
    
    ; Print complexity
    lea rcx, szComplexity
    call PrintString
    mov ecx, [r13+COMPILE_UNIT.dwComplexityScore]
    call PrintInt
    mov ecx, 10
    call putc
    
    ; Phase 4: Agent Manifestation
    lea rcx, szManifest
    call PrintString
    
    mov rcx, [r13+COMPILE_UNIT.pSourceBuffer]
    mov rdx, [r13+COMPILE_UNIT.cbSourceSize]
    call GetModuleHandleA   ; Get current image base for PE patching
    mov r8, rax
    call agent_controller_execute
    mov [r13+COMPILE_UNIT.pAgentState], rax
    
    ; Store signal mask
    mov eax, [rax+8]        ; dwSignal offset in AGENT_STATE
    mov [r13+COMPILE_UNIT.dwSignalMask], eax
    
    ; Print signals
    lea rcx, szSignal
    call PrintString
    mov ecx, eax
    call PrintHex
    mov ecx, 10
    call putc
    
    ; Phase 5: Compilation with Adaptive Limits
    lea rcx, szCompile
    call PrintString
    
    mov [r13+COMPILE_UNIT.dwState], UNIT_COMPILING
    mov g_Orchestration.dwActiveUnit, ebx
    
    call CompileUnit
    test eax, eax
    jnz compile_failed
    
    ; Phase 6: Temp Patch Application
    lea rcx, szPatch
    call PrintString
    
    mov [r13+COMPILE_UNIT.dwState], UNIT_PATCHING
    call TempPatch_ApplyAll
    
    ; Phase 7: Write Output
    call GenerateOutputPath
    
    lea rcx, [r13+COMPILE_UNIT.szOutputPath]
    call write_pe_file
    test eax, eax
    jnz write_failed
    
    ; Success
    lea rcx, szSuccess
    call PrintString
    lea rcx, [r13+COMPILE_UNIT.szOutputPath]
    call PrintString
    mov ecx, 10
    call putc
    
    ; Cleanup
    call ZeroState_Unlock
    xor ecx, ecx
    call ExitProcess
    
ShowUsage:
    lea rcx, szUsage
    call PrintString
    mov ecx, 1
    call ExitProcess
    
load_failed:
    lea rcx, szFailed
    call PrintString
    lea rcx, szErrLoad
    call PrintString
    mov ecx, 1
    call ExitProcess
    
compile_failed:
    lea rcx, szFailed
    call PrintString
    lea rcx, szErrCompile
    call PrintString
    mov ecx, 1
    call ExitProcess
    
write_failed:
    lea rcx, szFailed
    call PrintString
    lea rcx, szErrWrite
    call PrintString
    mov ecx, 1
    call ExitProcess

main endp

; ================================================
; Helper Functions
; ================================================

SkipExecutableName proc
    ; Skips first token in command line (the exe name)
    cmp byte ptr [rsi], '"'
    je skip_quoted
skip_norm:
    lodsb
    cmp al, ' '
    ja skip_norm
    jmp skip_spaces
skip_quoted:
    lodsb
    cmp al, '"'
    jne skip_quoted
skip_spaces:
    lodsb
    cmp al, ' '
    je skip_spaces
    cmp al, 0
    je done_skip
    dec rsi
done_skip:
    ret
SkipExecutableName endp

LoadSourceFile proc
    ; Loads source into unit buffer
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Open file
    lea rcx, [r13+COMPILE_UNIT.szInputPath]
    xor edx, edx              ; GENERIC_READ
    mov r8d, 3                ; FILE_SHARE_READ|WRITE
    xor r9d, r9d
    mov qword ptr [rsp+32], 3 ; OPEN_EXISTING
    mov qword ptr [rsp+40], 80h
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je load_fail
    
    mov [rbp+16], rax         ; hFile
    
    ; Get size
    lea rdx, [r13+COMPILE_UNIT.cbSourceSize]
    mov rcx, rax
    call GetFileSizeEx
    
    ; Allocate buffer
    mov rcx, [r13+COMPILE_UNIT.cbSourceSize]
    add rcx, 4096
    mov edx, 1000h            ; MEM_COMMIT
    mov r8d, 4                ; PAGE_READWRITE
    call VirtualAlloc
    mov [r13+COMPILE_UNIT.pSourceBuffer], rax
    
    ; Read
    mov rcx, [rbp+16]
    mov rdx, rax
    mov r8, [r13+COMPILE_UNIT.cbSourceSize]
    lea r9, [rbp-8]
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    ; Close
    mov rcx, [rbp+16]
    call CloseHandle
    
    xor eax, eax
    leave
    ret
    
load_fail:
    mov eax, 1
    leave
    ret
LoadSourceFile endp

CompileUnit proc
    ; Main compilation orchestration
    ; Setup lexer
    mov rcx, [r13+COMPILE_UNIT.pSourceBuffer]
    mov lpSource, rcx
    
    ; Lexical analysis
    call lex
    
    ; Parse loop
    mov cbOutput, 0
    
parse_loop:
    call cur_tok
    cmp eax, T_EOF
    je parse_done
    cmp eax, 100 + K_FN
    jne skip_token
    
    call parse_function
    jmp parse_loop
    
skip_token:
    call advance
    jmp parse_loop
    
parse_done:
    mov eax, cbOutput
    mov [r13+COMPILE_UNIT.cbOutputSize], eax
    mov rax, lpOutput
    mov [r13+COMPILE_UNIT.pOutputBuffer], rax
    
    xor eax, eax
    ret
CompileUnit endp

GenerateOutputPath proc
    ; Converts .tf to .exe in same directory
    lea rsi, [r13+COMPILE_UNIT.szInputPath]
    lea rdi, [r13+COMPILE_UNIT.szOutputPath]
    
copy_path:
    lodsb
    cmp al, '.'
    je found_ext
    test al, al
    jz add_ext
    stosb
    jmp copy_path
    
found_ext:
    stosb
    mov eax, 'exe'
    stosd
    xor al, al
    stosb
    ret
    
add_ext:
    dec rdi
    mov eax, '.exe'
    stosd
    xor al, al
    stosb
    ret
GenerateOutputPath endp

; ================================================
; I/O Utilities
; ================================================

PrintString proc
    ; RCX = String
    push rsi
    push rdi
    mov rsi, rcx
    call strlen
    mov rdx, rsi
    mov r8, rax
    mov rcx, -11            ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    mov r9, 0
    push 0
    sub rsp, 32
    call WriteConsoleA
    add rsp, 40
    pop rdi
    pop rsi
    ret
PrintString endp

PrintInt proc
    ; ECX = Integer
    sub rsp, 32
    lea r9, [rsp+16]
    mov r8d, 10
    xor edx, edx
    call itoa
    lea rcx, [rsp+16]
    call PrintString
    add rsp, 32
    ret
PrintInt endp

PrintHex proc
    ; ECX = Value
    sub rsp, 32
    lea rdi, [rsp+16]
    mov edx, ecx
    mov ecx, 8
hex_loop:
    rol edx, 4
    mov eax, edx
    and eax, 0Fh
    cmp al, 10
    jb hex_digit
    add al, 'A' - '0' - 10
hex_digit:
    add al, '0'
    mov [rdi], al
    inc rdi
    dec ecx
    jnz hex_loop
    mov byte ptr [rdi], 0
    
    lea rcx, [rsp+16]
    call PrintString
    add rsp, 32
    ret
PrintHex endp

putc proc
    sub rsp, 40
    mov [rsp+32], cl
    mov rcx, -11
    call GetStdHandle
    mov rcx, rax
    lea rdx, [rsp+32]
    mov r8d, 1
    lea r9, [rsp+24]
    push 0
    sub rsp, 32
    call WriteConsoleA
    add rsp, 40
    ret
putc endp

strlen proc
    xor rax, rax
    mov rsi, rcx
L1: cmp byte ptr [rsi+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

itoa proc
    ; RCX = value, RDX = base, R8 = buffer
    mov r9, r8
    add r9, 31
    mov byte ptr [r9], 0
    mov eax, ecx
    
L1: xor edx, edx
    div r8d
    add dl, '0'
    cmp dl, '9'
    jbe L2
    add dl, 7
L2: dec r9
    mov [r9], dl
    test eax, eax
    jnz L1
    
    mov rax, r9
    ret
itoa endp

; ================================================
; String Data
; ================================================
.data
szUsage db "Usage: terraform <source.tf>", 10, 0
szErrLoad db "Failed to load source file", 10, 0
szErrCompile db "Compilation error", 10, 0
szErrWrite db "Failed to write executable", 10, 0

; Import forward declarations (for linking)
externdef lpSource:qword
externdef lpOutput:qword
externdef cbOutput:dword

end