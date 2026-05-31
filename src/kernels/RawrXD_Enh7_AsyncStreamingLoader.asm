; RawrXD_Enh7_AsyncStreamingLoader.asm
OPTION CASEMAP:NONE

.DATA
g_file_size QWORD 0
g_bytes_loaded QWORD 0
g_is_cancelled BYTE 0

.CODE
AsyncStreamingLoader_Initialize PROC
    mov g_file_size, 0
    mov g_bytes_loaded, 0
    mov g_is_cancelled, 0
    xor eax, eax
    ret
AsyncStreamingLoader_Initialize ENDP

AsyncStreamingLoader_BeginStream PROC
    mov rax, g_file_size
    test rax, rax
    jnz have
    mov rax, 134217728
    mov g_file_size, rax
have:
    mov g_bytes_loaded, rax
    xor eax, eax
    ret
AsyncStreamingLoader_BeginStream ENDP

AsyncIO_WorkerThread PROC
    mov rax, g_file_size
    mov g_bytes_loaded, rax
    xor eax, eax
    ret
AsyncIO_WorkerThread ENDP

AsyncStreamingLoader_Cancel PROC
    mov g_is_cancelled, 1
    xor eax, eax
    ret
AsyncStreamingLoader_Cancel ENDP
END
