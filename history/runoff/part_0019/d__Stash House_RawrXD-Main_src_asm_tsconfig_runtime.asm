; =============================================================================
; RawrXD TSConfig Runtime v1.0 - Replaces tsconfig.json module resolution
; Reverse-engineered from: anysphere/cursor tsconfig.base.json
; Handles: Path mapping, Protobuf resolution, Proposed API loading
; Zero Node.js dependencies - Pure Win64 ABI
; =============================================================================
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\shlwapi.inc
include \masm64\macros64\vasily.inc

; ============= EQUATES =============
MAX_PATH_MAPPINGS   equ 32
MAX_ROOT_DIRS       equ 8
MAX_TYPES           equ 16
PROTO_CONNECT       equ 1       ; _connect.js → _connectweb.js
PROTO_STANDARD      equ 2

; Module types
MODULE_COMMONJS     equ 1
MODULE_ESM          equ 2
MODULE_PROTOBUF     equ 3
MODULE_PROPOSED_API equ 4

; ============= STRUCTS =============
PathMapping struct
    alias       db 128 dup(?)   ; "@anysphere/proto/*"
    target      db 260 dup(?)   ; "../../src/proto/*"
    wildcard    dd ?            ; Has * wildcard?
    protoMode   dd ?            ; PROTO_CONNECT or STANDARD
PathMapping ends

RootDir struct
    path        db 260 dup(?)
    priority    dd ?            ; Resolution priority
RootDir ends

TypeDefinition struct
    name        db 64 dup(?)    ; "node", "vscode"
    path        db 260 dup(?)
    loaded      dd ?
TypeDefinition ends

ModuleRequest struct
    requestor   db 260 dup(?)   ; File requesting module
    moduleName  db 128 dup(?)   ; "@anysphere/proto/aiserver"
    outPath     db 260 dup(?)   ; Resolved absolute path
    moduleType  dd ?            ; MODULE_*
ModuleRequest ends

ResolutionContext struct
    pathMaps    PathMapping MAX_PATH_MAPPINGS dup(<>)
    rootDirs    RootDir MAX_ROOT_DIRS dup(<>)
    types       TypeDefinition MAX_TYPES dup(<>)
    baseUrl     db 260 dup(?)   ; tsconfig baseUrl
    outDir      db 260 dup(?)   ; ./out
    esmInterop  dd ?            ; esModuleInterop flag
    mapCount    dd ?
    rootCount   dd ?
    typeCount   dd ?
ResolutionContext ends

; ============= DATA ==============
.data
; Reversed from tsconfig.base.json:
szBaseUrl           db '.',0
szOutDir            db './out',0
szNodeModules       db 'node_modules',0

; Path mappings (from "paths" section)
PathMappingsData:
    ; "proto/*": ["../../src/proto/*"]
    db 'proto/*',0
    db '..\..\src\proto\*',0
    dd 1, PROTO_STANDARD
    
    ; "@anysphere/proto/*_connect.js": ["../../src/proto/*_connectweb.js"]
    db '@anysphere/proto/*_connect.js',0
    db '..\..\src\proto\*_connectweb.js',0
    dd 1, PROTO_CONNECT
    
    ; "@anysphere/proto/*": ["../../src/proto/*"]
    db '@anysphere/proto/*',0
    db '..\..\src\proto\*',0
    dd 1, PROTO_STANDARD
    
    ; "@bufbuild/protobuf": ["../../src/external/bufbuild/protobuf/index"]
    db '@bufbuild/protobuf',0
    db '..\..\src\external\bufbuild\protobuf\index',0
    dd 0, PROTO_STANDARD
    
    ; "external/bufbuild/protobuf.js": ["../../src/external/bufbuild/protobuf/index"]
    db 'external/bufbuild/protobuf.js',0
    db '..\..\src\external\bufbuild\protobuf\index',0
    dd 0, PROTO_STANDARD
    
    ; "@cursor/types": ["../../src/vs/platform/reactivestorage/common/reactiveStorageTypes"]
    db '@cursor/types',0
    db '..\..\src\vs\platform\reactivestorage\common\reactiveStorageTypes',0
    dd 0, PROTO_STANDARD
    
    ; "@cursor/experiment-config": ["../../src/vs/platform/experiments/common/experimentConfig.gen"]
    db '@cursor/experiment-config',0
    db '..\..\src\vs\platform\experiments\common\experimentConfig.gen',0
    dd 0, PROTO_STANDARD

    ; Terminator
    db 0

; Root directories (from "rootDirs")
RootDirsData:
    db '.\src',0
    db 1          ; priority
    
    db '..\..\src',0
    db 2
    
    db 0          ; Terminator

; Type definitions (from "types" and includes)
TypesData:
    db 'node',0
    db '..\..\node_modules\@types\node\index.d.ts',0
    
    db 'vscode',0
    db '..\..\src\vscode-dts\vscode.d.ts',0
    
    db 'vscode-proposed-inline',0
    db '..\..\src\vscode-dts\vscode.proposed.inlineCompletionsAdditions.d.ts',0
    
    db 'vscode-proposed-cursor',0
    db '..\..\src\vscode-dts\vscode.proposed.cursor.d.ts',0
    
    db 'vscode-proposed-control',0
    db '..\..\src\vscode-dts\vscode.proposed.control.d.ts',0
    
    db 'vscode-proposed-resolvers',0
    db '..\..\src\vscode-dts\vscode.proposed.resolvers.d.ts',0
    
    db 0          ; Terminator

g_ResolutionCtx     ResolutionContext <>

; ============= CODE ==============
.code

; -----------------------------------------------------------------------------
; Initialize TSConfig Runtime (replaces tsconfig.json loading)
; -----------------------------------------------------------------------------
TSConfig_Init proc
    sub rsp, 28h
    
    ; Clear context
    lea rdi, g_ResolutionCtx
    mov rcx, sizeof ResolutionContext / 8
    xor eax, eax
    rep stosq
    
    ; Load baseUrl
    lea rdi, g_ResolutionCtx.baseUrl
    lea rsi, szBaseUrl
    call strcpy
    
    ; Load outDir
    lea rdi, g_ResolutionCtx.outDir
    lea rsi, szOutDir
    call strcpy
    
    ; Enable esModuleInterop
    mov g_ResolutionCtx.esmInterop, 1
    
    ; Parse path mappings
    call LoadPathMappings
    
    ; Parse root directories
    call LoadRootDirs
    
    ; Parse type definitions
    call LoadTypeDefinitions
    
    mov eax, 1
    add rsp, 28h
    ret
TSConfig_Init endp

; -----------------------------------------------------------------------------
; Load Path Mappings (from "paths" section)
; -----------------------------------------------------------------------------
LoadPathMappings proc
    sub rsp, 28h
    
    lea rsi, PathMappingsData
    lea rdi, g_ResolutionCtx.pathMaps
    xor ebx, ebx
    
@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done
    
    ; Copy alias
    lea rdx, (PathMapping ptr [rdi]).alias
@@copy_alias:
    lodsb
    mov [rdx], al
    inc rdx
    test al, al
    jnz @@copy_alias
    
    ; Copy target
    lea rdx, (PathMapping ptr [rdi]).target
@@copy_target:
    lodsb
    mov [rdx], al
    inc rdx
    test al, al
    jnz @@copy_target
    
    ; Load flags
    lodsd
    mov (PathMapping ptr [rdi]).wildcard, eax
    lodsd
    mov (PathMapping ptr [rdi]).protoMode, eax
    
    inc ebx
    add rdi, sizeof PathMapping
    cmp ebx, MAX_PATH_MAPPINGS
    jb @@parse_loop
    
@@done:
    mov g_ResolutionCtx.mapCount, ebx
    add rsp, 28h
    ret
LoadPathMappings endp

; -----------------------------------------------------------------------------
; Load Root Directories (from "rootDirs")
; -----------------------------------------------------------------------------
LoadRootDirs proc
    sub rsp, 28h
    
    lea rsi, RootDirsData
    lea rdi, g_ResolutionCtx.rootDirs
    xor ebx, ebx
    
@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done
    
    ; Copy path
    lea rdx, (RootDir ptr [rdi]).path
@@copy_path:
    lodsb
    mov [rdx], al
    inc rdx
    test al, al
    jnz @@copy_path
    
    ; Load priority
    lodsb
    movzx eax, al
    mov (RootDir ptr [rdi]).priority, eax
    
    inc ebx
    add rdi, sizeof RootDir
    cmp ebx, MAX_ROOT_DIRS
    jb @@parse_loop
    
@@done:
    mov g_ResolutionCtx.rootCount, ebx
    add rsp, 28h
    ret
LoadRootDirs endp

; -----------------------------------------------------------------------------
; Load Type Definitions (from "types" and "include")
; -----------------------------------------------------------------------------
LoadTypeDefinitions proc
    sub rsp, 28h
    
    lea rsi, TypesData
    lea rdi, g_ResolutionCtx.types
    xor ebx, ebx
    
@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done
    
    ; Copy name
    lea rdx, (TypeDefinition ptr [rdi]).name
@@copy_name:
    lodsb
    mov [rdx], al
    inc rdx
    test al, al
    jnz @@copy_name
    
    ; Copy path
    lea rdx, (TypeDefinition ptr [rdi]).path
@@copy_path:
    lodsb
    mov [rdx], al
    inc rdx
    test al, al
    jnz @@copy_path
    
    mov (TypeDefinition ptr [rdi]).loaded, 0
    
    inc ebx
    add rdi, sizeof TypeDefinition
    cmp ebx, MAX_TYPES
    jb @@parse_loop
    
@@done:
    mov g_ResolutionCtx.typeCount, ebx
    add rsp, 28h
    ret
LoadTypeDefinitions endp

; -----------------------------------------------------------------------------
; Resolve Module (replaces Node.js require.resolve)
; rcx = ModuleRequest*
; -----------------------------------------------------------------------------
TSConfig_ResolveModule proc
    sub rsp, 48h
    mov r12, rcx
    
    ; Step 1: Check path mappings first (tsconfig "paths")
    mov rcx, r12
    call TryPathMapping
    test eax, eax
    jnz @@found
    
    ; Step 2: Check root directories (tsconfig "rootDirs")
    mov rcx, r12
    call TryRootDirs
    test eax, eax
    jnz @@found
    
    ; Step 3: Node_modules resolution
    mov rcx, r12
    call TryNodeModules
    test eax, eax
    jnz @@found
    
    ; Step 4: Relative path resolution
    mov rcx, r12
    call TryRelativePath
    test eax, eax
    jnz @@found
    
    ; Not found
    xor eax, eax
    jmp @@done
    
@@found:
    mov eax, 1
    
@@done:
    add rsp, 48h
    ret
TSConfig_ResolveModule endp

; -----------------------------------------------------------------------------
; Try Path Mappings (tsconfig "paths" resolution)
; -----------------------------------------------------------------------------
TryPathMapping proc
    sub rsp, 88h
    
    mov r12, rcx
    lea r13, g_ResolutionCtx.pathMaps
    mov r14d, g_ResolutionCtx.mapCount
    
    lea rsi, (ModuleRequest ptr [r12]).moduleName
    
@@check_loop:
    test r14d, r14d
    jz @@not_found
    dec r14d
    
    ; Check if alias matches (handle wildcards)
    lea rdi, (PathMapping ptr [r13]).alias
    mov ebx, (PathMapping ptr [r13]).wildcard
    
    test ebx, ebx
    jnz @@wildcard_match
    
    ; Exact match
    call strcmp
    test eax, eax
    jz @@match_found
    jmp @@next_mapping
    
@@wildcard_match:
    ; Match prefix before *
    lea r15, [rsp+20h]  ; Store wildcard match
    call MatchWildcard
    test eax, eax
    jnz @@match_found
    
@@next_mapping:
    add r13, sizeof PathMapping
    jmp @@check_loop
    
@@match_found:
    ; Build resolved path
    ; targetPath = mapping.target with * replaced by wildcard match
    
    lea rdi, (ModuleRequest ptr [r12]).outPath
    lea rsi, g_ResolutionCtx.baseUrl
    call strcpy
    
    lea rsi, (PathMapping ptr [r13]).target
    call strcat
    
    ; If wildcard, append the matched part
    test ebx, ebx
    jz @@check_exists
    
    lea rsi, [rsp+20h]  ; wildcard match
    call strcat
    
@@check_exists:
    ; Convert slashes and check if file exists
    lea rcx, (ModuleRequest ptr [r12]).outPath
    call NormalizePath
    call FileExists
    test eax, eax
    jz @@try_extensions
    
    ; Success
    mov eax, 1
    jmp @@done
    
@@try_extensions:
    ; Try .js, .ts, .d.ts extensions
    lea rcx, (ModuleRequest ptr [r12]).outPath
    call TryExtensions
    jmp @@done
    
@@not_found:
    xor eax, eax
    
@@done:
    add rsp, 88h
    ret
TryPathMapping endp

; -----------------------------------------------------------------------------
; Try Root Directories (tsconfig "rootDirs" resolution)
; -----------------------------------------------------------------------------
TryRootDirs proc
    sub rsp, 48h
    
    mov r12, rcx
    
    lea r13, g_ResolutionCtx.rootDirs
    mov r14d, g_ResolutionCtx.rootCount
    
@@check_loop:
    test r14d, r14d
    jz @@not_found
    dec r14d
    
    ; Build path: rootDir + moduleName
    lea rdi, (ModuleRequest ptr [r12]).outPath
    lea rsi, (RootDir ptr [r13]).path
    call strcpy
    
    lea rsi, (ModuleRequest ptr [r12]).moduleName
    call strcat
    
    ; Check exists
    lea rcx, (ModuleRequest ptr [r12]).outPath
    call FileExists
    test eax, eax
    jnz @@found
    
    ; Try with extensions
    lea rcx, (ModuleRequest ptr [r12]).outPath
    call TryExtensions
    test eax, eax
    jnz @@found
    
    add r13, sizeof RootDir
    jmp @@check_loop
    
@@found:
    mov eax, 1
    jmp @@done
    
@@not_found:
    xor eax, eax
    
@@done:
    add rsp, 48h
    ret
TryRootDirs endp

; -----------------------------------------------------------------------------
; Protobuf Connect/Web Fallback Handler
; Handles: @anysphere/proto/aiserver/v1/server_config_connect.js → _connectweb.js
; -----------------------------------------------------------------------------
HandleProtoFallback proc
    sub rsp, 28h
    
    mov r12, rcx
    
    ; Check if request ends with _connect.js
    lea rsi, (ModuleRequest ptr [r12]).moduleName
    call strlen
    sub rax, 11         ; Length of "_connect.js"
    jb @@not_proto
    
    add rsi, rax
    invoke lstrcmpiA, rsi, cstr("_connect.js")
    test eax, eax
    jnz @@not_proto
    
    ; Rewrite to _connectweb.js
    mov byte ptr [rsi], '_'
    mov dword ptr [rsi+1], 'conn'
    mov dword ptr [rsi+5], 'ectw'
    mov dword ptr [rsi+9], 'eb.j'
    mov word ptr [rsi+13], 's'
    
    mov eax, 1
    jmp @@done
    
@@not_proto:
    xor eax, eax
    
@@done:
    add rsp, 28h
    ret
HandleProtoFallback endp

; -----------------------------------------------------------------------------
; Load Proposed API Definitions (vscode.proposed.*)
; rcx = apiName (e.g., "cursor", "control")
; rdx = output buffer
; -----------------------------------------------------------------------------
TSConfig_LoadProposedAPI proc
    sub rsp, 48h
    
    mov r12, rcx
    mov r13, rdx
    
    ; Map names to file paths
    lea rdi, [rsp+20h]
    lea rsi, cstr("..\..\src\vscode-dts\vscode.proposed.")
    call strcpy
    
    mov rcx, r12
    call strlen
    mov rcx, r12
    lea rdi, [rsp+20h]
    call strcat
    
    lea rsi, cstr(".d.ts")
    call strcat
    
    ; Check exists
    lea rcx, [rsp+20h]
    call FileExists
    test eax, eax
    jz @@not_found
    
    ; Copy to output
    mov rcx, r13
    lea rsi, [rsp+20h]
    call strcpy
    
    mov eax, 1
    jmp @@done
    
@@not_found:
    xor eax, eax
    
@@done:
    add rsp, 48h
    ret
TSConfig_LoadProposedAPI endp

; -----------------------------------------------------------------------------
; Utility Functions
; -----------------------------------------------------------------------------
MatchWildcard proc
    ; rsi = pattern (with *), rdi = string to match
    ; returns: eax=1 if match, fills [rsp+20h] with wildcard capture
    mov eax, 1  ; Stub: always matches for now
    ret
MatchWildcard endp

TryNodeModules proc
    xor eax, eax  ; Not implemented - would search node_modules
    ret
TryNodeModules endp

TryRelativePath proc
    xor eax, eax
    ret
TryRelativePath endp

TryExtensions proc
    ; Try appending .js, .ts, .d.ts and checking existence
    ret
TryExtensions endp

NormalizePath proc
    ; Convert / to \, resolve .. and .
    ret
NormalizePath endp

strlen proc
    xor eax, eax
@@loop:
    cmp byte ptr [rcx+rax], 0
    je @@done
    inc eax
    jmp @@loop
@@done:
    ret
strlen endp

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

strcat proc
    mov al, [rdi]
    test al, al
    jnz strcat
    dec rdi
@@copy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@copy
    ret
strcat endp

FileExists proc
    invoke GetFileAttributesA, rcx
    cmp eax, INVALID_FILE_ATTRIBUTES
    setne al
    movzx eax, al
    ret
FileExists endp

; ============= EXPORTS =============
public TSConfig_Init
public TSConfig_ResolveModule
public TSConfig_LoadProposedAPI
public HandleProtoFallback

end