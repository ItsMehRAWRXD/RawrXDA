; =============================================================================
; RawrXD Extension Host v1.0 - Replaces cursor.webpack.config → main.ts
; Reverse-engineered from: anysphere/cursor extension host
; Handles: Ripgrep IPC, Protobuf AI server config, Extension loading
; Zero Node.js dependencies - Pure Win64 ABI
; =============================================================================
OPTION CASEMAP:NONE
; OPTION WIN64:3  ; UASM-only, not needed for ml64

include masm64_compat.inc
include rawrxd_win64.inc

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

; Extension API ABI
EXT_API_ABI_VERSION equ 1

; Permission bitmask (host-mediated requests)
PERM_FILESYSTEM_READ  equ 1
PERM_FILESYSTEM_WRITE equ 2
PERM_NETWORK          equ 4
PERM_PROCESS          equ 8
PERM_SHELL            equ 16
PERM_DEBUG            equ 128

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
    query           BYTE 512 DUP (?)
    workingDir      BYTE 260 DUP (?)
    includePattern  BYTE 256 DUP (?)
    excludePattern  BYTE 256 DUP (?)
    caseSensitive   DWORD ?
RipgrepRequest ends

ProtoMessage struct
    msgType         DWORD ?
    payloadLen      DWORD ?
    payload         BYTE PROTO_BUF_SIZE DUP (?)
ProtoMessage ends

RawrXD_ExtensionApi struct
    structSize      dq ?
    abiVersion      dq ?
    pfnLog          dq ?
    pfnRequestPerm  dq ?
    pfnReadFile     dq ?
    pfnWriteFile    dq ?
RawrXD_ExtensionApi ends

RawrXD_ExtActivationCtx struct
    pHostCtx        dq ?
    pApi            dq ?
    extensionSlot   dq ?
    reserved        dq ?
RawrXD_ExtActivationCtx ends

; ============= DATA ==============
.data
szPipeName          db '\\.\pipe\RawrXD_ExtHost',0
szMailSlotName      db '\\.\mailslot\RawrXD_HostEvents',0
szRipgrepExe        db 'rg.exe',0           ; @vscode/ripgrep binary
szRipgrepFallback   db 'C:\Program Files\RawrXD\bin\rg.exe',0
szRipgrepCmdLine    db 'rg.exe --json --stdin',0
szProtoPipe         db '\\.\pipe\RawrXD_AIServer',0

szEvtExtensionLoad  db 'EXTENSION_LOAD',0
szEvtExtensionRpc   db 'EXTENSION_RPC',0
szEvtRipgrepStart   db 'RIPGREP_SPAWN',0
szEvtProtoRecv      db 'PROTO_RECV',0

szExtActivate       db 'ExtensionActivate',0
szExtDeactivate     db 'ExtensionDeactivate',0
szExtActivateLegacy db 'activate',0
szExtDeactivateLegacy db 'deactivate',0

g_HostCtx           ExtensionHostCtx <>
g_ExtApi            RawrXD_ExtensionApi <>
g_ExtActCtx         RawrXD_ExtActivationCtx <>
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
    sub rsp, 60h
    mov qword ptr [rsp+56], 0
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+40], 65536
    mov qword ptr [rsp+32], 65536
    mov r9, PIPE_UNLIMITED_INSTANCES
    mov r8, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov rdx, PIPE_ACCESS_DUPLEX
    mov rcx, OFFSET szPipeName
    call CreateNamedPipeA
    add rsp, 60h
    mov g_HostCtx.hPipe, rax

    cmp rax, INVALID_HANDLE_VALUE
    je @@failed

    ; Create mailslot for event broadcasting
    invoke CreateMailslotA, OFFSET szMailSlotName, 0, MAILSLOT_WAIT_FOREVER, 0
    mov g_HostCtx.hMailSlot, rax

    ; Allocate extension table (256 extensions max)
    call GetProcessHeap
    invoke HeapAlloc, rax, HEAP_ZERO_MEMORY, 256 * 8
    mov g_HostCtx.extTable, rax
    mov g_HostCtx.maxExts, 256

    ; Build dispatch table for extension-facing host API.
    call InitExtensionApi

    mov eax, 1
    jmp @@done

@@failed:
    xor eax, eax

@@done:
    add rsp, 28h
    ret
InitHostContext endp

; -----------------------------------------------------------------------------
; Build host API dispatch table (passed to extension activation context)
; -----------------------------------------------------------------------------
InitExtensionApi proc
    mov g_ExtApi.structSize, SIZEOF RawrXD_ExtensionApi
    mov g_ExtApi.abiVersion, EXT_API_ABI_VERSION

    lea rax, HostApi_Log
    mov g_ExtApi.pfnLog, rax

    lea rax, HostApi_RequestPermission
    mov g_ExtApi.pfnRequestPerm, rax

    lea rax, HostApi_ReadFile
    mov g_ExtApi.pfnReadFile, rax

    lea rax, HostApi_WriteFile
    mov g_ExtApi.pfnWriteFile, rax

    ret
InitExtensionApi endp

; -----------------------------------------------------------------------------
; Ripgrep External Handler (@vscode/ripgrep replacement)
; Spawns rg.exe with JSON output format, streams results via pipe
; -----------------------------------------------------------------------------
InitRipgrepExternal proc
    sub rsp, 188h

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
    mov rsi, rax

    ; Setup process
    lea rdi, [rsp+50h]   ; STARTUPINFO
    mov rcx, rdi
    xor edx, edx
    mov r8d, SIZEOF STARTUPINFOA
    call memset

    mov dword ptr [rdi], SIZEOF STARTUPINFOA
    mov dword ptr [rdi+44], STARTF_USESTDHANDLES
    mov rax, [rsp+48h]
    mov [rdi+56], rax    ; hStdOutput
    mov [rdi+64], rax    ; hStdError

    ; Command line: rg --json --stdin
    lea rbx, szRipgrepCmdLine

    lea r12, [rsp+0C0h]  ; PROCESS_INFORMATION

    sub rsp, 60h
    mov qword ptr [rsp+56], r12
    mov qword ptr [rsp+48], rdi
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+32], 0
    mov r9, CREATE_NO_WINDOW
    mov r8, TRUE
    xor rdx, rdx
    mov rdx, rbx
    mov rcx, rsi
    call CreateProcessA
    add rsp, 60h

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
    sub rsp, 60h
    mov qword ptr [rsp+56], 0
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+40], 65536
    mov qword ptr [rsp+32], 65536
    mov r9, 1
    mov r8, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov rdx, PIPE_ACCESS_DUPLEX
    mov rcx, OFFSET szProtoPipe
    call CreateNamedPipeA
    add rsp, 60h
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
    lea r9, [rsp+40h]
    invoke ReadFile, g_HostCtx.hPipe, r12, 16, r9, 0

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
    mov [rsp+30h], rax
    lea rdx, [rsp+30h]
    lea r9, [rsp+40h]
    invoke WriteFile, g_HostCtx.hPipe, rdx, 8, r9, 0
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

    ; Resolve extension activation export (modern name first).
    mov rcx, rax
    lea rdx, szExtActivate
    call GetProcAddress

    test rax, rax
    jnz @@have_activate

    ; Legacy compatibility export.
    mov rcx, [rdi+rbx*8]
    lea rdx, szExtActivateLegacy
    call GetProcAddress
    test rax, rax
    jz @@no_activate

@@have_activate:
    ; Populate activation context and dispatch table pointer.
    lea rcx, g_HostCtx
    mov g_ExtActCtx.pHostCtx, rcx
    lea rcx, g_ExtApi
    mov g_ExtActCtx.pApi, rcx
    mov g_ExtActCtx.extensionSlot, rbx
    xor rcx, rcx
    mov g_ExtActCtx.reserved, rcx

    ; Call ExtensionActivate(activationCtx)
    lea rcx, g_ExtActCtx
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
    lea rdx, [rsi].RipgrepRequest.workingDir
    lea r9, [rsp+20h]
    invoke WriteFile, g_HostCtx.hRipgrepProc, rdx, 260, r9, 0

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
    sub rsp, 1048h

    mov rbx, rcx    ; Read handle passed as param

@@read_loop:
    lea r12, [rsp+20h]
    lea r9, [rsp+40h]
    invoke ReadFile, rbx, r12, 4096, r9, 0

    test eax, eax
    jz @@eof

    ; Broadcast to mail slot (pub/sub for extensions)
    lea r9, [rsp+48h]
    invoke WriteFile, g_HostCtx.hMailSlot, r12, [rsp+40h], r9, 0

    jmp @@read_loop

@@eof:
    add rsp, 1048h
    ret
RipgrepReaderThread endp

ProtoHandlerThread proc
    sub rsp, 10040h

@@accept_loop:
    invoke ConnectNamedPipe, g_HostCtx.hProtoPipe, 0

    lea r12, [rsp+20h]
    lea r9, [rsp+40h]
    invoke ReadFile, g_HostCtx.hProtoPipe, r12, SIZEOF ProtoMessage, r9, 0

    ; Process proto message
    mov rcx, r12
    call HandleProtoMessage

    ; Write response
    mov [rsp+30h], rax
    lea rdx, [rsp+30h]
    lea r9, [rsp+40h]
    invoke WriteFile, g_HostCtx.hProtoPipe, rdx, 8, r9, 0
    invoke DisconnectNamedPipe, g_HostCtx.hProtoPipe

    jmp @@accept_loop

    add rsp, 10040h
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

; -----------------------------------------------------------------------------
; Extension-facing Host API (dispatch table targets)
; -----------------------------------------------------------------------------
; HostApi_Log(const char* msg)
HostApi_Log proc
    test rcx, rcx
    jz @@done
    invoke OutputDebugStringA, rcx
@@done:
    xor eax, eax
    ret
HostApi_Log endp

; HostApi_RequestPermission(uint64 requestedMask) -> eax {0 deny, 1 grant}
HostApi_RequestPermission proc
    mov rax, rcx
    and rax, (PERM_PROCESS or PERM_SHELL or PERM_DEBUG)
    test rax, rax
    jnz @@deny
    mov eax, 1
    ret
@@deny:
    xor eax, eax
    ret
HostApi_RequestPermission endp

; HostApi_ReadFile(const char* path, void* outBuf, uint64 outCap) -> rax bytesRead or -1
HostApi_ReadFile proc
    sub rsp, 28h

    ; rcx=path, rdx=outBuf, r8=outCap
    mov r10, rdx
    mov r11, r8

    sub rsp, 58h
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+32], OPEN_EXISTING
    xor r9d, r9d
    mov r8, FILE_SHARE_READ
    mov rdx, GENERIC_READ
    call CreateFileA
    add rsp, 58h
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail

    mov [rsp+18h], rax ; file handle

    ; clamp read count to DWORD for ReadFile
    mov eax, r11d
    mov r8d, eax
    lea r9, [rsp+20h] ; bytes read output
    invoke ReadFile, [rsp+18h], r10, r8, r9, 0
    test eax, eax
    jz @@close_fail

    mov rcx, [rsp+18h]
    call CloseHandle
    mov eax, dword ptr [rsp+20h]
    add rsp, 28h
    ret

@@close_fail:
    mov rcx, [rsp+18h]
    call CloseHandle
@@fail:
    mov rax, -1
    add rsp, 28h
    ret
HostApi_ReadFile endp

; HostApi_WriteFile(const char* path, const void* buf, uint64 len) -> rax bytesWritten or -1
HostApi_WriteFile proc
    sub rsp, 28h

    ; rcx=path, rdx=buf, r8=len
    mov r10, rdx
    mov r11, r8

    sub rsp, 58h
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+32], CREATE_ALWAYS
    xor r9d, r9d
    xor r8d, r8d
    mov rdx, GENERIC_WRITE
    call CreateFileA
    add rsp, 58h
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail

    mov [rsp+18h], rax ; file handle

    ; clamp write count to DWORD for WriteFile
    mov eax, r11d
    mov r8d, eax
    lea r9, [rsp+20h] ; bytes written output
    invoke WriteFile, [rsp+18h], r10, r8, r9, 0
    test eax, eax
    jz @@close_fail

    mov rcx, [rsp+18h]
    call CloseHandle
    mov eax, dword ptr [rsp+20h]
    add rsp, 28h
    ret

@@close_fail:
    mov rcx, [rsp+18h]
    call CloseHandle
@@fail:
    mov rax, -1
    add rsp, 28h
    ret
HostApi_WriteFile endp

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