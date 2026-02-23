; src/direct_io/nvme_thermal_sidecar_entry.asm
; Minimal entry stub that safely calls SidecarMain

OPTION casemap:none

EXTERN SidecarMain:PROC
EXTERN ExitProcess:PROC

.code
PUBLIC SidecarEntry
SidecarEntry PROC
    sub rsp, 20h
    call SidecarMain
    add rsp, 20h
    xor ecx, ecx
    call ExitProcess
    ret
SidecarEntry ENDP

END
