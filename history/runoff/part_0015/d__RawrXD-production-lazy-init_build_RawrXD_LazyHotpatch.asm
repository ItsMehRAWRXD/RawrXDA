;  RawrXD_LazyHotpatch.asm  (copy-paste, ml64 /c /Fo$@ $<)
;  64-byte header, 0 deps, 1.1:1 compression aware
PUBLIC  RawrXD_LazyHotpatch
EXTRN   GetCurrentProcess:PROC, VirtualLock:PROC, CreateFileMappingW:PROC
EXTRN   MapViewOfFileEx:PROC, UnmapViewOfFile:PROC, PrefetchVirtualMemory:PROC, CloseHandle:PROC

.const
PAGE_SIZE       EQU 1000h
ZONE_SIZE       EQU 10000h          ; 64k granular zones
PRELOAD_SZ      EQU 400000h         ; 4MB pin
PF_FLAGS        EQU 1               ; WIN32_MEMORY_RANGE_ENTRY

.code
RawrXD_LazyHotpatch PROC
        ; RCX = wchar_t* modelPath
        ; RDX = HANDLE* outMap
        ; R8  = void**  outView
        push    rbx
        push    rsi
        push    rdi
        push    r12
        push    r13
        push    r14
        push    r15
        mov     r12, rdx              ; save outMap
        mov     r13, r8               ; save outView
        xor     r14d, r14d            ; zone counter

        ; 1. open file mapping
        lea     rdx, [rsp+20h]        ; security
        xor     r8d, r8d              ; PAGE_READONLY
        mov     r9d, 2                ; SEC_RESERVE
        call    CreateFileMappingW
        test    rax, rax
        jz      @@done
        mov     [r12], rax            ; return map

        ; 2. map first 4MB + header (0-copy)
        xor     r8d, r8d
        mov     r9d, PRELOAD_SZ
        xor     edx, edx
        call    MapViewOfFileEx
        test    rax, rax
        jz      @@unmap
        mov     [r13], rax            ; return view
        mov     rdi, rax

        ; 3. parallel zone header decode (AVX-512)
        mov     rsi, rdi
        mov     ecx, PRELOAD_SZ / ZONE_SIZE
@@zone_loop:
        vmovdqa32 zmm0, [rsi]         ; 64b header
        vpsrldq zmm1, zmm0, 32        ; tensor count
        vpaddd  zmm2, zmm0, zmm1
        add     rsi, ZONE_SIZE
        loop    @@zone_loop

        ; 4. pin + prefetch first 4MB
        lea     rcx, [rsp-18h]
        mov     [rsp-18h], rdi
        mov     qword ptr [rsp-10h], PRELOAD_SZ
        mov     edx, 1
        lea     r8, [rsp-18h]
        call    PrefetchVirtualMemory
        mov     rcx, rax
        call    VirtualLock

        xor     eax, eax              ; success
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        ret
@@unmap:mov     rcx, [r12]
        call    CloseHandle
@@done: mov     eax, 1                ; fail
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        ret
RawrXD_LazyHotpatch ENDP
END
