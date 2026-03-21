; Build: ml64 /c /nologo sovereign_macros_smoke.asm
; Link:  link /nologo sovereign_macros_smoke.obj kernel32.lib /subsystem:console /entry:main
include sovereign_macros.inc

extern __imp_ExitProcess : qword

.code

main PROC
    xor ecx, ecx
    SovereignCallImp __imp_ExitProcess
main ENDP

END
