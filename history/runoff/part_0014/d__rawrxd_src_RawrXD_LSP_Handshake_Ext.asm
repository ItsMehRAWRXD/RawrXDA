; RawrXD_LSP_Handshake_Ext.asm
; Extended LSP Handshake with Server Detection & State Machine
; Pure MASM x64

option casemap:none

; External Win32 APIs
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN GetProcessHeap:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN GetCurrentProcessId:PROC

; External RawrXD functions (forward declarations)
EXTERN LSP_Transport_Write:PROC
EXTERN LSP_JsonRpc_BuildNotification:PROC

; Constants
HS_STATE_IDLE           equ 0
HS_STATE_INIT_SENT      equ 1
HS_STATE_INIT_RECEIVED  equ 2
HS_STATE_INITIALIZED    equ 3
HS_STATE_READY          equ 4
HS_STATE_ERROR          equ 5

SERVER_UNKNOWN          equ 0
SERVER_CLANGD           equ 1
SERVER_PYRIGHT          equ 2
SERVER_RUST_ANALYZER    equ 3
SERVER_TYPESCRIPT       equ 4
SERVER_GOLANG           equ 5
SERVER_ZIG              equ 6

MAX_SERVER_NAME         equ 64
MAX_ROOT_PATH           equ 260
MAX_INIT_OPTIONS        equ 4096
HS_TIMEOUT_INIT         equ 10000

; Structures
LSP_HANDSHAKE_CTX STRUCT
    State                   dd ?
    ServerType              dd ?
    DetectionMethod         dd ?
    _padding1               dd ?
    
    TransportHandle         dq ?
    WriteFn                 dq ?
    ReadFn                  dq ?
    UserData                dq ?
    
    ServerCommand           db MAX_SERVER_NAME dup(?)
    ServerName              db MAX_SERVER_NAME dup(?)
    ServerVersion           db 32 dup(?)
    _padding2               db 32 dup(?)
    
    RootUri                 dw MAX_ROOT_PATH dup(?)
    
    InitializationOptions   db MAX_INIT_OPTIONS dup(?)
    OptionsLength           dd ?
    
    RequestId               dd ?
    StartTime               dq ?
    TimeoutMs               dd ?
    _padding3               dd ?
    
    OnServerDetected        dq ?
    OnCapabilitiesReceived  dq ?
    OnHandshakeComplete     dq ?
    OnError                 dq ?
    
    LastErrorCode           dd ?
    _padding4               dd ?
    LastErrorMessage        db 256 dup(?)
LSP_HANDSHAKE_CTX ENDS

SERVER_PROFILE STRUCT
    ServerType              dd ?
    _padding1               dd ?
    DetectionStrings        dq ?
    StringCount             dd ?
    _padding2               dd ?
    InitOptionsTemplate     dq ?
    TemplateLength          dd ?
    RequestedCaps           dd ?
    Flags                   dd ?
    _padding3               dd ?
SERVER_PROFILE ENDS

.data
align 16

; Detection strings
szClangdDetect          db "clangd",0
szClangdExe             db "clangd.exe",0
szPyrightDetect         db "pyright",0
szPyrightExe            db "pyright.exe",0
szRustAnalyzerDetect    db "rust-analyzer",0
szTypeScriptDetect      db "typescript",0
szGoplsDetect           db "gopls",0
szZlsDetect             db "zls",0

ClangdStrings           dq offset szClangdDetect, offset szClangdExe, 0
PyrightStrings          dq offset szPyrightDetect, offset szPyrightExe, 0
RustAnalyzerStrings     dq offset szRustAnalyzerDetect, 0
TypeScriptStrings       dq offset szTypeScriptDetect, 0
GolangStrings           dq offset szGoplsDetect, 0
ZigStrings              dq offset szZlsDetect, 0

; Init option templates
ClangdInitTemplate      db '{"fallbackFlags":[],"completion":{"placeholder":true}}',0
ClangdInitTemplateLen   equ $ - ClangdInitTemplate - 1
PyrightInitTemplate     db '{"python":{"pythonPath":"","venvPath":""},"typeCheckingMode":"basic"}',0
PyrightInitTemplateLen  equ $ - PyrightInitTemplate - 1
RustAnalyzerInitTemplate db '{"cargo":{"allFeatures":true},"procMacro":{"enable":true}}',0
RustAnalyzerInitTemplateLen equ $ - RustAnalyzerInitTemplate - 1
TypeScriptInitTemplate  db '{"preferences":{"includePackageJsonAutoImports":"on"}}',0
TypeScriptInitTemplateLen equ $ - TypeScriptInitTemplate - 1
GolangInitTemplate      db '{"ui":{"semanticTokens":true}}',0
GolangInitTemplateLen   equ $ - GolangInitTemplate - 1
ZigInitTemplate         db '{"enable_build_on_save":true}',0
ZigInitTemplateLen      equ $ - ZigInitTemplate - 1

; Server profiles
align 16
ServerProfiles label SERVER_PROFILE
    SERVER_PROFILE \
        <SERVER_CLANGD, 0, offset ClangdStrings, 2, 0, \
         offset ClangdInitTemplate, ClangdInitTemplateLen, \
         0FFFh, 0, 0>
    SERVER_PROFILE \
        <SERVER_PYRIGHT, 0, offset PyrightStrings, 2, 0, \
         offset PyrightInitTemplate, PyrightInitTemplateLen, \
         7FFh, 1h, 0>
    SERVER_PROFILE \
        <SERVER_RUST_ANALYZER, 0, offset RustAnalyzerStrings, 1, 0, \
         offset RustAnalyzerInitTemplate, RustAnalyzerInitTemplateLen, \
         1FFFh, 0, 0>
    SERVER_PROFILE \
        <SERVER_TYPESCRIPT, 0, offset TypeScriptStrings, 1, 0, \
         offset TypeScriptInitTemplate, TypeScriptInitTemplateLen, \
         0FFFh, 0, 0>
    SERVER_PROFILE \
        <SERVER_GOLANG, 0, offset GolangStrings, 1, 0, \
         offset GolangInitTemplate, GolangInitTemplateLen, \
         0FFFh, 0, 0>
    SERVER_PROFILE \
        <SERVER_ZIG, 0, offset ZigStrings, 1, 0, \
         offset ZigInitTemplate, ZigInitTemplateLen, \
         7FFh, 0, 0>

NUM_SERVER_PROFILES     equ 6

; Error messages
szErrTimeout            db "Handshake timeout",0
szErrInvalidResponse    db "Invalid initialize response",0
szErrStateMachine       db "Invalid handshake state",0
szErrTransport          db "Transport write failed",0

; Initialize request template
szInitializeTemplate    db '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":',0
szInitializeParams2     db ',"clientInfo":{"name":"RawrXD","version":"1.0"},"capabilities":{},"initializationOptions":',0
szInitializeParams3     db ',"rootUri":null}}',0

; Initialized notification
szInitializedNotification db '{"jsonrpc":"2.0","method":"initialized","params":{}}',0
szInitializedNotificationLen equ $ - szInitializedNotification - 1

.code
align 16

; Create handshake context
LSP_Handshake_Create PROC FRAME
    sub rsp, 28h
    .endprolog
    
    call GetProcessHeap
    test rax, rax
    jz alloc_failed
    
    mov rcx, rax
    xor edx, edx
    mov r8d, sizeof(LSP_HANDSHAKE_CTX)
    call HeapAlloc
    test rax, rax
    jz alloc_failed
    
    ; Zero-initialize
    mov rcx, rax
    xor edx, edx
    mov r8d, sizeof(LSP_HANDSHAKE_CTX)
    call MemZero
    
    ; Set defaults
    mov rcx, rax
    mov [rcx].LSP_HANDSHAKE_CTX.State, HS_STATE_IDLE
    mov [rcx].LSP_HANDSHAKE_CTX.ServerType, SERVER_UNKNOWN
    mov [rcx].LSP_HANDSHAKE_CTX.TimeoutMs, HS_TIMEOUT_INIT
    
    add rsp, 28h
    ret
    
alloc_failed:
    xor rax, rax
    add rsp, 28h
    ret
LSP_Handshake_Create ENDP

; Destroy handshake context
LSP_Handshake_Destroy PROC FRAME
    sub rsp, 28h
    .endprolog
    
    test rcx, rcx
    jz destroy_done
    
    mov rdx, rcx
    call GetProcessHeap
    mov rcx, rax
    xor r8d, r8d
    call HeapFree
    
destroy_done:
    add rsp, 28h
    ret
LSP_Handshake_Destroy ENDP

; Detect server type from command string
LSP_Handshake_DetectServer PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 20h
    .endprolog
    
    mov r12, rcx                    ; pCtx
    mov r13, rdx                    ; serverCommand
    
    ; Extract filename from path
    mov rcx, r13
    call ExtractFileName
    mov r13, rax
    
    ; Try each profile
    xor ebx, ebx
    
detect_loop:
    cmp ebx, NUM_SERVER_PROFILES
    jge unknown_server
    
    ; Get profile
    mov eax, sizeof(SERVER_PROFILE)
    imul ebx
    lea rsi, [ServerProfiles + rax]
    
    ; Check detection strings
    mov rdi, [rsi].SERVER_PROFILE.DetectionStrings
    xor ecx, ecx
    
string_loop:
    mov rdx, [rdi + rcx*8]
    test rdx, rdx
    jz next_profile
    
    ; Case-insensitive compare
    push rcx
    mov rcx, r13
    call StrStrIA
    pop rcx
    test rax, rax
    jnz server_matched
    
    inc ecx
    cmp ecx, [rsi].SERVER_PROFILE.StringCount
    jl string_loop
    
next_profile:
    inc ebx
    jmp detect_loop
    
server_matched:
    ; Store server type
    mov eax, [rsi].SERVER_PROFILE.ServerType
    mov [r12].LSP_HANDSHAKE_CTX.ServerType, eax
    
    ; Copy init options template
    mov rdi, [rsi].SERVER_PROFILE.InitOptionsTemplate
    mov ecx, [rsi].SERVER_PROFILE.TemplateLength
    cmp ecx, MAX_INIT_OPTIONS
    jbe copy_opts
    mov ecx, MAX_INIT_OPTIONS
    
copy_opts:
    mov [r12].LSP_HANDSHAKE_CTX.OptionsLength, ecx
    lea rdx, [r12].LSP_HANDSHAKE_CTX.InitializationOptions
    call MemCopy
    
    mov rax, 1
    jmp detect_done
    
unknown_server:
    mov [r12].LSP_HANDSHAKE_CTX.ServerType, SERVER_UNKNOWN
    xor rax, rax
    
detect_done:
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_Handshake_DetectServer ENDP

; Main handshake sequence
LSP_Handshake_Sequence PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 2100h
    .endprolog
    
    mov rbx, rcx
    
    ; Validate state
    cmp [rbx].LSP_HANDSHAKE_CTX.State, HS_STATE_IDLE
    jne state_error
    
    ; Record start time
    lea rcx, [rsp+20h]
    call QueryPerformanceCounter
    mov rax, [rsp+20h]
    mov [rbx].LSP_HANDSHAKE_CTX.StartTime, rax
    
    ; Build initialize request
    lea rdi, [rsp+30h]
    
    ; Start with template
    mov rcx, rdi
    lea rdx, [szInitializeTemplate]
    call StrCopy
    add rdi, rax
    
    ; Add process ID
    call GetCurrentProcessId
    mov rcx, rax
    mov rdx, rdi
    call IntToStr
    add rdi, rax
    
    ; Continue template
    mov rcx, rdi
    lea rdx, [szInitializeParams2]
    call StrCopy
    add rdi, rax
    
    ; Add init options
    mov rcx, rdi
    lea rdx, [rbx].LSP_HANDSHAKE_CTX.InitializationOptions
    mov r8d, [rbx].LSP_HANDSHAKE_CTX.OptionsLength
    call MemCopy
    add rdi, r8
    
    ; Close request
    mov rcx, rdi
    lea rdx, [szInitializeParams3]
    call StrCopy
    add rdi, rax
    
    ; Send initialize
    mov [rbx].LSP_HANDSHAKE_CTX.State, HS_STATE_INIT_SENT
    
    mov rcx, rbx
    lea rdx, [rsp+30h]
    call LSP_Transport_Write
    test eax, eax
    jz transport_error
    
    ; For now, skip response parsing and mark ready
    ; (Full response handling would go here)
    mov [rbx].LSP_HANDSHAKE_CTX.State, HS_STATE_INITIALIZED
    
    ; Send initialized notification
    mov rcx, rbx
    lea rdx, [szInitializedNotification]
    mov r8d, szInitializedNotificationLen
    call LSP_Transport_Write
    
    mov [rbx].LSP_HANDSHAKE_CTX.State, HS_STATE_READY
    
    mov rax, 1
    jmp handshake_done
    
state_error:
    mov [rbx].LSP_HANDSHAKE_CTX.LastErrorCode, HS_STATE_ERROR
    lea rcx, [szErrStateMachine]
    jmp error_exit
    
transport_error:
    mov [rbx].LSP_HANDSHAKE_CTX.LastErrorCode, HS_STATE_ERROR
    lea rcx, [szErrTransport]
    
error_exit:
    mov [rbx].LSP_HANDSHAKE_CTX.State, HS_STATE_ERROR
    lea rdx, [rbx].LSP_HANDSHAKE_CTX.LastErrorMessage
    mov r8d, 256
    call StrCopyN
    xor rax, rax
    
handshake_done:
    add rsp, 2100h
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_Handshake_Sequence ENDP

; Check if handshake timed out
LSP_Handshake_CheckTimeout PROC FRAME
    sub rsp, 38h
    .endprolog
    
    mov rbx, rcx
    
    lea rcx, [rsp+20h]
    call QueryPerformanceCounter
    mov rax, [rsp+20h]
    
    lea rcx, [rsp+28h]
    call QueryPerformanceFrequency
    mov rdx, [rsp+28h]
    
    ; Calculate elapsed ms
    sub rax, [rbx].LSP_HANDSHAKE_CTX.StartTime
    xor rdx, rdx
    div qword ptr [rsp+28h]
    imul rax, 1000
    
    cmp eax, [rbx].LSP_HANDSHAKE_CTX.TimeoutMs
    seta al
    
    add rsp, 38h
    ret
LSP_Handshake_CheckTimeout ENDP

; Helper: Zero memory
MemZero PROC
    push rdi
    mov rdi, rcx
    mov al, dl
    mov rcx, r8
    rep stosb
    pop rdi
    ret
MemZero ENDP

; Helper: Copy memory (returns dest+len handled by caller, but returns void normally. MemCopy usually returns void or dest)
MemCopy PROC
    push rsi
    push rdi
    mov rdi, rcx    ; Dest
    mov rsi, rdx    ; Src
    mov rcx, r8     ; Count (fixed register usage)
    rep movsb
    pop rdi
    pop rsi
    ret
MemCopy ENDP

; Helper: Copy string (returns length)
; RCX = Dest, RDX = Src
StrCopy PROC
    push rdi
    push rsi
    mov rdi, rcx
    mov rsi, rdx
    xor rax, rax
    
    ; Check null
    test rsi, rsi
    jz copy_done
    
copy_char:
    lodsb
    stosb
    test al, al
    jnz copy_char
    
    sub rdi, rcx
    dec rdi         ; exclude null from length
    mov rax, rdi
    
copy_done:
    pop rsi
    pop rdi
    ret
StrCopy ENDP

; Helper: Copy string with max length
StrCopyN PROC
    push rdi
    push rsi
    mov rdi, rcx
    mov rsi, rdx
    mov r9, r8
    xor rax, rax
copyn_char:
    test r9, r9
    jz copyn_done
    lodsb
    stosb
    dec r9
    test al, al
    jnz copyn_char
copyn_done:
    mov byte ptr [rdi-1], 0
    pop rsi
    pop rdi
    ret
StrCopyN ENDP

; Helper: Case-insensitive substring search
StrStrIA PROC
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    
search_loop:
    movzx eax, byte ptr [rsi]
    test al, al
    jz not_found
    
    movzx ebx, byte ptr [rdi]
    
    ; Convert to lowercase
    cmp al, 'A'
    jb compare_char
    cmp al, 'Z'
    ja compare_char
    add al, 20h
compare_char:
    cmp bl, 'A'
    jb do_compare
    cmp bl, 'Z'
    ja do_compare
    add bl, 20h
do_compare:
    cmp al, bl
    jne search_next
    
    ; Match first char, check rest
    push rsi
    push rdi
    inc rsi
    inc rdi
match_loop:
    movzx eax, byte ptr [rdi]
    test al, al
    jz found_match
    
    movzx ebx, byte ptr [rsi]
    test bl, bl
    jz pop_not_found
    
    ; Lowercase
    cmp al, 'A'
    jb ml_compare
    cmp al, 'Z'
    ja ml_compare
    add al, 20h
ml_compare:
    cmp bl, 'A'
    jb ml_do_compare
    cmp bl, 'Z'
    ja ml_do_compare
    add bl, 20h
ml_do_compare:
    cmp al, bl
    jne pop_not_found
    inc rsi
    inc rdi
    jmp match_loop
    
found_match:
    pop rdi
    pop rsi
    mov rax, rsi
    jmp search_done
    
pop_not_found:
    pop rdi
    pop rsi
    
search_next:
    inc rsi
    jmp search_loop
    
not_found:
    xor rax, rax
    
search_done:
    pop rdi
    pop rsi
    pop rbx
    ret
StrStrIA ENDP

; Helper: Extract filename from path
ExtractFileName PROC FRAME
    push rsi
    .endprolog
    
    mov rax, rcx    ; Default result = input
    mov rsi, rcx
    
    ; Scan for last slash
scan_loop:
    mov dl, [rsi]
    test dl, dl
    jz scan_done
    
    cmp dl, '\'
    je found_slash
    cmp dl, '/'
    je found_slash
    
    inc rsi
    jmp scan_loop
    
found_slash:
    lea rax, [rsi + 1] ; Result = char after slash
    inc rsi
    jmp scan_loop
    
scan_done:
    pop rsi
    ret
ExtractFileName ENDP

; Helper: Integer to String
; RCX = Integer, RDX = Buffer
IntToStr PROC FRAME
    push rbx
    push rsi
    push rdi
    .endprolog
    
    mov rdi, rdx    ; Buffer
    mov rax, rcx
    mov rbx, 10
    xor rcx, rcx    ; Digit count
    
    test rax, rax
    jnz convert_loop
    
    ; Zero case
    mov byte ptr [rdi], '0'
    mov byte ptr [rdi+1], 0
    mov rax, 1
    jmp inttostr_exit
    
convert_loop:
    xor rdx, rdx
    div rbx
    push rdx        ; Push remainder/digit
    inc rcx
    test rax, rax
    jnz convert_loop
    
    mov rax, rcx    ; Return length
    
store_loop:
    pop rdx
    add dl, '0'
    mov [rdi], dl
    inc rdi
    dec rcx
    jnz store_loop
    
    mov byte ptr [rdi], 0   ; Null terminate
    
inttostr_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
IntToStr ENDP

; Exports
PUBLIC LSP_Handshake_Create
PUBLIC LSP_Handshake_Destroy
PUBLIC LSP_Handshake_DetectServer
PUBLIC LSP_Handshake_Sequence
PUBLIC LSP_Handshake_CheckTimeout

END
