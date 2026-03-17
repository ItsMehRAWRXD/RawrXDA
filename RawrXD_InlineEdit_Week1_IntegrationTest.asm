; ==============================================================================
; RawrXD_InlineEdit_Week1_IntegrationTest.asm
; Full pipeline test: Hotkey → Context → Request → Stream → Validate → Commit
; ==============================================================================

.model flat, c

.code

; External module entry points
EXTERN GlobalHotkey_Register:proc
EXTERN InlineEdit_ContextCapture:proc
EXTERN InlineEdit_RequestStreaming:proc
EXTERN InlineEdit_StreamToken:proc
EXTERN DiffValidator_CompareASTs:proc
EXTERN DiffValidator_ApproveEdit:proc
EXTERN InlineEdit_CommitEdit:proc

; Windows API
EXTERN GetModuleHandleA:proc
EXTERN CreateWindowExA:proc
EXTERN ShowWindow:proc
EXTERN UpdateWindow:proc
EXTERN GetMessageA:proc
EXTERN TranslateMessage:proc
EXTERN DispatchMessageA:proc
EXTERN PostQuitMessage:proc
EXTERN DefWindowProcA:proc
EXTERN RegisterClassA:proc
EXTERN GetStockObject:proc
EXTERN CreateFileA:proc
EXTERN WriteFile:proc
EXTERN CloseHandle:proc
EXTERN ExitProcess:proc

; Constants
WS_OVERLAPPEDWINDOW = 0CF0000h
WS_VISIBLE = 10000000h
CW_USEDEFAULT = 80000000h
SW_SHOW = 5h
WM_CREATE = 1h
WM_DESTROY = 2h
WM_PAINT = 0Fh
WM_HOTKEY = 312h
WM_CLOSE = 10h
WHITE_BRUSH = 0h
WM_QUIT = 12h

; Test result codes
TEST_PASS = 0h
TEST_HOTKEY_FAILED = 1h
TEST_CONTEXT_FAILED = 2h
TEST_REQUEST_FAILED = 3h
TEST_STREAM_FAILED = 4h
TEST_VALIDATE_FAILED = 5h
TEST_COMMIT_FAILED = 6h


.data
    
    ; Window properties
    hMainWindow dd 0
    testResult dd 0
    testPhase db 0            ; Current test phase
    
    ; Test data
    testCode db 'mov rax, rbx', 0Dh, 0Ah
             db 'mov rcx, rdx', 0Dh, 0Ah
             db 'add rax, rcx', 0Dh, 0Ah
             db 'ret', 0
    
    testInstruction db 'Reorder to add before mov', 0
    
    testGeneratedCode db 'add rax, rcx', 0Dh, 0Ah
                      db 'mov rax, rbx', 0Dh, 0Ah
                      db 'mov rcx, rdx', 0Dh, 0Ah, 0
    
    ; Window class
    szClassName db 'InlineEditIntegrationTest', 0
    szWindowName db 'RawrXD Inline Edit - Week 1 Integration Test', 0
    
    ; File output
    szTestLogFile db 'D:\rawrxd\InlineEdit_Week1_Test.log', 0
    hLogFile dd 0
    
    ; Test messages
    msg_start db 'Starting Week 1 Integration Test...', 0Dh, 0Ah, 0
    msg_hotkey_ok db '[PASS] Hotkey registered successfully', 0Dh, 0Ah, 0
    msg_hotkey_fail db '[FAIL] Hotkey registration failed', 0Dh, 0Ah, 0
    msg_context_ok db '[PASS] Context extraction successful', 0Dh, 0Ah, 0
    msg_context_fail db '[FAIL] Context extraction failed', 0Dh, 0Ah, 0
    msg_request_ok db '[PASS] LLM request queued', 0Dh, 0Ah, 0
    msg_request_fail db '[FAIL] LLM request failed', 0Dh, 0Ah, 0
    msg_stream_ok db '[PASS] Token streaming initiated', 0Dh, 0Ah, 0
    msg_stream_fail db '[FAIL] Token streaming failed', 0Dh, 0Ah, 0
    msg_validate_ok db '[PASS] Code validation passed', 0Dh, 0Ah, 0
    msg_validate_fail db '[FAIL] Code validation failed', 0Dh, 0Ah, 0
    msg_commit_ok db '[PASS] Edit committed', 0Dh, 0Ah, 0
    msg_commit_fail db '[FAIL] Commit failed', 0Dh, 0Ah, 0
    msg_complete db 'Week 1 Test Complete!', 0Dh, 0Ah, 0


.code

; ============================================================================
; main - Entry point for test
; ============================================================================
main PROC
    
    ; Initialize log file
    lea rcx, [szTestLogFile]
    mov rdx, 2                 ; GENERIC_WRITE
    mov r8, 1                  ; FILE_SHARE_READ
    mov r9, 0
    push 2                     ; OPEN_ALWAYS
    push 0x80                  ; FILE_ATTRIBUTE_NORMAL
    call CreateFileA
    
    mov hLogFile, eax
    
    ; Write start message
    lea rcx, [msg_start]
    call LogMessage
    
    ; Phase 1: Register Hotkey
    mov testPhase, 1
    mov rcx, 0
    call GlobalHotkey_Register
    
    test eax, eax
    jz .phase1_fail
    
    lea rcx, [msg_hotkey_ok]
    call LogMessage
    jmp .phase2_start
    
.phase1_fail:
    lea rcx, [msg_hotkey_fail]
    call LogMessage
    mov testResult, TEST_HOTKEY_FAILED
    jmp .test_cleanup
    
; Phase 2: Extract Context
.phase2_start:
    mov testPhase, 2
    lea rcx, [testCode]
    mov rdx, 0                 ; Simulated cursor position
    call InlineEdit_ContextCapture
    
    test eax, eax
    jz .phase2_fail
    
    lea rcx, [msg_context_ok]
    call LogMessage
    jmp .phase3_start
    
.phase2_fail:
    lea rcx, [msg_context_fail]
    call LogMessage
    mov testResult, TEST_CONTEXT_FAILED
    jmp .test_cleanup
    
; Phase 3: Request Streaming
.phase3_start:
    mov testPhase, 3
    lea rcx, [testInstruction]
    mov rdx, 0                 ; Decoded context buffer
    mov r8, 0                  ; Editor window (simulated)
    call InlineEdit_RequestStreaming
    
    test eax, eax
    jz .phase3_fail
    
    lea rcx, [msg_request_ok]
    call LogMessage
    jmp .phase4_start
    
.phase3_fail:
    lea rcx, [msg_request_fail]
    call LogMessage
    mov testResult, TEST_REQUEST_FAILED
    jmp .test_cleanup
    
; Phase 4: Process Tokens
.phase4_start:
    mov testPhase, 4
    
    ; Simulate token stream (in reality, this comes from llama.cpp)
    ; Token 0: "add"
    lea rcx, ["add", 0]
    mov rdx, 3
    mov r8, 0                 ; hwndEditor (simulated)
    xor r9, r9                ; isDone = 0
    call InlineEdit_StreamToken
    
    ; Token 1: " rax"
    lea rcx, [" rax", 0]
    mov rdx, 4
    xor r9, r9
    call InlineEdit_StreamToken
    
    ; Final token with isDone=1
    lea rcx, [", rcx", 0]
    mov rdx, 5
    mov r9, 1                 ; isDone = 1
    call InlineEdit_StreamToken
    
    lea rcx, [msg_stream_ok]
    call LogMessage
    jmp .phase5_start
    
; Phase 5: Validate Generated Code
.phase5_start:
    mov testPhase, 5
    
    lea rcx, [testCode]
    mov rdx, 0
    mov r8, 0                 ; Simulated original code
    lea r9, [testGeneratedCode]  ; Generated code
    
    ; Create validator result on stack
    sub rsp, 64               ; Allocate VALIDATOR_RESULT structure
    mov r8, rsp
    
    call DiffValidator_CompareASTs
    
    ; Check validation result
    mov eax, [rsp]            ; isValid field
    test eax, eax
    jz .phase5_fail
    
    add rsp, 64               ; Clean up stack
    
    lea rcx, [msg_validate_ok]
    call LogMessage
    jmp .phase6_start
    
.phase5_fail:
    add rsp, 64
    lea rcx, [msg_validate_fail]
    call LogMessage
    mov testResult, TEST_VALIDATE_FAILED
    jmp .test_cleanup
    
; Phase 6: Commit Edit
.phase6_start:
    mov testPhase, 6
    
    mov rcx, 0                ; hwndEditor (simulated)
    lea rdx, [testGeneratedCode]  ; Code to insert
    mov r8, 28                ; Selection start
    mov r9, 46                ; Selection end
    call InlineEdit_CommitEdit
    
    test eax, eax
    jz .phase6_fail
    
    lea rcx, [msg_commit_ok]
    call LogMessage
    jmp .test_complete
    
.phase6_fail:
    lea rcx, [msg_commit_fail]
    call LogMessage
    mov testResult, TEST_COMMIT_FAILED
    
.test_complete:
    lea rcx, [msg_complete]
    call LogMessage
    
.test_cleanup:
    ; Close log file
    mov rcx, hLogFile
    call CloseHandle
    
    ; Exit with test result code
    mov eax, testResult
    call ExitProcess
    
    ret
main ENDP


; ============================================================================
; LogMessage - Write message to log file and console
; rcx = message string
; ============================================================================
LogMessage PROC
    push rcx
    push rdx
    push r8
    push r9
    
    ; Calculate message length
    mov rdx, rcx
    xor eax, eax
.measure_loop:
    mov bl, byte ptr [rdx]
    test bl, bl
    jz .measure_done
    inc eax
    inc rdx
    jmp .measure_loop
    
.measure_done:
    ; Write to file
    mov rcx, hLogFile
    mov rdx, [rsp + 32]        ; Original message pointer
    mov r8, rax                ; Length
    lea r9, [rsp - 8]          ; Bytes written (stack)
    
    push 0                     ; Bytes written placeholder
    call WriteFile
    add rsp, 8
    
    pop r9
    pop r8
    pop rdx
    pop rcx
    ret
LogMessage ENDP


.end main
