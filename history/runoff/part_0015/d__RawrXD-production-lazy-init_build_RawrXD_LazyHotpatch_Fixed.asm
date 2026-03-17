;  RawrXD_LazyHotpatch_Fixed.asm
;  Complete hotpatch with proper file opening and mapping
;  Parameters: RCX = modelPath (wchar_t*), RDX = &outMap, R8 = &outView

PUBLIC  RawrXD_LazyHotpatch

EXTRN   CreateFileW:PROC
EXTRN   CreateFileMappingW:PROC
EXTRN   MapViewOfFileEx:PROC
EXTRN   UnmapViewOfFile:PROC
EXTRN   CloseHandle:PROC
EXTRN   VirtualLock:PROC
EXTRN   PrefetchVirtualMemory:PROC
EXTRN   GetCurrentProcess:PROC

.const
GENERIC_READ        EQU 80000000h
FILE_SHARE_READ     EQU 00000001h
OPEN_EXISTING       EQU 00000003h
FILE_ATTRIBUTE_NORMAL EQU 00000080h
PAGE_READONLY       EQU 00000002h
FILE_MAP_READ       EQU 00000004h
SEC_COMMIT          EQU 00000008h
PRELOAD_SZ          EQU 400000h      ; 4MB

.code
RawrXD_LazyHotpatch PROC
        ; RCX = wchar_t* modelPath (file path)
        ; RDX = HANDLE* outMap (output: file mapping handle)
        ; R8  = void**  outView (output: mapped view pointer)
        
        push    rbp
        mov     rbp, rsp
        push    rbx
        push    rsi
        push    rdi
        push    r12
        push    r13
        
        ; Save parameters
        mov     rsi, rcx            ; rsi = modelPath
        mov     r12, rdx            ; r12 = &outMap
        mov     r13, r8             ; r13 = &outView
        
        ; Step 1: Open file using CreateFileW
        ; CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        ;            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile)
        mov     rcx, rsi            ; lpFileName = modelPath
        mov     rdx, GENERIC_READ   ; dwDesiredAccess = GENERIC_READ
        mov     r8,  FILE_SHARE_READ ; dwShareMode = FILE_SHARE_READ
        xor     r9,  r9             ; lpSecurityAttributes = NULL
        mov     eax, OPEN_EXISTING  ; dwCreationDisposition
        push    rax
        mov     eax, FILE_ATTRIBUTE_NORMAL ; dwFlagsAndAttributes
        push    rax
        xor     eax, eax            ; hTemplateFile = NULL
        push    rax
        sub     rsp, 20h
        mov     eax, OPEN_EXISTING
        mov     [rsp + 20h], eax
        call    CreateFileW
        add     rsp, 38h
        
        cmp     rax, -1             ; INVALID_HANDLE_VALUE
        je      @@fail
        
        mov     rbx, rax            ; rbx = hFile
        
        ; Step 2: Create file mapping
        ; CreateFileMappingW(hFile, lpFileMappingAttributes, flProtect,
        ;                   dwMaximumSizeHigh, dwMaximumSizeLow, lpName)
        mov     rcx, rbx            ; hFile
        xor     rdx, rdx            ; lpFileMappingAttributes = NULL
        mov     r8d, PAGE_READONLY  ; flProtect = PAGE_READONLY
        xor     r9d, r9d            ; dwMaximumSizeHigh = 0 (use file size)
        xor     eax, eax            ; dwMaximumSizeLow = 0
        push    rax                 ; lpName = NULL
        sub     rsp, 20h
        call    CreateFileMappingW
        add     rsp, 28h
        
        test    rax, rax
        jz      @@close_file
        
        mov     [r12], rax          ; *outMap = hMapping
        mov     rdi, rax            ; rdi = hMapping
        
        ; Step 3: Map view of file
        ; MapViewOfFile(hFileMappingObject, dwDesiredAccess,
        ;              dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap)
        mov     rcx, rdi            ; hFileMappingObject
        mov     edx, FILE_MAP_READ  ; dwDesiredAccess
        xor     r8d, r8d            ; dwFileOffsetHigh
        xor     r9d, r9d            ; dwFileOffsetLow
        mov     eax, PRELOAD_SZ     ; dwNumberOfBytesToMap = 4MB
        push    rax
        sub     rsp, 20h
        call    MapViewOfFileEx
        add     rsp, 28h
        
        test    rax, rax
        jz      @@close_mapping
        
        mov     [r13], rax          ; *outView = pView
        
        ; Step 4: Lock memory (optional, but recommended)
        mov     rcx, rax            ; baseAddress = pView
        mov     rdx, PRELOAD_SZ     ; numberOfBytes
        mov     r8d, 1              ; flags
        sub     rsp, 20h
        call    VirtualLock
        add     rsp, 20h
        
        ; Success
        xor     eax, eax            ; return 0 = success
        
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        mov     rsp, rbp
        pop     rbp
        ret
        
@@close_mapping:
        mov     rcx, rdi
        sub     rsp, 20h
        call    CloseHandle
        add     rsp, 20h
        
@@close_file:
        mov     rcx, rbx
        sub     rsp, 20h
        call    CloseHandle
        add     rsp, 20h
        
@@fail:
        mov     eax, 1              ; return 1 = failure
        
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        mov     rsp, rbp
        pop     rbp
        ret

RawrXD_LazyHotpatch ENDP
END
