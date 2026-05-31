.code
extern ExitProcess: proc
GitNativeMain proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
GitNativeMain endp
end
