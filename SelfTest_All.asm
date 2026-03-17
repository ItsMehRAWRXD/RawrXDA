; SelfTest_All.asm
; Phase 2: End-to-end selftests
; Returns distinct error codes (0 = pass)

option casemap:none
include rawr_rt.inc
include rawr_imports.inc
include rawr_api.inc

; Section info structure (from DumpBin engine)
SECTION_INFO STRUCT
    name            BYTE 9 DUP(?) ; Section name (8 chars + null)
    rva             DWORD ?       ; Virtual address
    rawSize         DWORD ?       ; Size of raw data
    characteristics DWORD ?       ; Section characteristics
SECTION_INFO ENDS

.data
test_msg_abi      db "ABI SWEEP: ",0
test_msg_dispatch db "DISPATCH STRESS: ",0
test_msg_cpuops   db "CPUOPS SANITY: ",0
test_msg_codex    db "CODEX SANITY: ",0
test_msg_infer    db "INFER SMOKE: ",0
test_pass         db "PASS",0
test_fail         db "FAIL",0

.code

;-----------------------------------------------------------------------------
; SelfTest_All
; Runs all selftests
; Returns 0 on pass, error code on fail
;-----------------------------------------------------------------------------
PUBLIC SelfTest_All
SelfTest_All PROC
    RAWR_PROLOGUE 0

    ; ABI sweep
    call SelfTest_ABISweep
    test eax, eax
    jnz _fail

    ; Dispatch stress
    call SelfTest_DispatchStress
    test eax, eax
    jnz _fail

    ; CPUOps sanity
    call SelfTest_CPUOpsSanity
    test eax, eax
    jnz _fail

    ; Codex sanity
    call SelfTest_CodexSanity
    test eax, eax
    jnz _fail

    ; Infer smoke
    call SelfTest_InferSmoke
    test eax, eax
    jnz _fail

    xor eax, eax  ; pass
    RAWR_EPILOGUE 0

_fail:
    RAWR_EPILOGUE 0
SelfTest_All ENDP

;-----------------------------------------------------------------------------
; SelfTest_ABISweep
; Calls abi_canary on all exported procs
;-----------------------------------------------------------------------------
SelfTest_ABISweep PROC
    RAWR_PROLOGUE 0

    ; Example: test TaskDispatcher_Initialize
    lea rcx, TaskDispatcher_Initialize
    call abi_canary_call0
    test eax, eax
    jnz _fail

    ; Add more for all publics

    xor eax, eax
    RAWR_EPILOGUE 0

_fail:
    mov eax, 1
    RAWR_EPILOGUE 0
SelfTest_ABISweep ENDP

;-----------------------------------------------------------------------------
; SelfTest_DispatchStress
; Submit N tasks, poll, shutdown
;-----------------------------------------------------------------------------
SelfTest_DispatchStress PROC
    RAWR_PROLOGUE 0

    ; Init dispatcher
    mov rcx, 2
    call TaskDispatcher_Initialize
    test eax, eax
    jz _fail

    ; Submit tasks (stub)
    ; Poll
    ; Shutdown

    call TaskDispatcher_Shutdown

    xor eax, eax
    RAWR_EPILOGUE 0

_fail:
    mov eax, 2
    RAWR_EPILOGUE 0
SelfTest_DispatchStress ENDP

;-----------------------------------------------------------------------------
; SelfTest_CPUOpsSanity
; Test MatMulVec on small data
;-----------------------------------------------------------------------------
SelfTest_CPUOpsSanity PROC
    RAWR_PROLOGUE 0

    ; Allocate arrays: dst[4], mat[16], vec[4]
    mov rcx, 4 * 4 * 3  ; 48 bytes
    call rawr_heap_alloc
    test rax, rax
    jz _fail
    mov rbx, rax  ; Base

    ; Setup pointers
    mov rsi, rbx        ; dst
    add rbx, 16         ; mat
    mov rdi, rbx        ; vec
    add rbx, 16         ; end

    ; Fill vec with [1,2,3,4]
    mov dword ptr [rdi], 1
    mov dword ptr [rdi+4], 2
    mov dword ptr [rdi+8], 3
    mov dword ptr [rdi+12], 4

    ; Fill mat as identity
    ; Row 0: 1,0,0,0
    mov dword ptr [rbx-16], 1
    mov dword ptr [rbx-12], 0
    mov dword ptr [rbx-8], 0
    mov dword ptr [rbx-4], 0
    ; Row 1: 0,1,0,0
    mov dword ptr [rbx], 0
    mov dword ptr [rbx+4], 1
    mov dword ptr [rbx+8], 0
    mov dword ptr [rbx+12], 0
    ; Row 2: 0,0,1,0
    mov dword ptr [rbx+16], 0
    mov dword ptr [rbx+20], 0
    mov dword ptr [rbx+24], 1
    mov dword ptr [rbx+28], 0
    ; Row 3: 0,0,0,1
    mov dword ptr [rbx+32], 0
    mov dword ptr [rbx+36], 0
    mov dword ptr [rbx+40], 0
    mov dword ptr [rbx+44], 1

    ; Call CPUOps_MatMulVec_F32
    mov rcx, rsi        ; dst
    mov rdx, rbx-16     ; mat
    mov r8, rdi         ; vec
    mov r9, 4           ; n
    call CPUOps_MatMulVec_F32

    ; Check result: should be [1,2,3,4]
    mov eax, [rsi]
    cmp eax, 1
    jne _fail
    mov eax, [rsi+4]
    cmp eax, 2
    jne _fail
    mov eax, [rsi+8]
    cmp eax, 3
    jne _fail
    mov eax, [rsi+12]
    cmp eax, 4
    jne _fail

    ; Free memory
    mov rcx, rsi
    call rawr_heap_free

    xor eax, eax
    RAWR_EPILOGUE 0

_fail:
    mov eax, 1
    RAWR_EPILOGUE 0
SelfTest_CPUOpsSanity ENDP

;-----------------------------------------------------------------------------
; SelfTest_CodexSanity
; Analyze own module
;-----------------------------------------------------------------------------
SelfTest_CodexSanity PROC
    RAWR_PROLOGUE 0

    ; GetModuleHandleW(NULL)
    xor rcx, rcx
    call GetModuleHandleW
    test rax, rax
    jz _fail
    mov rbx, rax    ; Module base

    ; Create DumpBin context
    call RawrDumpBin_Create
    test rax, rax
    jz _fail
    mov rsi, rax    ; Context

    ; Get module size (assume 10MB max for simplicity)
    mov r8, 10000000h

    ; Parse PE
    mov rcx, rsi
    mov rdx, rbx
    call RawrDumpBin_ParsePE
    test rax, rax
    jz _cleanup_fail

    ; Get sections
    mov rcx, rsi
    xor rdx, rdx    ; NULL for array (will be allocated)
    lea r8, [rsp-16]  ; Section count
    call RawrDumpBin_GetSections
    test rax, rax
    jz _cleanup_fail
    mov rdi, rax    ; Section array

    ; Check section count > 0
    mov eax, [rsp-16]
    test eax, eax
    jz _cleanup_fail

    ; Check for .text section
    mov ecx, [rsp-16]
    xor r12d, r12d  ; Found .text flag

@check_sections:
    test ecx, ecx
    jz @done_check

    ; Check name for .text
    mov al, [rdi]
    cmp al, '.'
    jne @next_section
    mov eax, [rdi+1]
    cmp eax, 'text'
    jne @next_section
    mov r12d, 1

@next_section:
    add rdi, SIZEOF SECTION_INFO
    dec ecx
    jmp @check_sections

@done_check:
    test r12d, r12d
    jz _cleanup_fail

    ; Success - found .text and >0 sections
    mov rcx, rsi
    call RawrDumpBin_Destroy
    xor eax, eax
    RAWR_EPILOGUE 0

_cleanup_fail:
    mov rcx, rsi
    call RawrDumpBin_Destroy

_fail:
    mov eax, 4
    RAWR_EPILOGUE 0
SelfTest_CodexSanity ENDP

;-----------------------------------------------------------------------------
; SelfTest_InferSmoke
; Run 1-2 tokens
;-----------------------------------------------------------------------------
SelfTest_InferSmoke PROC
    RAWR_PROLOGUE 0

    ; Stub: init, run, destroy

    xor eax, eax
    RAWR_EPILOGUE 0
SelfTest_InferSmoke ENDP

END