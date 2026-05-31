.code
extern ExitProcess: proc
SpeculativeHubVerify proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
SpeculativeHubVerify endp
end
