;  RawrXD_LazyHotpatch_v3.asm
;  Working hotpatch: file open -> mapping -> view creation
;  Windows x64 calling convention: RCX, RDX, R8, R9, then stack

PUBLIC  RawrXD_LazyHotpatch

EXTERN  CreateFileW:PROC
EXTERN  CreateFileMappingW:PROC
EXTERN  MapViewOfFile:PROC
EXTERN  CloseHandle:PROC

.const
GENERIC_READ        = 080000000h
FILE_SHARE_READ     = 00000001h
OPEN_EXISTING       = 00000003h
FILE_ATTRIBUTE_NORMAL = 00000080h
PAGE_READONLY       = 00000002h
FILE_MAP_READ       = 00000004h

.code

RawrXD_LazyHotpatch PROC
        push    rbx
        push    r12
        push    r13
        push    r14
        
        mov     r12, rcx            ; r12 = modelPath
        mov     r13, rdx            ; r13 = &outMap
        mov     r14, r8             ; r14 = &outView
        
        ; 1. CreateFileW(modelPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
        mov     rcx, r12
        mov     rdx, GENERIC_READ
        mov     r8, FILE_SHARE_READ
        xor     r9, r9
        sub     rsp, 40
        mov     dword ptr [rsp+32], OPEN_EXISTING
        mov     dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
        call    CreateFileW
        add     rsp, 40
        
        cmp     rax, -1
        je      @@fail
        
        mov     rbx, rax            ; rbx = hFile
        
        ; 2. CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
        mov     rcx, rbx
        xor     rdx, rdx
        mov     r8, PAGE_READONLY
        xor     r9, r9
        sub     rsp, 40
        mov     qword ptr [rsp+32], 0
        mov     qword ptr [rsp+40], 0
        call    CreateFileMappingW
        add     rsp, 40
        
        test    rax, rax
        jz      @@close_file_and_fail
        
        mov     [r13], rax          ; *outMap = hMapping
        
        ; Close file handle
        mov     rcx, rbx
        sub     rsp, 32
        call    CloseHandle
        add     rsp, 32
        
        ; 3. MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 4MB)
        mov     rcx, [r13]          ; hMapping
        mov     rdx, FILE_MAP_READ
        xor     r8, r8
        xor     r9, r9
        sub     rsp, 40
        mov     dword ptr [rsp+32], 4194304    ; 4MB
        call    MapViewOfFile
        add     rsp, 40
        
        ; Store view pointer
        mov     [r14], rax
        
        ; Success
        xor     eax, eax
        pop     r14
        pop     r13
        pop     r12
        pop     rbx
        ret
        
@@close_file_and_fail:
        mov     rcx, rbx
        sub     rsp, 32
        call    CloseHandle
        add     rsp, 32
        
@@fail:
        mov     eax, 1
        pop     r14
        pop     r13
        pop     r12
        pop     rbx
        ret

RawrXD_LazyHotpatch ENDP
END
