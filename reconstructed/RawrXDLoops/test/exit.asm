; Ultra-minimal x64 Windows application - just exit
option casemap:none

extern ExitProcess:proc

.code

WinMain proc
    mov ecx, 42
    call ExitProcess
WinMain endp

end
