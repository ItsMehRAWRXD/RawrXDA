; =========================
; FILE: rawr_time.asm  (SYSTEM 4: time/metrics)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

.data
public g_qpf
g_qpf dq 0

.code

; BOOL rawr_time_init()
public rawr_time_init
rawr_time_init proc
    RAWR_PROLOGUE 0

    lea     rcx, g_qpf
    call    QueryPerformanceFrequency
    test    eax, eax
    jz      _fail
    mov     eax, 1
    RAWR_EPILOGUE 0
_fail:
    xor     eax, eax
    RAWR_EPILOGUE 0
rawr_time_init endp

; UINT64 rawr_qpc()
public rawr_qpc
rawr_qpc proc
    RAWR_PROLOGUE 0
    lea     rcx, [rsp+28h]        ; local qword
    call    QueryPerformanceCounter
    mov     rax, qword ptr [rsp+28h]
    RAWR_EPILOGUE 0
rawr_qpc endp

end