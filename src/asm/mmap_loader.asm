; mmap_loader.asm - Windows memory-mapped file I/O for GGUF
; Pure MASM64 - Zero CRT dependencies

PUBLIC MMap_Open
PUBLIC MMap_Close
PUBLIC MMap_Read
PUBLIC MMap_GetSize

EXTRN CreateFileA:PROC
EXTRN CreateFileMappingA:PROC
EXTRN MapViewOfFile:PROC
EXTRN UnmapViewOfFile:PROC
EXTRN CloseHandle:PROC
EXTRN GetFileSizeEx:PROC

.data
align 8
g_hFile     QWORD 0
g_hMap      QWORD 0
g_pView     QWORD 0
g_qwSize    QWORD 0

.code

MMap_Open PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    mov rsi, rcx
    mov rdi, rdx
    
    mov rcx, rsi
    mov edx, 80000000h
    mov r8d, 1
    xor r9d, r9d
    mov dword ptr [rsp+20h], 3
    mov dword ptr [rsp+28h], 80h
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    mov g_hFile, rax
    cmp rax, -1
    je mmap_fail_open
    
    lea rdx, g_qwSize
    mov rcx, rax
    call GetFileSizeEx
    test eax, eax
    jz mmap_fail_size
    
    test rdi, rdi
    jz mmap_use_full
    cmp rdi, g_qwSize
    jae mmap_use_full
    mov g_qwSize, rdi
    
mmap_use_full:
    mov rcx, g_hFile
    xor edx, edx
    mov r8d, 2
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateFileMappingA
    mov g_hMap, rax
    test rax, rax
    jz mmap_fail_map
    
    mov rcx, g_hMap
    mov edx, 4
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    call MapViewOfFile
    mov g_pView, rax
    test rax, rax
    jz mmap_fail_view
    jmp mmap_done
    
mmap_fail_view:
    mov rcx, g_hMap
    call CloseHandle
mmap_fail_map:
mmap_fail_size:
    mov rcx, g_hFile
    call CloseHandle
mmap_fail_open:
    xor eax, eax
    
mmap_done:
    add rsp, 60h
    pop rdi
    pop rsi
    pop rbx
    ret
MMap_Open ENDP

MMap_Close PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rcx, g_pView
    test rcx, rcx
    jz mmap_cu
    call UnmapViewOfFile
mmap_cu:
    mov rcx, g_hMap
    test rcx, rcx
    jz mmap_cm
    call CloseHandle
mmap_cm:
    mov rcx, g_hFile
    test rcx, rcx
    jz mmap_cf
    call CloseHandle
mmap_cf:
    mov qword ptr g_pView, 0
    mov qword ptr g_hMap, 0
    mov qword ptr g_hFile, 0
    
    add rsp, 28h
    pop rbx
    ret
MMap_Close ENDP

MMap_Read PROC FRAME
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rax, g_pView
    test rax, rax
    jz mmap_rf
    
    add rax, rdx
    mov rsi, rax
    mov rdi, r8
    mov rcx, r9
    mov r10, r9
    rep movsb
    mov rax, r10
    jmp mmap_rd
    
mmap_rf:
    xor eax, eax
mmap_rd:
    add rsp, 28h
    pop rdi
    pop rsi
    ret
MMap_Read ENDP

MMap_GetSize PROC FRAME
    .endprolog
    mov rax, g_qwSize
    ret
MMap_GetSize ENDP

END
