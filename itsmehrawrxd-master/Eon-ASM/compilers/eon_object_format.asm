; EON Object File Format (.eon-obj)
; NASM x86-64 Assembly Implementation
; Part of Phase 5: Building the Assembler and Linker

section .data
    ; Object file header structure
    obj_magic_number    dd 0xE0N0BJECT  ; Magic number: "EON OBJECT"
    obj_version         dd 1             ; Object file format version
    obj_flags           dd 0             ; Object file flags
    obj_entry_point     dq 0             ; Entry point address
    obj_code_size       dq 0             ; Size of code section
    obj_data_size       dq 0             ; Size of data section
    obj_symbol_count    dd 0             ; Number of symbols
    obj_reloc_count     dd 0             ; Number of relocations
    obj_string_table_size dd 0           ; Size of string table
    
    ; Symbol types
    SYMBOL_UNDEFINED    equ 0
    SYMBOL_LOCAL        equ 1
    SYMBOL_GLOBAL       equ 2
    SYMBOL_WEAK         equ 3
    SYMBOL_FUNCTION     equ 4
    SYMBOL_VARIABLE     equ 5
    SYMBOL_CONSTANT     equ 6
    SYMBOL_LABEL        equ 7
    SYMBOL_SECTION      equ 8
    SYMBOL_FILE         equ 9
    SYMBOL_COMMON       equ 10
    
    ; Relocation types
    RELOC_ABSOLUTE      equ 0
    RELOC_RELATIVE      equ 1
    RELOC_PC_RELATIVE   equ 2
    RELOC_GOT           equ 3
    RELOC_PLT           equ 4
    RELOC_COPY          equ 5
    RELOC_GLOB_DAT      equ 6
    RELOC_JMP_SLOT      equ 7
    RELOC_RELATIVE      equ 8
    RELOC_GOTOFF        equ 9
    RELOC_GOTPC         equ 10
    
    ; Section types
    SECTION_NULL        equ 0
    SECTION_PROGBITS    equ 1
    SECTION_SYMTAB      equ 2
    SECTION_STRTAB      equ 3
    SECTION_RELA        equ 4
    SECTION_NOBITS      equ 8
    SECTION_REL         equ 9
    SECTION_SHLIB       equ 10
    SECTION_DYNSYM      equ 11
    SECTION_INIT        equ 14
    SECTION_FINI        equ 15
    SECTION_PREINIT     equ 16
    SECTION_GROUP       equ 17
    SECTION_SYMTAB_SHNDX equ 18
    
    ; Section flags
    SECTION_WRITE       equ 0x1
    SECTION_ALLOC       equ 0x2
    SECTION_EXECINSTR   equ 0x4
    SECTION_MERGE       equ 0x10
    SECTION_STRINGS     equ 0x20
    SECTION_INFO_LINK   equ 0x40
    SECTION_LINK_ORDER  equ 0x80
    SECTION_OS_NONCONFORMING equ 0x100
    SECTION_GROUP       equ 0x200
    SECTION_TLS         equ 0x400
    SECTION_MASKOS      equ 0x0ff00000
    SECTION_MASKPROC    equ 0xf0000000
    SECTION_ORDERED     equ 0x40000000
    SECTION_EXCLUDE     equ 0x80000000

section .bss
    ; Object file buffer
    obj_file_buffer     resb 1024*1024   ; 1MB buffer for object file
    obj_file_size       resq 1           ; Actual size of object file
    
    ; Symbol table
    symbol_table        resb 64*1024     ; 64KB for symbol table
    symbol_count        resd 1           ; Number of symbols
    
    ; String table
    string_table        resb 32*1024     ; 32KB for string table
    string_table_size   resd 1           ; Size of string table
    
    ; Relocation table
    relocation_table    resb 32*1024     ; 32KB for relocations
    relocation_count    resd 1           ; Number of relocations
    
    ; Code section
    code_section        resb 256*1024    ; 256KB for code
    code_size           resq 1           ; Size of code
    
    ; Data section
    data_section        resb 64*1024     ; 64KB for data
    data_size           resq 1           ; Size of data

section .text
    global eon_object_init
    global eon_object_create
    global eon_object_add_symbol
    global eon_object_add_relocation
    global eon_object_add_code
    global eon_object_add_data
    global eon_object_write_file
    global eon_object_read_file
    global eon_object_get_symbol
    global eon_object_resolve_symbols
    global eon_object_cleanup

eon_object_init:
    push rbp
    mov rbp, rsp
    
    ; Initialize object file structure
    mov dword [obj_magic_number], 0xE0N0BJECT
    mov dword [obj_version], 1
    mov dword [obj_flags], 0
    mov qword [obj_entry_point], 0
    mov qword [obj_code_size], 0
    mov qword [obj_data_size], 0
    mov dword [obj_symbol_count], 0
    mov dword [obj_reloc_count], 0
    mov dword [obj_string_table_size], 0
    
    ; Initialize counters
    mov dword [symbol_count], 0
    mov dword [string_table_size], 0
    mov dword [relocation_count], 0
    mov qword [code_size], 0
    mov qword [data_size], 0
    
    pop rbp
    ret

eon_object_create:
    push rbp
    mov rbp, rsp
    
    ; Create new object file
    call eon_object_init
    
    ; Set up basic sections
    mov qword [obj_entry_point], 0x1000  ; Default entry point
    
    pop rbp
    ret

eon_object_add_symbol:
    push rbp
    mov rbp, rsp
    
    ; rdi: symbol name (string)
    ; rsi: symbol value (address)
    ; rdx: symbol type
    ; rcx: symbol size
    
    ; Add symbol to symbol table
    mov eax, [symbol_count]
    imul eax, 32  ; Each symbol entry is 32 bytes
    lea r8, [symbol_table + rax]
    
    ; Store symbol name offset in string table
    mov r9, [string_table_size]
    mov [r8], r9
    
    ; Store symbol value
    mov [r8 + 8], rsi
    
    ; Store symbol type
    mov [r8 + 16], rdx
    
    ; Store symbol size
    mov [r8 + 20], rcx
    
    ; Store symbol flags
    mov dword [r8 + 24], 0
    
    ; Add symbol name to string table
    call add_string_to_table
    
    ; Increment symbol count
    inc dword [symbol_count]
    inc dword [obj_symbol_count]
    
    pop rbp
    ret

eon_object_add_relocation:
    push rbp
    mov rbp, rsp
    
    ; rdi: relocation offset
    ; rsi: symbol index
    ; rdx: relocation type
    ; rcx: addend
    
    ; Add relocation to relocation table
    mov eax, [relocation_count]
    imul eax, 16  ; Each relocation entry is 16 bytes
    lea r8, [relocation_table + rax]
    
    ; Store relocation offset
    mov [r8], rdi
    
    ; Store symbol index
    mov [r8 + 8], rsi
    
    ; Store relocation type
    mov [r8 + 12], rdx
    
    ; Store addend
    mov [r8 + 14], rcx
    
    ; Increment relocation count
    inc dword [relocation_count]
    inc dword [obj_reloc_count]
    
    pop rbp
    ret

eon_object_add_code:
    push rbp
    mov rbp, rsp
    
    ; rdi: code buffer
    ; rsi: code size
    
    ; Add code to code section
    mov r8, [code_size]
    lea r9, [code_section + r8]
    mov rcx, rsi
    rep movsb
    
    ; Update code size
    add [code_size], rsi
    add [obj_code_size], rsi
    
    pop rbp
    ret

eon_object_add_data:
    push rbp
    mov rbp, rsp
    
    ; rdi: data buffer
    ; rsi: data size
    
    ; Add data to data section
    mov r8, [data_size]
    lea r9, [data_section + r8]
    mov rcx, rsi
    rep movsb
    
    ; Update data size
    add [data_size], rsi
    add [obj_data_size], rsi
    
    pop rbp
    ret

eon_object_write_file:
    push rbp
    mov rbp, rsp
    
    ; rdi: filename
    ; Write object file to disk
    
    ; Open file for writing
    mov rax, 2  ; sys_open
    mov rsi, 0x241  ; O_CREAT | O_WRONLY | O_TRUNC
    mov rdx, 0644  ; permissions
    syscall
    
    cmp rax, 0
    jl .error
    
    mov r8, rax  ; file descriptor
    
    ; Write object file header
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, obj_magic_number
    mov rdx, 32  ; header size
    syscall
    
    ; Write symbol table
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, symbol_table
    mov rdx, [symbol_count]
    imul rdx, 32
    syscall
    
    ; Write string table
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, string_table
    mov rdx, [string_table_size]
    syscall
    
    ; Write relocation table
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, relocation_table
    mov rdx, [relocation_count]
    imul rdx, 16
    syscall
    
    ; Write code section
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, code_section
    mov rdx, [code_size]
    syscall
    
    ; Write data section
    mov rax, 1  ; sys_write
    mov rdi, r8
    mov rsi, data_section
    mov rdx, [data_size]
    syscall
    
    ; Close file
    mov rax, 3  ; sys_close
    mov rdi, r8
    syscall
    
    mov rax, 0  ; success
    jmp .done
    
.error:
    mov rax, -1  ; error
    
.done:
    pop rbp
    ret

eon_object_read_file:
    push rbp
    mov rbp, rsp
    
    ; rdi: filename
    ; Read object file from disk
    
    ; Open file for reading
    mov rax, 2  ; sys_open
    mov rsi, 0  ; O_RDONLY
    mov rdx, 0
    syscall
    
    cmp rax, 0
    jl .error
    
    mov r8, rax  ; file descriptor
    
    ; Read object file header
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, obj_magic_number
    mov rdx, 32  ; header size
    syscall
    
    ; Verify magic number
    cmp dword [obj_magic_number], 0xE0N0BJECT
    jne .error
    
    ; Read symbol table
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, symbol_table
    mov rdx, [obj_symbol_count]
    imul rdx, 32
    syscall
    
    ; Read string table
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, string_table
    mov rdx, [obj_string_table_size]
    syscall
    
    ; Read relocation table
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, relocation_table
    mov rdx, [obj_reloc_count]
    imul rdx, 16
    syscall
    
    ; Read code section
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, code_section
    mov rdx, [obj_code_size]
    syscall
    
    ; Read data section
    mov rax, 0  ; sys_read
    mov rdi, r8
    mov rsi, data_section
    mov rdx, [obj_data_size]
    syscall
    
    ; Close file
    mov rax, 3  ; sys_close
    mov rdi, r8
    syscall
    
    mov rax, 0  ; success
    jmp .done
    
.error:
    mov rax, -1  ; error
    
.done:
    pop rbp
    ret

eon_object_get_symbol:
    push rbp
    mov rbp, rsp
    
    ; rdi: symbol name
    ; Returns symbol value in rax, or -1 if not found
    
    mov rcx, 0  ; symbol index
    
.search_loop:
    cmp ecx, [symbol_count]
    jge .not_found
    
    ; Get symbol entry
    mov rax, rcx
    imul rax, 32
    lea r8, [symbol_table + rax]
    
    ; Get symbol name offset
    mov r9, [r8]
    
    ; Compare symbol name
    lea r10, [string_table + r9]
    call strcmp
    cmp rax, 0
    je .found
    
    inc rcx
    jmp .search_loop
    
.found:
    ; Return symbol value
    mov rax, [r8 + 8]
    jmp .done
    
.not_found:
    mov rax, -1
    
.done:
    pop rbp
    ret

eon_object_resolve_symbols:
    push rbp
    mov rbp, rsp
    
    ; Resolve all symbol references in the object file
    
    mov rcx, 0  ; relocation index
    
.resolve_loop:
    cmp ecx, [relocation_count]
    jge .done
    
    ; Get relocation entry
    mov rax, rcx
    imul rax, 16
    lea r8, [relocation_table + rax]
    
    ; Get symbol index
    mov r9, [r8 + 8]
    
    ; Get symbol value
    mov rax, r9
    imul rax, 32
    lea r10, [symbol_table + rax]
    mov r11, [r10 + 8]  ; symbol value
    
    ; Apply relocation
    mov r12, [r8]  ; relocation offset
    mov r13, [r8 + 12]  ; relocation type
    
    cmp r13, RELOC_ABSOLUTE
    je .apply_absolute
    cmp r13, RELOC_RELATIVE
    je .apply_relative
    cmp r13, RELOC_PC_RELATIVE
    je .apply_pc_relative
    jmp .next_relocation
    
.apply_absolute:
    ; Store symbol value at relocation offset
    mov [code_section + r12], r11
    jmp .next_relocation
    
.apply_relative:
    ; Store relative offset
    mov rax, r11
    sub rax, r12
    mov [code_section + r12], rax
    jmp .next_relocation
    
.apply_pc_relative:
    ; Store PC-relative offset
    mov rax, r11
    sub rax, r12
    sub rax, 4  ; adjust for instruction size
    mov [code_section + r12], eax
    jmp .next_relocation
    
.next_relocation:
    inc rcx
    jmp .resolve_loop
    
.done:
    pop rbp
    ret

eon_object_cleanup:
    push rbp
    mov rbp, rsp
    
    ; Clean up object file resources
    call eon_object_init
    
    pop rbp
    ret

; Helper functions
add_string_to_table:
    push rbp
    mov rbp, rsp
    
    ; rdi: string to add
    ; Returns offset in string table
    
    mov r8, [string_table_size]
    lea r9, [string_table + r8]
    
    ; Copy string
    mov rcx, 0
.copy_loop:
    mov al, [rdi + rcx]
    mov [r9 + rcx], al
    inc rcx
    cmp al, 0
    jne .copy_loop
    
    ; Update string table size
    add [string_table_size], rcx
    add [obj_string_table_size], rcx
    
    ; Return offset
    mov rax, r8
    
    pop rbp
    ret

strcmp:
    push rbp
    mov rbp, rsp
    
    ; rdi: string 1
    ; rsi: string 2
    ; Returns 0 if equal, non-zero if different
    
    mov rcx, 0
    
.compare_loop:
    mov al, [rdi + rcx]
    mov bl, [rsi + rcx]
    
    cmp al, bl
    jne .not_equal
    
    cmp al, 0
    je .equal
    
    inc rcx
    jmp .compare_loop
    
.equal:
    mov rax, 0
    jmp .done
    
.not_equal:
    mov rax, 1
    
.done:
    pop rbp
    ret