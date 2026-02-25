; RawrXD_NativeHost.asm - Runs inside RawrXD process
OPTION CASEMAP:NONE

includelib kernel32.lib

.data
g_extArray      DQ 128 DUP(0)
g_extCount      DD 0
szPipeName      DB "\\.\pipe\RawrXD_Native",0

.code
InitNativeHost PROC
    ret
InitNativeHost ENDP

NativeCompletionTrigger PROC
    ret
NativeCompletionTrigger ENDP

PUBLIC InitNativeHost
END
