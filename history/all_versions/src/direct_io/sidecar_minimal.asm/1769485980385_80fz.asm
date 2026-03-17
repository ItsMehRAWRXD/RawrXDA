; src/direct_io/sidecar_minimal.asm
; Minimal connectivity test for SYSTEM context

EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN lstrlenA:PROC

.data
fileName DB "C:\\ProgramData\\Sovereign\\minimal.txt", 0
msg      DB "Minimal sidecar running as SYSTEM - SUCCESS", 13, 10, 0

.code
PUBLIC SidecarMinimalEntry
SidecarMinimalEntry PROC
    sub rsp, 48h       ; 64 bytes + 8 (ret) = 72. 72%16=8. Aligned for calls.
                       ; Wait. Entry: RSP%16=8.
                       ; sub 48h (72). 72+8=80. 80%16=0. 
                       ; Inside calls: Push 8. RSP%16=8. Correct.
    
    ; CreateFileA
    lea rcx, fileName
    mov edx, 40000000h ; GENERIC_WRITE
    mov r8d, 1         ; FILE_SHARE_READ
    xor r9d, r9d       ; NULL security
    mov QWORD PTR [rsp+20h], 2 ; CREATE_ALWAYS
    mov QWORD PTR [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+30h], 0 ; hTemplate
    call CreateFileA
    
    cmp rax, -1
    je _exit
    
    mov rbx, rax ; handle
    
    ; Write
    lea rcx, msg
    call lstrlenA
    mov r8d, eax
    
    mov rcx, rbx
    lea rdx, msg
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    call WriteFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
_exit:
    xor ecx, ecx
    call ExitProcess
SidecarMinimalEntry ENDP
END
