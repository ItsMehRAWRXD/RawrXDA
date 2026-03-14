EXTERN WritePEFile:PROC
EXTERN SavePEToDisk:PROC
EXTERN ExitProcess:PROC

PUBLIC g_peBuffer
PUBLIC g_peSize

.data
g_peBuffer db 8192 dup(0)
g_peSize dq 0

.code
main PROC
    sub rsp, 28h
    call WritePEFile
    call SavePEToDisk
    xor ecx, ecx
    call ExitProcess
main ENDP
END
