; Minimal WinMain stub for monolithic RawrXD.exe
; Provides entry point for Windows subsystem executable

.code

PUBLIC WinMain

WinMain PROC
    ; Parameters: hInstance, hPrevInstance, lpCmdLine, nCmdShow
    ; Minimal implementation - just return 0
    xor eax, eax    ; Return 0
    ret
WinMain ENDP

END
