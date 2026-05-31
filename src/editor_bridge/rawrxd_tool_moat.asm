.code
extern ExitProcess: proc
ToolMoatMain proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
ToolMoatMain endp
end
