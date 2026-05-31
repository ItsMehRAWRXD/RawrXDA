.code
extern ExitProcess: proc
VectorIndexMain proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
VectorIndexMain endp
end
