; ==============================================================================
; IDE Extension Marketplace & Language Support System
; Multi-language extension system for NASM IDE
; ==============================================================================

BITS 64
DEFAULT REL

; ==============================================================================
; Extension Structure
; ==============================================================================

struc Extension
    .id:                resb 16         ; UUID
    .name_ptr:          resq 1          ; Extension name
    .name_len:          resq 1
    .version:           resq 1          ; Version number
    .language:          resq 1          ; Language ID (0=ASM,1=Python,2=C,3=Rust,etc)
    .entry_point:       resq 1          ; Function pointer to entry
    .capabilities:      resq 1          ; Bitmask of capabilities
    .enabled:           resb 1          ; 1=enabled, 0=disabled
    .installed:         resb 1          ; 1=installed, 0=not installed
    .author_ptr:        resq 1          ; Author name
    .description_ptr:   resq 1          ; Description
    .icon_ptr:          resq 1          ; Icon data pointer
    .config_ptr:        resq 1          ; Configuration data
    .hooks:             resq 8          ; Function hook pointers
    .size:
endstruc

; Language IDs
LANG_ASM        equ 0
LANG_PYTHON     equ 1
LANG_C          equ 2
LANG_CPP        equ 3
LANG_RUST       equ 4
LANG_GO         equ 5
LANG_JAVASCRIPT equ 6
LANG_CUSTOM     equ 999

; Capability flags
CAP_SYNTAX_HIGHLIGHT    equ (1 << 0)
CAP_CODE_COMPLETION     equ (1 << 1)
CAP_DEBUGGING           equ (1 << 2)
CAP_LINTING             equ (1 << 3)
CAP_FORMATTING          equ (1 << 4)
CAP_REFACTORING         equ (1 << 5)
CAP_BUILD_SYSTEM        equ (1 << 6)
CAP_GIT_INTEGRATION     equ (1 << 7)
CAP_MODEL_DAMPENING     equ (1 << 8)
CAP_AI_ASSIST           equ (1 << 9)

; ==============================================================================
; Marketplace Entry Structure
; ==============================================================================

struc MarketplaceEntry
    .extension_id:      resb 16
    .downloads:         resq 1
    .rating:            resq 1          ; Rating * 100 (e.g., 450 = 4.50 stars)
    .last_updated:      resq 1          ; Unix timestamp
    .size_bytes:        resq 1
    .verified:          resb 1          ; 1=verified publisher
    .category:          resq 1          ; Category ID
    .tags_ptr:          resq 1          ; Tags array pointer
    .screenshot_ptr:    resq 1          ; Screenshot data
    .download_url:      resq 1
    .size:
endstruc

section .rodata

; Extension categories
msg_languages:      db "Programming Languages", 0
msg_tools:          db "Development Tools", 0
msg_themes:         db "Themes & UI", 0
msg_ai:             db "AI & ML Tools", 0
msg_debug:          db "Debuggers", 0
msg_build:          db "Build Systems", 0

; Built-in extensions
ext_python_name:    db "Python Language Support", 0
ext_python_desc:    db "Full Python IDE features with syntax highlighting, debugging, and linting", 0

ext_c_name:         db "C/C++ Development", 0
ext_c_desc:         db "C and C++ support with GCC/Clang integration", 0

ext_rust_name:      db "Rust Language Support", 0
ext_rust_desc:      db "Rust development with Cargo integration", 0

ext_dampener_name:  db "Model Dampener", 0
ext_dampener_desc:  db "On-the-fly AI model behavior modification without retraining", 0

ext_git_name:       db "Enhanced Git Integration", 0
ext_git_desc:       db "Advanced Git features with visual diff and merge tools", 0

section .bss

; Extension registry (supports up to 256 extensions)
extension_registry:     resb (Extension.size * 256)
extension_count:        resq 1

; Marketplace cache
marketplace_cache:      resb (MarketplaceEntry.size * 1024)
marketplace_count:      resq 1

section .text

; ==============================================================================
; FUNCTION: init_extension_system
; Initialize the extension system and load built-in extensions
; ==============================================================================
global init_extension_system
init_extension_system:
    push rbp
    mov rbp, rsp
    push rbx
    
    ; Clear extension count
    xor rax, rax
    mov [rel extension_count], rax
    
    ; Register built-in extensions
    call register_builtin_extensions
    
    ; Load user-installed extensions
    call load_user_extensions
    
    ; Initialize marketplace cache
    call init_marketplace
    
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: register_builtin_extensions
; Register all built-in extensions
; ==============================================================================
register_builtin_extensions:
    push rbp
    mov rbp, rsp
    
    ; Register Python extension
    lea rdi, [ext_python_name]
    mov rsi, LANG_PYTHON
    mov rdx, CAP_SYNTAX_HIGHLIGHT | CAP_CODE_COMPLETION | CAP_DEBUGGING | CAP_LINTING
    call register_extension
    
    ; Register C/C++ extension
    lea rdi, [ext_c_name]
    mov rsi, LANG_C
    mov rdx, CAP_SYNTAX_HIGHLIGHT | CAP_CODE_COMPLETION | CAP_DEBUGGING | CAP_BUILD_SYSTEM
    call register_extension
    
    ; Register Rust extension
    lea rdi, [ext_rust_name]
    mov rsi, LANG_RUST
    mov rdx, CAP_SYNTAX_HIGHLIGHT | CAP_CODE_COMPLETION | CAP_BUILD_SYSTEM
    call register_extension
    
    ; Register Model Dampener extension
    lea rdi, [ext_dampener_name]
    mov rsi, LANG_ASM
    mov rdx, CAP_MODEL_DAMPENING | CAP_AI_ASSIST
    call register_extension
    
    ; Register Git extension
    lea rdi, [ext_git_name]
    mov rsi, LANG_ASM
    mov rdx, CAP_GIT_INTEGRATION
    call register_extension
    
    pop rbp
    ret

; ==============================================================================
; FUNCTION: register_extension
; Register a new extension
; Input:
;   RDI = extension name pointer
;   RSI = language ID
;   RDX = capabilities bitmask
; Output:
;   RAX = extension index, or -1 on error
; ==============================================================================
global register_extension
register_extension:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r8
    
    mov r12, rdi                    ; Name pointer
    mov r13, rsi                    ; Language ID
    
    ; Get next extension slot index
    mov rax, [rel extension_count]
    cmp rax, 256
    jge .error                      ; Registry full
    mov r8, rax                     ; Save index
    
    ; Calculate extension address: base + index*size
    imul rax, Extension.size        ; rax = index * size
    lea rbx, [rel extension_registry]
    add rbx, rax
    
    ; Fill extension structure
    mov [rbx + Extension.name_ptr], r12
    mov [rbx + Extension.language], r13
    mov [rbx + Extension.capabilities], rdx
    mov byte [rbx + Extension.enabled], 1
    mov byte [rbx + Extension.installed], 1
    
    ; Calculate name length
    mov rdi, r12
    call strlen
    mov [rbx + Extension.name_len], rax
    
    ; Generate UUID (simplified)
    call generate_uuid
    lea rdi, [rbx + Extension.id]
    mov rsi, rax
    call write_uuid
    
    ; Increment extension count
    mov rax, [rel extension_count]
    inc rax
    mov [rel extension_count], rax
    
    ; Return extension index
    mov rax, r8
    jmp .done
    
.error:
    mov rax, -1
    
.done:
    pop r8
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: enable_extension
; Enable an extension by index
; Input:
;   RDI = extension index
; Output:
;   RAX = 1 on success, 0 on failure
; ==============================================================================
global enable_extension
enable_extension:
    push rbp
    mov rbp, rsp
    
    ; Validate index
    mov rax, [rel extension_count]
    cmp rdi, rax
    jge .error
    
    ; Calculate extension address: rbx = base + index*size
    mov rax, rdi
    imul rax, Extension.size        ; rax = index * size
    lea rbx, [rel extension_registry]
    add rbx, rax
    
    ; Enable extension
    mov byte [rbx + Extension.enabled], 1
    
    ; Call extension entry point if it exists
    mov rax, [rbx + Extension.entry_point]
    test rax, rax
    jz .success
    call rax
.success:
    mov rax, 1
    jmp .done
.error:
    xor rax, rax
.done:
    pop rbp
    ret

; ==============================================================================
; FUNCTION: list_extensions
; List all installed extensions
; Output:
;   Prints extension list to stdout
; ==============================================================================
global list_extensions
list_extensions:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov r12, [rel extension_count]
    test r12, r12
    jz .empty
    
    xor rbx, rbx                    ; Index counter
    
.loop:
    cmp rbx, r12
    jge .done
    
    ; Calculate extension address: rax = index*size; rdx = base + offset
    mov rax, rbx
    imul rax, Extension.size
    lea rdx, [rel extension_registry]
    add rdx, rax
    
    ; Print extension info
    mov rdi, [rdx + Extension.name_ptr]
    call print_string
    
    ; Print enabled status
    cmp byte [rdx + Extension.enabled], 1
    je .enabled
    
    lea rdi, [msg_disabled]
    jmp .print_status
    
.enabled:
    lea rdi, [msg_enabled]
    
.print_status:
    call print_string
    
    ; Print newline
    mov rdi, 10
    call print_char
    
    inc rbx
    jmp .loop
    
.empty:
    lea rdi, [msg_no_extensions]
    call print_string
    
.done:
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: marketplace_search
; Search the extension marketplace
; Input:
;   RDI = search query string pointer
;   RSI = language filter (or -1 for all)
; Output:
;   RAX = number of results found
; ==============================================================================
global marketplace_search
marketplace_search:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, rdi                    ; Search query
    mov r13, rsi                    ; Language filter
    
    ; Print search header
    lea rdi, [msg_searching]
    call print_string
    
    mov rdi, r12
    call print_string
    
    mov rdi, 10
    call print_char
    
    ; Search marketplace cache
    xor rbx, rbx                    ; Result counter
    
    ; Simple search implementation
    ; Compare query against extension names and descriptions
    lea r13, [marketplace_cache]
    mov r14d, [marketplace_count]
    
.search_loop:
    test r14d, r14d
    jz .search_done
    
    ; Check if extension name contains query
    mov rdi, [r13 + MarketplaceEntry.name_ptr]
    mov rsi, r12
    call strstr                      ; Check if query is substring
    test rax, rax
    jnz .match_found
    
    ; Check description
    mov rdi, [r13 + MarketplaceEntry.desc_ptr]
    mov rsi, r12
    call strstr
    test rax, rax
    jz .next_entry
    
.match_found:
    inc rbx                          ; Increment match count
    
.next_entry:
    add r13, MarketplaceEntry.size
    dec r14d
    jmp .search_loop
    
.search_done:
    mov rax, rbx                    ; Return result count
    
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; FUNCTION: download_extension
; Download and install an extension from marketplace
; Input:
;   RDI = marketplace entry ID
; Output:
;   RAX = 1 on success, 0 on failure
; ==============================================================================
global download_extension
download_extension:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov r12, rdi                    ; Marketplace entry ID
    
    ; Print download message
    lea rdi, [msg_downloading]
    call print_string
    
    ; Validate entry ID
    cmp r12, [marketplace_count]
    jge .error
    
    ; Get marketplace entry
    lea rbx, [marketplace_cache]
    imul r13, r12, MarketplaceEntry.size
    add rbx, r13
    
    ; Download implementation:
    ; 1. Get download URL from entry
    mov rdi, [rbx + MarketplaceEntry.download_url]
    test rdi, rdi
    jz .error
    
    ; 2. Download file (HTTP GET request)
    ; For now, simulate download by copying from cache
    
    ; 3. Extract/install to extensions directory
    lea rdi, [extensions_dir]
    mov rsi, [rbx + MarketplaceEntry.name_ptr]
    call install_extension_file
    
    test rax, rax
    jz .error
    
    ; 4. Register extension
    mov rdi, [rbx + MarketplaceEntry.name_ptr]
    call register_extension
    
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax
    
.done:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; UTILITY FUNCTIONS
; ==============================================================================

strlen:
    xor rax, rax
.loop:
    cmp byte [rdi + rax], 0
    je .done
    inc rax
    jmp .loop
.done:
    ret

print_string:
    push rbp
    mov rbp, rsp
    push rdi
    call strlen
    mov rdx, rax
    pop rsi
    mov rdi, 1
    mov rax, 1
    syscall
    pop rbp
    ret

print_char:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rsp], dil
    lea rsi, [rsp]
    mov rdx, 1
    mov rdi, 1
    mov rax, 1
    syscall
    add rsp, 16
    pop rbp
    ret

generate_uuid:
    ; Simplified UUID generation
    rdrand rax
    ret

write_uuid:
    mov [rdi], rsi
    mov [rdi + 8], rsi
    ret

load_user_extensions:
    ret                             ; Stub

init_marketplace:
    ret                             ; Stub

; Substring search (strstr)
; RDI = haystack
; RSI = needle
; Returns: RAX = pointer to match or 0
strstr:
    push rbx
    push rcx
    push rdx
    
    mov rbx, rdi                    ; Haystack
    mov rcx, rsi                    ; Needle
    
    ; Get needle length
    mov rdi, rcx
    call strlen
    test rax, rax
    jz .not_found
    mov rdx, rax                    ; Needle length
    
.search_loop:
    mov al, [rbx]
    test al, al
    jz .not_found
    
    ; Compare substring
    mov rdi, rbx
    mov rsi, rcx
    mov r8, rdx
    call strncmp
    test rax, rax
    jz .found
    
    inc rbx
    jmp .search_loop
    
.found:
    mov rax, rbx
    jmp .done
    
.not_found:
    xor rax, rax
    
.done:
    pop rdx
    pop rcx
    pop rbx
    ret

; String compare n bytes
; RDI = str1
; RSI = str2
; R8 = count
; Returns: RAX = 0 if equal
strncmp:
    push rcx
    mov rcx, r8
.loop:
    test rcx, rcx
    jz .equal
    mov al, [rdi]
    mov ah, [rsi]
    cmp al, ah
    jne .not_equal
    inc rdi
    inc rsi
    dec rcx
    jmp .loop
.equal:
    xor rax, rax
    pop rcx
    ret
.not_equal:
    mov rax, 1
    pop rcx
    ret

; Install extension file
; RDI = destination directory
; RSI = extension name
; Returns: RAX = 1 on success
install_extension_file:
    push rbp
    mov rbp, rsp
    
    ; Simplified: just return success
    ; Real implementation would copy files
    mov rax, 1
    
    pop rbp
    ret

section .rodata
msg_disabled:       db " [DISABLED]", 0
msg_enabled:        db " [ENABLED]", 0
msg_no_extensions:  db "No extensions installed", 10, 0
msg_searching:      db "Searching marketplace for: ", 0
msg_downloading:    db "Downloading extension...", 10, 0
