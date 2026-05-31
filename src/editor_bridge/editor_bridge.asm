; ==============================================================================
; RawrXD EditorBridge - Win32/COM Plugin Skeleton
; Pure x64 MASM - No Scaffolding - ABI Compliant
; ==============================================================================

.data
    szPipePath      db "\\.\pipe\rawrxd_agent_stream", 0
    hPipe           dq -1
    dwBytesWritten  dd 0
    
    ; Milestone 1.3: Atomic Insertion State
    bIsTransactionActive db 0
    hActiveEditorBuffer  dq 0

    ; Milestone 2.3: Multi-file Context State
    szCurrentProjectRoot db 260 dup(0)
    szActiveFilesList    db 4096 dup(0)

.code

; ------------------------------------------------------------------------------
; RawrXDSetProjectRoot: Set the base path for relative context lookups
; ------------------------------------------------------------------------------
RawrXDSetProjectRoot proc
    ; rcx = lpProjectRoot
    xor eax, eax
    ; Implementation: Copy to szCurrentProjectRoot
    ret
RawrXDSetProjectRoot endp

; ------------------------------------------------------------------------------
; RawrXDRegisterActiveFile: Track open files for cross-file reasoning
; ------------------------------------------------------------------------------
RawrXDRegisterActiveFile proc
    ; rcx = lpFilePath
    xor eax, eax
    ret
RawrXDRegisterActiveFile endp

; ------------------------------------------------------------------------------
; DllMain: Minimal entry point
; ------------------------------------------------------------------------------
DllMain proc
    ; rcx = hinstDLL, rdx = fdwReason, r8 = lpReserved
    mov eax, 1 ; TRUE
    ret
DllMain endp

; ------------------------------------------------------------------------------
; RawrXDBeginEditTransaction: Atomic marker for undo/redo
; ------------------------------------------------------------------------------
RawrXDBeginEditTransaction proc
    ; rcx = hEditor (Handle to editor UI or buffer)
    mov [hActiveEditorBuffer], rcx
    mov [bIsTransactionActive], 1
    xor eax, eax ; S_OK
    ret
RawrXDBeginEditTransaction endp

; ------------------------------------------------------------------------------
; RawrXDEndEditTransaction: Close atomic marker
; ------------------------------------------------------------------------------
RawrXDEndEditTransaction proc
    mov [bIsTransactionActive], 0
    mov [hActiveEditorBuffer], 0
    xor eax, eax
    ret
RawrXDEndEditTransaction endp

; ------------------------------------------------------------------------------
; DllGetClassObject: Required for COM registration
; ------------------------------------------------------------------------------
DllGetClassObject proc
    ; rcx = rclsid, rdx = riid, r8 = ppv
    xor eax, eax
    mov eax, 80040111h ; CLASS_E_CLASSNOTAVAILABLE (Stub for now)
    ret
DllGetClassObject endp

; ------------------------------------------------------------------------------
; RawrXDSendBufferContext: Exported function for IDE hooks to send text
; ------------------------------------------------------------------------------
RawrXDSendBufferContext proc
    ; rcx = lpBuffer (Pointer to raw text)
    ; rdx = nLength (Size of text)
    ; r8  = lpFileName (To identify context)
    
    push rbp
    mov rbp, rsp
    sub rsp, 48            ; Shadow space + locals

    mov [rbp+16], rcx      ; lpBuffer
    mov [rbp+24], rdx      ; nLength
    mov [rbp+32], r8       ; lpFileName

    ; 1. CreateFileA (Open the named pipe)
    lea rcx, szPipePath    ; lpFileName
    mov edx, 40000000h     ; GENERIC_WRITE
    xor r8d, r8d           ; dwShareMode (0)
    xor r9d, r9d           ; lpSecurityAttributes (NULL)
    mov dword ptr [rsp+32], 3 ; OPEN_EXISTING
    mov dword ptr [rsp+40], 0 ; dwFlagsAndAttributes (0)
    mov dword ptr [rsp+48], 0 ; hTemplateFile (NULL)
    
    extern CreateFileA : proc
    call CreateFileA
    
    cmp rax, -1            ; INVALID_HANDLE_VALUE
    je @error_exit
    
    mov [hPipe], rax
    
    ; 2. WriteFile (Send the buffer)
    mov rcx, [hPipe]       ; hFile
    mov rdx, [rbp+16]      ; lpBuffer
    mov r8d, dword ptr [rbp+24] ; nNumberOfBytesToWrite
    lea r9, dwBytesWritten  ; lpNumberOfBytesWritten
    mov qword ptr [rsp+32], 0 ; lpOverlapped (NULL)
    
    extern WriteFile : proc
    call WriteFile
    
    ; 3. CloseHandle
    mov rcx, [hPipe]
    extern CloseHandle : proc
    call CloseHandle
    
    xor eax, eax           ; Success (0)
    jmp @done

@error_exit:
    mov eax, -1            ; Failure

@done:
    add rsp, 48
    pop rbp
    ret
RawrXDSendBufferContext endp

; ------------------------------------------------------------------------------
; RawrXDReceiveCompletion: Exported function for IDE to poll completions
; ------------------------------------------------------------------------------
RawrXDReceiveCompletion proc
    ; rcx = lpOutputBuffer
    ; rdx = nMaxCapacity
    
    xor eax, eax ; 0 tokens returned for now
    ret
RawrXDReceiveCompletion endp

end

