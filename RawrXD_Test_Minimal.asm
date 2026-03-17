;=============================================================================
; RawrXD_Amphibious_Minimal_Test.asm
; Minimal test to isolate console write issue
;=============================================================================

OPTION CASEMAP:NONE

EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN WriteConsoleA:PROC
EXTERN OutputDebugStringA:PROC

STD_OUTPUT_HANDLE        EQU -11

.data

    szBanner                 db "===== RawrXD AMPHIBIOUS =====",13,10,0
    szLine1                  db "[INIT] System starting",13,10,0
    szLine2                  db "[SUCCESS] Complete",13,10,0

.code

main PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 48
    .allocstack 48
    .endprolog

    ; Get stdout
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [rsp+32], rax

    ; Write line 1 (banner)
    mov rcx, [rsp+32]
    lea rdx, szBanner
    mov r8d, 31
    lea r9, [rsp+40]
    mov qword ptr [rsp+20], 0
    call WriteConsoleA

    mov ecx, 100
  @@Wait1:
    dec ecx
    cmp ecx, 0
    jne @@Wait1

    ; Write line 2
    mov rcx, [rsp+32]
    lea rdx, szLine1
    mov r8d, 24
    lea r9, [rsp+40]
    mov qword ptr [rsp+20], 0
    call WriteConsoleA

    mov ecx, 100
  @@Wait2:
    dec ecx
    cmp ecx, 0
    jne @@Wait2

    ; Write line 3
    mov rcx, [rsp+32]
    lea rdx, szLine2
    mov r8d, 20
    lea r9, [rsp+40]
    mov qword ptr [rsp+20], 0
    call WriteConsoleA

    add rsp, 48
    pop rbp
    xor ecx, ecx
    call ExitProcess
main ENDP

END
