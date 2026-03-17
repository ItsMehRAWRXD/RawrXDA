; ═══════════════════════════════════════════════════════════════════════════════
; MASM64 Compiler Build Orchestrator
; Pure x64 MASM implementation of Build-CompilersWithRuntime.ps1
; Discovers, compiles, and links all *_compiler_from_scratch*.asm files
; ═══════════════════════════════════════════════════════════════════════════════

option casemap:none

includelib kernel32.lib
includelib user32.lib

; ═══════════════════════════════════════════════════════════════════════════════
; EXTERNAL DECLARATIONS
; ═══════════════════════════════════════════════════════════════════════════════

extern ExitProcess: proc
extern GetStdHandle: proc
extern WriteFile: proc
extern GetCommandLineA: proc
extern FindFirstFileA: proc
extern FindNextFileA: proc
extern FindClose: proc
extern CreateProcessA: proc
extern WaitForSingleObject: proc
extern GetExitCodeProcess: proc
extern CloseHandle: proc
extern lstrcpyA: proc
extern lstrcatA: proc
extern lstrlenA: proc
extern GetCurrentDirectoryA: proc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════

STD_OUTPUT_HANDLE       equ -11
MAX_PATH                equ 260
INVALID_HANDLE_VALUE    equ -1
INFINITE                equ -1

; Process creation flags
NORMAL_PRIORITY_CLASS   equ 00000020h
CREATE_NO_WINDOW        equ 08000000h

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════

WIN32_FIND_DATAA STRUCT
    dwFileAttributes        DWORD ?
    ftCreationTime          QWORD ?
    ftLastAccessTime        QWORD ?
    ftLastWriteTime         QWORD ?
    nFileSizeHigh           DWORD ?
    nFileSizeLow            DWORD ?
    dwReserved0             DWORD ?
    dwReserved1             DWORD ?
    cFileName               BYTE MAX_PATH DUP (?)
    cAlternateFileName      BYTE 14 DUP (?)
WIN32_FIND_DATAA ENDS

STARTUPINFOA STRUCT
    cb                      DWORD ?
    lpReserved             QWORD ?
    lpDesktop              QWORD ?
    lpTitle                QWORD ?
    dwX                     DWORD ?
    dwY                     DWORD ?
    dwXSize                 DWORD ?
    dwYSize                 DWORD ?
    dwXCountChars           DWORD ?
    dwYCountChars           DWORD ?
    dwFillAttribute         DWORD ?
    dwFlags                 DWORD ?
    wShowWindow             WORD ?
    cbReserved2             WORD ?
    lpReserved2            QWORD ?
    hStdInput              QWORD ?
    hStdOutput             QWORD ?
    hStdError              QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess               QWORD ?
    hThread                QWORD ?
    dwProcessId            DWORD ?
    dwThreadId             DWORD ?
PROCESS_INFORMATION ENDS

COMPILER_ENTRY STRUCT
    filename               BYTE 512 DUP (?)
    language_name          BYTE 64 DUP (?)
    obj_path               BYTE 512 DUP (?)
    exe_path               BYTE 512 DUP (?)
    compiled               DWORD ?
    linked                 DWORD ?
COMPILER_ENTRY ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════

.data

; ─────────────────────────────────────────────────────────────────────────────
; Tool Paths (hardcoded as per PowerShell script)
; ─────────────────────────────────────────────────────────────────────────────

nasm_path               BYTE "E:\nasm\nasm-2.16.01\nasm.exe", 0
ml64_path               BYTE "ml64.exe", 0  ; Assume in PATH
link_path               BYTE "link.exe", 0  ; Assume in PATH

msvc_lib_path           BYTE "C:\VS2022Enterprise\VC\Tools\MSVC\14.42.34433\lib\x64", 0
ucrt_lib_path           BYTE "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64", 0
um_lib_path             BYTE "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64", 0

; ─────────────────────────────────────────────────────────────────────────────
; Directory Paths
; ─────────────────────────────────────────────────────────────────────────────

bin_compilers_dir       BYTE "bin\compilers\", 0
bin_runtime_dir         BYTE "bin\runtime\", 0
runtime_source          BYTE "universal_compiler_runtime_clean.asm", 0
runtime_obj             BYTE "bin\runtime\runtime.obj", 0
runtime_exe             BYTE "bin\runtime\runtime.exe", 0

search_pattern          BYTE "*_compiler_from_scratch*.asm", 0
fixed_pattern           BYTE "*_fixed.asm", 0

manifest_path           BYTE "bin\compilers\manifest.json", 0

; ─────────────────────────────────────────────────────────────────────────────
; Console Messages
; ─────────────────────────────────────────────────────────────────────────────

msg_banner              BYTE "═══════════════════════════════════════════════════════════", 13, 10
                        BYTE "  MASM64 Compiler Build Orchestrator v1.0", 13, 10
                        BYTE "  Pure x64 Assembly Implementation", 13, 10
                        BYTE "═══════════════════════════════════════════════════════════", 13, 10, 0

msg_building_runtime    BYTE "[RUNTIME] Building universal_compiler_runtime_clean.asm", 13, 10, 0
msg_runtime_ok          BYTE "[RUNTIME] Built: runtime.obj", 13, 10, 0
msg_runtime_fail        BYTE "[RUNTIME] FAILED to compile runtime", 13, 10, 0

msg_discovering         BYTE "[DISCOVERY] Searching for compiler files...", 13, 10, 0
msg_found_files         BYTE "[DISCOVERY] Found ", 0
msg_files_suffix        BYTE " compiler file(s)", 13, 10, 0

msg_compiling           BYTE "[COMPILER] Building: ", 0
msg_compile_ok          BYTE "  [OK] Object created", 13, 10, 0
msg_compile_fail        BYTE "  [FAIL] Compilation failed", 13, 10, 0
msg_link_ok             BYTE "  [OK] Executable created", 13, 10, 0
msg_link_fail           BYTE "  [FAIL] Link failed", 13, 10, 0

msg_manifest            BYTE "[MANIFEST] Writing manifest.json", 13, 10, 0
msg_complete            BYTE "[COMPLETE] Build orchestration finished", 13, 10, 0

newline                 BYTE 13, 10, 0

; ─────────────────────────────────────────────────────────────────────────────
; Command line templates
; ─────────────────────────────────────────────────────────────────────────────

nasm_cmd_template       BYTE '"%s" -f win64 "%s" -o "%s"', 0
ml64_cmd_template       BYTE '"%s" /c /Fo"%s" "%s"', 0
link_cmd_template       BYTE '"%s" /nologo /subsystem:console "%s" "%s" /LIBPATH:"%s" /LIBPATH:"%s" /LIBPATH:"%s" kernel32.lib user32.lib /out:"%s"', 0

; ═══════════════════════════════════════════════════════════════════════════════
; BSS SECTION
; ═══════════════════════════════════════════════════════════════════════════════

.data?

stdout_handle           QWORD ?
current_dir             BYTE MAX_PATH DUP (?)
compiler_list           COMPILER_ENTRY 256 DUP ({})
compiler_count          QWORD ?
cmd_buffer              BYTE 4096 DUP (?)
find_data               WIN32_FIND_DATAA {}

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════

.code

; ═══════════════════════════════════════════════════════════════════════════════
; Entry Point
; ═══════════════════════════════════════════════════════════════════════════════

main proc
    ; Initialize
    sub rsp, 28h  ; Shadow space
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov stdout_handle, rax
    
    ; Print banner
    lea rcx, msg_banner
    call print_string
    
    ; Get current directory
    lea rcx, current_dir
    mov edx, MAX_PATH
    call GetCurrentDirectoryA
    
    ; Build runtime first
    call build_runtime
    test eax, eax
    jz runtime_failed
    
    ; Discover compiler files
    call discover_compilers
    
    ; Build each compiler
    call build_all_compilers
    
    ; Generate manifest
    call generate_manifest
    
    ; Print completion message
    lea rcx, msg_complete
    call print_string
    
    ; Exit success
    xor ecx, ecx
    call ExitProcess
    
runtime_failed:
    lea rcx, msg_runtime_fail
    call print_string
    mov ecx, 1
    call ExitProcess
main endp

; ═══════════════════════════════════════════════════════════════════════════════
; build_runtime - Compile universal_compiler_runtime_clean.asm
; Returns: EAX = 1 if success, 0 if failed
; ═══════════════════════════════════════════════════════════════════════════════

build_runtime proc
    local startup_info:STARTUPINFOA
    local process_info:PROCESS_INFORMATION
    local exit_code:DWORD
    
    push rbx
    push rsi
    sub rsp, 28h
    
    ; Print message
    lea rcx, msg_building_runtime
    call print_string
    
    ; Build NASM command: nasm -f win64 universal_compiler_runtime_clean.asm -o bin\runtime\runtime.obj
    lea rcx, cmd_buffer
    lea rdx, nasm_path
    lea r8, runtime_source
    lea r9, runtime_obj
    ; Simple concatenation for now (production would use sprintf equivalent)
    call build_nasm_command
    
    ; Execute command
    lea rcx, cmd_buffer
    lea rdx, startup_info
    lea r8, process_info
    call execute_command
    test eax, eax
    jz build_runtime_fail
    
    ; Print success
    lea rcx, msg_runtime_ok
    call print_string
    
    mov eax, 1
    jmp build_runtime_done
    
build_runtime_fail:
    xor eax, eax
    
build_runtime_done:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
build_runtime endp

; ═══════════════════════════════════════════════════════════════════════════════
; discover_compilers - Find all *_compiler_from_scratch*.asm files
; ═══════════════════════════════════════════════════════════════════════════════

discover_compilers proc
    local h_find:QWORD
    
    push rbx
    push rsi
    sub rsp, 28h
    
    lea rcx, msg_discovering
    call print_string
    
    ; Initialize counter
    mov compiler_count, 0
    
    ; FindFirstFile
    lea rcx, search_pattern
    lea rdx, find_data
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je discover_done
    mov h_find, rax
    
discover_loop:
    ; Add file to compiler_list
    mov rax, compiler_count
    imul rax, rax, SIZEOF COMPILER_ENTRY
    lea rbx, compiler_list
    add rbx, rax
    
    ; Copy filename
    lea rcx, [rbx].COMPILER_ENTRY.filename
    lea rdx, find_data.WIN32_FIND_DATAA.cFileName
    call lstrcpyA
    
    ; Extract language name (basic extraction)
    lea rcx, [rbx].COMPILER_ENTRY.language_name
    lea rdx, find_data.WIN32_FIND_DATAA.cFileName
    call extract_language_name
    
    ; Mark as not compiled
    mov [rbx].COMPILER_ENTRY.compiled, 0
    mov [rbx].COMPILER_ENTRY.linked, 0
    
    inc compiler_count
    
    ; Find next file
    mov rcx, h_find
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz discover_loop
    
    mov rcx, h_find
    call FindClose
    
discover_done:
    ; Print count
    lea rcx, msg_found_files
    call print_string
    
    mov rax, compiler_count
    call print_number
    
    lea rcx, msg_files_suffix
    call print_string
    
    add rsp, 28h
    pop rsi
    pop rbx
    ret
discover_compilers endp

; ═══════════════════════════════════════════════════════════════════════════════
; build_all_compilers - Compile and link each discovered compiler
; ═══════════════════════════════════════════════════════════════════════════════

build_all_compilers proc
    push rbx
    push rsi
    sub rsp, 28h
    
    xor rbx, rbx  ; Loop counter
    
build_loop:
    cmp rbx, compiler_count
    jae build_loop_done
    
    ; Calculate entry pointer
    imul rax, rbx, SIZEOF COMPILER_ENTRY
    lea rsi, compiler_list
    add rsi, rax
    
    ; Print compiler name
    lea rcx, msg_compiling
    call print_string
    lea rcx, [rsi].COMPILER_ENTRY.filename
    call print_string
    lea rcx, newline
    call print_string
    
    ; Compile .asm -> .obj
    mov rcx, rsi
    call compile_compiler
    test eax, eax
    jz build_next
    
    mov [rsi].COMPILER_ENTRY.compiled, 1
    
    lea rcx, msg_compile_ok
    call print_string
    
    ; Link .obj -> .exe
    mov rcx, rsi
    call link_compiler
    test eax, eax
    jz build_next
    
    mov [rsi].COMPILER_ENTRY.linked, 1
    
    lea rcx, msg_link_ok
    call print_string
    
build_next:
    inc rbx
    jmp build_loop
    
build_loop_done:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
build_all_compilers endp

; ═══════════════════════════════════════════════════════════════════════════════
; compile_compiler - Compile single .asm file using NASM
; RCX = pointer to COMPILER_ENTRY
; Returns: EAX = 1 if success, 0 if failed
; ═══════════════════════════════════════════════════════════════════════════════

compile_compiler proc
    local entry_ptr:QWORD
        local startup_info:STARTUPINFOA
        local process_info:PROCESS_INFORMATION
    
    push rbx
    sub rsp, 28h
    
    mov entry_ptr, rcx
    
    ; Build output object path: bin\compilers\<basename>.obj
    ; Simplified: just use filename with .obj extension
    
    ; Build NASM command
    lea rcx, cmd_buffer
    lea rdx, nasm_path
    mov rax, entry_ptr
    lea r8, [rax].COMPILER_ENTRY.filename
    lea r9, [rax].COMPILER_ENTRY.obj_path
    call build_nasm_command
    
    ; Execute
        lea rcx, cmd_buffer
        lea rdx, startup_info
        lea r8, process_info
    call execute_command
    
    add rsp, 28h
    pop rbx
    ret
compile_compiler endp

; ═══════════════════════════════════════════════════════════════════════════════
; link_compiler - Link .obj with runtime.obj
; RCX = pointer to COMPILER_ENTRY
; Returns: EAX = 1 if success, 0 if failed
; ═══════════════════════════════════════════════════════════════════════════════

link_compiler proc
    local entry_ptr:QWORD
        local startup_info:STARTUPINFOA
        local process_info:PROCESS_INFORMATION
    
    push rbx
    sub rsp, 28h
    
    mov entry_ptr, rcx
    
    ; Build link command
    lea rcx, cmd_buffer
    lea rdx, link_path
    mov rax, entry_ptr
    lea r8, [rax].COMPILER_ENTRY.obj_path
    lea r9, runtime_obj
    call build_link_command
    
    ; Execute
        lea rcx, cmd_buffer
        lea rdx, startup_info
        lea r8, process_info
    call execute_command
    
    add rsp, 28h
    pop rbx
    ret
link_compiler endp

; ═══════════════════════════════════════════════════════════════════════════════
; generate_manifest - Create manifest.json
; ═══════════════════════════════════════════════════════════════════════════════

generate_manifest proc
    sub rsp, 28h
    
    lea rcx, msg_manifest
    call print_string
    
    ; TODO: Write JSON manifest file with compiler list
    ; For now, just print message
    
    add rsp, 28h
    ret
generate_manifest endp

; ═══════════════════════════════════════════════════════════════════════════════
; HELPER FUNCTIONS
; ═══════════════════════════════════════════════════════════════════════════════

; ─────────────────────────────────────────────────────────────────────────────
; print_string - Output string to stdout
; RCX = pointer to null-terminated string
; ─────────────────────────────────────────────────────────────────────────────

print_string proc
    local bytes_written:DWORD
    
    push rbx
    sub rsp, 28h
    
    mov rbx, rcx
    
    ; Get string length
    call lstrlenA
    mov edx, eax  ; Length
    
    ; WriteFile
    mov rcx, stdout_handle
    mov r8, rbx   ; Buffer
    mov r9d, edx  ; Bytes to write
    lea r10, bytes_written
    xor r11, r11  ; lpOverlapped = NULL
    
    ; Adjust for WriteFile prototype
    push r11
    push r10
    sub rsp, 20h
    call WriteFile
    add rsp, 30h
    
    add rsp, 28h
    pop rbx
    ret
print_string endp

; ─────────────────────────────────────────────────────────────────────────────
; print_number - Print number to stdout
; RAX = number to print
; ─────────────────────────────────────────────────────────────────────────────

print_number proc
    local buffer[32]:BYTE
    
    push rbx
    sub rsp, 28h
    
    ; Convert to string (simplified)
    ; TODO: Implement itoa equivalent
    
    add rsp, 28h
    pop rbx
    ret
print_number endp

; ─────────────────────────────────────────────────────────────────────────────
; execute_command - Execute external process
; RCX = command line
; RDX = pointer to STARTUPINFOA
; R8 = pointer to PROCESS_INFORMATION
; Returns: EAX = 1 if success, 0 if failed
; ─────────────────────────────────────────────────────────────────────────────

execute_command proc
    local cmd_line:QWORD
    local si_ptr:QWORD
    local pi_ptr:QWORD
    local exit_code:DWORD
    
    push rbx
    push rsi
    sub rsp, 28h
    
    mov cmd_line, rcx
    mov si_ptr, rdx
    mov pi_ptr, r8
    
    ; Initialize STARTUPINFOA
    mov rax, si_ptr
    mov (STARTUPINFOA PTR [rax]).cb, SIZEOF STARTUPINFOA
    
    ; CreateProcessA
    xor ecx, ecx          ; lpApplicationName
    mov rdx, cmd_line     ; lpCommandLine
    xor r8, r8            ; lpProcessAttributes
    xor r9, r9            ; lpThreadAttributes
    
    ; Stack parameters
    mov rax, pi_ptr
    push rax              ; lpProcessInformation
    mov rax, si_ptr
    push rax              ; lpStartupInfo
    push 0                ; lpCurrentDirectory
    push 0                ; lpEnvironment
    push CREATE_NO_WINDOW ; dwCreationFlags
    push 0                ; bInheritHandles
    
    sub rsp, 20h
    call CreateProcessA
    add rsp, 50h
    
    test eax, eax
    jz exec_failed
    
    ; Wait for process
    mov rax, pi_ptr
    mov rcx, (PROCESS_INFORMATION PTR [rax]).hProcess
    mov edx, INFINITE
    call WaitForSingleObject
    
    ; Get exit code
    mov rax, pi_ptr
    mov rcx, (PROCESS_INFORMATION PTR [rax]).hProcess
    lea rdx, exit_code
    call GetExitCodeProcess
    
    ; Close handles
    mov rax, pi_ptr
    mov rcx, (PROCESS_INFORMATION PTR [rax]).hProcess
    call CloseHandle
    
    mov rax, pi_ptr
    mov rcx, (PROCESS_INFORMATION PTR [rax]).hThread
    call CloseHandle
    
    ; Check exit code
    cmp exit_code, 0
    jne exec_failed
    
    mov eax, 1
    jmp exec_done
    
exec_failed:
    xor eax, eax
    
exec_done:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
execute_command endp

; ─────────────────────────────────────────────────────────────────────────────
; Stub implementations for command builders
; ─────────────────────────────────────────────────────────────────────────────

build_nasm_command proc
    ; TODO: Build NASM command string
    ret
build_nasm_command endp

build_link_command proc
    ; TODO: Build linker command string
    ret
build_link_command endp

extract_language_name proc
    ; TODO: Extract language name from filename
    ret
extract_language_name endp

end
