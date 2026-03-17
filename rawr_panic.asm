; =========================
; FILE: rawr_panic.asm  (PHASE 1: panic)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

.data
g_panic_prefix  dw 'R','a','w','r','X','D',' ','P','A','N','I','C',':',' ',0

.code

; void rawr_panic_w(LPCWSTR msg, DWORD code)
; RCX=msg, EDX=exit code
public rawr_panic_w
rawr_panic_w proc
    RAWR_PROLOGUE 0

    mov     qword ptr [rsp+28h], rcx
    mov     dword ptr [rsp+30h], edx

    lea     rcx, g_panic_prefix
    call    OutputDebugStringW

    mov     rcx, qword ptr [rsp+28h]
    call    OutputDebugStringW

    mov     ecx, dword ptr [rsp+30h]
    call    ExitProcess

    RAWR_EPILOGUE 0
rawr_panic_w endp
end