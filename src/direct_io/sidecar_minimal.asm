; src/direct_io/sidecar_minimal.asm
; Minimal connectivity test for SYSTEM context


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN lstrlenA:PROC
EXTERN CreateDirectoryA:PROC

.data
debugDir DB "C:\\ProgramData\\Sovereign", 0
fileName DB "C:\\ProgramData\\Sovereign\\minimal.txt", 0
msg      DB "Minimal sidecar running - SUCCESS", 13, 10, 0

.code
PUBLIC SidecarMinimalEntry
SidecarMinimalEntry PROC
    sub rsp, 78h
    
    ; Create directory first
    lea rcx, debugDir
    xor rdx, rdx
    call CreateDirectoryA
    
    ; CreateFileA with GENERIC_WRITE
    lea rcx, fileName
    mov edx, 40000000h ; GENERIC_WRITE
    mov r8d, 0         ; No sharing
    xor r9d, r9d       ; NULL security
    mov DWORD PTR [rsp+20h], 4 ; OPEN_ALWAYS
    mov DWORD PTR [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+30h], 0 ; hTemplate
    call CreateFileA
    
    cmp rax, -1
    je _exit
    
    mov rbx, rax ; handle
    
    ; Get message length
    lea rcx, msg
    call lstrlenA
    mov r8d, eax
    
    ; Write
    mov rcx, rbx
    lea rdx, msg
    lea r9, [rsp+40h]  ; lpBytesWritten
    mov QWORD PTR [rsp+20h], 0 ; lpOverlapped
    call WriteFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
_exit:
    xor ecx, ecx
    call ExitProcess
    ret
SidecarMinimalEntry ENDP
END
