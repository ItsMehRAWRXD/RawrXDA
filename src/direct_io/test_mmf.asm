; Test: Minimal MMF creation only
EXTERN CreateFileMappingA:PROC
EXTERN CloseHandle:PROC
EXTERN ExitProcess:PROC
EXTERN Sleep:PROC

.data
mapName DB "Local\\TEST_MMF", 0

.code
PUBLIC TestEntry
TestEntry PROC
    sub rsp, 38h
    
    ; CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "Local\\TEST_MMF")
    mov rcx, -1                    ; INVALID_HANDLE_VALUE
    xor rdx, rdx                   ; NULL security
    mov r8d, 4                     ; PAGE_READWRITE
    xor r9d, r9d                   ; High size
    mov DWORD PTR [rsp+20h], 4096  ; Low size
    lea rax, mapName
    mov [rsp+28h], rax             ; lpName
    call CreateFileMappingA
    
    test rax, rax
    jz _fail
    
    mov rbx, rax
    mov ecx, 10000
    call Sleep
    
    mov rcx, rbx
    call CloseHandle
    
    xor ecx, ecx
    call ExitProcess
    
_fail:
    mov ecx, 1
    call ExitProcess
TestEntry ENDP
END
