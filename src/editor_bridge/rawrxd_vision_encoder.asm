.code
extern ExitProcess: proc
VisionEncoderMain proc
    sub rsp, 40
    xor rcx, rcx
    call ExitProcess
VisionEncoderMain endp
end
