.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; Prototypes
GGUF_LoadModel PROTO :DWORD
PiRam_CompressGGUF PROTO :DWORD
PiRam_GetCompressionRatio PROTO
ExitProcess PROTO :DWORD

.data
    testBuffer db 1024 dup(0)  ; 1KB test data

.code
start:
    ; Fill buffer with pattern
    mov ecx, 1024
    mov edi, offset testBuffer
    mov al, 0
fill:
    mov [edi], al
    inc al
    inc edi
    loop fill

    ; Simulate model structure, set size at +16
    mov eax, offset testBuffer
    mov dword ptr [eax+16], 1024

    ; Compress with π-RAM
    invoke PiRam_CompressGGUF, eax
    test eax, eax
    jz fail_compress  ; exit 2 if compress fail

    ; Get compression ratio
    invoke PiRam_GetCompressionRatio

    ; Exit with ratio as code
    invoke ExitProcess, eax

fail_compress:
    invoke ExitProcess, 2

END start