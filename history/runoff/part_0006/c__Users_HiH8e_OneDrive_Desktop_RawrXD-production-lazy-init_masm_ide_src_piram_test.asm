.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

PiRam_Compress PROTO
PiRam_Stream PROTO :DWORD, :DWORD

ExitProcess PROTO :DWORD

.data
    testData db 256 dup(?)
    
.code
start:
    ; Test basic compression
    mov eax, offset testData
    mov edx, 256
    call PiRam_Compress
    
    ; Test streaming (4KB chunks)
    mov eax, offset testData
    mov edx, 256
    call PiRam_Stream
    
    ; Exit with success
    invoke ExitProcess, 0
end start
