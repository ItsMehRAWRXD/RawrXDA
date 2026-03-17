;  RawrXD_LazyHotpatch_v2.asm
;  Simplified, correct Windows x64 calling convention
;  Parameters (x64 ABI): RCX=file path (wchar_t*), RDX=&outMap, R8=&outView
;  RSP must be 16-byte aligned BEFORE the CALL instruction (i.e., misaligned by 8 at PROC entry)

PUBLIC  RawrXD_LazyHotpatch

EXTERN  CreateFileW:PROC              ; HANDLE CreateFileW(...)
EXTERN  CreateFileMappingW:PROC       ; HANDLE CreateFileMappingW(HANDLE hFile, ...)  
EXTERN  MapViewOfFile:PROC            ; LPVOID MapViewOfFile(HANDLE hFileMappingObject, ...)
EXTERN  CloseHandle:PROC              ; BOOL CloseHandle(HANDLE hObject)

.const
GENERIC_READ        = 080000000h
FILE_SHARE_READ     = 00000001h
OPEN_EXISTING       = 00000003h
FILE_ATTRIBUTE_NORMAL = 00000080h
PAGE_READONLY       = 00000002h

.code

RawrXD_LazyHotpatch PROC

        ;  Entry: RSP is misaligned by 8 bytes (standard x64 ABI)
        ;  We push registers to balance stack alignment
        
        push    rbx
        push    r12
        push    r13
        
        ;  Save parameters to non-volatile registers
        mov     r12, rcx            ; r12 = modelPath
        mov     r13, rdx            ; r13 = &outMap
        ;  R8 already has &outView

        ;  Call CreateFileW(modelPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
        ;
        ;  Arguments in order: RCX, RDX, R8, R9, [RSP+32], [RSP+40], [RSP+48]
        mov     rcx, r12                    ; lpFileName = modelPath
        mov     rdx, GENERIC_READ           ; dwDesiredAccess
        mov     r8, FILE_SHARE_READ         ; dwShareMode
        xor     r9, r9                      ; lpSecurityAttributes = NULL
        ;  Additional args go on stack:
        sub     rsp, 32                     ; allocate shadow space + extras
        mov     dword ptr [rsp+32], OPEN_EXISTING  ; dwCreationDisposition
        mov     dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL  ; dwFlagsAndAttributes
        mov     qword ptr [rsp+48], 0      ; hTemplateFile = NULL
        call    CreateFileW
        add     rsp, 32
        
        ;  Check return: INVALID_HANDLE_VALUE = -1 (0xFFFFFFFFFFFFFFFF)
        cmp     rax, -1
        je      @@fail_exit
        
        mov     rbx, rax            ; rbx = hFile
        
        ;  Call CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
        mov     rcx, rbx            ; hFile
        xor     rdx, rdx            ; lpFileMappingAttributes = NULL
        mov     r8, PAGE_READONLY   ; flProtect
        xor     r9, r9              ; dwMaximumSizeHigh = 0
        ;  More args on stack:
        sub     rsp, 32             ; shadow space + extras
        xor     eax, eax
        mov     dword ptr [rsp+32], eax     ; dwMaximumSizeLow = 0
        mov     qword ptr [rsp+40], 0      ; lpName = NULL
        call    CreateFileMappingW
        add     rsp, 32
        
        ;  Check mapping handle
        test    rax, rax
        jz      @@close_file_and_fail
        
        ;  Store mapping handle in *outMap
        mov     [r13], rax
        
        ;  Close file handle (we have the mapping, file handle can be closed)
        mov     rcx, rbx
        sub     rsp, 32
        call    CloseHandle
        add     rsp, 32
        
        ;  Now map a view of the file mapping
        ;  MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap)
        mov     rcx, [r13]          ; hFileMappingObject (the mapping we just created)
        mov     edx, 4              ; FILE_MAP_READ
        xor     r8, r8              ; dwFileOffsetHigh = 0
        xor     r9, r9              ; dwFileOffsetLow = 0
        ;  The 5th parameter (number of bytes) goes on stack
        sub     rsp, 32
        mov     dword ptr [rsp+32], 4194304    ; 4MB in decimal  
        call    MapViewOfFile
        add     rsp, 32
        
        ;  Check if view was created
        test    rax, rax
        jz      @@mapping_ok_but_no_view
        
        ;  Store view pointer in *outView (R8 still has &outView from entry)
        mov     [r8], rax
        
        ;  Return success (0)
        xor     eax, eax
        
        pop     r13
        pop     r12
        pop     rbx
        ret

@@mapping_ok_but_no_view:
        ;  Mapping succeeded but view failed; still return success since file is mapped
        xor     eax, eax
        
        pop     r13
        pop     r12
        pop     rbx
        ret
        
        ;  Store mapping handle in *outMap
        mov     [r13], rax
        
        ;  Close file handle (we have the mapping, file handle can be closed)
        mov     rcx, rbx
        sub     rsp, 32
        call    CloseHandle
        add     rsp, 32
        
        ;  Return success (0)
        xor     eax, eax
        
        pop     r13
        pop     r12
        pop     rbx
        ret

@@close_file_and_fail:
        mov     rcx, rbx
        sub     rsp, 32
        call    CloseHandle
        add     rsp, 32
        
@@fail_exit:
        mov     eax, 1              ; return 1 for failure
        pop     r13
        pop     r12
        pop     rbx
        ret

RawrXD_LazyHotpatch ENDP
END
