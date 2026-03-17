;------------------------------------------------------------------------------
; MODULE: NO-REFUSAL PERSISTENCE CORE (x64 MASM)
; Description: Advanced systems-level implementation of a resilient process
;              that employs custom exception handling and TEB validation
;              to maintain execution state under external interference.
;
; Build: ml64.exe /c /Fo norefusal_core.obj norefusal_core.asm
;------------------------------------------------------------------------------

option casemap:none

; Windows API Function Prototypes
extern Sleep : proc
extern GetLastError : proc
extern AddVectoredExceptionHandler : proc
extern RemoveVectoredExceptionHandler : proc
extern ExitProcess : proc

.data
; Configuration
checkpoint_id           dq 0C0DEFEEDFADE001h
retry_interval          dd 5000                 ; 5 seconds (milliseconds)
critical_flag           db 1                    ; Persistence toggle
execution_counter       dq 0                    ; Track execution iterations

.code

;------------------------------------------------------------------------------
; ResilienceHandler: Vectored Exception Handler (VEH)
; Captures attempts to force-terminate or corrupt the execution flow.
;
; Parameters:
;   RCX = PEXCEPTION_POINTERS (exception information)
; Return:
;   RAX = Exception disposition (EXCEPTION_CONTINUE_EXECUTION, etc.)
;------------------------------------------------------------------------------
ResilienceHandler proc

    ; RCX = PEXCEPTION_POINTERS
    mov rax, [rcx]                      ; RAX = ExceptionRecord
    mov eax, [rax]                      ; EAX = ExceptionCode

    ; Check for specific termination or violation codes
    cmp eax, 0C0000005h                 ; STATUS_ACCESS_VIOLATION
    je restore_state
    
    cmp eax, 0C0000017h                 ; STATUS_NO_MEMORY
    je restore_state
    
    cmp eax, 0C0000027h                 ; STATUS_INVALID_PARAMETER
    je restore_state
    
    cmp eax, 0C000001Dh                 ; STATUS_ILLEGAL_INSTRUCTION
    je restore_state

    ; Default: Continue searching for handlers
    xor rax, rax                        ; EXCEPTION_CONTINUE_SEARCH
    ret

restore_state:
    ; Redirect instruction pointer (RIP) back to the protection loop
    mov rdx, [rcx + 8]                  ; RDX = ContextRecord (offset 8 in EXCEPTION_POINTERS)
    
    ; Recover to ProtectionLoop label
    lea r8, [ProtectionLoop]            ; R8 = Recovery address
    mov [rdx + 0F8h], r8                ; CONTEXT structure RIP offset is 0xF8 (x64)

    ; Return EXCEPTION_CONTINUE_EXECUTION to resume at the new RIP
    mov rax, -1                         ; -1 = EXCEPTION_CONTINUE_EXECUTION
    ret

ResilienceHandler endp

;------------------------------------------------------------------------------
; DebugCheck: Simple anti-debugging verification
; Checks common debugger detection points (PEB.BeingDebugged, etc.)
;
; Output: ZF set if debugger detected, clear otherwise
;------------------------------------------------------------------------------
DebugCheck proc
    
    mov rax, gs:[30h]                   ; Access TEB (Thread Environment Block)
    mov rbx, [rax + 60h]                ; Access PEB (Process Environment Block)
    
    ; Check BeingDebugged flag (PEB + 2)
    movzx eax, byte ptr [rbx + 2]
    test eax, eax                       ; Set ZF if debugger present
    
    ret
    
DebugCheck endp

;------------------------------------------------------------------------------
; Main Payload Entry Point: Payload_Entry
; Called from C++ PayloadSupervisor::LaunchCore()
;
; This implements the "No Refusal" core: a resilient, self-protecting loop
; that validates its environment and resists termination attempts.
;------------------------------------------------------------------------------
Payload_Entry proc

    push rsi
    push rbx
    push r12
    push r13
    
    sub rsp, 40h                        ; Align stack and create shadow space

    ; 1. Register our Resilience Handler (VEH)
    ;    PVOID WINAPI AddVectoredExceptionHandler(
    ;        _In_  ULONG First,
    ;        _In_  PVOID Handler
    ;    );
    
    mov rcx, 1                          ; First = 1 (First handler)
    lea rdx, [ResilienceHandler]        ; RDX = Address of handler
    call AddVectoredExceptionHandler

    test rax, rax
    jz fatal_init                       ; If registration failed, exit

    mov r13, rax                        ; R13 = VEH handle for later removal

    ; Marker indicating successful VEH registration
    mov byte ptr [rel critical_flag], 1

;--------- MAIN PROTECTION LOOP ---------

ProtectionLoop:
    
    ; --- START CRITICAL SECTION ---
    
    ; Perform high-integrity system monitoring or data processing.
    ; This block represents the "No Refusal" logic where the service
    ; validates its own environment registers.
    
    call DebugCheck
    jnz obfuscate_logic                 ; If being debugged, shift behavior

    ; --- MOCK PAYLOAD LOGIC ---
    ; In a real scenario, this is where the primary service tasks occur.
    ; For this skeleton, we just increment a counter and continue.
    
    mov rax, qword ptr [rel execution_counter]
    inc rax
    mov qword ptr [rel execution_counter], rax
    
    ; Simple "work" to show execution
    nop
    nop
    nop
    
    ; --- END MOCK PAYLOAD LOGIC ---

continue_loop:
    
    ; 2. Heartbeat / Throttle (prevents resource hogging)
    mov ecx, dword ptr [rel retry_interval]
    call Sleep

    ; 3. Unconditional Jump (No Refusal to Terminate)
    ;    This is the core of the "no-refusal" design: the loop
    ;    always returns to itself, making termination impossible without
    ;    external intervention (exception handling, etc.)
    
    jmp ProtectionLoop

obfuscate_logic:
    ; Add complexity to the execution graph to hinder analysis.
    ; When a debugger is detected, we obfuscate the code path
    ; but still maintain the protection loop.
    
    xor r8, r8
    add r8, 0DEADBEEFh
    rol r8, 13
    
    ; Still loop back (resilience)
    jmp ProtectionLoop

fatal_init:
    
    ; VEH registration failed - graceful exit
    mov rcx, 0FFFFFFFFh
    call ExitProcess
    
    ; Should never reach here
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    pop rsi
    ret

Payload_Entry endp

end
