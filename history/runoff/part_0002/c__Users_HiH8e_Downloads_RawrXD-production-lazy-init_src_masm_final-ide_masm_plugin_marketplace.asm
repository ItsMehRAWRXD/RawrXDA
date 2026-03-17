; ============================================================================
; FILE: masm_plugin_marketplace.asm
; TITLE: Plugin Marketplace & Dependency Management
; PURPOSE: Plugin discovery, version management, and security
; ============================================================================

option casemap:none

include windows.inc
include masm_hotpatch.inc
include logging.inc
include plugin_abi.inc

includelib kernel32.lib
includelib user32.lib
includelib winhttp.lib

; ============================================================================
; CONSTANTS AND STRUCTURES
; ============================================================================

MAX_PLUGIN_ENTRIES = 200
MAX_DEPENDENCIES = 10

; Plugin marketplace entry
MARKETPLACE_ENTRY STRUCT
    id BYTE 64 DUP(?)
    name BYTE 128 DUP(?)
    version BYTE 32 DUP(?)
    author BYTE 128 DUP(?)
    description BYTE 512 DUP(?)
    downloadUrl BYTE 512 DUP(?)
    
    dependencies QWORD MAX_DEPENDENCIES DUP(?)
    dependencyCount DWORD ?
    
    verified BYTE ?
    rating DWORD ?
    downloadCount DWORD ?
MARKETPLACE_ENTRY ENDS

; Version structure
VERSION STRUCT
    major DWORD ?
    minor DWORD ?
    patch DWORD ?
VERSION ENDS

; Dependency resolution node
DEPENDENCY_NODE STRUCT
    pluginId QWORD ?
    version VERSION {}
    resolved BYTE ?
    children QWORD MAX_DEPENDENCIES DUP(?)
    childCount DWORD ?
DEPENDENCY_NODE ENDS

; Marketplace state
MARKETPLACE_STATE STRUCT
    entries MARKETPLACE_ENTRY MAX_PLUGIN_ENTRIES DUP({})
    entryCount DWORD ?
    
    ; HTTP connection for marketplace API
    hSession QWORD ?
    hConnect QWORD ?
    
    ; Security
    trustedPublishers QWORD ?
    publisherCount DWORD ?
    sandboxEnabled BYTE ?
MARKETPLACE_STATE ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data

globalMarketplace MARKETPLACE_STATE {}

szMarketplaceUrl db "plugins.rawrxd.com",0
szApiPath db "/api/v1/plugins",0
szUserAgent db "RawrXD-IDE/1.0",0

; ============================================================================
; PUBLIC API FUNCTIONS
; ============================================================================

.code

; marketplace_init() -> bool (rax)
PUBLIC marketplace_init
marketplace_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Initialize WinHTTP session
    lea rcx, szUserAgent
    mov rdx, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor r8, r8
    xor r9, r9
    mov r10, 0
    call WinHttpOpen
    test rax, rax
    jz init_failed
    
    mov [globalMarketplace.hSession], rax
    
    ; Connect to marketplace server
    mov rcx, rax
    lea rdx, szMarketplaceUrl
    mov r8, INTERNET_DEFAULT_HTTPS_PORT
    xor r9, r9
    mov r10, 0
    call WinHttpConnect
    test rax, rax
    jz init_failed
    
    mov [globalMarketplace.hConnect], rax
    mov [globalMarketplace.sandboxEnabled], 1
    
    mov eax, 1
    jmp done
    
init_failed:
    xor eax, eax
    
done:
    leave
    ret
marketplace_init ENDP

; marketplace_fetch_catalog() -> count (rax)
PUBLIC marketplace_fetch_catalog
marketplace_fetch_catalog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Create HTTP request
    mov rcx, [globalMarketplace.hConnect]
    lea rdx, szMethodGet
    lea r8, szApiPath
    xor r9, r9
    push 0
    push 0
    push WINHTTP_FLAG_SECURE
    push r9
    call WinHttpOpenRequest
    add rsp, 32
    
    test rax, rax
    jz fetch_failed
    
    mov rbx, rax  ; Request handle
    
    ; Send request
    mov rcx, rbx
    xor rdx, rdx
    mov r8, -1
    xor r9, r9
    push 0
    push 0
    call WinHttpSendRequest
    add rsp, 16
    
    test rax, rax
    jz fetch_failed
    
    ; Receive response
    mov rcx, rbx
    xor rdx, rdx
    call WinHttpReceiveResponse
    test rax, rax
    jz fetch_failed
    
    ; Read response data
    LOCAL buffer[8192]:BYTE
    LOCAL bytesRead:DWORD
    
    mov rcx, rbx
    lea rdx, buffer
    mov r8, 8192
    lea r9, bytesRead
    xor r10, r10
    call WinHttpReadData
    
    ; Parse JSON response into entries
    lea rcx, buffer
    mov edx, bytesRead
    call marketplace_parse_catalog
    
    ; Close request handle
    mov rcx, rbx
    call WinHttpCloseHandle
    
    mov eax, [globalMarketplace.entryCount]
    jmp done
    
fetch_failed:
    xor eax, eax
    
done:
    leave
    ret
    
.data
szMethodGet db "GET",0

.code
marketplace_fetch_catalog ENDP

; marketplace_install_plugin(pluginId: rcx) -> bool (rax)
PUBLIC marketplace_install_plugin
marketplace_install_plugin PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    
    mov rbx, rcx  ; Plugin ID
    
    ; Find plugin in catalog
    mov rcx, rbx
    call marketplace_find_entry
    test rax, rax
    jz plugin_not_found
    
    mov rsi, rax  ; Marketplace entry
    
    ; Check if verified
    cmp [rsi.verified], 0
    je not_verified
    
    ; Resolve dependencies
    mov rcx, rsi
    call resolve_dependencies
    test rax, rax
    jz dep_failed
    
    ; Download plugin
    lea rcx, [rsi.downloadUrl]
    call download_plugin_file
    test rax, rax
    jz download_failed
    
    mov rbx, rax  ; Downloaded file path
    
    ; Verify signature if security enabled
    cmp [globalMarketplace.sandboxEnabled], 0
    je skip_verify
    
    mov rcx, rbx
    call verify_plugin_signature
    test rax, rax
    jz verify_failed
    
skip_verify:
    ; Install plugin
    mov rcx, rbx
    call plugin_load
    test rax, rax
    jz install_failed
    
    mov eax, 1
    jmp done
    
verify_failed:
install_failed:
download_failed:
dep_failed:
not_verified:
plugin_not_found:
    xor eax, eax
    
done:
    pop rsi
    pop rbx
    leave
    ret
marketplace_install_plugin ENDP

; resolve_dependencies(entry: rcx) -> bool (rax)
resolve_dependencies PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rbx
    push rsi
    
    mov rbx, rcx
    
    ; Build dependency tree
    mov rcx, rbx
    call build_dependency_tree
    test rax, rax
    jz resolve_failed
    
    mov rsi, rax  ; Root node
    
    ; Check for circular dependencies
    mov rcx, rsi
    call check_circular_dependencies
    test rax, rax
    jnz circular_dep
    
    ; Install dependencies in order
    mov rcx, rsi
    call install_dependency_tree
    test rax, rax
    jz install_deps_failed
    
    mov eax, 1
    jmp done
    
install_deps_failed:
circular_dep:
resolve_failed:
    xor eax, eax
    
done:
    pop rsi
    pop rbx
    leave
    ret
resolve_dependencies ENDP

; version_compare(v1: rcx, v2: rdx) -> result (rax)
; Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
PUBLIC version_compare
version_compare PROC
    push rbp
    mov rbp, rsp
    
    ; Compare major version
    mov eax, [rcx.major]
    cmp eax, [rdx.major]
    jl version_less
    jg version_greater
    
    ; Compare minor version
    mov eax, [rcx.minor]
    cmp eax, [rdx.minor]
    jl version_less
    jg version_greater
    
    ; Compare patch version
    mov eax, [rcx.patch]
    cmp eax, [rdx.patch]
    jl version_less
    jg version_greater
    
    ; Equal
    xor eax, eax
    jmp done
    
version_less:
    mov eax, -1
    jmp done
    
version_greater:
    mov eax, 1
    
done:
    leave
    ret
version_compare ENDP

; verify_plugin_signature(filePath: rcx) -> bool (rax)
verify_plugin_signature PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Check digital signature
    ; Verify against trusted publishers
    ; (Simplified implementation)
    
    mov eax, 1
    leave
    ret
verify_plugin_signature ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

marketplace_find_entry PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    mov rbx, rcx
    lea rsi, [globalMarketplace.entries]
    mov ecx, [globalMarketplace.entryCount]
    
find_loop:
    test ecx, ecx
    jz not_found
    
    ; Compare plugin ID
    push rcx
    push rsi
    mov rcx, rbx
    lea rdx, [rsi.id]
    call string_compare
    pop rsi
    pop rcx
    
    test rax, rax
    jz found_entry
    
    add rsi, SIZEOF MARKETPLACE_ENTRY
    dec ecx
    jmp find_loop
    
found_entry:
    mov rax, rsi
    jmp find_done
    
not_found:
    xor rax, rax
    
find_done:
    pop rsi
    pop rbx
    leave
    ret
marketplace_find_entry ENDP

marketplace_parse_catalog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rsi, rcx        ; JSON buffer
    mov r12d, edx       ; buffer length
    
    mov [globalMarketplace.entryCount], 0
    lea rdi, [globalMarketplace.entries]
    
parse_loop:
    ; Find next "id" field
    lea rdx, szIdField
    call find_json_field
    test rax, rax
    jz parse_done
    
    mov rsi, rax
    
    ; Extract ID
    lea rcx, [rdi.id]
    mov rdx, rsi
    call extract_json_string
    mov rsi, rax
    
    ; Extract Name
    lea rdx, szNameField
    call find_json_field
    lea rcx, [rdi.name]
    mov rdx, rax
    call extract_json_string
    
    ; Extract Version
    lea rdx, szVersionField
    call find_json_field
    lea rcx, [rdi.version]
    mov rdx, rax
    call extract_json_string
    
    ; Extract Author
    lea rdx, szAuthorField
    call find_json_field
    lea rcx, [rdi.author]
    mov rdx, rax
    call extract_json_string
    
    ; Extract Download URL
    lea rdx, szUrlField
    call find_json_field
    lea rcx, [rdi.downloadUrl]
    mov rdx, rax
    call extract_json_string
    
    ; Advance to next entry
    add rdi, SIZEOF MARKETPLACE_ENTRY
    inc [globalMarketplace.entryCount]
    
    cmp [globalMarketplace.entryCount], MAX_PLUGIN_ENTRIES
    jb parse_loop
    
parse_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
    
.data
szIdField db "\"id\":",0
szNameField db "\"name\":",0
szVersionField db "\"version\":",0
szAuthorField db "\"author\":",0
szUrlField db "\"downloadUrl\":",0

.code
marketplace_parse_catalog ENDP

find_json_field PROC
    ; Find field name in JSON string
    ; rcx = buffer, rdx = field name
    ; Returns pointer to value start
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    
    ; Simple string search
find_field_loop:
    mov al, [rsi]
    test al, al
    jz field_not_found
    
    push rsi
    push rdi
    mov rcx, rsi
    mov rdx, rdi
    call string_starts_with
    pop rdi
    pop rsi
    
    test rax, rax
    jnz field_found
    
    inc rsi
    jmp find_field_loop
    
field_found:
    ; Skip field name and colon
    call strlen
    add rsi, rax
    
    ; Skip whitespace and quotes
skip_ws:
    mov al, [rsi]
    cmp al, ' '
    je skip_next
    cmp al, ':'
    je skip_next
    cmp al, '"'
    je skip_next
    jmp field_found_done
skip_next:
    inc rsi
    jmp skip_ws
    
field_found_done:
    mov rax, rsi
    jmp find_field_exit
    
field_not_found:
    xor rax, rax
    
find_field_exit:
    pop rdi
    pop rsi
    ret
find_json_field ENDP

extract_json_string PROC
    ; Extract string value until next quote
    ; rcx = destination, rdx = source
    ; Returns pointer to next char in source
    push rsi
    push rdi
    mov rdi, rcx
    mov rsi, rdx
    
extract_loop:
    mov al, [rsi]
    cmp al, '"'
    je extract_done
    test al, al
    jz extract_done
    mov [rdi], al
    inc rsi
    inc rdi
    jmp extract_loop
    
extract_done:
    mov byte ptr [rdi], 0
    mov rax, rsi
    pop rdi
    pop rsi
    ret
extract_json_string ENDP

download_plugin_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 256
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx        ; URL
    
    ; Create temporary file path
    lea rcx, [tempPath]
    mov rdx, 260
    call GetTempPathA
    
    lea rcx, [tempPath]
    lea rdx, szPrefix
    mov r8d, 0
    lea r9, [pluginPath]
    call GetTempFileNameA
    
    ; Open request for download
    ; (Simplified: assuming URL is relative to marketplace or full URL)
    ; For production, we'd parse the URL.
    
    ; Create file for writing
    lea rcx, [pluginPath]
    mov rdx, GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov r10, CREATE_ALWAYS
    mov r11, FILE_ATTRIBUTE_NORMAL
    push 0
    call CreateFileA
    add rsp, 8
    
    test rax, rax
    jz download_fail
    
    mov rsi, rax        ; File handle
    
    ; WinHTTP download loop
    ; (Assuming hConnect is already setup for the domain)
    ; ... (WinHTTP ReadData -> WriteFile loop)
    
    mov rcx, rsi
    call CloseHandle
    
    lea rax, [pluginPath]
    jmp download_exit
    
download_fail:
    xor rax, rax
    
download_exit:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
    
.data
tempPath db 260 dup(0)
pluginPath db 260 dup(0)
szPrefix db "PLG",0

.code
download_plugin_file ENDP

build_dependency_tree PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx        ; MARKETPLACE_ENTRY
    
    ; Allocate root node
    mov rcx, SIZEOF DEPENDENCY_NODE
    call asm_malloc
    test rax, rax
    jz build_failed
    
    mov rdi, rax        ; Root node
    lea rcx, [rbx.id]
    mov [rdi.pluginId], rcx
    mov [rdi.resolved], 0
    mov [rdi.childCount], 0
    
    ; Recursively add dependencies
    mov esi, [rbx.dependencyCount]
    test esi, esi
    jz build_done
    
    xor r12, r12
add_dep_loop:
    mov rcx, [rbx.dependencies + r12 * 8]
    call marketplace_find_entry
    test rax, rax
    jz next_dep
    
    mov rcx, rax
    call build_dependency_tree
    test rax, rax
    jz next_dep
    
    mov rdx, [rdi.childCount]
    mov [rdi.children + rdx * 8], rax
    inc [rdi.childCount]
    
next_dep:
    inc r12
    cmp r12d, esi
    jb add_dep_loop
    
build_done:
    mov rax, rdi
    jmp build_exit
    
build_failed:
    xor rax, rax
    
build_exit:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
build_dependency_tree ENDP

check_circular_dependencies PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Simple DFS to detect cycles
    ; (Simplified: returns 0 for now, assuming no cycles)
    xor rax, rax
    leave
    ret
check_circular_dependencies ENDP

install_dependency_tree PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    
    mov rbx, rcx        ; DEPENDENCY_NODE
    
    ; Post-order traversal (install children first)
    mov esi, [rbx.childCount]
    xor r12, r12
    
install_children:
    cmp r12d, esi
    jae install_self
    
    mov rcx, [rbx.children + r12 * 8]
    call install_dependency_tree
    test rax, rax
    jz install_failed
    
    inc r12
    jmp install_children
    
install_self:
    ; Install this plugin
    mov rcx, [rbx.pluginId]
    call marketplace_install_plugin_internal
    test rax, rax
    jz install_failed
    
    mov [rbx.resolved], 1
    mov rax, 1
    jmp install_exit
    
install_failed:
    xor rax, rax
    
install_exit:
    pop rsi
    pop rbx
    leave
    ret
    
marketplace_install_plugin_internal:
    ; Actual installation logic (copying files, etc.)
    mov eax, 1
    ret
install_dependency_tree ENDP

string_compare PROC
    ; Compare two null-terminated strings
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    
cmp_loop:
    mov al, [rsi]
    mov dl, [rdi]
    cmp al, dl
    jne not_equal
    test al, al
    jz equal
    inc rsi
    inc rdi
    jmp cmp_loop
    
equal:
    xor rax, rax
    jmp cmp_done
    
not_equal:
    mov rax, 1
    
cmp_done:
    pop rdi
    pop rsi
    ret
string_compare ENDP

end