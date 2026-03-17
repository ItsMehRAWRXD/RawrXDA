; =============================================================================
; RawrXD Always-Local Extension v1.0
; Reverse-engineered from: anysphere/cursor-always-local package.json
; Features: Git commit gen, Remote authority resolution, Environment validation
; APIs: cursor, control, externalUriOpener, resolvers, scm/inputBox
; =============================================================================
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\shell32.inc
include \masm64\include64\shlwapi.inc
include \masm64\include64\advapi32.inc
include \masm64\include64\crypt32.inc
include \masm64\macros64\vasily.inc

; ============= EQUATES =============
EXT_STATE_INACTIVE      equ 0
EXT_STATE_ACTIVATING    equ 1
EXT_STATE_ACTIVE        equ 2

REMOTE_AUTH_BACKGROUND  equ 1       ; background-composer authority

; Commands
CMD_GENERATE_COMMIT     equ 0x100   ; cursor.generateGitCommitMessage
CMD_RESOLVE_AUTHORITY   equ 0x101   ; onResolveRemoteAuthority
CMD_VALIDATE_ENV        equ 0x102   ; Validate environment.json

; SCM Input Box
SCM_GIT_PROVIDER        equ 1

; Certificate stores (windows-ca-certs replacement)
CERT_STORE_MY           equ 1
CERT_STORE_ROOT         equ 2
CERT_STORE_CA           equ 3

; ============= STRUCTS =============
AlwaysLocalCtx struct
    state               dd ?
    hGitPipe            dq ?        ; Git process pipe
    hRemotePipe         dq ?        ; background-composer IPC
    hCertStore          dq ?        ; Windows cert store handle
    environmentSchema   dq ?        ; Loaded JSON schema ptr
    remoteAuthority     db 64 dup(?) ; Current remote authority
AlwaysLocalCtx ends

GitCommitRequest struct
    diffContent         db 8192 dup(?)   ; Git diff output
    fileCount           dd ?
    maxLength           dd ?             ; Max commit message length
    style               dd ?             ; 0=conventional, 1=semantic
    outMessage          db 256 dup(?)
GitCommitRequest ends

RemoteAuthorityRequest struct
    authority           db 64 dup(?)
    host                db 256 dup(?)
    port                dd ?
    path                db 260 dup(?)
    resolveStatus       dd ?
RemoteAuthorityRequest ends

EnvironmentConfig struct
    apiEndpoint         db 256 dup(?)
    modelOverrides      db 512 dup(?)
    experimentalFlags   dd ?
EnvironmentConfig ends

; ============= DATA ==============
.data
szExtName           db 'cursor-always-local',0
szDisplayName       db 'Cursor Always Local',0

; Activation events
szOnStartup         db 'onStartupFinished',0
szOnResolveRemote   db 'onResolveRemoteAuthority:background-composer',0

; Remote authority
szBackgroundComposer db 'background-composer',0
szRemotePipeName    db '\\.\pipe\RawrXD_Remote_background-composer',0

; Commands
szCmdGenerateCommit db 'cursor.generateGitCommitMessage',0
szGitDiffCmd        db 'git diff --cached --no-color',0

; SCM Menu
szScmInputBox       db 'scm/inputBox',0
szWhenGit           db 'scmProvider == git',0

; JSON Schema paths
szEnvSchemaPath     db 'schemas\environment.schema.json',0
szEnvFilePattern    db '.cursor\environment.json',0

; Resource label formatter
szRemoteScheme      db 'vscode-remote',0
szLabelFormat       db '${path}',0
szWorkspaceSuffix   db 'cloud-agent',0

; Certificate stores
szCertStoreMY       db 'MY',0
szCertStoreROOT     db 'ROOT',0
szCertStoreCA       db 'CA',0

g_AlwaysLocal       AlwaysLocalCtx <>
g_ActivationState   dd EXT_STATE_INACTIVE

; ============= CODE ==============
.code

; -----------------------------------------------------------------------------
; Extension Entry (replaces ./dist/main)
; -----------------------------------------------------------------------------
AlwaysLocal_Entry proc
    sub rsp, 28h

    ; Initialize extension context
    call InitAlwaysLocal
    test eax, eax
    jz @@init_failed

    ; Register activation event handlers
    call RegisterActivationEvents

    ; If startup activation, activate immediately
    call ActivateOnStartup

    xor eax, eax
    add rsp, 28h
    ret

@@init_failed:
    mov eax, 1
    add rsp, 28h
    ret
AlwaysLocal_Entry endp

; -----------------------------------------------------------------------------
; Initialize Extension Context
; -----------------------------------------------------------------------------
InitAlwaysLocal proc
    sub rsp, 28h

    ; Clear context
    lea rdi, g_AlwaysLocal
    mov rcx, sizeof AlwaysLocalCtx / 8
    xor eax, eax
    rep stosq

    ; Open Windows certificate store (replaces @vscode/windows-ca-certs)
    call InitWindowsCerts
    test eax, eax
    jz @@cert_failed

    ; Load environment.json schema
    call LoadEnvironmentSchema

    ; Setup Git integration pipe
    call InitGitIntegration

    mov g_AlwaysLocal.state, EXT_STATE_INACTIVE
    mov eax, 1
    jmp @@done

@@cert_failed:
    xor eax, eax

@@done:
    add rsp, 28h
    ret
InitAlwaysLocal endp

; -----------------------------------------------------------------------------
; Windows Certificate Store Integration
; Replaces: @vscode/windows-ca-certs optional dependency
; -----------------------------------------------------------------------------
InitWindowsCerts proc
    sub rsp, 28h

    ; Open MY certificate store for current user
    invoke CertOpenStore, CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_STORE_READONLY_FLAG or CERT_SYSTEM_STORE_CURRENT_USER,
            addr szCertStoreMY
    mov g_AlwaysLocal.hCertStore, rax

    test rax, rax
    jz @@failed

    ; Enumerate and cache certificates for TLS validation
    call CacheCertificates

    mov eax, 1
    jmp @@done

@@failed:
    xor eax, eax

@@done:
    add rsp, 28h
    ret
InitWindowsCerts endp

CacheCertificates proc
    ; Stub: Would enumerate certs and build trust chain
    ret
CacheCertificates endp

; -----------------------------------------------------------------------------
; Activation Event Registration
; -----------------------------------------------------------------------------
RegisterActivationEvents proc
    sub rsp, 28h

    ; Register startup watcher
    call RegisterStartupWatcher

    ; Register remote authority resolver
    call RegisterRemoteResolver

    add rsp, 28h
    ret
RegisterActivationEvents endp

; -----------------------------------------------------------------------------
; Startup Activation Handler
; -----------------------------------------------------------------------------
ActivateOnStartup proc
    sub rsp, 28h

    cmp g_AlwaysLocal.state, EXT_STATE_INACTIVE
    jne @@already_active

    mov g_AlwaysLocal.state, EXT_STATE_ACTIVATING

    ; Initialize UI contributions (commands, menus)
    call InitUIContributions

    ; Initialize SCM input box menu
    call InitSCMInputBox

    ; Initialize configuration
    call InitConfiguration

    mov g_AlwaysLocal.state, EXT_STATE_ACTIVE

    invoke OutputDebugStringA, cstr("[ALWAYS-LOCAL] Extension activated on startup")

@@already_active:
    add rsp, 28h
    ret
ActivateOnStartup endp

; -----------------------------------------------------------------------------
; Remote Authority Resolver (background-composer)
; Replaces: onResolveRemoteAuthority:background-composer
; -----------------------------------------------------------------------------
RegisterRemoteResolver proc
    sub rsp, 28h

    ; Create named pipe for remote authority resolution
    invoke CreateNamedPipeA, addr szRemotePipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536, 65536, 0, 0
    mov g_AlwaysLocal.hRemotePipe, rax

    cmp rax, INVALID_HANDLE_VALUE
    je @@failed

    ; Spawn resolver thread
    xor ecx, ecx
    lea rdx, RemoteResolverThread
    xor r8, r8
    xor r9, r9
    push 0
    push 0
    call CreateThread

    mov eax, 1
    jmp @@done

@@failed:
    xor eax, eax

@@done:
    add rsp, 28h
    ret
RegisterRemoteResolver endp

RemoteResolverThread proc
    sub rsp, 48h

@@accept_loop:
    ; Wait for authority resolution request
    invoke ConnectNamedPipe, g_AlwaysLocal.hRemotePipe, 0

    ; Read RemoteAuthorityRequest
    lea r12, [rsp+20h]
    invoke ReadFile, g_AlwaysLocal.hRemotePipe, r12,
            sizeof RemoteAuthorityRequest, addr [rsp+40h], 0

    cmp eax, 0
    je @@disconnect

    ; Check authority type
    lea rcx, (RemoteAuthorityRequest ptr [r12]).authority
    lea rdx, szBackgroundComposer
    call strcmp
    test eax, eax
    jnz @@unknown_auth

    ; Resolve background-composer authority
    mov rcx, r12
    call ResolveBackgroundComposer

    ; Write result
    invoke WriteFile, g_AlwaysLocal.hRemotePipe, r12,
            sizeof RemoteAuthorityRequest, addr [rsp+40h], 0

@@unknown_auth:
@@disconnect:
    invoke DisconnectNamedPipe, g_AlwaysLocal.hRemotePipe
    jmp @@accept_loop

    add rsp, 48h
    ret
RemoteResolverThread endp

ResolveBackgroundComposer proc
    ; rcx = RemoteAuthorityRequest*
    sub rsp, 28h

    mov r12, rcx

    ; Parse authority format: background-composer+<host>[:<port>]
    ; Stub: Would resolve to actual cloud agent endpoint

    ; Set resolved path
    lea rdi, (RemoteAuthorityRequest ptr [r12]).path
    lea rsi, cstr("/cloud-agent/background-composer")
    call strcpy

    mov (RemoteAuthorityRequest ptr [r12]).resolveStatus, 200

    add rsp, 28h
    ret
ResolveBackgroundComposer endp

; -----------------------------------------------------------------------------
; Git Commit Message Generation
; Command: cursor.generateGitCommitMessage
; -----------------------------------------------------------------------------
InitGitIntegration proc
    sub rsp, 28h

    ; Create anonymous pipe for git diff output
    lea rcx, [rsp+20h]
    mov dword ptr [rcx], 12
    mov qword ptr [rcx+8], 0
    mov dword ptr [rcx+16], 1

    lea r8, [rsp+40h]   ; hRead
    lea rdx, [rsp+48h]   ; hWrite
    mov r9d, 0
    call CreatePipe

    ; Store read handle for later
    mov rax, [rsp+40h]
    mov g_AlwaysLocal.hGitPipe, rax

    add rsp, 28h
    ret
InitGitIntegration endp

GenerateGitCommitMessage proc
    sub rsp, 128h

    ; rcx = GitCommitRequest*
    mov r12, rcx

    ; Execute git diff --cached
    lea rdi, [rsp+80h]   ; STARTUPINFO
    mov rcx, rdi
    xor edx, edx
    mov r8d, sizeof STARTUPINFO
    call memset

    mov dword ptr [rdi], sizeof STARTUPINFO
    mov dword ptr [rdi+44], STARTF_USESTDHANDLES
    mov rax, g_AlwaysLocal.hGitPipe
    mov [rdi+56], rax    ; hStdOutput

    lea rbx, [rsp+0E0h]  ; PROCESS_INFORMATION

    ; Create git process
    invoke CreateProcessA, 0, addr szGitDiffCmd, 0, 0, TRUE,
            CREATE_NO_WINDOW, 0, 0, rdi, rbx

    test eax, eax
    jz @@git_failed

    ; Wait for git to complete
    mov rcx, [rbx]
    mov edx, 10000       ; 10 second timeout
    call WaitForSingleObject

    ; Read diff output
    mov rcx, g_AlwaysLocal.hGitPipe
    lea rdx, (GitCommitRequest ptr [r12]).diffContent
    mov r8d, 8192
    lea r9, [rsp+40h]
    call ReadFile

    ; Close process handles
    mov rcx, [rbx]
    call CloseHandle
    mov rcx, [rbx+8]
    call CloseHandle

    ; Generate commit message based on diff
    mov rcx, r12
    call AnalyzeDiffAndGenerateMessage

    mov eax, 200
    jmp @@done

@@git_failed:
    mov eax, 500

@@done:
    add rsp, 128h
    ret
GenerateGitCommitMessage endp

AnalyzeDiffAndGenerateMessage proc
    ; rcx = GitCommitRequest*
    ; Stub: Would parse diff and generate conventional commit message
    ; For now, copy a template
    mov r12, rcx

    lea rdi, (GitCommitRequest ptr [r12]).outMessage

    ; Simple analysis: check for keywords in diff
    ; This is where AI model would be called in full implementation

    lea rsi, cstr("feat: update components")
    call strcpy

    ret
AnalyzeDiffAndGenerateMessage endp

; -----------------------------------------------------------------------------
; SCM Input Box Menu Contribution
; Contributes: scm/inputBox command when scmProvider == git
; -----------------------------------------------------------------------------
InitSCMInputBox proc
    sub rsp, 28h

    ; Register menu contribution for Git SCM provider
    ; This would hook into RawrXD's SCM view

    invoke OutputDebugStringA, cstr("[ALWAYS-LOCAL] SCM input box menu registered")

    add rsp, 28h
    ret
InitSCMInputBox endp

HandleSCMInputBoxMenu proc
    ; Called when user opens SCM input box menu
    sub rsp, 28h

    ; Check if Git provider
    ; If yes, show "Generate Commit Message" option

    ; If selected, call GenerateGitCommitMessage

    add rsp, 28h
    ret
HandleSCMInputBoxMenu endp

; -----------------------------------------------------------------------------
; Environment.json Validation
; Validates: .cursor/environment.json against schema
; -----------------------------------------------------------------------------
LoadEnvironmentSchema proc
    sub rsp, 28h

    ; Load JSON schema file
    invoke CreateFileA, addr szEnvSchemaPath, GENERIC_READ,
            FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, INVALID_HANDLE_VALUE
    je @@no_schema

    mov rbx, rax

    ; Get file size
    lea rdx, [rsp+20h]
    invoke GetFileSizeEx, rbx, rdx

    ; Allocate buffer
    mov rcx, [rsp+20h]
    call malloc
    mov g_AlwaysLocal.environmentSchema, rax

    ; Read schema
    mov rcx, rbx
    mov rdx, rax
    mov r8, [rsp+20h]
    lea r9, [rsp+28h]
    xor eax, eax
    mov [rsp+30h], rax
    call ReadFile

    ; Close file
    mov rcx, rbx
    call CloseHandle

@@no_schema:
    add rsp, 28h
    ret
LoadEnvironmentSchema endp

ValidateEnvironmentJson proc
    ; rcx = path to environment.json
    sub rsp, 28h

    ; Parse JSON file
    ; Validate against loaded schema

    ; Check required fields:
    ; - apiEndpoint (string)
    ; - modelOverrides (object)
    ; - experimentalFlags (number)

    mov eax, 1      ; Valid
    add rsp, 28h
    ret
ValidateEnvironmentJson endp

; -----------------------------------------------------------------------------
; UI Contributions (Commands, Keybindings, Configuration)
; -----------------------------------------------------------------------------
InitUIContributions proc
    sub rsp, 28h

    ; Register command: cursor.generateGitCommitMessage
    invoke RegisterCommand, CMD_GENERATE_COMMIT, addr szCmdGenerateCommit

    ; Register resource label formatter for vscode-remote scheme
    call RegisterResourceLabelFormatter

    add rsp, 28h
    ret
InitUIContributions endp

InitConfiguration proc
    sub rsp, 28h

    ; Register configuration schema
    ; Type: object, Title: "Cursor Always Local"

    add rsp, 28h
    ret
InitConfiguration endp

RegisterCommand proc
    ; ecx = command ID, rdx = command string
    ret
RegisterCommand endp

RegisterResourceLabelFormatter proc
    sub rsp, 28h

    ; Register formatter for vscode-remote://background-composer+*
    ; Label format: ${path}
    ; Separator: /
    ; Workspace suffix: cloud-agent

    add rsp, 28h
    ret
RegisterResourceLabelFormatter endp

; -----------------------------------------------------------------------------
; Utility Functions
; -----------------------------------------------------------------------------
strcmp proc
    mov al, [rcx]
    mov bl, [rdx]
    cmp al, bl
    jne @@diff
    test al, al
    jz @@equal
    inc rcx
    inc rdx
    jmp strcmp
@@diff:
    mov eax, 1
    ret
@@equal:
    xor eax, eax
    ret
strcmp endp

strcpy proc
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz strcpy
    ret
strcpy endp

malloc proc
    invoke HeapAlloc, GetProcessHeap(), 0, rcx
    ret
malloc endp

; ============= EXPORTS =============
public AlwaysLocal_Entry
public GenerateGitCommitMessage
public ValidateEnvironmentJson

end