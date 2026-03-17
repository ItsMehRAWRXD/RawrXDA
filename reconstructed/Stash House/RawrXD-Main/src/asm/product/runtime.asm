; =============================================================================
; RawrXD Product Runtime v1.0 - Replaces product.json + Node.js config loader
; Reverse-engineered from: Cursor product.json (commit 32cfbe84)
; Handles: Extension gallery, telemetry blocking, update checks, recommendations
; Zero JSON parsing - Direct memory-mapped structs
; =============================================================================
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\shlwapi.inc
include \masm64\include64\crypt32.inc
include \masm64\include64\bcrypt.inc

; ============= EQUATES =============
MAX_EXTENSIONS        equ 256
MAX_RECOMMENDATIONS   equ 128
MAX_FILE_ASSOCIATIONS equ 64
TELEMETRY_DISABLED    equ 0
TELEMETRY_LOCAL       equ 1
TELEMETRY_FULL        equ 2

; Product info
PRODUCT_NAME          equ 1
PRODUCT_VERSION       equ "2.2.43"
PRODUCT_COMMIT        equ "32cfbe848b35d9eb320980195985450f244b3030"

; ============= STRUCTS =============
ExtensionGallery struct
    serviceUrl      db 256 dup(?)   ; marketplace endpoint
    itemUrl         db 256 dup(?)   ; extension detail URL
    controlUrl      db 256 dup(?)   ; extension control/update
    cacheEnabled    dd ?
ExtensionGallery ends

TelemetryConfig struct
    statsigKey      db 128 dup(?)   ; Client key (blocked in local mode)
    proxyUrl        db 256 dup(?)   ; Event proxy (redirected to null)
    level           dd ?            ; 0=off, 1=error only, 2=full
    machineIdRemove dd ?            ; Privacy flag
TelemetryConfig ends

ExtensionRecommendation struct
    filePattern     db 128 dup(?)   ; *.py, **/*.rs, etc.
    languageId      db 32 dup(?)    ; python, rust
    extensionId     db 64 dup(?)    ; ms-python.python
    priority        dd ?            ; 0-10 importance
    whenInstalled   dd ?            ; Dependency mask
ExtensionRecommendation ends

FileAssociation struct
    ext             db 16 dup(?)    ; py, rs, cpp
    keywords        db 128 dup(?)   ; Search keywords
    langId          db 32 dup(?)    ; Language identifier
FileAssociation ends

BuiltInExtension struct
    name            db 64 dup(?)    ; ms-vscode.js-debug
    version         db 32 dup(?)
    sha256          db 64 dup(?)    ; SHA256 hash for verification
    repo            db 256 dup(?)   ; Source repo
    active          dd ?
BuiltInExtension ends

ProductConfig struct
    nameShort       db 32 dup(?)    ; "Cursor"
    nameLong        db 64 dup(?)    ; "Cursor"
    version         db 16 dup(?)    ; "2.2.43"
    commit          db 64 dup(?)    ; Git commit
    date            db 32 dup(?)    ; Build date
    dataFolder      db 32 dup(?)    ; .cursor
    gallery         ExtensionGallery <>
    telemetry       TelemetryConfig <>
    builtinCount    dd ?
    builtins        BuiltInExtension 16 dup(<>)  ; js-debug, etc.
    recCount        dd ?
    recommendations ExtensionRecommendation MAX_RECOMMENDATIONS dup(<>)
    fileAssocCount  dd ?
    fileAssociations FileAssociation MAX_FILE_ASSOCIATIONS dup(<>)
ProductConfig ends

; ============= DATA ==============
.data
align 16

; Product identity (immutable)
ProductIdentity:
nameShort       db "RawrXD",0
nameLong        db "RawrXD Agentic IDE",0
versionStr      db "2.2.43",0
commitStr       db "32cfbe848b35d9eb320980195985450f244b3030",0
buildDate       db "2025-12-19T06:06:44.644Z",0
dataFolderName  db ".rawrxd",0

; Local Gallery (replaces marketplace.cursorapi.com)
LocalGallery:
serviceUrl      db "http://localhost:11434/v1/extensions",0  ; Local Ollama-style
itemUrl         db "http://localhost:11434/v1/extension/",0
controlUrl      db "",0                                      ; No remote control
cacheEnabled    dd 1

; Telemetry Blocked (replaces Statsig with local null sink)
BlockedTelemetry:
statsigKey      db "DISABLED",0
proxyUrl        db "http://localhost:9",0                    ; Null route
level           dd TELEMETRY_DISABLED
machineIdRemove dd 1

; Built-in Extensions (reversed from JSON)
BuiltinExtensionsList:
; js-debug-companion
db "ms-vscode.js-debug-companion",0
db "1.1.3",0
db "7380a890787452f14b2db7835dfa94de538caf358ebc263f9d46dd68ac52de93",0
db "https://github.com/microsoft/vscode-js-debug-companion",0
dd 1

; js-debug
db "ms-vscode.js-debug",0
db "1.93.0",0
db "9339cb8e6b77f554df54d79e71f533279cb76b0f9b04c207f633bfd507442b6a",0
db "https://github.com/microsoft/vscode-js-debug",0
dd 1

; vscode-js-profile-table
db "ms-vscode.vscode-js-profile-table",0
db "1.0.9",0
db "3b62ee4276a2bbea3fe230f94b1d5edd915b05966090ea56f882e1e0ab53e1a6",0
db "https://github.com/microsoft/vscode-js-profile-visualizer",0
dd 1

db 0  ; Terminator

; Extension Recommendations (file type → extension)
RecommendationTable:
; Python
db "*.py",0
db "python",0
db "ms-python.python",0
dd 10, 0

db "**/*.ipynb",0
db "python",0
db "ms-toolsai.jupyter",0
dd 9, 0

; Rust
db "**/*.rs",0
db "rust",0
db "rust-lang.rust-analyzer",0
dd 10, 0

; Go
db "**/*.go",0
db "go",0
db "golang.Go",0
dd 10, 0

; C/C++
db "**/*.cpp",0
db "cpp",0
db "ms-vscode.cpptools-extension-pack",0
dd 10, 0

db "**/*.c",0
db "c",0
db "ms-vscode.cpptools-extension-pack",0
dd 10, 0

; JavaScript/TypeScript
db "**/*.ts",0
db "typescript",0
db "esbenp.prettier-vscode",0
dd 8, 0

db "**/*.tsx",0
db "typescriptreact",0
db "esbenp.prettier-vscode",0
dd 8, 0

; Java
db "**/*.java",0
db "java",0
db "vscjava.vscode-java-pack",0
dd 10, 0

; PowerShell
db "**/*.ps1",0
db "powershell",0
db "ms-vscode.PowerShell",0
dd 10, 0

db 0  ; Terminator

; File Associations (ext → language → keywords)
FileAssociationTable:
db "py",0, "python",0, "Python python3 pip venv conda",0
db "rs",0, "rust",0, "Rust cargo rustc",0
db "go",0, "go",0, "Go golang gopher",0
db "cpp",0, "cpp",0, "C++ cpp cxx cc hpp",0
db "c",0, "c",0, "C language gcc clang",0
db "js",0, "javascript",0, "JavaScript js node npm",0
db "ts",0, "typescript",0, "TypeScript ts node",0
db "java",0, "java",0, "Java jdk jvm maven gradle",0
db "cs",0, "csharp",0, "C# dotnet aspnet",0
db "rb",0, "ruby",0, "Ruby rails gem",0
db "php",0, "php",0, "PHP laravel composer",0
db "md",0, "markdown",0, "Markdown md docs",0
db 0  ; Terminator

; Checksums (security verification)
ChecksumTable:
db "vs/base/parts/sandbox/electron-sandbox/preload.js",0
db "sTyuO7cKrZVGOE122EU4TQQvM674BFZxZUC7qhBxClY",0

db "vs/workbench/workbench.desktop.main.js",0
db "aOkhDlbDq4KxZJlO60MvyT4SsY7OhVkHmPvONfEZNWo",0

db 0  ; Terminator

g_ProductConfig     ProductConfig <>
g_ConfigLoaded      dd 0

; ============= MISSING DATA =============
align 16
szStatsigNull   db "http://localhost:9/statsig",0
szEventsNull    db "http://localhost:9/events",0
.code

; -----------------------------------------------------------------------------
; Initialize Product Runtime (replaces JSON loader)
; -----------------------------------------------------------------------------
ProductRuntime_Init proc
    sub rsp, 28h

    cmp g_ConfigLoaded, 1
    je @@already_loaded

    ; Clear config structure
    lea rdi, g_ProductConfig
    mov rcx, sizeof ProductConfig / 8
    xor eax, eax
    rep stosq

    ; Load identity
    lea rsi, nameShort
    lea rdi, (ProductConfig ptr [g_ProductConfig]).nameShort
    call strcpy

    lea rsi, nameLong
    lea rdi, (ProductConfig ptr [g_ProductConfig]).nameLong
    call strcpy

    lea rsi, versionStr
    lea rdi, (ProductConfig ptr [g_ProductConfig]).version
    call strcpy

    lea rsi, commitStr
    lea rdi, (ProductConfig ptr [g_ProductConfig]).commit
    call strcpy

    lea rsi, dataFolderName
    lea rdi, (ProductConfig ptr [g_ProductConfig]).dataFolder
    call strcpy

    ; Load gallery config (LOCAL MODE)
    lea rsi, LocalGallery
    lea rdi, (ProductConfig ptr [g_ProductConfig]).gallery
    mov ecx, sizeof ExtensionGallery
    call memcpy

    ; Load telemetry config (BLOCKED)
    lea rsi, BlockedTelemetry
    lea rdi, (ProductConfig ptr [g_ProductConfig]).telemetry
    mov ecx, sizeof TelemetryConfig
    call memcpy

    ; Parse built-in extensions
    call ParseBuiltinExtensions

    ; Parse recommendations
    call ParseRecommendations

    ; Parse file associations
    call ParseFileAssociations

    mov g_ConfigLoaded, 1

@@already_loaded:
    mov eax, 1
    add rsp, 28h
    ret
ProductRuntime_Init endp

; -----------------------------------------------------------------------------
; Parse Built-in Extensions List
; -----------------------------------------------------------------------------
ParseBuiltinExtensions proc
    sub rsp, 28h

    lea rsi, BuiltinExtensionsList
    lea rdi, (ProductConfig ptr [g_ProductConfig]).builtins
    xor ebx, ebx

@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done

    ; Copy name
    lea rdx, (BuiltInExtension ptr [rdi]).name
    call strcpy_dest

    ; Copy version
    lea rdx, (BuiltInExtension ptr [rdi]).version
    call strcpy_dest

    ; Copy SHA256
    lea rdx, (BuiltInExtension ptr [rdi]).sha256
    call strcpy_dest

    ; Copy repo
    lea rdx, (BuiltInExtension ptr [rdi]).repo
    call strcpy_dest

    ; Set active and increment
    mov (BuiltInExtension ptr [rdi]).active, 1
    inc ebx
    add rdi, sizeof BuiltInExtension

    cmp ebx, 16
    jb @@parse_loop

@@done:
    mov (ProductConfig ptr [g_ProductConfig]).builtinCount, ebx
    add rsp, 28h
    ret

strcpy_dest:
    mov al, [rsi]
    mov [rdx], al
    inc rsi
    inc rdx
    test al, al
    jnz strcpy_dest
    ret
ParseBuiltinExtensions endp

; -----------------------------------------------------------------------------
; Parse Recommendations Table
; -----------------------------------------------------------------------------
ParseRecommendations proc
    sub rsp, 28h

    lea rsi, RecommendationTable
    lea rdi, (ProductConfig ptr [g_ProductConfig]).recommendations
    xor ebx, ebx

@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done

    ; filePattern
    lea rdx, (ExtensionRecommendation ptr [rdi]).filePattern
    call strcpy_dest

    ; languageId
    lea rdx, (ExtensionRecommendation ptr [rdi]).languageId
    call strcpy_dest

    ; extensionId
    lea rdx, (ExtensionRecommendation ptr [rdi]).extensionId
    call strcpy_dest

    ; priority
    mov eax, dword ptr [rsi]
    mov (ExtensionRecommendation ptr [rdi]).priority, eax
    add rsi, 4

    ; whenInstalled
    mov eax, dword ptr [rsi]
    mov (ExtensionRecommendation ptr [rdi]).whenInstalled, eax
    add rsi, 4

    inc ebx
    add rdi, sizeof ExtensionRecommendation

    cmp ebx, MAX_RECOMMENDATIONS
    jb @@parse_loop

@@done:
    mov (ProductConfig ptr [g_ProductConfig]).recCount, ebx
    add rsp, 28h
    ret
ParseRecommendations endp

; -----------------------------------------------------------------------------
; Parse File Associations
; -----------------------------------------------------------------------------
ParseFileAssociations proc
    sub rsp, 28h

    lea rsi, FileAssociationTable
    lea rdi, (ProductConfig ptr [g_ProductConfig]).fileAssociations
    xor ebx, ebx

@@parse_loop:
    cmp byte ptr [rsi], 0
    je @@done

    ; ext
    lea rdx, (FileAssociation ptr [rdi]).ext
    call strcpy_dest

    ; langId
    lea rdx, (FileAssociation ptr [rdi]).langId
    call strcpy_dest

    ; keywords
    lea rdx, (FileAssociation ptr [rdi]).keywords
    call strcpy_dest

    inc ebx
    add rdi, sizeof FileAssociation

    cmp ebx, MAX_FILE_ASSOCIATIONS
    jb @@parse_loop

@@done:
    mov (ProductConfig ptr [g_ProductConfig]).fileAssocCount, ebx
    add rsp, 28h
    ret
ParseFileAssociations endp

; -----------------------------------------------------------------------------
; Get Extension Recommendation for File
; rcx = filename, rdx = extensionId buffer
; -----------------------------------------------------------------------------
ProductRuntime_GetRecommendation proc
    sub rsp, 48h

    mov r12, rcx    ; filename
    mov r13, rdx    ; output buffer

    ; Find file extension
    mov rcx, r12
    call GetFileExtension
    mov r14, rax

    ; Search recommendations
    mov rdi, offset (ProductConfig ptr [g_ProductConfig]).recommendations
    mov ebx, (ProductConfig ptr [g_ProductConfig]).recCount

@@search_loop:
    test ebx, ebx
    jz @@not_found
    dec ebx

    ; Check if extension matches pattern
    lea rcx, (ExtensionRecommendation ptr [rdi]).filePattern
    mov rdx, r14
    call MatchWildcard

    test eax, eax
    jnz @@found

    add rdi, sizeof ExtensionRecommendation
    jmp @@search_loop

@@found:
    lea rsi, (ExtensionRecommendation ptr [rdi]).extensionId
    mov rdi, r13
    call strcpy

    mov eax, 1
    jmp @@done

@@not_found:
    xor eax, eax

@@done:
    add rsp, 48h
    ret
ProductRuntime_GetRecommendation endp

; -----------------------------------------------------------------------------
; Verify File Checksum (security)
; rcx = filepath, rdx = expected hash
; -----------------------------------------------------------------------------
ProductRuntime_VerifyChecksum proc
    sub rsp, 88h

    ; Open file
    invoke CreateFileA, rcx, GENERIC_READ, FILE_SHARE_READ, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, INVALID_HANDLE_VALUE
    je @@failed
    mov rbx, rax

    ; Get file size
    lea rdx, [rsp+20h]
    invoke GetFileSizeEx, rbx, rdx

    ; Allocate buffer
    mov rcx, [rsp+20h]
    call malloc
    mov r12, rax

    ; Read file
    mov rcx, rbx
    mov rdx, r12
    mov r8, [rsp+20h]
    lea r9, [rsp+28h]
    xor eax, eax
    mov [rsp+30h], rax
    call ReadFile

    ; Close file
    mov rcx, rbx
    call CloseHandle

    ; Calculate SHA256
    mov rcx, r12                    ; buffer
    mov rdx, [rsp+20h]              ; size
    lea r8, [rsp+40h]               ; output hash buffer
    call CalculateSHA256

    ; Compare with expected
    mov rcx, r8
    mov rdx, rdx                    ; expected hash
    call strcmp

    test eax, eax
    jnz @@failed

    ; Cleanup
    mov rcx, r12
    call free

    mov eax, 1
    jmp @@done

@@failed:
    xor eax, eax

@@done:
    add rsp, 88h
    ret
ProductRuntime_VerifyChecksum endp

; ============= FIXED TELEMETRY ENDPOINT =============
ProductRuntime_GetTelemetryEndpoint:
    cmp ecx, 1
    je @@statsig
    cmp ecx, 2
    je @@proxy
    xor eax, eax
    ret
    
@@statsig:
    lea rax, szStatsigNull
    mov rsi, rax
    jmp @@copy
    
@@proxy:
    lea rax, szEventsNull
    mov rsi, rax
    
@@copy:
    mov rdi, rdx
    call strcpy
    mov eax, TELEMETRY_DISABLED
    ret

; -----------------------------------------------------------------------------
; Get Gallery URL (LOCAL MODE)
; rcx = URL type (1=service, 2=item, 3=control)
; rdx = output buffer
; -----------------------------------------------------------------------------
ProductRuntime_GetGalleryUrl proc
    sub rsp, 28h

    lea rsi, (ProductConfig ptr [g_ProductConfig]).gallery

    cmp ecx, 1
    je @@service
    cmp ecx, 2
    je @@item
    cmp ecx, 3
    je @@control

    xor eax, eax
    jmp @@done

@@service:
    add rsi, offset ExtensionGallery.serviceUrl
    jmp @@copy

@@item:
    add rsi, offset ExtensionGallery.itemUrl
    jmp @@copy

@@control:
    add rsi, offset ExtensionGallery.controlUrl

@@copy:
    mov rdi, rdx
    call strcpy
    mov eax, 1

@@done:
    add rsp, 28h
    ret
ProductRuntime_GetGalleryUrl endp

; -----------------------------------------------------------------------------
; Utility Functions
; -----------------------------------------------------------------------------
GetFileExtension proc
    ; Find last '.' in string
    mov rsi, rcx
    xor rax, rax
@@loop:
    lodsb
    test al, al
    jz @@done
    cmp al, '.'
    jne @@loop
    mov rax, rsi
    jmp @@loop
@@done:
    ret
GetFileExtension endp

; ============= WILDCARD MATCHER =============
MatchWildcard proc
    ; rcx = pattern (with * ?), rdx = string
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    
@@loop:
    mov al, [rsi]
    mov ah, [rdi]
    
    cmp al, '*'
    je @@star
    cmp al, '?'
    je @@question
    test al, al
    jz @@end_check
    cmp al, ah
    jne @@fail
    inc rsi
    inc rdi
    jmp @@loop
    
@@question:
    test ah, ah
    jz @@fail
    inc rsi
    inc rdi
    jmp @@loop
    
@@star:
    inc rsi
    mov al, [rsi]
    test al, al
    jz @@success      ; * at end matches all
    
@@star_loop:
    push rsi
    push rdi
    call MatchWildcard  ; Recursive check remainder
    pop rdi
    pop rsi
    test eax, eax
    jnz @@success
    
    mov ah, [rdi]
    test ah, ah
    jz @@fail
    inc rdi
    jmp @@star_loop
    
@@end_check:
    cmp al, ah        ; Both should be null
    jne @@fail
@@success:
    mov eax, 1
    pop rdi
    pop rsi
    ret
@@fail:
    xor eax, eax
    pop rdi
    pop rsi
    ret
MatchWildcard endp

; ============= SHA256 VIA CNG =============
CalculateSHA256 proc
    ; rcx = buffer, rdx = size, r8 = output (32 bytes)
    sub rsp, 88h
    
    mov r12, rcx      ; buffer
    mov r13, rdx      ; size  
    mov r14, r8       ; output
    
    ; Open algorithm provider
    lea rcx, [rsp+20h]    ; hAlgorithm
    lea rdx, [rsp+28h]    ; BCRYPT_SHA256_ALGORITHM
    lea r8, [rsp+30h]     ; MS_PRIMITIVE_PROVIDER
    xor r9, r9
    mov qword ptr [rsp+40h], 0
    call BCryptOpenAlgorithmProvider
    
    ; Create hash
    lea rcx, [rsp+20h]    ; hAlgorithm
    lea rdx, [rsp+38h]    ; hHash
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+40h], 0
    call BCryptCreateHash
    
    ; Hash data
    mov rcx, [rsp+38h]    ; hHash
    mov rdx, r12          ; buffer
    mov r8, r13           ; size
    xor r9, r9
    call BCryptHashData
    
    ; Finish hash
    mov rcx, [rsp+38h]    ; hHash
    mov rdx, r14          ; output
    mov r8, 32            ; SHA256_SIZE
    xor r9, r9
    call BCryptFinishHash
    
    ; Cleanup
    mov rcx, [rsp+38h]
    call BCryptDestroyHash
    mov rcx, [rsp+20h]
    call BCryptCloseAlgorithmProvider
    
    add rsp, 88h
    ret
    
BCRYPT_SHA256_ALGORITHM:
    dw 'S','H','A','2','5','6',0
MS_PRIMITIVE_PROVIDER:
    dw 'M','i','c','r','o','s','o','f','t',' ','P','r','i','m','i','t','i','v','e',' ','P','r','o','v','i','d','e','r',0
CalculateSHA256 endp

; ============= STRING UTILS =============
strcmp proc
    ; rcx = s1, rdx = s2
    mov r8, rcx
    mov r9, rdx
@@loop:
    mov al, [r8]
    mov dl, [r9]
    cmp al, dl
    jne @@diff
    test al, al
    jz @@equal
    inc r8
    inc r9
    jmp @@loop
@@diff:
    sub rax, rdx
    ret
@@equal:
    xor eax, eax
    ret
strcmp endp

memcpy proc
    rep movsb
    ret
memcpy endp

malloc proc
    invoke HeapAlloc, GetProcessHeap(), 0, rcx
    ret
malloc endp

free proc
    invoke HeapFree, GetProcessHeap(), 0, rcx
    ret
free endp

; ============= EXPORTS =============
public ProductRuntime_Init
public ProductRuntime_GetRecommendation
public ProductRuntime_VerifyChecksum
public ProductRuntime_GetTelemetryEndpoint
public ProductRuntime_GetGalleryUrl

end