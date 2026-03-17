; =========================
; FILE: rawr_log.asm  (SYSTEM 3: logging)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

.data
public g_rawr_log_file
g_rawr_log_file dq 0

.data
g_nl_w      dw 13,10,0

.code

; BOOL rawr_log_open(LPCWSTR path)
; RCX=path
public rawr_log_open
rawr_log_open proc
    RAWR_PROLOGUE 0

    ; CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, NORMAL, NULL)
    mov     rdx, GENERIC_WRITE
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], OPEN_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+30h], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      _fail
    mov     g_rawr_log_file, rax
    mov     eax, 1
    RAWR_EPILOGUE 0
_fail:
    xor     eax, eax
    RAWR_EPILOGUE 0
rawr_log_open endp

; void rawr_log_close()
public rawr_log_close
rawr_log_close proc
    RAWR_PROLOGUE 0
    mov     rax, g_rawr_log_file
    test    rax, rax
    jz      _done
    mov     rcx, rax
    call    CloseHandle
    mov     g_rawr_log_file, 0
_done:
    RAWR_EPILOGUE 0
rawr_log_close endp

; BOOL rawr_log_write_w(LPCWSTR s)
; RCX=s
public rawr_log_write_w
rawr_log_write_w proc
    RAWR_PROLOGUE 10h
    RAWR_SAVE_NONVOL

    mov     r12, rcx
    test    r12, r12
    jz      _fail

    ; lenW = rawr_wcslen(s)
    extrn rawr_wcslen : proc
    mov     rcx, r12
    call    rawr_wcslen
    mov     r13, rax               ; wchar count
    test    r13, r13
    jz      _nl_only

    ; bytes = lenW * 2
    lea     r14, [r13*2]

    ; try file first if open, else stderr
    mov     rbx, g_rawr_log_file
    test    rbx, rbx
    jnz     _to_file

    ; stderr handle
    mov     ecx, STD_ERROR_HANDLE
    call    GetStdHandle
    mov     rbx, rax
_to_file:
    ; WriteFile(h, buf, bytes, &w, NULL)
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8,  r14
    lea     r9,  [rsp+20h]
    mov     qword ptr [rsp+28h], 0
    call    WriteFile
    test    eax, eax
    jz      _fail

_nl_only:
    ; write CRLF
    mov     rbx, g_rawr_log_file
    test    rbx, rbx
    jnz     _nl_file
    mov     ecx, STD_ERROR_HANDLE
    call    GetStdHandle
    mov     rbx, rax
_nl_file:
    mov     rcx, rbx
    lea     rdx, g_nl_w
    mov     r8,  4
    lea     r9,  [rsp+20h]
    mov     qword ptr [rsp+28h], 0
    call    WriteFile
    test    eax, eax
    jz      _fail

    mov     eax, 1
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 10h

_fail:
    xor     eax, eax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 10h
rawr_log_write_w endp

end