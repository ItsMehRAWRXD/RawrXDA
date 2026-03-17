; ui_bridge.asm — MASM/C++ bridge for Qtg integration
; stdcall convention for x64 compatibility

; --- External C++ Symbols ---
EXTERN QuantumOrchestrator_ExecuteTaskAuto : PROC
EXTERN ExecutionResult_GetDetail : PROC
EXTERN MultiFileSessionTracker_CreateSession : PROC
EXTERN MultiFileSessionTracker_StageEdit : PROC

.data
g_qtgSessionId db 64 dup(0)

.code

; UIBridge_GenerateFeature(const char* prompt, const char* file)
; RCX = prompt, RDX = file
UIBridge_GenerateFeature PROC FRAME
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40                 ; Shadow space + alignment
    .allocstack 40
    .endprolog

    mov rsi, rcx                ; rsi = prompt
    mov rdi, rdx                ; rdi = current file

    ; 1. Call QuantumOrchestrator::executeTaskAuto(prompt, {file})
    ; RCX = prompt
    ; RDX = &file (array of pointers)
    ; R8  = 1 (count)
    mov rcx, rsi
    sub rsp, 8                  ; local space for pointer array
    mov [rsp], rdi
    mov rdx, rsp
    mov r8, 1
    call QuantumOrchestrator_ExecuteTaskAuto
    add rsp, 8

    ; Result in RAX (ExecutionResult*), check success
    test rax, rax
    jz @ui_fail

    ; Extract detail (generated code) string pointer via thunk helper
    mov rcx, rax                ; result pointer
    call ExecutionResult_GetDetail
    mov rbx, rax                ; rbx = const char* detail string pointer
    
    ; 2. Create edit session
    lea rcx, [g_qtgSessionId]
    call MultiFileSessionTracker_CreateSession
    
    ; 3. Stage edit
    ; void StageEdit(const char* session, const char* file, int start, int end, const char* text)
    lea rcx, [g_qtgSessionId]   ; session
    mov rdx, rdi                ; file
    mov r8d, 1                  ; startLine (insert at top)
    mov r9d, 1                  ; endLine
    sub rsp, 8                  ; Alignment
    push rbx                    ; 5th param: newText (generated code)
    call MultiFileSessionTracker_StageEdit
    add rsp, 16                 ; Cleanup push/alignment

    ; Return session ID for preview/apply
    lea rax, [g_qtgSessionId]
    jmp @ui_exit

@ui_fail:
    xor rax, rax                ; Return NULL on failure

@ui_exit:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbp
    ret
UIBridge_GenerateFeature ENDP

END
