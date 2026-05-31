; RawrXD_MCPServer.asm
; Pure x64 MASM MCP JSON-RPC server over stdio.

OPTION CASEMAP:NONE

EXTERN GetStdHandle   : PROC
EXTERN ReadFile       : PROC
EXTERN WriteFile      : PROC
EXTERN ExitProcess    : PROC
EXTERN CreateFileA    : PROC
EXTERN CloseHandle    : PROC
EXTERN FindFirstFileA : PROC
EXTERN FindNextFileA  : PROC
EXTERN FindClose      : PROC

STD_INPUT_HANDLE  EQU -10
STD_OUTPUT_HANDLE EQU -11
GENERIC_READ      EQU 80000000h
FILE_SHARE_READ   EQU 1
OPEN_EXISTING     EQU 3

.DATA
in_buf            DB 8192 DUP(0)
out_buf           DB 8192 DUP(0)
method_buf        DB 128 DUP(0)
id_buf            DB 64 DUP(0)
name_buf          DB 128 DUP(0)
text_buf          DB 4096 DUP(0)
scan_key_buf      DB 128 DUP(0)
num_buf           DB 32 DUP(0)
bytes_read        DQ 0
bytes_written     DQ 0
pending_bytes     DQ 0

hdr_prefix        DB 'Content-Length: ',0
hdr_end           DB 13,10,13,10,0
payload_delim     DB 13,10,13,10,0
cl_key            DB 'Content-Length:',0

k_id              DB 'id',0
k_method          DB 'method',0
k_name            DB 'name',0
k_text            DB 'text',0

m_initialize      DB 'initialize',0
m_initialized     DB 'notifications/initialized',0
m_tools_list      DB 'tools/list',0
m_tools_call      DB 'tools/call',0
m_shutdown        DB 'shutdown',0
m_exit            DB 'exit',0
n_echo            DB 'echo',0
n_read_file       DB 'read_file',0
n_list_dir        DB 'list_dir',0
k_path            DB 'path',0
err_open          DB 'ERR: cannot open file',0
err_read          DB 'ERR: read failed',0
err_dir           DB 'ERR: cannot list directory',0
path_buf          DB 512 DUP(0)
find_data         DB 328 DUP(0)
dir_out           DB 4096 DUP(0)
file_rd_bytes     DQ 0

json_prefix       DB '{"jsonrpc":"2.0","id":',0
suffix_init       DB ',"result":{"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"rawrxd-masm-mcp","version":"2.0"}}}',0
suffix_tools      DB ',"result":{"tools":['
                  DB '{"name":"echo","description":"Echoes input text","inputSchema":{"type":"object","properties":{"text":{"type":"string"}}}},'
                  DB '{"name":"read_file","description":"Read file contents","inputSchema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}},'
                  DB '{"name":"list_dir","description":"List directory contents","inputSchema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}'
                  DB ']}}',0
suffix_echo_head  DB ',"result":{"content":[{"type":"text","text":"',0
suffix_echo_tail  DB '"}]}}',0
suffix_unknown    DB ',"error":{"code":-32601,"message":"unknown method"}}',0
suffix_bad_tool   DB ',"error":{"code":-32601,"message":"unknown tool"}}',0
suffix_shutdown   DB ',"result":{"shutdown":"ok"}}',0

.CODE

str_len PROC
    xor rax, rax
@@:
    cmp BYTE PTR [rcx+rax], 0
    je  @F
    inc rax
    jmp @B
@@:
    ret
str_len ENDP

streq PROC
    ; rcx=s1, rdx=s2, eax=1 equal else 0
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
@@:
    mov al, BYTE PTR [rsi]
    mov dl, BYTE PTR [rdi]
    cmp al, dl
    jne neq
    test al, al
    je streq_eq
    inc rsi
    inc rdi
    jmp @B
streq_eq:
    mov eax, 1
    jmp done
neq:
    xor eax, eax
done:
    pop rdi
    pop rsi
    ret
streq ENDP

copy_mem PROC
    ; rcx=dst, rdx=src, r8=len, returns rax=dst+len
    test r8, r8
    jz short copy_done
copy_loop:
    mov al, BYTE PTR [rdx]
    mov BYTE PTR [rcx], al
    inc rcx
    inc rdx
    dec r8
    jnz copy_loop
copy_done:
    mov rax, rcx
    ret
copy_mem ENDP

skip_ws_ptr PROC
    ; rcx=ptr, returns rax=ptr after ws
    mov rax, rcx
@@:
    mov dl, BYTE PTR [rax]
    cmp dl, ' '
    je ws
    cmp dl, 9
    je ws
    cmp dl, 10
    je ws
    cmp dl, 13
    je ws
    ret
ws:
    inc rax
    jmp @B
skip_ws_ptr ENDP

parse_json_string_copy PROC
    ; rcx=in ptr at quote, rdx=out, r8=cap, returns rax=next ptr or 0 on fail
    push rbx
    push rsi
    push rdi

    test rcx, rcx
    jz ps_fail
    cmp BYTE PTR [rcx], '"'
    jne ps_fail

    lea rsi, [rcx+1]
    mov rdi, rdx
    xor ebx, ebx

ps_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz ps_fail
    cmp al, '"'
    je ps_done
    cmp al, 92
    jne ps_plain

    ; escaped char
    inc rsi
    mov al, BYTE PTR [rsi]
    test al, al
    jz ps_fail

    cmp al, '"'
    je ps_emit
    cmp al, 92
    je ps_emit
    cmp al, '/'
    je ps_emit
    cmp al, 'b'
    jne ps_chkf
    mov al, 8
    jmp ps_emit
ps_chkf:
    cmp al, 'f'
    jne ps_chkn
    mov al, 12
    jmp ps_emit
ps_chkn:
    cmp al, 'n'
    jne ps_chkr
    mov al, 10
    jmp ps_emit
ps_chkr:
    cmp al, 'r'
    jne ps_chkt
    mov al, 13
    jmp ps_emit
ps_chkt:
    cmp al, 't'
    jne ps_chku
    mov al, 9
    jmp ps_emit
ps_chku:
    cmp al, 'u'
    jne ps_emit
    ; keep parser forward-only: skip unicode hex and emit '?'
    add rsi, 4
    mov al, '?'

ps_emit:
    mov r10d, ebx
    add r10d, 1
    cmp r10d, r8d
    jae ps_skip_store
    mov BYTE PTR [rdi+rbx], al
    inc ebx
ps_skip_store:
    inc rsi
    jmp ps_loop

ps_plain:
    mov r10d, ebx
    add r10d, 1
    cmp r10d, r8d
    jae ps_skip_plain
    mov BYTE PTR [rdi+rbx], al
    inc ebx
ps_skip_plain:
    inc rsi
    jmp ps_loop

ps_done:
    cmp r8d, 0
    jle ps_ret
    mov r10d, ebx
    cmp r10d, r8d
    jb ps_term_ok
    mov r10d, r8d
    dec r10d
ps_term_ok:
    mov BYTE PTR [rdi+r10], 0
ps_ret:
    lea rax, [rsi+1]
    jmp ps_exit

ps_fail:
    xor rax, rax

ps_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
parse_json_string_copy ENDP

parse_json_scalar_copy PROC
    ; rcx=in ptr (not quote), rdx=out, r8=cap, returns rax=next ptr or 0
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    xor ebx, ebx

    mov al, BYTE PTR [rsi]
    test al, al
    jz psc_fail

psc_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz psc_done
    cmp al, ','
    je psc_done
    cmp al, '}'
    je psc_done
    cmp al, ']'
    je psc_done
    cmp al, ' '
    je psc_done
    cmp al, 9
    je psc_done
    cmp al, 10
    je psc_done
    cmp al, 13
    je psc_done

    mov r10d, ebx
    add r10d, 1
    cmp r10d, r8d
    jae psc_skip_store
    mov BYTE PTR [rdi+rbx], al
    inc ebx
psc_skip_store:
    inc rsi
    jmp psc_loop

psc_done:
    cmp r8d, 0
    jle psc_ret
    mov r10d, ebx
    cmp r10d, r8d
    jb psc_term_ok
    mov r10d, r8d
    dec r10d
psc_term_ok:
    mov BYTE PTR [rdi+r10], 0
psc_ret:
    mov rax, rsi
    jmp psc_exit

psc_fail:
    xor rax, rax

psc_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
parse_json_scalar_copy ENDP

extract_json_key_value PROC
    ; rcx=json payload, rdx=key literal, r8=out, r9=out cap, eax=1 found else 0
    push rbx
    push rsi
    push rdi

    push r12
    push r13
    push r14

    mov rsi, rcx
    mov r12, rdx
    mov r13, r8
    mov r14, r9

ej_scan:
    mov al, BYTE PTR [rsi]
    test al, al
    jz ej_not_found

    cmp al, '"'
    jne ej_next

    ; parse candidate key into scan_key_buf
    mov rcx, rsi
    lea rdx, [scan_key_buf]
    mov r8, 128
    call parse_json_string_copy
    test rax, rax
    jz ej_not_found
    mov rdi, rax

    ; compare key name
    lea rcx, [scan_key_buf]
    mov rdx, r12
    call streq
    test eax, eax
    jz ej_advance

    ; found key: expect : then parse value
    mov rcx, rdi
    call skip_ws_ptr
    mov rsi, rax
    cmp BYTE PTR [rsi], ':'
    jne ej_advance
    inc rsi
    mov rcx, rsi
    call skip_ws_ptr
    mov rsi, rax

    ; parse value into caller output
    cmp BYTE PTR [rsi], '"'
    jne ej_scalar
    mov rcx, rsi
    mov rdx, r13
    mov r8,  r14
    call parse_json_string_copy
    test rax, rax
    jz ej_not_found
    mov eax, 1
    jmp ej_done

ej_scalar:
    mov rcx, rsi
    mov rdx, r13
    mov r8,  r14
    call parse_json_scalar_copy
    test rax, rax
    jz ej_not_found
    mov eax, 1
    jmp ej_done

ej_advance:
    ; move to char after this parsed string
    mov rsi, rdi
    jmp ej_scan

ej_next:
    inc rsi
    jmp ej_scan

ej_not_found:
    xor eax, eax

ej_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
extract_json_key_value ENDP

find_payload_start PROC
    ; rcx=input buffer, returns rax=payload ptr or 0
    push rsi
    mov rsi, rcx
fps_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz fps_fail
    cmp BYTE PTR [rsi], 13
    jne fps_next
    cmp BYTE PTR [rsi+1], 10
    jne fps_next
    cmp BYTE PTR [rsi+2], 13
    jne fps_next
    cmp BYTE PTR [rsi+3], 10
    jne fps_next
    lea rax, [rsi+4]
    jmp fps_done
fps_next:
    inc rsi
    jmp fps_loop
fps_fail:
    xor rax, rax
fps_done:
    pop rsi
    ret
find_payload_start ENDP

find_header_end_bounded PROC
    ; rcx=start, rdx=end(exclusive), returns rax=payload_start or 0
    push rsi
    mov rsi, rcx
fh_loop:
    lea r8, [rsi+3]
    cmp r8, rdx
    jae fh_fail
    cmp BYTE PTR [rsi], 13
    jne fh_next
    cmp BYTE PTR [rsi+1], 10
    jne fh_next
    cmp BYTE PTR [rsi+2], 13
    jne fh_next
    cmp BYTE PTR [rsi+3], 10
    jne fh_next
    lea rax, [rsi+4]
    jmp fh_done
fh_next:
    inc rsi
    jmp fh_loop
fh_fail:
    xor rax, rax
fh_done:
    pop rsi
    ret
find_header_end_bounded ENDP

parse_content_length_bounded PROC
    ; rcx=frame_start, rdx=header_end(payload_start), returns rax=len or 0
    push rbx
    push rsi
    push rdi

    mov rsi, rcx

pcl_scan:
    cmp rsi, rdx
    jae pcl_fail
    cmp BYTE PTR [rsi], 'C'
    jne pcl_adv

    ; Compare "Content-Length:" (15 chars)
    lea rdi, [cl_key]
    mov rbx, rsi
    mov r8d, 15
pcl_cmp:
    cmp rbx, rdx
    jae pcl_fail
    mov al, BYTE PTR [rbx]
    mov r10b, BYTE PTR [rdi]
    cmp al, r10b
    jne pcl_adv
    inc rbx
    inc rdi
    dec r8d
    jnz pcl_cmp

    ; Skip spaces after colon
    mov rsi, rbx
pcl_ws:
    cmp rsi, rdx
    jae pcl_fail
    cmp BYTE PTR [rsi], ' '
    jne pcl_num_start
    inc rsi
    jmp pcl_ws

pcl_num_start:
    xor rax, rax
    xor r9d, r9d
pcl_num:
    cmp rsi, rdx
    jae pcl_num_done
    mov bl, BYTE PTR [rsi]
    cmp bl, '0'
    jb pcl_num_done
    cmp bl, '9'
    ja pcl_num_done
    imul rax, rax, 10
    movzx r8, bl
    sub r8, '0'
    add rax, r8
    inc rsi
    inc r9d
    jmp pcl_num

pcl_num_done:
    test r9d, r9d
    jz pcl_fail
    jmp pcl_done

pcl_adv:
    inc rsi
    jmp pcl_scan

pcl_fail:
    xor rax, rax

pcl_done:
    pop rdi
    pop rsi
    pop rbx
    ret
parse_content_length_bounded ENDP

u64_to_ascii PROC
    ; rax=value, rdx=dest, returns eax=len
    ; Fixed: uses stack-local scratch to avoid aliasing when dest == num_buf.
    push rbx
    push rsi
    push rdi
    sub rsp, 32             ; stack-local scratch for reversed digits

    mov rdi, rdx            ; dest (caller's output buffer)
    lea rsi, [rsp]          ; scratch on stack (NOT num_buf)
    xor ecx, ecx

    test rax, rax
    jnz u_conv
    mov BYTE PTR [rdi], '0'
    mov BYTE PTR [rdi+1], 0
    mov eax, 1
    jmp u_done

u_conv:
    xor edx, edx
    mov rbx, 10
u_loop:
    div rbx
    add dl, '0'
    mov BYTE PTR [rsi+rcx], dl
    inc ecx
    xor edx, edx
    test rax, rax
    jnz u_loop

    xor ebx, ebx
u_rev:
    cmp ebx, ecx
    jae u_rev_done
    mov r9d, ecx
    dec r9d
    sub r9d, ebx
    mov al, BYTE PTR [rsi+r9]
    mov BYTE PTR [rdi+rbx], al
    inc ebx
    jmp u_rev
u_rev_done:
    mov BYTE PTR [rdi+rbx], 0
    mov eax, ebx

u_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
u64_to_ascii ENDP

json_escape_copy PROC
    ; rcx=dst, rdx=src, r8=max_out, returns rax=end ptr
    push rbx
    push rsi
    push rdi

    mov rdi, rcx
    mov rsi, rdx
    xor ebx, ebx

jec_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz jec_done

    cmp ebx, r8d
    jae jec_done

    cmp al, '"'
    je jec_q
    cmp al, 92
    je jec_bs
    cmp al, 10
    je jec_n
    cmp al, 13
    je jec_r
    cmp al, 9
    je jec_t
    cmp al, 32
    jb jec_ctrl

    mov BYTE PTR [rdi], al
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_q:
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], '"'
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_bs:
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_n:
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 'n'
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_r:
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 'r'
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_t:
    mov BYTE PTR [rdi], 92
    inc rdi
    inc ebx
    cmp ebx, r8d
    jae jec_done
    mov BYTE PTR [rdi], 't'
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_ctrl:
    mov BYTE PTR [rdi], '?'
    inc rdi
    inc ebx
    inc rsi
    jmp jec_loop

jec_done:
    mov rax, rdi
    pop rdi
    pop rsi
    pop rbx
    ret
json_escape_copy ENDP

; ---------------------------------------------------------------------------
; exec_read_file — reads file at path_buf, stores content in text_buf
; ---------------------------------------------------------------------------
exec_read_file PROC
    push rbx
    push r12
    sub rsp, 56                     ; shadow(32) + 3 extra args (24) + align

    ; CreateFileA(path_buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 128, NULL)
    lea rcx, [path_buf]
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp+32], OPEN_EXISTING
    mov QWORD PTR [rsp+40], 128     ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+48], 0
    call CreateFileA
    cmp rax, -1
    je erf_fail

    mov rbx, rax                    ; file handle

    ; ReadFile(handle, text_buf, 3900, &file_rd_bytes, NULL)
    mov rcx, rbx
    lea rdx, [text_buf]
    mov r8d, 3900
    lea r9, [file_rd_bytes]
    mov QWORD PTR [rsp+32], 0
    call ReadFile
    mov r12d, eax                   ; save success flag

    ; Null-terminate whatever was read
    mov rax, QWORD PTR [file_rd_bytes]
    cmp rax, 3900
    jbe erf_clamp_ok
    mov rax, 3900
erf_clamp_ok:
    lea rcx, [text_buf]
    mov BYTE PTR [rcx+rax], 0

    ; CloseHandle
    mov rcx, rbx
    call CloseHandle

    test r12d, r12d
    jz erf_fail

    add rsp, 56
    pop r12
    pop rbx
    ret

erf_fail:
    ; Copy error message into text_buf
    lea rcx, [err_open]
    call str_len
    mov r8, rax
    lea rcx, [text_buf]
    lea rdx, [err_open]
    call copy_mem
    mov BYTE PTR [rax], 0

    add rsp, 56
    pop r12
    pop rbx
    ret
exec_read_file ENDP

; ---------------------------------------------------------------------------
; exec_list_dir — lists directory at path_buf, stores names in text_buf
;   Builds search pattern: path_buf + "\\*"
;   Walks FindFirstFileA / FindNextFileA, appends each cFileName + newline
; ---------------------------------------------------------------------------
exec_list_dir PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 56                     ; shadow(32) + align with 4 pushes

    ; Build dir_pattern = path_buf + "\\*"
    lea rcx, [path_buf]
    call str_len
    mov r12, rax                    ; path length

    lea rcx, [dir_out]
    lea rdx, [path_buf]
    mov r8, r12
    call copy_mem                   ; rax = dir_out + pathlen

    ; Append backslash + star + null
    mov BYTE PTR [rax], 5Ch         ; backslash
    mov BYTE PTR [rax+1], '*'
    mov BYTE PTR [rax+2], 0

    ; FindFirstFileA(dir_out, &find_data)
    lea rcx, [dir_out]
    lea rdx, [find_data]
    call FindFirstFileA
    cmp rax, -1
    je eld_fail

    mov rbx, rax                    ; find handle
    xor r13d, r13d                  ; output offset into text_buf
    mov r14d, 3900                  ; max output bytes

eld_entry:
    ; cFileName is at offset 44 in WIN32_FIND_DATAA
    lea rcx, [find_data+44]
    call str_len
    mov r8, rax                     ; name length

    ; Check if it fits
    lea eax, [r13d+r8d+1]
    cmp eax, r14d
    ja eld_close

    ; Copy filename
    lea rcx, [text_buf]
    add rcx, r13
    lea rdx, [find_data+44]
    call copy_mem
    ; rax = text_buf + old_r13 + namelen

    ; Append newline after the copied name
    mov BYTE PTR [rax], 10          ; LF
    lea rcx, [text_buf]
    sub rax, rcx                    ; rax = old_r13 + namelen
    lea r13d, [eax+1]              ; r13 = offset past LF

eld_next:
    ; FindNextFileA(handle, &find_data)
    mov rcx, rbx
    lea rdx, [find_data]
    call FindNextFileA
    test eax, eax
    jnz eld_entry

eld_close:
    ; Null-terminate
    lea rcx, [text_buf]
    mov BYTE PTR [rcx+r13], 0

    ; FindClose
    mov rcx, rbx
    call FindClose

    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

eld_fail:
    lea rcx, [err_dir]
    call str_len
    mov r8, rax
    lea rcx, [text_buf]
    lea rdx, [err_dir]
    call copy_mem
    mov BYTE PTR [rax], 0

    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
exec_list_dir ENDP

build_payload PROC
    ; rcx=method string, rdx=payload_out, returns rax=len
    push rbx
    push rsi
    push rdi

    mov rbx, rcx
    mov rdi, rdx
    mov r10, rdx

    ; prefix
    lea rsi, [json_prefix]
    mov rcx, rsi
    call str_len
    mov r8, rax
    mov rcx, rdi
    mov rdx, rsi
    call copy_mem
    mov rdi, rax

    ; id
    lea rsi, [id_buf]
    mov rcx, rsi
    call str_len
    mov r8, rax
    mov rcx, rdi
    mov rdx, rsi
    call copy_mem
    mov rdi, rax

    ; method switch
    lea rdx, [m_initialize]
    mov rcx, rbx
    call streq
    test eax, eax
    jz bp_chk_list
    lea rsi, [suffix_init]
    jmp bp_copy_tail

bp_chk_list:
    lea rdx, [m_tools_list]
    mov rcx, rbx
    call streq
    test eax, eax
    jz bp_chk_call
    lea rsi, [suffix_tools]
    jmp bp_copy_tail

bp_chk_call:
    lea rdx, [m_shutdown]
    mov rcx, rbx
    call streq
    test eax, eax
    jz bp_chk_exit
    lea rsi, [suffix_shutdown]
    jmp bp_copy_tail

bp_chk_exit:
    lea rdx, [m_exit]
    mov rcx, rbx
    call streq
    test eax, eax
    jz bp_chk_tool_call
    lea rsi, [suffix_shutdown]
    jmp bp_copy_tail

bp_chk_tool_call:
    lea rdx, [m_tools_call]
    mov rcx, rbx
    call streq
    test eax, eax
    jz bp_unknown

    ; tools/call: check known tool names
    lea rdx, [n_echo]
    lea rcx, [name_buf]
    call streq
    test eax, eax
    jnz bp_do_tool

    lea rdx, [n_read_file]
    lea rcx, [name_buf]
    call streq
    test eax, eax
    jnz bp_do_tool

    lea rdx, [n_list_dir]
    lea rcx, [name_buf]
    call streq
    test eax, eax
    jnz bp_do_tool

    jmp bp_bad_tool

bp_do_tool:
    ; head
    lea rsi, [suffix_echo_head]
    mov rcx, rsi
    call str_len
    mov r8, rax
    mov rcx, rdi
    mov rdx, rsi
    call copy_mem
    mov rdi, rax

    ; escaped dynamic text
    mov rcx, rdi
    lea rdx, [text_buf]
    mov r8, 2000
    call json_escape_copy
    mov rdi, rax

    ; tail
    lea rsi, [suffix_echo_tail]
    mov rcx, rsi
    call str_len
    mov r8, rax
    mov rcx, rdi
    mov rdx, rsi
    call copy_mem
    mov rdi, rax
    jmp bp_finish

bp_bad_tool:
    lea rsi, [suffix_bad_tool]
    jmp bp_copy_tail

bp_unknown:
    lea rsi, [suffix_unknown]

bp_copy_tail:
    mov rcx, rsi
    call str_len
    mov r8, rax
    mov rcx, rdi
    mov rdx, rsi
    call copy_mem
    mov rdi, rax

bp_finish:
    mov BYTE PTR [rdi], 0
    mov rax, rdi
    sub rax, r10

    pop rdi
    pop rsi
    pop rbx
    ret
build_payload ENDP

write_framed PROC
    ; rcx=hOut, rdx=payload_ptr, r8=payload_len
    push rbx
    push rsi
    push rdi
    sub rsp, 40

    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8

    ; header prefix
    lea rcx, [hdr_prefix]
    call str_len
    mov r8, rax
    mov rcx, rbx
    lea rdx, [hdr_prefix]
    lea r9, [bytes_written]
    mov QWORD PTR [rsp+32], 0
    call WriteFile

    ; payload length digits
    mov rax, rdi
    lea rdx, [num_buf]
    call u64_to_ascii
    mov r8, rax
    mov rcx, rbx
    lea rdx, [num_buf]
    lea r9, [bytes_written]
    mov QWORD PTR [rsp+32], 0
    call WriteFile

    ; header end
    lea rcx, [hdr_end]
    call str_len
    mov r8, rax
    mov rcx, rbx
    lea rdx, [hdr_end]
    lea r9, [bytes_written]
    mov QWORD PTR [rsp+32], 0
    call WriteFile

    ; payload
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    lea r9, [bytes_written]
    mov QWORD PTR [rsp+32], 0
    call WriteFile

    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
write_framed ENDP

PUBLIC RawrXD_MCPServer_Entry
RawrXD_MCPServer_Entry PROC
    sub rsp, 40

    mov rcx, STD_INPUT_HANDLE
    call GetStdHandle
    mov r12, rax

    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r13, rax

main_loop:
    mov rax, QWORD PTR [pending_bytes]
    cmp rax, 8191
    jb read_more
    xor rax, rax
    mov QWORD PTR [pending_bytes], 0

read_more:
    mov rcx, r12
    lea rdx, [in_buf]
    add rdx, rax
    mov r8, 8191
    sub r8, rax
    lea r9, [bytes_read]
    mov QWORD PTR [rsp+32], 0
    call ReadFile
    test eax, eax
    jz exit_ok

    mov r10, QWORD PTR [pending_bytes]
    mov rax, QWORD PTR [bytes_read]
    add rax, r10
    mov QWORD PTR [bytes_read], rax
    test rax, rax
    jz exit_ok

    lea rdx, [in_buf]
    mov BYTE PTR [rdx+rax], 0

    lea r15, [in_buf]
    lea r11, [in_buf]
    add r11, rax

frame_loop:
    cmp r15, r11
    jae no_tail

    mov rcx, r15
    mov rdx, r11
    call find_header_end_bounded
    test rax, rax
    jz store_tail
    mov r14, rax

    mov rcx, r15
    mov rdx, r14
    call parse_content_length_bounded
    test rax, rax
    jz store_tail

    lea r9, [r14+rax]
    cmp r9, r11
    ja store_tail
    mov QWORD PTR [rsp+24], r9

    ; defaults
    mov BYTE PTR [id_buf], '0'
    mov BYTE PTR [id_buf+1], 0
    mov BYTE PTR [method_buf], 0
    mov BYTE PTR [name_buf], 0
    mov BYTE PTR [text_buf], 0

    mov al, BYTE PTR [r9]
    mov BYTE PTR [rsp+16], al
    mov BYTE PTR [r9], 0

    ; id
    mov rcx, r14
    lea rdx, [k_id]
    lea r8, [id_buf]
    mov r9, 64
    call extract_json_key_value

    ; method
    mov rcx, r14
    lea rdx, [k_method]
    lea r8, [method_buf]
    mov r9, 128
    call extract_json_key_value

    ; name (for tools/call)
    mov rcx, r14
    lea rdx, [k_name]
    lea r8, [name_buf]
    mov r9, 128
    call extract_json_key_value

    ; text (for echo argument)
    mov rcx, r14
    lea rdx, [k_text]
    lea r8, [text_buf]
    mov r9, 1024
    call extract_json_key_value

    ; path (for read_file / list_dir)
    mov rcx, r14
    lea rdx, [k_path]
    lea r8, [path_buf]
    mov r9, 512
    call extract_json_key_value

    ; Execute file tools before build_payload (puts result in text_buf)
    lea rcx, [method_buf]
    lea rdx, [m_tools_call]
    call streq
    test eax, eax
    jz skip_tool_exec

    lea rcx, [name_buf]
    lea rdx, [n_read_file]
    call streq
    test eax, eax
    jz chk_ld
    call exec_read_file
    jmp skip_tool_exec
chk_ld:
    lea rcx, [name_buf]
    lea rdx, [n_list_dir]
    call streq
    test eax, eax
    jz skip_tool_exec
    call exec_list_dir
skip_tool_exec:

    mov r9, QWORD PTR [rsp+24]
    mov al, BYTE PTR [rsp+16]
    mov BYTE PTR [r9], al

    ; notifications/initialized => no response
    lea rdx, [m_initialized]
    lea rcx, [method_buf]
    call streq
    test eax, eax
    jnz advance_frame

    ; unknown/empty method still gets JSON-RPC error response
    lea rcx, [method_buf]
    lea rdx, [out_buf]
    call build_payload

    mov rcx, r13
    lea rdx, [out_buf]
    mov r8, rax
    call write_framed

    ; shutdown/exit => acknowledge and terminate
    lea rdx, [m_shutdown]
    lea rcx, [method_buf]
    call streq
    test eax, eax
    jnz exit_ok

    lea rdx, [m_exit]
    lea rcx, [method_buf]
    call streq
    test eax, eax
    jnz exit_ok

advance_frame:
    mov r15, QWORD PTR [rsp+24]
    jmp frame_loop

no_tail:
    mov QWORD PTR [pending_bytes], 0
    jmp main_loop

store_tail:
    mov rax, r11
    sub rax, r15
    mov QWORD PTR [pending_bytes], rax
    test rax, rax
    jz main_loop
    lea rdi, [in_buf]
    cmp r15, rdi
    je main_loop
    mov rcx, rdi
    mov rdx, r15
    mov r8, rax
    call copy_mem

    jmp main_loop

exit_ok:
    xor ecx, ecx
    call ExitProcess
    xor eax, eax
    add rsp, 40
    ret
RawrXD_MCPServer_Entry ENDP

END
