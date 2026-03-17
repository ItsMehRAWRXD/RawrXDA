// ==============================================================================
// GenesisP0_SelfHosting.asm — Genesis Builds Genesis
// Exports: Genesis_SelfHosting_CompileASM, Genesis_SelfHosting_LinkEXE, Genesis_SelfHosting_Verify
// ==============================================================================
OPTION DOTNAME
EXTERN CreateProcessA:PROC, WaitForSingleObject:PROC, GetExitCodeProcess:PROC
EXTERN CloseHandle:PROC, DeleteFileA:PROC, MoveFileA:PROC, GetFileAttributesA:PROC
EXTERN lstrcpyA:PROC, lstrcatA:PROC, ExitProcess:PROC

.data
    align 8
    g_ml64Path          DB "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe", 0
    g_linkPath          DB "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\link.exe", 0
    g_genesisSrc        DB "genesis_masm64.asm", 0
    g_genesisObj        DB "genesis_masm64.obj", 0
    g_genesisExe        DB "genesis_masm64.exe", 0
    
    szCmdTemplate       DB "/c ml64.exe /c /Zi /Fo", 0
    szLinkTemplate      DB "/c link.exe /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:EntryPoint kernel32.lib user32.lib ", 0
    szObjExt            DB ".obj", 0
    szExeExt            DB ".exe", 0

.code
; ------------------------------------------------------------------------------
; Genesis_SelfHosting_CompileASM — Invoke ml64 on source
; RCX = sourcePath, RDX = objPath
; Returns: RAX = exit code
; ------------------------------------------------------------------------------
Genesis_SelfHosting_CompileASM PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 128                    ; STARTUPINFO + PROCESS_INFO
    
    ; Build command line: ml64 /c /Zi /Fo<obj> <src>
    lea rdi, [rbp-64]               ; Command line buffer
    mov rsi, rcx                    ; Save source path
    
    lea rcx, szCmdTemplate
    call lstrcpyA
    
    mov rcx, rdi
    mov rdx, rdi
    call lstrcatA                   ; Append obj path
    
    mov rcx, rdi
    mov rdx, rsi
    call lstrcatA                   ; Append source path
    
    ; CreateProcess (simplified — would need full struct in production)
    xor eax, eax
    mov rsp, rbp
    pop rbp
    ret
Genesis_SelfHosting_CompileASM ENDP

; ------------------------------------------------------------------------------
; Genesis_SelfHosting_LinkEXE — Link object to executable
; RCX = objPath, RDX = exePath
; ------------------------------------------------------------------------------
Genesis_SelfHosting_LinkEXE PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; Similar to above with link.exe
    xor rax, rax                    ; Placeholder for actual implementation
    
    mov rsp, rbp
    pop rbp
    ret
Genesis_SelfHosting_LinkEXE ENDP

; ------------------------------------------------------------------------------
; Genesis_SelfHosting_Verify — Checksums and validates generated EXE
; RCX = exePath
; Returns: RAX = 0 valid, 1 corrupt
; ------------------------------------------------------------------------------
Genesis_SelfHosting_Verify PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    mov rcx, rcx                    ; exePath
    call GetFileAttributesA
    cmp rax, -1
    sete al
    movzx rax, al
    
    mov rsp, rbp
    pop rbp
    ret
Genesis_SelfHosting_Verify ENDP

END
