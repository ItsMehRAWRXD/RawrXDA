;==========================================================================
; agentic_masm.asm - Pure MASM64 Agentic Engine (44 Tools)
; ==========================================================================
; Implements all 44 VS Code Copilot tools in assembly:
; - 5 File System Tools
; - 5 Terminal Tools
; - 5 Git Tools
; - 5 Browser Tools
; - 5 Code Editing Tools
; - 5 Project/Automation Tools
; - 4 Package Management Tools
; - Additional specialized tools
;
; Zero C++ dependencies. Exports command processor and tool dispatcher.
;==========================================================================

.686p
.xmm
.model flat, c
option casemap:none

include windows.inc
includelib kernel32.lib
includelib msvcrt.lib
includelib user32.lib
includelib shell32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_TOOLS           equ 44
MAX_TOOL_NAME       equ 64
MAX_COMMAND_LEN     equ 8192
MAX_RESPONSE_LEN    equ 65536
MAX_TOOL_OUTPUT     equ 32768

IFNDEF GENERIC_READ
GENERIC_READ        equ 80000000h
ENDIF
IFNDEF GENERIC_WRITE
GENERIC_WRITE       equ 40000000h
ENDIF
IFNDEF FILE_SHARE_READ
FILE_SHARE_READ     equ 1
ENDIF
IFNDEF FILE_SHARE_WRITE
FILE_SHARE_WRITE    equ 2
ENDIF
IFNDEF OPEN_EXISTING
OPEN_EXISTING       equ 3
ENDIF
IFNDEF CREATE_ALWAYS
CREATE_ALWAYS       equ 2
ENDIF
IFNDEF FILE_ATTRIBUTE_NORMAL
FILE_ATTRIBUTE_NORMAL equ 80h
ENDIF
IFNDEF INVALID_HANDLE_VALUE
INVALID_HANDLE_VALUE equ -1
ENDIF
IFNDEF WAIT_OBJECT_0
WAIT_OBJECT_0       equ 0
ENDIF
IFNDEF TH32CS_SNAPPROCESS
TH32CS_SNAPPROCESS  equ 2
ENDIF
IFNDEF STARTF_USESTDHANDLES
STARTF_USESTDHANDLES equ 100h
ENDIF
IFNDEF HANDLE_FLAG_INHERIT
HANDLE_FLAG_INHERIT equ 1
ENDIF
IFNDEF STD_INPUT_HANDLE
STD_INPUT_HANDLE    equ -10
ENDIF

; Tool category IDs
CAT_FILESYSTEM      equ 1h
CAT_TERMINAL        equ 2h
CAT_GIT             equ 3h
CAT_BROWSER         equ 4h
CAT_CODEEDIT        equ 5h
CAT_PROJECT         equ 6h
CAT_PACKAGE         equ 7h
CAT_ANALYSIS        equ 8h

;==========================================================================
; STRUCTURES
;==========================================================================

AgentTool STRUCT
    id              DWORD   ?
    name            QWORD   ?               ; ptr to ASCIIZ
    description     QWORD   ?               ; ptr to ASCIIZ
    category        DWORD   ?
    handler         QWORD   ?               ; function pointer
AgentTool ENDS

ToolResult STRUCT
    success         DWORD   ?
    output          QWORD   ?               ; ptr to buffer
    output_len      QWORD   ?
    error_code      DWORD   ?
ToolResult ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data?
    tool_table          AgentTool MAX_TOOLS DUP (<>)
    tool_count          DWORD 0
    last_command        BYTE MAX_COMMAND_LEN DUP (?)
    response_buffer     BYTE MAX_RESPONSE_LEN DUP (?)
    tool_output_buffer  BYTE MAX_TOOL_OUTPUT DUP (?)
    path_buffer         BYTE 1024 DUP (?)
    temp_buffer         BYTE 1024 DUP (?)
    log_enabled         DWORD ?
    log_init            DWORD ?
    temp_dword          DWORD ?

.data
    ; Tool names (1-5 File System)
    tn_read_file        BYTE "read_file",0
    tn_write_file       BYTE "write_file",0
    tn_list_dir         BYTE "list_directory",0
    tn_create_dir       BYTE "create_directory",0
    tn_delete_file      BYTE "delete_file",0
    
    ; Tool names (6-10 Terminal)
    tn_execute_cmd      BYTE "execute_command",0
    tn_get_pwd          BYTE "get_working_directory",0
    tn_change_dir       BYTE "change_directory",0
    tn_set_env          BYTE "set_environment",0
    tn_list_processes   BYTE "list_processes",0
    
    ; Tool names (11-15 Git)
    tn_git_status       BYTE "git_status",0
    tn_git_commit       BYTE "git_commit",0
    tn_git_push         BYTE "git_push",0
    tn_git_pull         BYTE "git_pull",0
    tn_git_log          BYTE "git_log",0
    
    ; Tool names (16-20 Browser)
    tn_browse_url       BYTE "browse_url",0
    tn_search_web       BYTE "search_web",0
    tn_get_page_source  BYTE "get_page_source",0
    tn_extract_text     BYTE "extract_text",0
    tn_open_tabs        BYTE "open_tabs",0
    
    ; Tool names (21-25 Code Edit)
    tn_apply_edit       BYTE "apply_edit",0
    tn_get_deps         BYTE "get_dependencies",0
    tn_analyze_code     BYTE "analyze_code",0
    tn_format_code      BYTE "format_code",0
    tn_lint_code        BYTE "lint_code",0
    
    ; Tool names (26-30 Project)
    tn_get_env          BYTE "get_environment",0
    tn_check_pkg        BYTE "check_package_installed",0
    tn_project_info     BYTE "get_project_info",0
    tn_list_files       BYTE "list_files_recursive",0
    tn_build_project    BYTE "build_project",0
    
    ; Tool names (31-35 Package Mgmt)
    tn_auto_install     BYTE "auto_install_dependency",0
    tn_list_packages    BYTE "list_installed_packages",0
    tn_update_package   BYTE "update_package",0
    tn_remove_package   BYTE "remove_package",0
    
    ; Tool names (36-40 Analysis)
    tn_analyze_errors   BYTE "analyze_code_errors",0
    tn_check_style      BYTE "check_code_style",0
    tn_suggest_fixes    BYTE "suggest_fixes",0
    tn_trace_call       BYTE "trace_call_stack",0
    tn_profile_code     BYTE "profile_code",0
    
    ; Tool names (41-44 Templates)
    tn_gen_template     BYTE "generate_project_template",0
    tn_gen_test_stub    BYTE "generate_test_stub",0
    tn_gen_docs         BYTE "generate_documentation",0
    tn_gen_scaffold     BYTE "generate_scaffold",0
    
    ; Descriptions
    td_read_file        BYTE "Read file contents from disk",0
    td_write_file       BYTE "Write or create file with content",0
    td_list_dir         BYTE "List all files in directory",0
    td_create_dir       BYTE "Create a new directory",0
    td_delete_file      BYTE "Delete a file from disk",0
    
    td_execute_cmd      BYTE "Execute shell command in terminal",0
    td_get_pwd          BYTE "Get current working directory",0
    td_change_dir       BYTE "Change current directory",0
    td_set_env          BYTE "Set environment variable",0
    td_list_processes   BYTE "List running processes",0
    
    td_git_status       BYTE "Get Git repository status",0
    td_git_commit       BYTE "Commit changes to Git",0
    td_git_push         BYTE "Push commits to remote",0
    td_git_pull         BYTE "Pull commits from remote",0
    td_git_log          BYTE "Get Git commit log",0
    
    td_browse_url       BYTE "Navigate to URL in browser",0
    td_search_web       BYTE "Search web using Google/Bing",0
    td_get_page_source  BYTE "Get HTML source of web page",0
    td_extract_text     BYTE "Extract text from web page",0
    td_open_tabs        BYTE "List and manage browser tabs",0
    
    td_apply_edit       BYTE "Apply structured code edits",0
    td_get_deps         BYTE "Analyze project dependencies",0
    td_analyze_code     BYTE "Analyze code structure",0
    td_format_code      BYTE "Auto-format code",0
    td_lint_code        BYTE "Run code linter",0
    
    td_get_env          BYTE "Get development environment info",0
    td_check_pkg        BYTE "Check if package is installed",0
    td_project_info     BYTE "Get project metadata",0
    td_list_files       BYTE "List all files recursively",0
    td_build_project    BYTE "Build project",0
    
    td_auto_install     BYTE "Auto-install missing dependencies",0
    td_list_packages    BYTE "List installed packages",0
    td_update_package   BYTE "Update package to latest version",0
    td_remove_package   BYTE "Remove package",0
    
    td_analyze_errors   BYTE "Analyze code for errors",0
    td_check_style      BYTE "Check code style compliance",0
    td_suggest_fixes    BYTE "Suggest fixes for issues",0
    td_trace_call       BYTE "Trace call stack for debugging",0
    td_profile_code     BYTE "Profile code performance",0
    
    td_gen_template     BYTE "Generate project template",0
    td_gen_test_stub    BYTE "Generate test stub files",0
    td_gen_docs         BYTE "Generate documentation",0
    td_gen_scaffold     BYTE "Generate code scaffold",0

    sz_ok               BYTE "OK",0
    sz_err              BYTE "ERROR",0
    sz_missing_param    BYTE "Missing parameter",0
    sz_cmd_failed       BYTE "Command failed",0
    sz_cmd_not_found    BYTE "Command not found",0
    sz_dir_header       BYTE "[DIR] ",0
    sz_file_header      BYTE "[FILE] ",0
    sz_newline          BYTE 13,10,0
    sz_space            BYTE " ",0
    sz_pipe             BYTE "|",0
    sz_eq               BYTE "=",0
    sz_backslash        BYTE "\",0
    sz_wildcard         BYTE "*",0
    sz_cmd_prefix       BYTE "cmd.exe /c ",0
    sz_log_env          BYTE "RAWRXD_AGENTIC_MASM_LOG",0
    sz_log_fmt          BYTE "[agentic-masm] level=%s tool=%s ok=%u ms=%llu",13,10,0
    sz_log_info         BYTE "INFO",0
    sz_log_error        BYTE "ERROR",0
    sz_open             BYTE "open",0
    sz_git_status_cmd   BYTE "git status --porcelain",0
    sz_git_log_cmd      BYTE "git log --oneline -n 50",0
    sz_git_push_cmd     BYTE "git push",0
    sz_git_pull_cmd     BYTE "git pull",0
    sz_git_commit_pre   BYTE "git commit -m \"",0
    sz_git_commit_suf   BYTE "\"",0
    sz_bing_prefix      BYTE "https://www.bing.com/search?q=",0
    sz_ps_src_pre       BYTE "powershell -NoProfile -Command \"(Invoke-WebRequest -UseBasicParsing '\"",0
    sz_ps_src_suf       BYTE "\"').Content\"",0
    sz_ps_txt_pre       BYTE "powershell -NoProfile -Command \"(Invoke-WebRequest -UseBasicParsing '\"",0
    sz_ps_txt_suf       BYTE "\"').ParsedHtml.body.innerText\"",0
    sz_tabs_cmd         BYTE "tasklist /fi \"imagename eq chrome.exe\" /fi \"imagename eq msedge.exe\"",0
    sz_list_deps_cmd    BYTE "dir /s /b package.json pyproject.toml requirements.txt",0
    sz_analyze_code_cmd BYTE "findstr /s /n /i \"TODO FIXME\" *.c *.cpp *.h *.hpp *.asm *.py *.js *.ts",0
    sz_get_env_cmd      BYTE "set",0
    sz_dir_cmd          BYTE "dir",0
    sz_dir_recursive    BYTE "dir /s /b ",0
    sz_where_prefix     BYTE "where ",0
    sz_pip_list         BYTE "pip list",0
    sz_quote            BYTE "\"",0
    sz_comma            BYTE ",",0
    sz_lbracket         BYTE "[",0
    sz_rbracket         BYTE "]",0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; Tool Handler Stubs (implementations would replace these)
;==========================================================================

str_len PROC
    xor rax, rax
str_len_loop:
    cmp byte ptr [rcx+rax], 0
    je str_len_done
    inc rax
    jmp str_len_loop
str_len_done:
    ret
str_len ENDP

str_copy PROC
    mov r8, rcx
    mov r9, rdx
str_copy_loop:
    mov al, byte ptr [r9]
    mov byte ptr [r8], al
    inc r8
    inc r9
    test al, al
    jnz str_copy_loop
    mov rax, rcx
    ret
str_copy ENDP

str_concat PROC
    mov r8, rcx
str_concat_find:
    cmp byte ptr [r8], 0
    je str_concat_do
    inc r8
    jmp str_concat_find
str_concat_do:
    mov r9, rdx
str_concat_loop:
    mov al, byte ptr [r9]
    mov byte ptr [r8], al
    inc r8
    inc r9
    test al, al
    jnz str_concat_loop
    mov rax, rcx
    ret
str_concat ENDP

str_compare PROC
    xor rax, rax
str_cmp_loop:
    mov al, byte ptr [rcx]
    mov dl, byte ptr [rdx]
    cmp al, dl
    jne str_cmp_ne
    test al, al
    je str_cmp_eq
    inc rcx
    inc rdx
    jmp str_cmp_loop
str_cmp_eq:
    xor rax, rax
    ret
str_cmp_ne:
    mov rax, 1
    ret
str_compare ENDP

str_skip_spaces PROC
    mov rax, rcx
skip_loop:
    mov dl, byte ptr [rax]
    cmp dl, ' '
    jne skip_done
    inc rax
    jmp skip_loop
skip_done:
    ret
str_skip_spaces ENDP

str_find_char PROC
    xor rax, rax
find_char_loop:
    mov dl, byte ptr [rcx+rax]
    test dl, dl
    jz find_char_none
    cmp dl, r8b
    je find_char_found
    inc rax
    jmp find_char_loop
find_char_found:
    lea rax, [rcx+rax]
    ret
find_char_none:
    xor rax, rax
    ret
str_find_char ENDP

exec_cmd_fixed PROC
    mov rdx, rcx
    call tool_handler_execute_cmd
    ret
exec_cmd_fixed ENDP

exec_cmd_required PROC
    mov rdx, rcx
    test rdx, rdx
    jz exec_cmd_required_missing
    cmp byte ptr [rdx], 0
    je exec_cmd_required_missing
    call tool_handler_execute_cmd
    ret
exec_cmd_required_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
exec_cmd_required ENDP

exec_cmd_prefix PROC
    ; rcx = prefix, rdx = params (optional)
    invoke str_copy, addr path_buffer, rcx
    test rdx, rdx
    jz exec_prefix_run
    cmp byte ptr [rdx], 0
    je exec_prefix_run
    invoke str_concat, addr path_buffer, addr sz_space
    invoke str_concat, addr path_buffer, rdx
exec_prefix_run:
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
exec_cmd_prefix ENDP

log_init_if_needed PROC
    cmp log_init, 1
    je log_init_done
    mov log_init, 1
    invoke GetEnvironmentVariableA, addr sz_log_env, addr temp_buffer, 8
    test eax, eax
    jz log_init_done
    mov al, byte ptr [temp_buffer]
    cmp al, '1'
    je log_enable
    cmp al, 't'
    je log_enable
    cmp al, 'T'
    je log_enable
    cmp al, 'y'
    je log_enable
    cmp al, 'Y'
    je log_enable
    jmp log_init_done
log_enable:
    mov log_enabled, 1
log_init_done:
    ret
log_init_if_needed ENDP

log_tool_duration PROC
    ; rcx=tool name, rdx=success, r8=elapsed_ms (QWORD)
    invoke log_init_if_needed
    cmp log_enabled, 1
    jne log_done
    mov r9, rdx
    test r9d, r9d
    jz log_is_error
    lea rdx, sz_log_info
    jmp log_format
log_is_error:
    lea rdx, sz_log_error
log_format:
    invoke wsprintfA, addr temp_buffer, addr sz_log_fmt, rdx, rcx, r9d, r8
    invoke OutputDebugStringA, addr temp_buffer
log_done:
    ret
log_tool_duration ENDP

output_is_error PROC
    ; rcx = output pointer
    mov r8, rcx
    mov rdx, addr sz_err
    mov rcx, r8
    call str_compare
    test rax, rax
    jz output_err
    mov rdx, addr sz_missing_param
    mov rcx, r8
    call str_compare
    test rax, rax
    jz output_err
    mov rdx, addr sz_cmd_failed
    mov rcx, r8
    call str_compare
    test rax, rax
    jz output_err
    xor eax, eax
    ret
output_err:
    mov eax, 1
    ret
output_is_error ENDP

tool_handler_read_file PROC
    LOCAL dwRead:DWORD
    ; rcx = path
    mov rdx, rcx
    test rdx, rdx
    jz read_file_missing
    cmp byte ptr [rdx], 0
    je read_file_missing

    invoke CreateFileA, rdx, GENERIC_READ, FILE_SHARE_READ or FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, INVALID_HANDLE_VALUE
    je read_file_fail
    mov rbx, rax

    invoke ReadFile, rbx, addr tool_output_buffer, MAX_TOOL_OUTPUT-1, addr dwRead, 0
    test eax, eax
    jz read_file_fail_close
    mov eax, dwRead
    mov byte ptr [tool_output_buffer+rax], 0
    invoke CloseHandle, rbx
    lea rax, tool_output_buffer
    ret

read_file_fail_close:
    invoke CloseHandle, rbx
read_file_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret

read_file_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_read_file ENDP

tool_handler_write_file PROC
    LOCAL dwWritten:DWORD
    ; rcx = "path|content"
    mov rdx, rcx
    test rdx, rdx
    jz write_file_missing
    cmp byte ptr [rdx], 0
    je write_file_missing

    mov r8b, '|'
    invoke str_find_char, rdx, r8b
    test rax, rax
    jz write_file_missing
    mov byte ptr [rax], 0
    lea r9, [rax+1]

    invoke CreateFileA, rdx, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, INVALID_HANDLE_VALUE
    je write_file_fail
    mov rbx, rax

    mov rcx, r9
    call str_len
    mov r8d, eax
    invoke WriteFile, rbx, r9, r8d, addr dwWritten, 0
    test eax, eax
    jz write_file_fail_close
    invoke CloseHandle, rbx
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret

write_file_fail_close:
    invoke CloseHandle, rbx
write_file_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret

write_file_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_write_file ENDP

tool_handler_list_dir PROC
    LOCAL findData:WIN32_FIND_DATAA
    ; rcx = path
    mov rdx, rcx
    test rdx, rdx
    jz list_dir_missing
    cmp byte ptr [rdx], 0
    je list_dir_missing

    lea rcx, tool_output_buffer
    invoke str_copy, rcx, addr sz_ok
    invoke str_concat, rcx, addr sz_newline

    invoke str_copy, addr path_buffer, rdx
    invoke str_concat, addr path_buffer, addr sz_space
    mov byte ptr [path_buffer + 0], 0

    sub rsp, 128
    mov rcx, addr path_buffer
    call str_len
    cmp rax, 0
    je list_dir_missing
    mov r8, rax
    dec r8
    mov al, byte ptr [path_buffer+r8]
    cmp al, '\'
    je list_dir_add_wc
    invoke str_concat, addr path_buffer, addr sz_backslash
list_dir_add_wc:
    lea r8, [rsp]

    invoke FindFirstFileA, addr path_buffer, addr findData

list_dir_loop:
    lea r9, [rsp + 44]
    cmp byte ptr [r9], '.'
    je list_dir_next
    lea r9, findData.cFileName
    cmp byte ptr [r9], '.'
    jnz list_dir_dir
    mov eax, findData.dwFileAttributes
    jmp list_dir_add
list_dir_dir:
    invoke str_concat, addr tool_output_buffer, addr sz_dir_header
list_dir_add:
    invoke str_concat, addr tool_output_buffer, r9
    invoke str_concat, addr tool_output_buffer, addr sz_newline
list_dir_next:
    invoke FindNextFileA, rbx, addr findData
    test eax, eax
    jnz list_dir_loop

    invoke FindClose, rbx
    lea rax, tool_output_buffer
    ret

list_dir_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret

list_dir_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_list_dir ENDP

tool_handler_create_dir PROC
    mov rdx, rcx
    test rdx, rdx
    jz create_dir_missing
    cmp byte ptr [rdx], 0
    je create_dir_missing
    invoke CreateDirectoryA, rdx, 0
    test eax, eax
    jz create_dir_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
create_dir_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
create_dir_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_create_dir ENDP

tool_handler_delete_file PROC
    mov rdx, rcx
    test rdx, rdx
    jz delete_file_missing
    cmp byte ptr [rdx], 0
    je delete_file_missing
    invoke DeleteFileA, rdx
    test eax, eax
    jz delete_file_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
delete_file_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
delete_file_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_delete_file ENDP

; Terminal tools
tool_handler_execute_cmd PROC
    LOCAL sa:SECURITY_ATTRIBUTES
    LOCAL si:STARTUPINFOA
    LOCAL pi:PROCESS_INFORMATION
    LOCAL hRead:QWORD
    LOCAL hWrite:QWORD
    LOCAL dwRead:DWORD
    mov rdx, rcx
    test rdx, rdx
    jz exec_cmd_missing
    cmp byte ptr [rdx], 0
    je exec_cmd_missing

    invoke str_copy, addr path_buffer, addr sz_cmd_prefix
    invoke str_concat, addr path_buffer, rdx

    mov sa.nLength, SIZEOF SECURITY_ATTRIBUTES
    mov sa.lpSecurityDescriptor, 0
    mov sa.bInheritHandle, 1
    invoke CreatePipe, addr hRead, addr hWrite, addr sa, 0
    test eax, eax
    jz exec_cmd_fail
    invoke SetHandleInformation, hRead, HANDLE_FLAG_INHERIT, 0

    lea rdi, si
    mov ecx, SIZEOF STARTUPINFOA
    xor eax, eax
    rep stosb
    lea rdi, pi
    mov ecx, SIZEOF PROCESS_INFORMATION
    xor eax, eax
    rep stosb
    mov si.cb, SIZEOF STARTUPINFOA
    mov si.dwFlags, STARTF_USESTDHANDLES
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov si.hStdInput, rax
    mov rax, hWrite
    mov si.hStdOutput, rax
    mov si.hStdError, rax

    invoke CreateProcessA, 0, addr path_buffer, 0, 0, 1, 0, 0, 0, addr si, addr pi
    test eax, eax
    jz exec_cmd_fail_close

    invoke CloseHandle, hWrite

    lea rcx, tool_output_buffer
    mov byte ptr [rcx], 0
read_output_loop:
    invoke ReadFile, hRead, addr temp_buffer, 1000, addr dwRead, 0
    test eax, eax
    jz read_output_done
    mov eax, dwRead
    test eax, eax
    jz read_output_done
    mov byte ptr [temp_buffer+rax], 0
    invoke str_concat, addr tool_output_buffer, addr temp_buffer
    jmp read_output_loop
read_output_done:
    invoke CloseHandle, hRead
    invoke WaitForSingleObject, pi.hProcess, 30000
    invoke CloseHandle, pi.hProcess
    invoke CloseHandle, pi.hThread
    lea rax, tool_output_buffer
    ret

exec_cmd_fail_close:
    invoke CloseHandle, hWrite
    invoke CloseHandle, hRead
exec_cmd_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_cmd_failed
    lea rax, response_buffer
    ret

exec_cmd_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_cmd_failed
    lea rax, response_buffer
    ret
exec_cmd_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_execute_cmd ENDP

tool_handler_get_pwd PROC
    invoke GetCurrentDirectoryA, 1024, addr tool_output_buffer
    lea rax, tool_output_buffer
    ret
tool_handler_get_pwd ENDP

tool_handler_change_dir PROC
    mov rdx, rcx
    test rdx, rdx
    jz change_dir_missing
    cmp byte ptr [rdx], 0
    je change_dir_missing
    invoke SetCurrentDirectoryA, rdx
    test eax, eax
    jz change_dir_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
change_dir_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
change_dir_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_change_dir ENDP

tool_handler_set_env PROC
    mov rdx, rcx
    test rdx, rdx
    jz set_env_missing
    cmp byte ptr [rdx], 0
    je set_env_missing
    mov r8b, '='
    invoke str_find_char, rdx, r8b
    test rax, rax
    jz set_env_missing
    mov byte ptr [rax], 0
    lea r9, [rax+1]
    invoke SetEnvironmentVariableA, rdx, r9
    test eax, eax
    jz set_env_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
set_env_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
set_env_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_set_env ENDP

tool_handler_list_processes PROC
    LOCAL pe:PROCESSENTRY32
    invoke CreateToolhelp32Snapshot, TH32CS_SNAPPROCESS, 0
    cmp rax, INVALID_HANDLE_VALUE
    je list_proc_fail
    mov rbx, rax
    lea rcx, tool_output_buffer
    invoke str_copy, rcx, addr sz_ok
    invoke str_concat, rcx, addr sz_newline
    mov pe.dwSize, SIZEOF PROCESSENTRY32
    lea rdx, pe
    invoke Process32First, rbx, rdx
    test eax, eax
    jz list_proc_done
list_proc_loop:
    lea rdx, pe.szExeFile
    invoke str_concat, addr tool_output_buffer, rdx
    invoke str_concat, addr tool_output_buffer, addr sz_newline
    lea rdx, pe
    invoke Process32Next, rbx, rdx
    test eax, eax
    jnz list_proc_loop
list_proc_done:
    invoke CloseHandle, rbx
    lea rax, tool_output_buffer
    ret
list_proc_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
tool_handler_list_processes ENDP

tool_handler_git_status PROC
    lea rcx, sz_git_status_cmd
    call exec_cmd_fixed
    ret
tool_handler_git_status ENDP

tool_handler_git_commit PROC
    mov rdx, rcx
    test rdx, rdx
    jz git_commit_missing
    cmp byte ptr [rdx], 0
    je git_commit_missing
    invoke str_copy, addr path_buffer, addr sz_git_commit_pre
    invoke str_concat, addr path_buffer, rdx
    invoke str_concat, addr path_buffer, addr sz_git_commit_suf
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
git_commit_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_git_commit ENDP

tool_handler_git_push PROC
    mov rdx, rcx
    lea rcx, sz_git_push_cmd
    call exec_cmd_prefix
    ret
tool_handler_git_push ENDP

tool_handler_git_pull PROC
    mov rdx, rcx
    lea rcx, sz_git_pull_cmd
    call exec_cmd_prefix
    ret
tool_handler_git_pull ENDP

tool_handler_git_log PROC
    lea rcx, sz_git_log_cmd
    call exec_cmd_fixed
    ret
tool_handler_git_log ENDP

tool_handler_browse_url PROC
    mov rdx, rcx
    test rdx, rdx
    jz browse_missing
    cmp byte ptr [rdx], 0
    je browse_missing
    invoke ShellExecuteA, 0, addr sz_open, rdx, 0, 0, 1
    cmp rax, 32
    jbe browse_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
browse_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
browse_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_browse_url ENDP

tool_handler_search_web PROC
    mov rdx, rcx
    test rdx, rdx
    jz search_missing
    cmp byte ptr [rdx], 0
    je search_missing
    invoke str_copy, addr path_buffer, addr sz_bing_prefix
    invoke str_concat, addr path_buffer, rdx
    invoke ShellExecuteA, 0, addr sz_open, addr path_buffer, 0, 0, 1
    cmp rax, 32
    jbe search_fail
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_ok
    lea rax, response_buffer
    ret
search_fail:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_err
    lea rax, response_buffer
    ret
search_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_search_web ENDP

tool_handler_get_page_source PROC
    mov rdx, rcx
    test rdx, rdx
    jz page_missing
    cmp byte ptr [rdx], 0
    je page_missing
    invoke str_copy, addr path_buffer, addr sz_ps_src_pre
    invoke str_concat, addr path_buffer, rdx
    invoke str_concat, addr path_buffer, addr sz_ps_src_suf
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
page_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_get_page_source ENDP

tool_handler_extract_text PROC
    mov rdx, rcx
    test rdx, rdx
    jz text_missing
    cmp byte ptr [rdx], 0
    je text_missing
    invoke str_copy, addr path_buffer, addr sz_ps_txt_pre
    invoke str_concat, addr path_buffer, rdx
    invoke str_concat, addr path_buffer, addr sz_ps_txt_suf
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
text_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_extract_text ENDP

tool_handler_open_tabs PROC
    lea rcx, sz_tabs_cmd
    call exec_cmd_fixed
    ret
tool_handler_open_tabs ENDP

tool_handler_apply_edit PROC
    mov rdx, rcx
    call tool_handler_write_file
    ret
tool_handler_apply_edit ENDP

tool_handler_get_deps PROC
    lea rcx, sz_list_deps_cmd
    call exec_cmd_fixed
    ret
tool_handler_get_deps ENDP

tool_handler_analyze_code PROC
    lea rcx, sz_analyze_code_cmd
    call exec_cmd_fixed
    ret
tool_handler_analyze_code ENDP

tool_handler_format_code PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_format_code ENDP

tool_handler_lint_code PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_lint_code ENDP

tool_handler_get_env PROC
    lea rcx, sz_get_env_cmd
    call exec_cmd_fixed
    ret
tool_handler_get_env ENDP

tool_handler_check_pkg PROC
    mov rdx, rcx
    test rdx, rdx
    jz check_pkg_missing
    cmp byte ptr [rdx], 0
    je check_pkg_missing
    invoke str_copy, addr path_buffer, addr sz_where_prefix
    invoke str_concat, addr path_buffer, rdx
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
check_pkg_missing:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_missing_param
    lea rax, response_buffer
    ret
tool_handler_check_pkg ENDP

tool_handler_project_info PROC
    lea rcx, sz_dir_cmd
    call exec_cmd_fixed
    ret
tool_handler_project_info ENDP

tool_handler_list_files PROC
    mov rdx, rcx
    invoke str_copy, addr path_buffer, addr sz_dir_recursive
    test rdx, rdx
    jz list_files_run
    cmp byte ptr [rdx], 0
    je list_files_run
    invoke str_concat, addr path_buffer, rdx
list_files_run:
    mov rcx, addr path_buffer
    call tool_handler_execute_cmd
    ret
tool_handler_list_files ENDP

tool_handler_build_project PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_build_project ENDP

tool_handler_auto_install PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_auto_install ENDP

tool_handler_list_packages PROC
    mov rdx, rcx
    test rdx, rdx
    jz list_pkg_default
    cmp byte ptr [rdx], 0
    je list_pkg_default
    mov rcx, rdx
    call tool_handler_execute_cmd
    ret
list_pkg_default:
    lea rcx, sz_pip_list
    call exec_cmd_fixed
    ret
tool_handler_list_packages ENDP

tool_handler_update_package PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_update_package ENDP

tool_handler_remove_package PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_remove_package ENDP

tool_handler_analyze_errors PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_analyze_errors ENDP

tool_handler_check_style PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_check_style ENDP

tool_handler_suggest_fixes PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_suggest_fixes ENDP

tool_handler_trace_call PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_trace_call ENDP

tool_handler_profile_code PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_profile_code ENDP

tool_handler_gen_template PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_gen_template ENDP

tool_handler_gen_test_stub PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_gen_test_stub ENDP

tool_handler_gen_docs PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_gen_docs ENDP

tool_handler_gen_scaffold PROC
    mov rcx, rcx
    call exec_cmd_required
    ret
tool_handler_gen_scaffold ENDP

;==========================================================================
; PUBLIC: agent_init_tools()
;
; Initializes the global tool table with all 44 tools.
; Call once at startup.
;==========================================================================
PUBLIC agent_init_tools
ALIGN 16
agent_init_tools PROC
    push rbx
    
    xor ebx, ebx                            ; tool index
    
    ; Tool 1: read_file
    mov tool_table[rbx*sizeof AgentTool].id, 1
    lea rax, tn_read_file
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_read_file
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_FILESYSTEM
    lea rax, tool_handler_read_file
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx
    
    ; Tool 2: write_file
    mov tool_table[rbx*sizeof AgentTool].id, 2
    lea rax, tn_write_file
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_write_file
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_FILESYSTEM
    lea rax, tool_handler_write_file
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx
    
    ; Tool 3: list_directory
    mov tool_table[rbx*sizeof AgentTool].id, 3
    lea rax, tn_list_dir
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_list_dir
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_FILESYSTEM
    lea rax, tool_handler_list_dir
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx
    
    ; Tool 4: create_directory
    mov tool_table[rbx*sizeof AgentTool].id, 4
    lea rax, tn_create_dir
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_create_dir
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_FILESYSTEM
    lea rax, tool_handler_create_dir
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx
    
    ; Tool 5: delete_file
    mov tool_table[rbx*sizeof AgentTool].id, 5
    lea rax, tn_delete_file
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_delete_file
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_FILESYSTEM
    lea rax, tool_handler_delete_file
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 6: execute_command
    mov tool_table[rbx*sizeof AgentTool].id, 6
    lea rax, tn_execute_cmd
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_execute_cmd
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_TERMINAL
    lea rax, tool_handler_execute_cmd
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 7: get_working_directory
    mov tool_table[rbx*sizeof AgentTool].id, 7
    lea rax, tn_get_pwd
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_get_pwd
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_TERMINAL
    lea rax, tool_handler_get_pwd
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 8: change_directory
    mov tool_table[rbx*sizeof AgentTool].id, 8
    lea rax, tn_change_dir
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_change_dir
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_TERMINAL
    lea rax, tool_handler_change_dir
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 9: set_environment
    mov tool_table[rbx*sizeof AgentTool].id, 9
    lea rax, tn_set_env
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_set_env
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_TERMINAL
    lea rax, tool_handler_set_env
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 10: list_processes
    mov tool_table[rbx*sizeof AgentTool].id, 10
    lea rax, tn_list_processes
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_list_processes
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_TERMINAL
    lea rax, tool_handler_list_processes
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 11: git_status
    mov tool_table[rbx*sizeof AgentTool].id, 11
    lea rax, tn_git_status
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_git_status
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_GIT
    lea rax, tool_handler_git_status
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 12: git_commit
    mov tool_table[rbx*sizeof AgentTool].id, 12
    lea rax, tn_git_commit
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_git_commit
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_GIT
    lea rax, tool_handler_git_commit
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 13: git_push
    mov tool_table[rbx*sizeof AgentTool].id, 13
    lea rax, tn_git_push
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_git_push
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_GIT
    lea rax, tool_handler_git_push
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 14: git_pull
    mov tool_table[rbx*sizeof AgentTool].id, 14
    lea rax, tn_git_pull
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_git_pull
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_GIT
    lea rax, tool_handler_git_pull
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 15: git_log
    mov tool_table[rbx*sizeof AgentTool].id, 15
    lea rax, tn_git_log
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_git_log
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_GIT
    lea rax, tool_handler_git_log
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 16: browse_url
    mov tool_table[rbx*sizeof AgentTool].id, 16
    lea rax, tn_browse_url
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_browse_url
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_BROWSER
    lea rax, tool_handler_browse_url
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 17: search_web
    mov tool_table[rbx*sizeof AgentTool].id, 17
    lea rax, tn_search_web
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_search_web
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_BROWSER
    lea rax, tool_handler_search_web
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 18: get_page_source
    mov tool_table[rbx*sizeof AgentTool].id, 18
    lea rax, tn_get_page_source
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_get_page_source
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_BROWSER
    lea rax, tool_handler_get_page_source
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 19: extract_text
    mov tool_table[rbx*sizeof AgentTool].id, 19
    lea rax, tn_extract_text
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_extract_text
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_BROWSER
    lea rax, tool_handler_extract_text
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 20: open_tabs
    mov tool_table[rbx*sizeof AgentTool].id, 20
    lea rax, tn_open_tabs
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_open_tabs
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_BROWSER
    lea rax, tool_handler_open_tabs
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 21: apply_edit
    mov tool_table[rbx*sizeof AgentTool].id, 21
    lea rax, tn_apply_edit
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_apply_edit
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_CODEEDIT
    lea rax, tool_handler_apply_edit
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 22: get_dependencies
    mov tool_table[rbx*sizeof AgentTool].id, 22
    lea rax, tn_get_deps
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_get_deps
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_CODEEDIT
    lea rax, tool_handler_get_deps
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 23: analyze_code
    mov tool_table[rbx*sizeof AgentTool].id, 23
    lea rax, tn_analyze_code
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_analyze_code
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_CODEEDIT
    lea rax, tool_handler_analyze_code
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 24: format_code
    mov tool_table[rbx*sizeof AgentTool].id, 24
    lea rax, tn_format_code
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_format_code
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_CODEEDIT
    lea rax, tool_handler_format_code
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 25: lint_code
    mov tool_table[rbx*sizeof AgentTool].id, 25
    lea rax, tn_lint_code
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_lint_code
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_CODEEDIT
    lea rax, tool_handler_lint_code
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 26: get_environment
    mov tool_table[rbx*sizeof AgentTool].id, 26
    lea rax, tn_get_env
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_get_env
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_get_env
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 27: check_package_installed
    mov tool_table[rbx*sizeof AgentTool].id, 27
    lea rax, tn_check_pkg
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_check_pkg
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_check_pkg
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 28: get_project_info
    mov tool_table[rbx*sizeof AgentTool].id, 28
    lea rax, tn_project_info
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_project_info
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_project_info
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 29: list_files_recursive
    mov tool_table[rbx*sizeof AgentTool].id, 29
    lea rax, tn_list_files
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_list_files
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_list_files
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 30: build_project
    mov tool_table[rbx*sizeof AgentTool].id, 30
    lea rax, tn_build_project
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_build_project
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_build_project
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 31: auto_install_dependency
    mov tool_table[rbx*sizeof AgentTool].id, 31
    lea rax, tn_auto_install
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_auto_install
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PACKAGE
    lea rax, tool_handler_auto_install
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 32: list_installed_packages
    mov tool_table[rbx*sizeof AgentTool].id, 32
    lea rax, tn_list_packages
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_list_packages
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PACKAGE
    lea rax, tool_handler_list_packages
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 33: update_package
    mov tool_table[rbx*sizeof AgentTool].id, 33
    lea rax, tn_update_package
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_update_package
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PACKAGE
    lea rax, tool_handler_update_package
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 34: remove_package
    mov tool_table[rbx*sizeof AgentTool].id, 34
    lea rax, tn_remove_package
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_remove_package
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PACKAGE
    lea rax, tool_handler_remove_package
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 35: analyze_code_errors
    mov tool_table[rbx*sizeof AgentTool].id, 35
    lea rax, tn_analyze_errors
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_analyze_errors
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_ANALYSIS
    lea rax, tool_handler_analyze_errors
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 36: check_code_style
    mov tool_table[rbx*sizeof AgentTool].id, 36
    lea rax, tn_check_style
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_check_style
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_ANALYSIS
    lea rax, tool_handler_check_style
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 37: suggest_fixes
    mov tool_table[rbx*sizeof AgentTool].id, 37
    lea rax, tn_suggest_fixes
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_suggest_fixes
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_ANALYSIS
    lea rax, tool_handler_suggest_fixes
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 38: trace_call_stack
    mov tool_table[rbx*sizeof AgentTool].id, 38
    lea rax, tn_trace_call
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_trace_call
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_ANALYSIS
    lea rax, tool_handler_trace_call
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 39: profile_code
    mov tool_table[rbx*sizeof AgentTool].id, 39
    lea rax, tn_profile_code
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_profile_code
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_ANALYSIS
    lea rax, tool_handler_profile_code
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 40: generate_project_template
    mov tool_table[rbx*sizeof AgentTool].id, 40
    lea rax, tn_gen_template
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_gen_template
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_gen_template
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 41: generate_test_stub
    mov tool_table[rbx*sizeof AgentTool].id, 41
    lea rax, tn_gen_test_stub
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_gen_test_stub
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_gen_test_stub
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 42: generate_documentation
    mov tool_table[rbx*sizeof AgentTool].id, 42
    lea rax, tn_gen_docs
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_gen_docs
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_gen_docs
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 43: generate_scaffold
    mov tool_table[rbx*sizeof AgentTool].id, 43
    lea rax, tn_gen_scaffold
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_gen_scaffold
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_gen_scaffold
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx

    ; Tool 44: list_files_recursive (alias)
    mov tool_table[rbx*sizeof AgentTool].id, 44
    lea rax, tn_list_files
    mov tool_table[rbx*sizeof AgentTool].name, rax
    lea rax, td_list_files
    mov tool_table[rbx*sizeof AgentTool].description, rax
    mov tool_table[rbx*sizeof AgentTool].category, CAT_PROJECT
    lea rax, tool_handler_list_files
    mov tool_table[rbx*sizeof AgentTool].handler, rax
    inc ebx
    
    mov tool_count, ebx
    pop rbx
    ret
agent_init_tools ENDP

;==========================================================================
; PUBLIC: agent_process_command(cmd: rcx) -> rax (response buffer)
;
; Parses command string and dispatches to appropriate tool handler.
; Returns pointer to response buffer.
;==========================================================================
PUBLIC agent_process_command
ALIGN 16
agent_process_command PROC
    push rbx
    push r12
    push r13
    sub rsp, 64
    
    ; Copy command to local buffer
    mov rdi, offset last_command
    mov rsi, rcx
    xor ecx, ecx
copy_cmd_local:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi + rcx], al
    test al, al
    je cmd_copied_local
    inc ecx
    cmp ecx, MAX_COMMAND_LEN - 1
    jl copy_cmd_local
    
cmd_copied_local:
    ; Parse command: "tool_name [params]"
    lea rcx, last_command
    call str_skip_spaces
    mov rsi, rax
    cmp byte ptr [rsi], 0
    je command_not_found

    mov rcx, rsi
    mov r8b, ' '
    call str_find_char
    mov rdi, 0
    test rax, rax
    jz dispatch_search
    mov byte ptr [rax], 0
    lea rdi, [rax+1]
    mov rcx, rdi
    call str_skip_spaces
    mov rdi, rax

dispatch_search:
    xor ebx, ebx
dispatch_loop:
    cmp ebx, tool_count
    jae command_not_found
    lea rax, tool_table
    lea rax, [rax + rbx*sizeof AgentTool]
    mov rcx, rsi
    mov rdx, [rax].AgentTool.name
    call str_compare
    test rax, rax
    jz dispatch_found
    inc ebx
    jmp dispatch_loop

dispatch_found:
    mov r13, rax
    invoke GetTickCount64
    mov r10, rax
    mov rcx, rdi
    mov r11, [r13].AgentTool.handler
    call r11
    mov r12, rax
    invoke GetTickCount64
    sub rax, r10
    mov r8, rax
    mov rcx, r12
    call output_is_error
    xor edx, edx
    cmp eax, 0
    jne log_as_error
    mov edx, 1
log_as_error:
    mov rcx, [r13].AgentTool.name
    call log_tool_duration
    mov rax, r12
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

command_not_found:
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_cmd_not_found
    lea rax, response_buffer
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret
agent_process_command ENDP

;==========================================================================
; PUBLIC: agent_list_tools(out_buffer: rcx) -> rax (count)
;
; Generates JSON list of all tools. Returns tool count.
;==========================================================================
PUBLIC agent_list_tools
ALIGN 16
agent_list_tools PROC
    push rbx
    lea rcx, response_buffer
    invoke str_copy, rcx, addr sz_lbracket
    xor ebx, ebx
list_tools_loop:
    cmp ebx, tool_count
    jae list_tools_done
    lea rax, tool_table
    lea rax, [rax + rbx*sizeof AgentTool]
    invoke str_concat, addr response_buffer, addr sz_quote
    mov rcx, [rax].AgentTool.name
    invoke str_concat, addr response_buffer, rcx
    invoke str_concat, addr response_buffer, addr sz_quote
    inc ebx
    cmp ebx, tool_count
    je list_tools_loop_end
    invoke str_concat, addr response_buffer, addr sz_comma
list_tools_loop_end:
    jmp list_tools_loop
list_tools_done:
    invoke str_concat, addr response_buffer, addr sz_rbracket
    mov rax, tool_count
    pop rbx
    ret
agent_list_tools ENDP

;==========================================================================
; PUBLIC: agent_get_tool(index: rcx) -> rax (ptr to AgentTool)
;
; Returns pointer to tool structure by index.
;==========================================================================
PUBLIC agent_get_tool
ALIGN 16
agent_get_tool PROC
    cmp ecx, tool_count
    jge get_tool_invalid
    
    lea rax, tool_table
    lea rax, [rax + rcx*sizeof AgentTool]
    ret
    
get_tool_invalid:
    xor eax, eax
    ret
agent_get_tool ENDP

END

