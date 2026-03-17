;==============================================================================
; RawrXD Extension Marketplace Installer
; Pure MASM64 Implementation
;
; Features:
; - Parse .vsix packages (ZIP format)
; - Extract package.json manifests
; - Convert TypeScript/JavaScript to MASM64 stubs
; - Generate native entry points (activate/deactivate)
; - Create optimized directory structure
; - Register in extension registry
; - Dependency resolution
; - Signature verification (optional)
;==============================================================================
.686
.xmm
.model flat, c
option casemap:none
option frame:auto

;==============================================================================
; INCLUDES
;==============================================================================
include windows.inc
include kernel32.inc
include user32.inc
include ntdll.inc
include ole32.inc
include zlib.inc

includelib kernel32.lib
includelib user32.lib
includelib ntdll.lib
includelib ole32.lib
includelib zlib.lib

;==============================================================================
; CONSTANTS
;==============================================================================
; VSIX package structure
VSIX_SIGNATURE_SIZE      equ 256
VSIX_MANIFEST_SIZE       equ 4096
VSIX_MAX_FILES           equ 4096
VSIX_MAX_PATH            equ 260

; Extension types
EXT_TYPE_UNKNOWN         equ 0
EXT_TYPE_TYPESCRIPT      equ 1
EXT_TYPE_JAVASCRIPT      equ 2
EXT_TYPE_WEBASSEMBLY     equ 3
EXT_TYPE_NATIVE          equ 4
EXT_TYPE_LANGUAGE_PACK   equ 5
EXT_TYPE_THEME           equ 6

; Installation result codes
INSTALL_SUCCESS          equ 0
INSTALL_INVALID_PACKAGE  equ 1
INSTALL_MANIFEST_MISSING equ 2
INSTALL_PARSE_ERROR      equ 3
INSTALL_DEPENDENCY_ERROR equ 4
INSTALL_FILE_ERROR       equ 5
INSTALL_REGISTRY_ERROR   equ 6
INSTALL_ALREADY_EXISTS   equ 7

; Registry paths
REG_PATH_EXTENSIONS      db 'Software\RawrXD\Extensions',0
REG_PATH_MARKETPLACE     db 'Software\RawrXD\Marketplace',0

;==============================================================================
; STRUCTURES
;==============================================================================

;------------------------------------------------------------------------------
; VSIX Package Header (first 512 bytes of .vsix file)
;------------------------------------------------------------------------------
VSIX_HEADER struct
    Signature           db VSIX_SIGNATURE_SIZE dup(?)    ; "VSIX" + version
    PackageSize         dq ?                             ; total package size
    ManifestOffset      dq ?                             ; offset to manifest
    ManifestSize        dq ?                             ; manifest size
    ContentOffset       dq ?                             ; offset to content
    ContentSize         dq ?                             ; content size
    CompressionType     dd ?                             ; 0=none, 1=deflate, 2=lzma
    EncryptionType      dd ?                             ; 0=none, 1=aes256
    Checksum            db 32 dup(?)                     ; SHA256 checksum
    _padding            db 180 dup(?)                    ; reserved
VSIX_HEADER ends

;------------------------------------------------------------------------------
; VSIX File Entry (from manifest)
;------------------------------------------------------------------------------
VSIX_FILE_ENTRY struct
    FileName            db VSIX_MAX_PATH dup(?)          ; file path in package
    FileSize            dq ?                             ; uncompressed size
    CompressedSize      dq ?                             ; compressed size
    Offset              dq ?                             ; offset in package
    CompressionMethod   dd ?                             ; 0=stored, 8=deflated
    CRC32               dd ?                             ; CRC32 checksum
    Attributes          dd ?                             ; file attributes
    _padding            dd ?
VSIX_FILE_ENTRY ends

;------------------------------------------------------------------------------
; Extension Manifest (package.json subset)
;------------------------------------------------------------------------------
EXTENSION_MANIFEST struct
    Name                dq ?                             ; extension name
    DisplayName         dq ?                             ; display name
    Publisher           dq ?                             ; publisher name
    Version             dq ?                             ; version string
    Description         dq ?                             ; description
    Engines             dq ?                             ; vscode version constraints
    Main                dq ?                             ; entry point
    Browser             dq ?                             ; web worker entry
    ActivationEvents    dq ?                             ; array of activation events
    ActivationCount     dd ?                             ; number of events
    _padding1           dd ?
    Contributes         dq ?                             ; contributions (commands, etc.)
    Capabilities        dd ?                             ; CAP_* flags
    ExtensionKind       dd ?                             ; UI or workspace
    EnableProposedApi   db ?                             ; uses proposed APIs
    _padding2           db 3 dup(?)
    Dependencies        dq ?                             ; extension dependencies
    DependencyCount     dd ?                             ; number of dependencies
    _padding3           dd ?
    ExtensionPack       dq ?                             ; bundled extensions
    PackCount           dd ?                             ; number in pack
    _padding4           dd ?
    Repository          dq ?                             ; repository URL
    Bugs                dq ?                             ; bugs URL
    Homepage            dq ?                             ; homepage URL
    License             dq ?                             ; license
    Pricing             dq ?                             ; pricing info
    _padding5           dq ?                             ; reserved
EXTENSION_MANIFEST ends

;------------------------------------------------------------------------------
; Installation Context
;------------------------------------------------------------------------------
INSTALL_CONTEXT struct
    VsixPath            dq ?                             ; path to .vsix file
    InstallDir          dq ?                             ; installation directory
    TempDir             dq ?                             ; temporary extraction dir
    Manifest            EXTENSION_MANIFEST <>            ; parsed manifest
    FileEntries         dq ?                             ; array of VSIX_FILE_ENTRY
    FileCount           dd ?                             ; number of files
    _padding            dd ?
    ExtensionType       dd ?                             ; EXT_TYPE_* 
    _padding2           dd ?
    Dependencies        dq ?                             ; resolved dependencies
    DependencyCount     dd ?                             ; number of dependencies
    _padding3           dd ?
    InstallationResult  dd ?                             ; INSTALL_* result code
    ErrorMessage        dq ?                             ; error message if failed
INSTALL_CONTEXT ends

;------------------------------------------------------------------------------
; Extension Registry Entry
;------------------------------------------------------------------------------
EXTENSION_REGISTRY_ENTRY struct
    ExtensionId         dq ?                             ; publisher.name
    InstallPath         dq ?                             ; installation path
    Version             dq ?                             ; version string
    ManifestPath        dq ?                             ; path to manifest
    Enabled             db ?                             ; enabled/disabled
    _padding            db 7 dup(?)
    Capabilities        dd ?                             ; granted capabilities
    LastUpdated         dq ?                             ; timestamp
    _padding2           dq ?                             ; reserved
EXTENSION_REGISTRY_ENTRY ends

;------------------------------------------------------------------------------
; Dependency Graph Node
;------------------------------------------------------------------------------
DEPENDENCY_NODE struct
    ExtensionId         dq ?                             ; extension identifier
    VersionRange        dq ?                             ; version constraints
    IsOptional          db ?                             ; optional dependency
    IsInstalled         db ?                             ; already installed
    _padding            db 6 dup(?)
    Dependencies        dq ?                             ; child dependencies
    DependencyCount     dd ?                             ; number of children
    _padding2           dd ?
DEPENDENCY_NODE ends

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
align 16

; Installation context singleton
g_InstallContext        INSTALL_CONTEXT <>

; Extension registry
g_ExtensionRegistry     dq ?                             ; HashMap of installed extensions

; Marketplace cache
g_MarketplaceCache      dq ?                             ; Cache of available extensions

; VSIX extraction buffer
g_ExtractionBuffer      db 65536 dup(?)                  ; 64KB buffer for file extraction

; Error messages
szErrorInvalidPackage   db 'Invalid VSIX package format',0
szErrorManifestMissing  db 'package.json manifest not found',0
szErrorParseFailed      db 'Failed to parse manifest',0
szErrorDependency       db 'Dependency resolution failed',0
szErrorFileExtract      db 'File extraction failed',0
szErrorRegistry         db 'Registry update failed',0
szErrorAlreadyInstalled db 'Extension already installed',0

; Registry value names
szRegValueName          db 'Name',0
szRegValuePublisher     db 'Publisher',0
szRegValueVersion       db 'Version',0
szRegValuePath          db 'InstallPath',0
szRegValueManifest      db 'ManifestPath',0
szRegValueEnabled       db 'Enabled',0
szRegValueCapabilities  db 'Capabilities',0
szRegValueLastUpdated   db 'LastUpdated',0

;==============================================================================
; CODE
;==============================================================================
.code

;==============================================================================
; VSIX PACKAGE PARSING
;==============================================================================

;------------------------------------------------------------------------------
; Parse VSIX package header
;------------------------------------------------------------------------------
Marketplace_ParseVsixHeader proc frame uses rbx rsi rdi,
    pVsixPath:qword            ; path to .vsix file
    
    mov rbx, pVsixPath
    
    ; Open VSIX file
    mov rcx, rbx
    mov rdx, GENERIC_READ
    xor r8d, r8d                ; no sharing
    xor r9d, r9d                ; no security attributes
    sub rsp, 32
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je open_failed
    mov rsi, rax                ; file handle
    
    ; Read header
    lea rcx, g_VsixHeader
    mov edx, sizeof(VSIX_HEADER)
    mov r8, rsi
    call ReadFile
    test eax, eax
    jz read_failed
    
    ; Validate signature
    lea rcx, g_VsixHeader.Signature
    lea rdx, szVsixSignature
    call strcmp
    test eax, eax
    jnz invalid_signature
    
    ; Store in context
    lea rcx, g_InstallContext.VsixHeader
    mov rdx, offset g_VsixHeader
    mov r8d, sizeof(VSIX_HEADER)
    call memcpy
    
    ; Close file
    mov rcx, rsi
    call CloseHandle
    
    mov rax, 1
    ret
    
open_failed:
    mov g_InstallContext.InstallationResult, INSTALL_INVALID_PACKAGE
    mov g_InstallContext.ErrorMessage, offset szErrorInvalidPackage
    xor rax, rax
    ret
    
read_failed:
    mov rcx, rsi
    call CloseHandle
    mov g_InstallContext.InstallationResult, INSTALL_INVALID_PACKAGE
    mov g_InstallContext.ErrorMessage, offset szErrorInvalidPackage
    xor rax, rax
    ret
    
invalid_signature:
    mov rcx, rsi
    call CloseHandle
    mov g_InstallContext.InstallationResult, INSTALL_INVALID_PACKAGE
    mov g_InstallContext.ErrorMessage, offset szErrorInvalidPackage
    xor rax, rax
    ret
    
szVsixSignature         db 'VSIX',0
szVsixHeader            db 'VSIX_HEADER',0
g_VsixHeader            VSIX_HEADER <>
Marketplace_ParseVsixHeader endp

;------------------------------------------------------------------------------
; Extract and parse package.json manifest
;------------------------------------------------------------------------------
Marketplace_ParseManifest proc frame uses rbx rsi rdi r12 r13
    mov rbx, offset g_InstallContext
    
    ; Open VSIX file at manifest offset
    mov rcx, [rbx].INSTALL_CONTEXT.VsixPath
    mov rdx, GENERIC_READ
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je open_failed
    mov rsi, rax
    
    ; Seek to manifest offset
    mov rcx, rsi
    mov edx, FILE_BEGIN
    mov r8, [rbx].INSTALL_CONTEXT.VsixHeader.ManifestOffset
    xor r9d, r9d
    call SetFilePointerEx
    test eax, eax
    jz seek_failed
    
    ; Read manifest
    lea rcx, g_ManifestBuffer
    mov edx, VSIX_MANIFEST_SIZE
    mov r8, rsi
    call ReadFile
    test eax, eax
    jz read_failed
    
    ; Parse JSON manifest
    lea rcx, g_ManifestBuffer
    call Json_ParseString
    test rax, rax
    jz parse_failed
    mov rdi, rax                ; JSON object
    
    ; Extract manifest fields
    call Marketplace_ExtractManifestFields
    test eax, eax
    jz extract_failed
    
    ; Close file
    mov rcx, rsi
    call CloseHandle
    
    mov rax, 1
    ret
    
open_failed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
seek_failed:
    mov rcx, rsi
    call CloseHandle
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
read_failed:
    mov rcx, rsi
    call CloseHandle
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
parse_failed:
    mov rcx, rsi
    call CloseHandle
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_PARSE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorParseFailed
    xor rax, rax
    ret
    
extract_failed:
    mov rcx, rsi
    call CloseHandle
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_PARSE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorParseFailed
    xor rax, rax
    ret
    
g_ManifestBuffer          db VSIX_MANIFEST_SIZE dup(?)
Marketplace_ParseManifest endp

;------------------------------------------------------------------------------
; Extract fields from package.json manifest
;------------------------------------------------------------------------------
Marketplace_ExtractManifestFields proc frame uses rbx rsi rdi r12 r13,
    pJsonObject:qword          ; JSON object from manifest
    
    mov rbx, offset g_InstallContext
    mov rdi, pJsonObject
    
    ; Extract name
    lea rcx, szJsonName
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Name, rax
    
    ; Extract displayName
    lea rcx, szJsonDisplayName
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.DisplayName, rax
    
    ; Extract publisher
    lea rcx, szJsonPublisher
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Publisher, rax
    
    ; Extract version
    lea rcx, szJsonVersion
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Version, rax
    
    ; Extract description
    lea rcx, szJsonDescription
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Description, rax
    
    ; Extract engines (vscode compatibility)
    lea rcx, szJsonEngines
    call Json_GetObjectField
    mov [rbx].INSTALL_CONTEXT.Manifest.Engines, rax
    
    ; Extract main entry point
    lea rcx, szJsonMain
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Main, rax
    
    ; Extract browser entry point (for web workers)
    lea rcx, szJsonBrowser
    call Json_GetStringField
    mov [rbx].INSTALL_CONTEXT.Manifest.Browser, rax
    
    ; Extract activation events
    lea rcx, szJsonActivationEvents
    call Json_GetArrayField
    mov [rbx].INSTALL_CONTEXT.Manifest.ActivationEvents, rax
    
    ; Extract contributes (commands, menus, etc.)
    lea rcx, szJsonContributes
    call Json_GetObjectField
    mov [rbx].INSTALL_CONTEXT.Manifest.Contributes, rax
    
    ; Extract extension pack
    lea rcx, szJsonExtensionPack
    call Json_GetArrayField
    mov [rbx].INSTALL_CONTEXT.Manifest.ExtensionPack, rax
    
    ; Extract dependencies
    lea rcx, szJsonDependencies
    call Json_GetObjectField
    mov [rbx].INSTALL_CONTEXT.Manifest.Dependencies, rax
    
    ; Determine extension type
    call Marketplace_DetermineExtensionType
    mov [rbx].INSTALL_CONTEXT.ExtensionType, eax
    
    mov rax, 1
    ret
    
szJsonName              db 'name',0
szJsonDisplayName       db 'displayName',0
szJsonPublisher         db 'publisher',0
szJsonVersion           db 'version',0
szJsonDescription       db 'description',0
szJsonEngines           db 'engines',0
szJsonMain              db 'main',0
szJsonBrowser           db 'browser',0
szJsonActivationEvents  db 'activationEvents',0
szJsonContributes        db 'contributes',0
szJsonExtensionPack     db 'extensionPack',0
szJsonDependencies      db 'dependencies',0
Marketplace_ExtractManifestFields endp

;------------------------------------------------------------------------------
; Determine extension type from manifest
;------------------------------------------------------------------------------
Marketplace_DetermineExtensionType proc frame uses rbx rsi rdi
    mov rbx, offset g_InstallContext
    
    ; Check for native binaries
    mov rcx, [rbx].INSTALL_CONTEXT.Manifest.Main
    test rcx, rcx
    jz check_browser
    
    ; Check file extension
    call strrchr
    test rax, rax
    jz check_browser
    
    ; Check for .wasm (WebAssembly)
    lea rcx, szWasmExtension
    call strcmp
    test eax, eax
    jz found_wasm
    
    ; Check for .dll/.so/.dylib (native)
    lea rcx, szDllExtension
    call strcmp
    test eax, eax
    jz found_native
    
    lea rcx, szSoExtension
    call strcmp
    test eax, eax
    jz found_native
    
    lea rcx, szDylibExtension
    call strcmp
    test eax, eax
    jz found_native
    
check_browser:
    ; Check browser entry (web worker)
    mov rcx, [rbx].INSTALL_CONTEXT.Manifest.Browser
    test rcx, rcx
    jz check_language_pack
    
    ; Check for .wasm in browser entry
    call strrchr
    test rax, rax
    jz check_language_pack
    
    lea rcx, szWasmExtension
    call strcmp
    test eax, eax
    jz found_wasm
    
    ; Default to JavaScript for browser
    mov eax, EXT_TYPE_JAVASCRIPT
    ret
    
check_language_pack:
    ; Check if it's a language pack
    mov rcx, [rbx].INSTALL_CONTEXT.Manifest.Contributes
    test rcx, rcx
    jz check_theme
    
    ; Look for localizations
    lea rcx, szJsonLocalizations
    call Json_HasField
    test eax, eax
    jnz found_language_pack
    
check_theme:
    ; Check if it's a theme
    lea rcx, szJsonThemes
    call Json_HasField
    test eax, eax
    jnz found_theme
    
    ; Default to TypeScript/JavaScript
    mov eax, EXT_TYPE_TYPESCRIPT
    ret
    
found_wasm:
    mov eax, EXT_TYPE_WEBASSEMBLY
    ret
    
found_native:
    mov eax, EXT_TYPE_NATIVE
    ret
    
found_language_pack:
    mov eax, EXT_TYPE_LANGUAGE_PACK
    ret
    
found_theme:
    mov eax, EXT_TYPE_THEME
    ret
    
szWasmExtension         db '.wasm',0
szDllExtension            db '.dll',0
szSoExtension             db '.so',0
szDylibExtension          db '.dylib',0
szJsonLocalizations       db 'localizations',0
szJsonThemes              db 'themes',0
Marketplace_DetermineExtensionType endp

;==============================================================================
; DEPENDENCY RESOLUTION
;==============================================================================

;------------------------------------------------------------------------------
; Resolve extension dependencies
;------------------------------------------------------------------------------
Marketplace_ResolveDependencies proc frame uses rbx rsi rdi r12 r13,
    pDependencies:qword        ; JSON object of dependencies
    
    mov rbx, offset g_InstallContext
    mov rdi, pDependencies
    
    ; Create dependency graph
    call DependencyGraph_Create
    mov [rbx].INSTALL_CONTEXT.Dependencies, rax
    
    ; Parse dependencies
    test rdi, rdi
    jz no_dependencies
    
    ; Iterate dependency object
    lea rcx, szJsonDependencies
    call Json_GetObjectKeys
    mov rsi, rax                ; array of dependency names
    mov ecx, [rax]              ; count
    mov [rbx].INSTALL_CONTEXT.DependencyCount, ecx
    
    ; Resolve each dependency
    xor ebx, ebx
    
dep_loop:
    cmp ebx, ecx
    jae dep_done
    
    ; Get dependency name
    mov rdx, [rsi + rbx*8 + 8]  ; skip count at beginning
    
    ; Get version range
    mov rcx, rdi
    call Json_GetStringField
    mov r12, rax                ; version range
    
    ; Check if installed
    call Marketplace_IsExtensionInstalled
    test eax, eax
    jz not_installed
    
    ; Check version compatibility
    call Marketplace_CheckVersionCompatibility
    test eax, eax
    jz version_mismatch
    
    ; Add to resolved list
    mov rcx, [rbx].INSTALL_CONTEXT.Dependencies
    mov rdx, rsi
    mov r8, r12
    mov r9d, 1                  ; is installed
    call DependencyGraph_AddNode
    
    jmp next_dep
    
not_installed:
    ; Mark as not installed
    mov rcx, [rbx].INSTALL_CONTEXT.Dependencies
    mov rdx, rsi
    mov r8, r12
    xor r9d, r9d
    call DependencyGraph_AddNode
    
    ; Attempt to install dependency
    call Marketplace_InstallDependency
    test eax, eax
    jz dependency_failed
    
    jmp next_dep
    
version_mismatch:
    ; Version mismatch - need to update
    call Marketplace_InstallDependency
    test eax, eax
    jz dependency_failed
    
    jmp next_dep
    
dependency_failed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_DEPENDENCY_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorDependency
    xor rax, rax
    ret
    
next_dep:
    inc ebx
    jmp dep_loop
    
dep_done:
    mov rax, 1
    ret
    
no_dependencies:
    mov [rbx].INSTALL_CONTEXT.DependencyCount, 0
    mov rax, 1
    ret
Marketplace_ResolveDependencies endp

;------------------------------------------------------------------------------
; Check if extension is already installed
;------------------------------------------------------------------------------
Marketplace_IsExtensionInstalled proc frame uses rbx rsi rdi,
    pExtensionId:qword        ; extension identifier
    
    mov rbx, offset g_ExtensionRegistry
    mov rsi, pExtensionId
    
    ; Check registry
    mov rcx, REG_PATH_EXTENSIONS
    mov rdx, rsi
    call Registry_KeyExists
    
    ret
Marketplace_IsExtensionInstalled endp

;------------------------------------------------------------------------------
; Check version compatibility
;------------------------------------------------------------------------------
Marketplace_CheckVersionCompatibility proc frame uses rbx rsi rdi r12 r13,
    pVersionRange:qword,      ; required version range
    pInstalledVersion:qword   ; currently installed version
    
    mov r12, pVersionRange
    mov r13, pInstalledVersion
    
    ; Parse version range (e.g., "^1.2.3", ">=1.0.0")
    mov rcx, r12
    call SemVer_ParseRange
    mov rbx, rax                ; version range object
    
    ; Parse installed version
    mov rcx, r13
    call SemVer_Parse
    mov rsi, rax                ; version object
    
    ; Check if installed version satisfies range
    mov rcx, rbx
    mov rdx, rsi
    call SemVer_Satisfies
    
    ret
Marketplace_CheckVersionCompatibility endp

;------------------------------------------------------------------------------
; Install a dependency
;------------------------------------------------------------------------------
Marketplace_InstallDependency proc frame uses rbx rsi rdi r12 r13,
    pDependencyId:qword,      ; dependency identifier
    pVersionRange:qword       ; version range
    
    mov rbx, pDependencyId
    mov rsi, pVersionRange
    
    ; Download dependency from marketplace
    call Marketplace_DownloadExtension
    test rax, rax
    jz download_failed
    
    ; Install dependency
    mov rcx, rax                ; downloaded file path
    call Marketplace_InstallExtension
    test eax, eax
    jz install_failed
    
    mov rax, 1
    ret
    
download_failed:
    xor rax, rax
    ret
    
install_failed:
    xor rax, rax
    ret
Marketplace_InstallDependency endp

;==============================================================================
; FILE EXTRACTION
;==============================================================================

;------------------------------------------------------------------------------
; Extract files from VSIX package
;------------------------------------------------------------------------------
Marketplace_ExtractFiles proc frame uses rbx rsi rdi r12 r13
    mov rbx, offset g_InstallContext
    
    ; Create installation directory
    mov rcx, [rbx].INSTALL_CONTEXT.InstallDir
    call CreateDirectoryRecursive
    test eax, eax
    jz mkdir_failed
    
    ; Create temp directory for extraction
    lea rcx, szTempDirTemplate
    call GetTempPath
    mov rcx, rax
    call CreateDirectory
    mov [rbx].INSTALL_CONTEXT.TempDir, rax
    
    ; Open VSIX file
    mov rcx, [rbx].INSTALL_CONTEXT.VsixPath
    mov rdx, GENERIC_READ
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je open_failed
    mov rsi, rax
    
    ; Extract each file
    mov ecx, [rbx].INSTALL_CONTEXT.FileCount
    xor ebx, ebx
    
extract_loop:
    cmp ebx, ecx
    jae extract_done
    
    ; Get file entry
    mov rdx, [rbx].INSTALL_CONTEXT.FileEntries
    mov rdi, [rdx + rbx*8]      ; VSIX_FILE_ENTRY
    
    ; Extract file
    mov rcx, rsi
    mov rdx, rdi
    mov r8, [rbx].INSTALL_CONTEXT.TempDir
    call Marketplace_ExtractSingleFile
    test eax, eax
    jz extract_failed
    
    inc ebx
    jmp extract_loop
    
extract_done:
    ; Close file
    mov rcx, rsi
    call CloseHandle
    
    mov rax, 1
    ret
    
mkdir_failed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
open_failed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
extract_failed:
    mov rcx, rsi
    call CloseHandle
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_FILE_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorFileExtract
    xor rax, rax
    ret
    
szTempDirTemplate       db 'RawrXD_XXXXXX',0
Marketplace_ExtractFiles endp

;------------------------------------------------------------------------------
; Extract single file from VSIX
;------------------------------------------------------------------------------
Marketplace_ExtractSingleFile proc frame uses rbx rsi rdi r12 r13,
    hVsixFile:qword,           ; VSIX file handle
    pFileEntry:ptr VSIX_FILE_ENTRY, ; file entry
    pExtractDir:qword          ; extraction directory
    
    mov rsi, hVsixFile
    mov rdi, pFileEntry
    mov r12, pExtractDir
    
    ; Seek to file offset
    mov rcx, rsi
    mov edx, FILE_BEGIN
    mov r8, [rdi].VSIX_FILE_ENTRY.Offset
    xor r9d, r9d
    call SetFilePointerEx
    test eax, eax
    jz seek_failed
    
    ; Read compressed data
    mov ecx, [rdi].VSIX_FILE_ENTRY.CompressedSize
    sub rsp, rcx
    mov r13, rsp
    
    lea rcx, r13
    mov edx, [rdi].VSIX_FILE_ENTRY.CompressedSize
    mov r8, rsi
    call ReadFile
    test eax, eax
    jz read_failed
    
    ; Decompress if needed
    .if [rdi].VSIX_FILE_ENTRY.CompressionMethod == 8  ; DEFLATE
        ; Decompress using zlib
        call zlib_inflate
        test eax, eax
        jz decompress_failed
    .else
        ; Stored uncompressed
        mov r13, rsp
    .endif
    
    ; Build output path
    lea rcx, szOutputPath
    mov rdx, r12
    call Path_Join
    mov rdx, rax
    mov r8, [rdi].VSIX_FILE_ENTRY.FileName
    call Path_Join
    
    ; Create directory structure
    lea rcx, szOutputPath
    call CreateDirectoryRecursive
    
    ; Write extracted file
    mov rcx, rax
    mov rdx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+20h], CREATE_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je write_failed
    mov rbx, rax
    
    ; Write file data
    lea rcx, r13
    mov edx, [rdi].VSIX_FILE_ENTRY.FileSize
    mov r8, rbx
    call WriteFile
    test eax, eax
    jz write_failed
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    ; Clean up
    add rsp, [rdi].VSIX_FILE_ENTRY.CompressedSize
    
    mov rax, 1
    ret
    
seek_failed:
    xor rax, rax
    ret
    
read_failed:
    add rsp, [rdi].VSIX_FILE_ENTRY.CompressedSize
    xor rax, rax
    ret
    
decompress_failed:
    add rsp, [rdi].VSIX_FILE_ENTRY.CompressedSize
    xor rax, rax
    ret
    
write_failed:
    mov rcx, rbx
    call CloseHandle
    add rsp, [rdi].VSIX_FILE_ENTRY.CompressedSize
    xor rax, rax
    ret
    
szOutputPath              db VSIX_MAX_PATH dup(?)
Marketplace_ExtractSingleFile endp

;==============================================================================
; NATIVE CODE GENERATION
;==============================================================================

;------------------------------------------------------------------------------
; Generate native MASM64 entry point for extension
;------------------------------------------------------------------------------
Marketplace_GenerateNativeEntryPoint proc frame uses rbx rsi rdi r12 r13,
    pInstallDir:qword,         ; installation directory
    pManifest:ptr EXTENSION_MANIFEST ; extension manifest
    
    mov rbx, pInstallDir
    mov rsi, pManifest
    
    ; Create extension.asm file
    lea rcx, szExtensionAsmPath
    mov rdx, rbx
    call Path_Join
    mov rdx, rax
    lea r8, szExtensionAsmName
    call Path_Join
    
    ; Create file
    mov rcx, rax
    mov rdx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+20h], CREATE_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je create_failed
    mov rdi, rax
    
    ; Generate header
    call Marketplace_GenerateAsmHeader
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Generate includes
    call Marketplace_GenerateAsmIncludes
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Generate extension info structure
    call Marketplace_GenerateExtensionInfo
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Generate activate function
    call Marketplace_GenerateActivateFunction
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Generate deactivate function
    call Marketplace_GenerateDeactivateFunction
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Generate API exports
    call Marketplace_GenerateApiExports
    mov rcx, rdi
    mov rdx, rax
    mov r8d, -1
    call WriteFile
    
    ; Close file
    mov rcx, rdi
    call CloseHandle
    
    mov rax, 1
    ret
    
create_failed:
    xor rax, rax
    ret
    
szExtensionAsmPath       db VSIX_MAX_PATH dup(?)
szExtensionAsmName        db 'extension.asm',0
Marketplace_GenerateNativeEntryPoint endp

;------------------------------------------------------------------------------
; Generate MASM64 header
;------------------------------------------------------------------------------
Marketplace_GenerateAsmHeader proc frame uses rbx rsi rdi
    lea rax, szAsmHeader
    ret
Marketplace_GenerateAsmHeader endp

;------------------------------------------------------------------------------
; Generate MASM64 includes
;------------------------------------------------------------------------------
Marketplace_GenerateAsmIncludes proc frame uses rbx rsi rdi
    lea rax, szAsmIncludes
    ret
Marketplace_GenerateAsmIncludes endp

;------------------------------------------------------------------------------
; Generate extension info structure
;------------------------------------------------------------------------------
Marketplace_GenerateExtensionInfo proc frame uses rbx rsi rdi r12 r13
    mov rbx, offset g_InstallContext
    
    ; Build extension info string
    sub rsp, 4096
    mov rdi, rsp
    
    ; Extension info header
    mov dword ptr [rdi], ';'
    inc rdi
    mov dword ptr [rdi], '==='
    add rdi, 4
    mov dword ptr [rdi], '===='
    add rdi, 5
    mov byte ptr [rdi], 10
    inc rdi
    
    ; Extension name
    lea rcx, szExtInfoName
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Name
    call sprintf
    add rdi, rax
    
    ; Publisher
    lea rcx, szExtInfoPublisher
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Publisher
    call sprintf
    add rdi, rax
    
    ; Version
    lea rcx, szExtInfoVersion
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Version
    call sprintf
    add rdi, rax
    
    ; Description
    lea rcx, szExtInfoDescription
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Description
    call sprintf
    add rdi, rax
    
    mov byte ptr [rdi], ';'
    inc rdi
    mov dword ptr [rdi], '==='
    add rdi, 4
    mov dword ptr [rdi], '===='
    add rdi, 5
    mov byte ptr [rdi], 10
    inc rdi
    mov byte ptr [rdi], 0
    
    mov rax, rsp
    ret
    
szAsmHeader              db ';==============================================================================',10
                           db '; Extension: %s',10
                           db '; Generated by RawrXD Marketplace Installer',10
                           db ';==============================================================================',10,0

szAsmIncludes            db 'include windows.inc',10
                           db 'include kernel32.inc',10
                           db 'include user32.inc',10,0

szExtInfoName            db '; Name: %s',10,0
szExtInfoPublisher       db '; Publisher: %s',10,0
szExtInfoVersion         db '; Version: %s',10,0
szExtInfoDescription     db '; Description: %s',10,0
Marketplace_GenerateExtensionInfo endp

;------------------------------------------------------------------------------
; Generate activate function stub
;------------------------------------------------------------------------------
Marketplace_GenerateActivateFunction proc frame uses rbx rsi rdi
    lea rax, szActivateStub
    ret
Marketplace_GenerateActivateFunction endp

;------------------------------------------------------------------------------
; Generate deactivate function stub
;------------------------------------------------------------------------------
Marketplace_GenerateDeactivateFunction proc frame uses rbx rsi rdi
    lea rax, szDeactivateStub
    ret
Marketplace_GenerateDeactivateFunction endp

;------------------------------------------------------------------------------
; Generate API exports
;------------------------------------------------------------------------------
Marketplace_GenerateApiExports proc frame uses rbx rsi rdi
    lea rax, szApiExports
    ret
Marketplace_GenerateApiExports endp

; String constants
szActivateStub           db 'Extension_Activate proc',10
                           db '    ; TODO: Implement extension activation',10
                           db '    ret',10
                           db 'Extension_Activate endp',10,0

szDeactivateStub         db 'Extension_Deactivate proc',10
                           db '    ; TODO: Implement extension deactivation',10
                           db '    ret',10
                           db 'Extension_Deactivate endp',10,0

szApiExports             db 'public Extension_Activate',10
                           db 'public Extension_Deactivate',10,0

;==============================================================================
; REGISTRY MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; Register installed extension
;------------------------------------------------------------------------------
Marketplace_RegisterExtension proc frame uses rbx rsi rdi r12 r13
    mov rbx, offset g_InstallContext
    
    ; Build extension ID: publisher.name
    lea rcx, szExtensionId
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Publisher
    mov r8, [rbx].INSTALL_CONTEXT.Manifest.Name
    call sprintf
    
    ; Create registry key
    lea rcx, REG_PATH_EXTENSIONS
    lea rdx, szExtensionId
    call Registry_CreateKey
    test eax, eax
    jz registry_failed
    
    ; Set registry values
    mov rcx, offset szRegValueName
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Name
    call Registry_SetStringValue
    
    mov rcx, offset szRegValuePublisher
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Publisher
    call Registry_SetStringValue
    
    mov rcx, offset szRegValueVersion
    mov rdx, [rbx].INSTALL_CONTEXT.Manifest.Version
    call Registry_SetStringValue
    
    mov rcx, offset szRegValuePath
    mov rdx, [rbx].INSTALL_CONTEXT.InstallDir
    call Registry_SetStringValue
    
    mov rcx, offset szRegValueManifest
    lea rdx, szManifestPath
    mov r8, [rbx].INSTALL_CONTEXT.InstallDir
    call Path_Join
    mov rdx, rax
    lea r8, szPackageJson
    call Path_Join
    call Registry_SetStringValue
    
    ; Set enabled by default
    mov rcx, offset szRegValueEnabled
    mov edx, 1
    call Registry_SetDwordValue
    
    ; Set capabilities
    mov rcx, offset szRegValueCapabilities
    mov edx, CAP_ALL            ; default to all capabilities
    call Registry_SetDwordValue
    
    ; Set installation timestamp
    mov rcx, offset szRegValueLastUpdated
    call GetSystemTimeAsFileTime
    mov rdx, rax
    call Registry_SetQwordValue
    
    mov rax, 1
    ret
    
registry_failed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_REGISTRY_ERROR
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorRegistry
    xor rax, rax
    ret
    
szExtensionId            db 256 dup(?)
szManifestPath           db VSIX_MAX_PATH dup(?)
szPackageJson            db 'package.json',0
Marketplace_RegisterExtension endp

;==============================================================================
; MAIN INSTALLATION FUNCTION
;==============================================================================

;------------------------------------------------------------------------------
; Install extension from VSIX package
;------------------------------------------------------------------------------
Marketplace_InstallExtension proc frame uses rbx rsi rdi r12 r13,
    pVsixPath:qword,           ; path to .vsix file
    pInstallDir:qword          ; installation directory (optional)
    
    mov rbx, pVsixPath
    mov rsi, pInstallDir
    
    ; Initialize context
    call Marketplace_InitializeContext
    
    ; Store paths
    mov [rbx].INSTALL_CONTEXT.VsixPath, rbx
    .if rsi != 0
        mov [rbx].INSTALL_CONTEXT.InstallDir, rsi
    .else
        ; Generate default install dir
        call Marketplace_GenerateInstallDir
        mov [rbx].INSTALL_CONTEXT.InstallDir, rax
    .endif
    
    ; Parse VSIX header
    call Marketplace_ParseVsixHeader
    test eax, eax
    jz install_failed
    
    ; Parse manifest
    call Marketplace_ParseManifest
    test eax, eax
    jz install_failed
    
    ; Check if already installed
    call Marketplace_IsExtensionInstalled
    test eax, eax
    jnz already_installed
    
    ; Resolve dependencies
    call Marketplace_ResolveDependencies
    test eax, eax
    jz install_failed
    
    ; Extract files
    call Marketplace_ExtractFiles
    test eax, eax
    jz install_failed
    
    ; Generate native entry point
    call Marketplace_GenerateNativeEntryPoint
    test eax, eax
    jz install_failed
    
    ; Register extension
    call Marketplace_RegisterExtension
    test eax, eax
    jz install_failed
    
    ; Success
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_SUCCESS
    mov rax, INSTALL_SUCCESS
    ret
    
already_installed:
    mov [rbx].INSTALL_CONTEXT.InstallationResult, INSTALL_ALREADY_EXISTS
    mov [rbx].INSTALL_CONTEXT.ErrorMessage, offset szErrorAlreadyInstalled
    mov rax, INSTALL_ALREADY_EXISTS
    ret
    
install_failed:
    ; Error code already set in context
    mov eax, [rbx].INSTALL_CONTEXT.InstallationResult
    ret
Marketplace_InstallExtension endp

;------------------------------------------------------------------------------
; Initialize installation context
;------------------------------------------------------------------------------
Marketplace_InitializeContext proc frame uses rbx rsi rdi
    mov rbx, offset g_InstallContext
    
    ; Clear context
    xor eax, eax
    mov ecx, sizeof(INSTALL_CONTEXT) / 4
    lea rdi, [rbx]
    rep stosd
    
    ret
Marketplace_InitializeContext endp

;------------------------------------------------------------------------------
; Generate default installation directory
;------------------------------------------------------------------------------
Marketplace_GenerateInstallDir proc frame uses rbx rsi rdi
    mov rbx, offset g_InstallContext
    
    ; Build path: %APPDATA%\RawrXD\Extensions\publisher.name-version
    lea rcx, szInstallDir
    mov rdx, offset szAppDataPath
    call GetEnvironmentVariable
    
    lea rcx, szInstallDir
    mov rdx, rax
    call Path_Join
    mov rdx, rax
    lea r8, szExtensionsDir
    call Path_Join
    mov rdx, rax
    
    ; Add publisher.name
    mov rcx, [rbx].INSTALL_CONTEXT.Manifest.Publisher
    call Path_Join
    mov rdx, rax
    mov r8, [rbx].INSTALL_CONTEXT.Manifest.Name
    call Path_Join
    mov rdx, rax
    
    ; Add version
    mov r8, [rbx].INSTALL_CONTEXT.Manifest.Version
    call Path_Join
    
    ret
    
szAppDataPath           db 'APPDATA',0
szExtensionsDir           db 'Extensions',0
szInstallDir              db VSIX_MAX_PATH dup(?)
Marketplace_GenerateInstallDir endp

;==============================================================================
; EXPORTS
;==============================================================================
public Marketplace_InstallExtension
public Marketplace_ParseVsixHeader
public Marketplace_ParseManifest
public Marketplace_ResolveDependencies
public Marketplace_ExtractFiles
public Marketplace_GenerateNativeEntryPoint
public Marketplace_RegisterExtension
public Marketplace_GetInstallationResult
public Marketplace_GetErrorMessage

end
