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
    mov qword [extension_count], 0
    
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
    
    mov r12, rdi                    ; Name pointer
    mov r13, rsi                    ; Language ID
    
    ; Get next extension slot
    mov rax, [extension_count]
    cmp rax, 256
    jge .error                      ; Registry full
    
    ; Calculate extension address
    mov rbx, Extension.size
    mul rbx
    lea rbx, [extension_registry + rax]
    
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
    inc qword [extension_count]
    
    ; Return extension index
    mov rax, [extension_count]
    dec rax
    jmp .done
    
.error:
    mov rax, -1
    
.done:
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
    cmp rdi, [extension_count]
    jge .error
    
    ; Calculate extension address
    mov rax, Extension.size
    mul rdi
    lea rax, [extension_registry + rax]
    
    ; Enable extension
    mov byte [rax + Extension.enabled], 1
    
    ; Call extension entry point if it exists
    mov rbx, [rax + Extension.entry_point]
    test rbx, rbx
    jz .success
    
    call rbx                        ; Call entry point
    
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
    
    mov r12, [extension_count]
    test r12, r12
    jz .empty
    
    xor rbx, rbx                    ; Index counter
    
.loop:
    cmp rbx, r12
    jge .done
    
    ; Calculate extension address
    mov rax, Extension.size
    mul rbx
    lea rax, [extension_registry + rax]
    
    ; Print extension info
    mov rdi, [rax + Extension.name_ptr]
    call print_string
    
    ; Print enabled status
    cmp byte [rax + Extension.enabled], 1
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
    
    ; TODO: Implement actual search logic
    ; For now, just list all extensions matching language
    
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
    
    ; Print download message
    lea rdi, [msg_downloading]
    call print_string
    
    ; TODO: Implement actual download logic
    ; For now, just simulate success
    
    mov rax, 1
    
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

section .rodata
msg_disabled:       db " [DISABLED]", 0
msg_enabled:        db " [ENABLED]", 0
msg_no_extensions:  db "No extensions installed", 10, 0
msg_searching:      db "Searching marketplace for: ", 0
msg_downloading:    db "Downloading extension...", 10, 0
