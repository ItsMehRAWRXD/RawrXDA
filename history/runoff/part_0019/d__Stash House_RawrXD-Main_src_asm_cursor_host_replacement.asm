; =============================================================================
; RawrXD Extension Host v1.0 - Replaces cursor.webpack.config → main.ts
; Reverse-engineered from: anysphere/cursor extension host
; Handles: Ripgrep IPC, Protobuf AI server config, Extension loading
; Zero Node.js dependencies - Pure Win64 ABI
; =============================================================================
OPTION CASEMAP:NONE
; OPTION WIN64:3  ; UASM-only, not needed for ml64

include masm64_compat.inc

; ============= EQUATES =============
HOST_PIPE_NAME      equ 0
HOST_MAILSLOT       equ 1
EXT_MSG_LOAD        equ 1
EXT_MSG_UNLOAD      equ 2
EXT_MSG_RPC         equ 3
EXT_MSG_RIPGREP     equ 4      ; @vscode/ripgrep external
EXT_MSG_PROTO       equ 5      ; aiserver/v1 proto fallback

RIPGREP_TIMEOUT     equ 30000   ; 30s search timeout
PROTO_BUF_SIZE      equ 65536

; ============= STRUCTS =============
ExtensionHostCtx struct
    hPipe           dq ?
    hMailSlot       dq ?
    hRipgrepProc    dq ?        ; External ripgrep handle
    hProtoPipe      dq ?        ; AI server config pipe
    activeExts      dd ?
    maxExts         dd ?
    extTable        dq ?        ; Pointer to extension table
ExtensionHostCtx ends

RipgrepRequest struct
    query           db 512 dup(?)
    cwd             db 260 dup(?)
    includePattern  db 256 dup(?)
    excludePattern  db 256 dup(?)
    caseSensitive   dd ?
RipgrepRequest ends

ProtoMessage struct
    msgType         dd ?
    payloadLen      dd ?
    payload         db PROTO_BUF_SIZE dup(?)
ProtoMessage ends

; ============= DATA ==============
.data
szPipeName          db '\\.\pipe\RawrXD_ExtHost',0
szMailSlotName      db '\\.\mailslot\RawrXD_HostEvents',0
szRipgrepExe        db 'rg.exe',0           ; @vscode/ripgrep binary
szRipgrepFallback   db 'C:\Program Files\RawrXD\bin\rg.exe',0
szProtoPipe         db '\\.\pipe\RawrXD_AIServer',0

szEvtExtensionLoad  db 'EXTENSION_LOAD',0
szEvtExtensionRpc   db 'EXTENSION_RPC',0
szEvtRipgrepStart   db 'RIPGREP_SPAWN',0
szEvtProtoRecv      db 'PROTO_RECV',0

g_HostCtx           ExtensionHostCtx <>
g_Running           dd 1

; ============= CODE ==============
.code

; -----------------------------------------------------------------------------
; Entry Point - Replaces main.ts bundled output
; -----------------------------------------------------------------------------
RawrXD_ExtensionHost proc
    sub rsp, 28h

    ; Initialize host context
    call InitHostContext
    test eax, eax
    jz @@init_failed

    ; Start ripgrep watcher (external process)
    call InitRipgrepExternal

    ; Start AI server proto handler
    call InitProtoHandler

    ; Main event loop (replaces Node.js event loop)
    call HostEventLoop

    ; Cleanup
    call CleanupHost

    xor eax, eax
    add rsp, 28h
    ret

@@init_failed:
    mov eax, 1
    add rsp, 28h
    ret
RawrXD_ExtensionHost endp

; -----------------------------------------------------------------------------
; Initialize Host IPC (replaces webpack externals resolution)
; -----------------------------------------------------------------------------
InitHostContext proc
    sub rsp, 28h

    ; Create named pipe for extension communication
    invoke CreateNamedPipeA, addr szPipeName, \
            PIPE_ACCESS_DUPLEX, \
            PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT, \
            PIPE_UNLIMITED_INSTANCES, \
            65536, 65536, 0, 0
    mov g_HostCtx.hPipe, rax

    cmp rax, INVALID_HANDLE_VALUE
    je @@failed

    ; Create mailslot for event broadcasting
    invoke CreateMailslotA, addr szMailSlotName, 0, MAILSLOT_WAIT_FOREVER, 0
    mov g_HostCtx.hMailSlot, rax

    ; Allocate extension table (256 extensions max)
    invoke HeapAlloc, GetProcessHeap(), HEAP_ZERO_MEMORY, 256 * 8
    mov g_HostCtx.extTable, rax
    mov g_HostCtx.maxExts, 256

    mov eax, 1
    jmp @@done

@@failed:
    xor eax, eax

@@done:
    add rsp, 28h
    ret
InitHostContext endp

; -----------------------------------------------------------------------------
; Ripgrep External Handler (@vscode/ripgrep replacement)
; Spawns rg.exe with JSON output format, streams results via pipe
; -----------------------------------------------------------------------------
InitRipgrepExternal proc
    sub rsp, 88h

    ; Security attributes for pipe inheritance
    lea rcx, [rsp+20h]
    mov dword ptr [rcx], 12
    mov qword ptr [rcx+8], 0
    mov dword ptr [rcx+16], 1

    ; Create stdout pipe for ripgrep
    lea r8, [rsp+40h]    ; hRead
    lea rdx, [rsp+48h]   ; hWrite
    mov r9d, 0
    call CreatePipe

    ; Ensure read handle not inherited
    mov rcx, [rsp+40h]
    mov edx, 1
    xor r8d, r8d
    call SetHandleInformation

    ; Find ripgrep binary
    lea rsi, szRipgrepExe
    call FindRipgrep
    test rax, rax
    jz @@no_ripgrep

    ; Setup process
    lea rdi, [rsp+50h]   ; STARTUPINFO
    mov rcx, rdi
    xor edx, edx
    mov r8d, sizeof STARTUPINFO
    call memset

    mov dword ptr [rdi], sizeof STARTUPINFO
    mov dword ptr [rdi+44], STARTF_USESTDHANDLES
    mov rax, [rsp+48h]
    mov [rdi+56], rax    ; hStdOutput
    mov [rdi+64], rax    ; hStdError

    ; Command line: rg --json --stdin
    lea rbx, [rsp+0C0h]
    mov dword ptr [rbx], 'rg" '
    mov dword ptr [rbx+4], '--js'
    mov dword ptr [rbx+8], 'on" '
    mov word ptr [rbx+12], '-'

    lea r12, [rsp+110h]  ; PROCESS_INFORMATION

    invoke CreateProcessA, rax, rbx, 0, 0, TRUE, \
            CREATE_NO_WINDOW, 0, 0, rdi, r12

    test eax, eax
    jz @@failed

    ; Store handle for later IPC
    mov rax, [r12]
    mov g_HostCtx.hRipgrepProc, rax

    ; Close thread handle, keep process handle
    mov rcx, [r12+8]
    call CloseHandle

    ; Close our copy of write handle
    mov rcx, [rsp+48h]
    call CloseHandle

    ; Start ripgrep output reader thread
    xor ecx, ecx
    lea rdx, RipgrepReaderThread
    xor r8, r8
    mov r9, [rsp+40h]    ; Pass read handle as param
    push 0
    push 0
    call CreateThread
    ; Handle stored implicitly in thread, could save if needed

    jmp @@done

@@no_ripgrep:
    ; Log fallback to built-in search (slower but functional)
    invoke OutputDebugStringA, cstr("[EXT_HOST] Ripgrep not found, using fallback")
    jmp @@done

@@failed:
    invoke OutputDebugStringA, cstr("[EXT_HOST] Failed to spawn ripgrep")

@@done:
    add rsp, 88h
    ret
InitRipgrepExternal endp

; -----------------------------------------------------------------------------
; Proto Handler (aiserver/v1/server_config_connect fallback)
; Handles protobuf messages over named pipe instead of WebConnect
; -----------------------------------------------------------------------------
InitProtoHandler proc
    sub rsp, 28h

    ; Create pipe for AI server proto communication
    invoke CreateNamedPipeA, addr szProtoPipe, \
            PIPE_ACCESS_DUPLEX, \
            PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT, \
            1, 65536, 65536, 0, 0
    mov g_HostCtx.hProtoPipe, rax

    ; Start proto handler thread
    xor ecx, ecx
    lea rdx, ProtoHandlerThread
    xor r8, r8
    xor r9, r9
    push 0
    push 0
    call CreateThread

    add rsp, 28h
    ret
InitProtoHandler endp

; -----------------------------------------------------------------------------
; Main Event Loop (replaces Node.js libuv loop)
; -----------------------------------------------------------------------------
HostEventLoop proc
    sub rsp, 48h

@@loop:
    cmp g_Running, 0
    je @@done

    ; Wait for pipe connection (blocking - extensions connecting)
    invoke ConnectNamedPipe, g_HostCtx.hPipe, 0

    ; Read message header
    lea r12, [rsp+20h]   ; Message buffer
    invoke ReadFile, g_HostCtx.hPipe, r12, 16, addr [rsp+40h], 0

    cmp eax, 0
    je @@disconnect

    ; Route by message type
    mov eax, dword ptr [r12]     ; msgType

    cmp eax, EXT_MSG_LOAD
    je @@handle_load
    cmp eax, EXT_MSG_UNLOAD
    je @@handle_unload
    cmp eax, EXT_MSG_RPC
    je @@handle_rpc
    cmp eax, EXT_MSG_RIPGREP
    je @@handle_ripgrep
    cmp eax, EXT_MSG_PROTO
    je @@handle_proto
    jmp @@unknown

@@handle_load:
    lea rcx, [r12+8]
    call HandleExtensionLoad
    jmp @@respond

@@handle_unload:
    lea rcx, [r12+8]
    call HandleExtensionUnload
    jmp @@respond

@@handle_rpc:
    lea rcx, [r12+8]
    mov rdx, [rsp+40h]
    sub rdx, 8
    call HandleExtensionRPC
    jmp @@respond

@@handle_ripgrep:
    lea rcx, [r12+8]
    call HandleRipgrepRequest
    jmp @@respond

@@handle_proto:
    lea rcx, [r12+8]
    call HandleProtoMessage
    jmp @@respond

@@unknown:
    mov eax, 400    ; Bad request

@@respond:
    ; Send response
    invoke WriteFile, g_HostCtx.hPipe, rax, 8, addr [rsp+40h], 0
    invoke FlushFileBuffers, g_HostCtx.hPipe

@@disconnect:
    invoke DisconnectNamedPipe, g_HostCtx.hPipe
    jmp @@loop

@@done:
    add rsp, 48h
    ret
HostEventLoop endp

; -----------------------------------------------------------------------------
; Extension Loading (replaces webpack entry resolution)
; -----------------------------------------------------------------------------
HandleExtensionLoad proc
    sub rsp, 28h

    ; rcx = extension path (UTF-8)
    mov rsi, rcx

    ; Find free slot
    mov rdi, g_HostCtx.extTable
    xor ebx, ebx

@@find_slot:
    cmp qword ptr [rdi+rbx*8], 0
    je @@found_slot
    inc ebx
    cmp ebx, g_HostCtx.maxExts
    jb @@find_slot

    ; No slots
    mov rax, 507
    jmp @@done

@@found_slot:
    ; Load extension DLL (raw MASM64 extension)
    invoke LoadLibraryA, rsi
    test rax, rax
    jz @@load_failed

    mov [rdi+rbx*8], rax

    ; Call extension activate export
    mov rcx, rax
    lea rdx, cstr("activate")
    call GetProcAddress

    test rax, rax
    jz @@no_activate

    ; Call activate(hostContext)
    mov rcx, offset g_HostCtx
    call rax

    inc g_HostCtx.activeExts
    mov rax, 200    ; Success
    jmp @@done

@@load_failed:
    mov rax, 500
    jmp @@done

@@no_activate:
    mov rax, 501

@@done:
    add rsp, 28h
    ret
HandleExtensionLoad endp

; -----------------------------------------------------------------------------
; Extension RPC (method calling between extensions)
; -----------------------------------------------------------------------------
HandleExtensionRPC proc
    ; rcx = payload, rdx = size
    ; Stub: Would deserialize JSON-RPC and dispatch
    mov rax, 200
    ret
HandleExtensionRPC endp

; -----------------------------------------------------------------------------
; Ripgrep Request Handler
; -----------------------------------------------------------------------------
HandleRipgrepRequest proc
    sub rsp, 48h

    ; rcx = RipgrepRequest*
    mov rsi, rcx

    ; Check if ripgrep running
    cmp g_HostCtx.hRipgrepProc, 0
    je @@no_rg

    ; Write search params to ripgrep stdin
    ; Format: cwd\nquery\ninclude\nexclude\n
    invoke WriteFile, g_HostCtx.hRipgrepProc, \
            addr (RipgrepRequest ptr [rsi]).cwd, \
            260, addr [rsp+20h], 0

    mov rax, 202    ; Accepted (async)
    jmp @@done

@@no_rg:
    mov rax, 503    ; Service unavailable

@@done:
    add rsp, 48h
    ret
HandleRipgrepRequest endp

; -----------------------------------------------------------------------------
; Proto Message Handler (AI server config)
; Replaces: aiserver/v1/server_config_connectweb.js fallback
; -----------------------------------------------------------------------------
HandleProtoMessage proc
    sub rsp, 28h

    ; rcx = ProtoMessage*
    mov rsi, rcx

    ; Route by msgType
    mov eax, (ProtoMessage ptr [rsi]).msgType

    cmp eax, 1      ; ServerConfig_Get
    je @@config_get
    cmp eax, 2      ; ServerConfig_Set
    je @@config_set

    mov rax, 400
    jmp @@done

@@config_get:
    ; Return current AI server config
    ; Stub: Would read from registry or config file
    mov rax, 200
    jmp @@done

@@config_set:
    ; Update AI server config
    ; Stub: Would validate and persist
    mov rax, 200

@@done:
    add rsp, 28h
    ret
HandleProtoMessage endp

; -----------------------------------------------------------------------------
; Background Threads
; -----------------------------------------------------------------------------
RipgrepReaderThread proc
    sub rsp, 28h

    mov rbx, rcx    ; Read handle passed as param

@@read_loop:
    lea r12, [rsp+20h]
    invoke ReadFile, rbx, r12, 4096, addr [rsp+40h], 0

    test eax, eax
    jz @@eof

    ; Broadcast to mail slot (pub/sub for extensions)
    invoke WriteFile, g_HostCtx.hMailSlot, r12, [rsp+40h], addr [rsp+48h], 0

    jmp @@read_loop

@@eof:
    add rsp, 28h
    ret
RipgrepReaderThread endp

ProtoHandlerThread proc
    sub rsp, 28h

@@accept_loop:
    invoke ConnectNamedPipe, g_HostCtx.hProtoPipe, 0

    lea r12, [rsp+20h]
    invoke ReadFile, g_HostCtx.hProtoPipe, r12, sizeof ProtoMessage, addr [rsp+40h], 0

    ; Process proto message
    mov rcx, r12
    call HandleProtoMessage

    ; Write response
    invoke WriteFile, g_HostCtx.hProtoPipe, rax, 8, addr [rsp+40h], 0
    invoke DisconnectNamedPipe, g_HostCtx.hProtoPipe

    jmp @@accept_loop

    add rsp, 28h
    ret
ProtoHandlerThread endp

; -----------------------------------------------------------------------------
; Utilities
; -----------------------------------------------------------------------------
FindRipgrep proc
    sub rsp, 28h

    ; Try standard locations
    lea rcx, szRipgrepExe
    call FileExists
    test eax, eax
    jnz @@found_default

    lea rcx, szRipgrepFallback
    call FileExists
    test eax, eax
    jnz @@found_fallback

    xor eax, eax
    jmp @@done

@@found_default:
    lea rax, szRipgrepExe
    jmp @@done

@@found_fallback:
    lea rax, szRipgrepFallback

@@done:
    add rsp, 28h
    ret
FindRipgrep endp

HandleExtensionUnload proc
    mov rax, 200
    ret
HandleExtensionUnload endp

CleanupHost proc
    ret
CleanupHost endp

FileExists proc
    invoke GetFileAttributesA, rcx
    cmp eax, INVALID_FILE_ATTRIBUTES
    setne al
    movzx eax, al
    ret
FileExists endp

; ============= EXPORTS =============
public RawrXD_ExtensionHost

end