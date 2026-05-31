.code
extern ExitProcess: proc
MultiFileComposerMain proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
MultiFileComposerMain endp
end
