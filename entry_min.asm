; =========================
; FILE: entry_min.asm  (PHASE 1: entry)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

extrn rawr_main : proc

.code
public mainCRTStartup
mainCRTStartup proc
    RAWR_PROLOGUE 0

    xor     rcx, rcx
    call    GetModuleHandleW

    call    GetCommandLineW

    call    rawr_main

    mov     ecx, eax
    call    ExitProcess

    RAWR_EPILOGUE 0
mainCRTStartup endp
end