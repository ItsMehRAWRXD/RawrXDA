; =========================
; FILE: rawr_cfg.asm  (SYSTEM 6: config loader, key=value, UTF-16)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

; API:
;   BOOL rawr_cfg_load_file_w(LPCWSTR path, LPVOID* out_buf, SIZE_T* out_sz)
;   (reads entire file into heap buffer, returns pointer+size)
; Caller parses as desired.

.code

extrn rawr_heap_alloc : proc
extrn rawr_heap_free  : proc

public rawr_cfg_load_file_w
rawr_cfg_load_file_w proc
    RAWR_PROLOGUE 30h
    RAWR_SAVE_NONVOL

    mov     r12, rcx            ; path
    mov     r13, rdx            ; out_buf*
    mov     r14, r8             ; out_sz*

    test    r12, r12
    jz      _fail
    test    r13, r13
    jz      _fail
    test    r14, r14
    jz      _fail

    ; h = CreateFileW(path, GENERIC_READ, SHARE_READ, NULL, OPEN_EXISTING, NORMAL, NULL)
    mov     rcx, r12
    mov     rdx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], OPEN_EXISTING
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+30h], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      _fail
    mov     r15, rax            ; h

    ; GetFileSizeEx(h, &sz64)
    lea     rdx, [rsp+20h]      ; reuse stack slot as LARGE_INTEGER
    mov     rcx, r15
    call    GetFileSizeEx
    test    eax, eax
    jz      _close_fail

    mov     rbx, qword ptr [rsp+20h]   ; size
    test    rbx, rbx
    jz      _close_fail

    ; buf = heap_alloc(size+2, zero=0)
    lea     rcx, [rbx+2]
    xor     edx, edx
    call    rawr_heap_alloc
    test    rax, rax
    jz      _close_fail
    mov     r12, rax            ; buf

    ; ReadFile(h, buf, size, &read, NULL)
    mov     rcx, r15
    mov     rdx, r12
    mov     r8,  rbx
    lea     r9,  [rsp+28h]
    mov     qword ptr [rsp+30h], 0
    call    ReadFile
    test    eax, eax
    jz      _free_close_fail

    ; null terminate (byte)
    mov     byte ptr [r12+rbx], 0
    mov     byte ptr [r12+rbx+1], 0

    ; out_buf = buf, out_sz = size
    mov     [r13], r12
    mov     [r14], rbx

    mov     rcx, r15
    call    CloseHandle

    mov     eax, 1
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 30h

_free_close_fail:
    mov     rcx, r12
    call    rawr_heap_free
_close_fail:
    mov     rcx, r15
    call    CloseHandle
_fail:
    xor     eax, eax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 30h
rawr_cfg_load_file_w endp

end