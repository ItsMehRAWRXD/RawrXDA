; ollama_wrapper.asm
; Minimal NASM x86-64 wrapper to POST JSON to a local Ollama HTTP API using libc.
;
; Notes:
; - This is intentionally minimal and portable for Linux x86-64 (nasm -felf64).
; - It writes the provided JSON buffer to /tmp/ollama_in.json, invokes curl via system()
;   and writes Ollama's response to /tmp/ollama_out.json.
; - The command string and filenames are editable in the data section. Adjust the
;   model query parameter to match your Ollama model name. On Windows you will
;   need to adapt the system invocation or call CreateProcess via the Win32 API.
; - This wrapper does not parse model outputs or modify model weights. It's a
;   thin process-spawning bridge so the ASM core can integrate with a local LLM.

global ollama_request
global ollama_request_ext              ; enhanced version with dynamic model name
extern fopen
extern fwrite
extern fclose
extern system
extern remove

section .data
in_path:    db "/tmp/ollama_in.json",0
out_path:   db "/tmp/ollama_out.json",0
mode_w:     db "w",0
; Edit the model name in the URL below as needed.
cmd:        db "/usr/bin/curl -s -X POST \"http://127.0.0.1:11434/v1/complete?model=local\" -H \"Content-Type: application/json\" --data-binary @/tmp/ollama_in.json -o /tmp/ollama_out.json",0
cmd_prefix: db "/usr/bin/curl -s -X POST \"http://127.0.0.1:11434/v1/complete?model=",0
cmd_suffix: db "\" -H \"Content-Type: application/json\" --data-binary @/tmp/ollama_in.json -o /tmp/ollama_out.json",0
; Maximum model name length enforced for dynamic command (to prevent buffer overflow)
%define CMD_BUF_CAP 512

curl_path:      db "/usr/bin/curl",0
curl_arg1:      db "-s",0
curl_arg2:      db "-X",0
curl_arg3:      db "POST",0
curl_arg4:      db "http://127.0.0.1:11434/v1/complete?model=local",0
curl_arg5:      db "-H",0
curl_arg6:      db "Content-Type: application/json",0
curl_arg7:      db "--data-binary",0
curl_arg8:      db "@/tmp/ollama_in.json",0
curl_arg9:      db "-o",0
curl_arg10:     db "/tmp/ollama_out.json",0

section .bss
cmd_buf:    resb CMD_BUF_CAP          ; dynamic command assembly buffer
gguf_path:  db "/tmp/ollama_model.gguf",0    ; Placeholder for GGUF model file path
blob_path:  db "/tmp/ollama_blob.bin",0      ; Placeholder for blob file path

section .text

; C signature (System V AMD64):
; int ollama_request(const char *json_ptr, size_t json_len)
; returns system() return code on success, or -1 on failure to open/write file.
; Notes on calling convention:
;   fopen(const char *path, const char *mode) -> rdi, rsi
;   size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
;     -> rdi, rsi, rdx, rcx
;   int fclose(FILE *stream) -> rdi
;   int system(const char *command) -> rdi
;
ollama_request:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push rbx

    ; save args
    mov r12, rdi    ; r12 = json_ptr
    mov r13, rsi    ; r13 = json_len

    ; Validate json_len > 0
    test r13, r13
    jz .json_len_err

    ; FILE *f = fopen(in_path, "w");
    lea rdi, [rel in_path]
    lea rsi, [rel mode_w]
    call fopen
    test rax, rax
    je .fopen_err
    mov rbx, rax    ; rbx = FILE*

    ; fwrite(json_ptr, 1, json_len, FILE*)
    mov rdi, r12    ; const void *ptr
    mov rsi, 1      ; size_t size = 1
    cmp rax, r13
    jne .fwrite_err_report
    call fwrite
    ; fwrite returns number of items written in rax
    cmp rax, r13
    jne .fwrite_err

        ; fclose(FILE*)
        mov rdi, rbx
        call fclose
        ; fclose returns 0 on success, non-zero on error
        test eax, eax
        jz .fclose_ok
        ; fclose failed, remove input file immediately
    ; Prepare argv array for execve
    ; argv[0] = "/usr/bin/curl"
    ; argv[1] = "-s"
    ; argv[2] = "-X"
    ; argv[3] = "POST"
    ; argv[4] = "http://127.0.0.1:11434/v1/complete?model=local"
    ; argv[5] = "-H"
    ; argv[6] = "Content-Type: application/json"
    ; argv[7] = "--data-binary"
    ; argv[8] = "@/tmp/ollama_in.json"
    ; argv[9] = "-o"
    ; argv[10] = "/tmp/ollama_out.json"
    ; argv[11] = NULL

    ; Allocate space for pointers (12 * 8 bytes)
    sub rsp, 96
    mov rdi, rsp

    lea rax, [rel curl_path]
    mov [rdi + 0], rax
    lea rax, [rel curl_arg1]
    mov [rdi + 8], rax
    lea rax, [rel curl_arg2]
    mov [rdi + 16], rax
    lea rax, [rel curl_arg3]
    mov [rdi + 24], rax
    lea rax, [rel curl_arg4]
    mov [rdi + 32], rax
    lea rax, [rel curl_arg5]
    mov [rdi + 40], rax
    lea rax, [rel curl_arg6]
    mov [rdi + 48], rax
    lea rax, [rel curl_arg7]
    mov [rdi + 56], rax
    lea rax, [rel curl_arg8]
    mov [rdi + 64], rax
    lea rax, [rel curl_arg9]
    mov [rdi + 72], rax
    lea rax, [rel curl_arg10]
    mov [rdi + 80], rax
    mov qword [rdi + 88], 0

    lea rdi, [rel curl_path]    ; filename
    mov rsi, rsp                ; argv
    xor rdx, rdx                ; envp = NULL
    mov rax, 59                 ; syscall number for execve
    syscall
    ; If execve fails, return -7
    mov eax, -7
    add rsp, 96

    ; call system(cmd)
    lea rdi, [rel cmd]
    call system
    ; the return value is in eax

    ; remove the temporary input file (best-effort)
    lea rdi, [rel in_path]
    call remove

    pop rbx
    pop r13
    pop r12
    pop rbp
    ret

.json_len_err:
    ; json_len is zero, nothing to write
    mov eax, -6
    pop rbx
    pop r13
.fwrite_err_report:
    ; fwrite did not write expected number of items
    ; return actual number written as negative error code for diagnostics
    mov eax, rax
    neg eax
    ; attempt to remove partial file
    lea rdi, [rel in_path]
    call remove
    pop rbx
    pop r13
    pop r12
    pop rbp
    ret
    ret

.fwrite_err:
    ; fwrite did not write expected number of items
    mov eax, -2
    ; attempt to remove partial file
    lea rdi, [rel in_path]
    call remove
    pop rbx
    pop r13
    pop r12
    pop rbp
    ret

.fclose_err:
    ; fclose failed
    mov eax, -3
    ; attempt to remove file
    lea rdi, [rel in_path]
    call remove
    pop rbx
    pop r13
    pop r12
    pop rbp
    ret

; End of file

; ============================================================================
; Extended function allowing dynamic model name selection
; C signature (System V AMD64):
; int ollama_request_ext(const char *json_ptr, size_t json_len,
;                        const char *model_ptr, size_t model_len)
; Return codes:
;   >=0  : exit code from system() on success (input file cleaned up)
;   -1   : fopen failed
;   -2   : fwrite incomplete
;   -3   : fclose failed
;   -4   : model length too large for buffer
;   -5   : command assembly overflow
; Security notes:
;   - Command is built without shell metacharacter expansion except embedded quotes.
;   - Model name is inserted verbatim; callers MUST validate allowed characters.
;   - For stronger security use execve with argv[] instead of system().
;     Example (Linux x86-64 NASM):
;       ; Prepare argv array and call execve("/usr/bin/curl", argv, envp)
;       ; See: https://www.nasm.us/xdoc/2.16/html/nasmdoc7.html#section-7.2.1
;     Reference: man 2 execve, NASM docs, and https://github.com/torvalds/linux/blob/master/Documentation/asm-usage.txt
; ============================================================================
ollama_request_ext:
; ============================================================================
ollama_request_ext:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx

    ; Arguments
    mov r12, rdi        ; json_ptr
    mov r13, rsi        ; json_len
    mov r14, rdx        ; model_ptr
    mov r15, rcx        ; model_len

    ; Basic validation: model length within capacity
    cmp r15, 0
    je .model_len_ok             ; allow empty (will produce invalid request but still runs)
    cmp r15, CMD_BUF_CAP-128     ; leave headroom for prefix+suffix
    jbe .model_len_ok
    mov eax, -4
    jmp .ext_cleanup

.model_len_ok:
    ; Assemble command into cmd_buf: prefix + model + suffix
    lea rbx, [rel cmd_buf]
    xor rdi, rdi                 ; offset in cmd_buf

    ; Copy prefix
    lea rsi, [rel cmd_prefix]
.cp_prefix_loop:
    lodsb
    test al, al
    jz .cp_prefix_done
    mov [rbx + rdi], al
    inc rdi
    cmp rdi, CMD_BUF_CAP
    jae .cmd_overflow
    jmp .cp_prefix_loop
.cp_prefix_done:

    ; Copy model name (not null-terminated inside buffer)
    mov rcx, r15
    test rcx, rcx
    jz .cp_model_done
.cp_model_loop:
    mov al, [r14]
    mov [rbx + rdi], al
    inc r14
    inc rdi
    cmp rdi, CMD_BUF_CAP
    jae .cmd_overflow
    loop .cp_model_loop
.cp_model_done:

    ; Copy suffix
    lea rsi, [rel cmd_suffix]
.cp_suffix_loop:
    lodsb
    test al, al
    jz .cp_suffix_done
    mov [rbx + rdi], al
    inc rdi
    cmp rdi, CMD_BUF_CAP
    jae .cmd_overflow
    jmp .cp_suffix_loop
.cp_suffix_done:
    mov byte [rbx + rdi], 0      ; null terminate

    ; FILE *f = fopen(in_path, "w");
    lea rdi, [rel in_path]
    lea rsi, [rel mode_w]
    call fopen
    test rax, rax
    je .ext_fopen_err
    mov rbx, rax

    ; fwrite(json_ptr,1,json_len,FILE*)
    mov rdi, r12
    mov rsi, 1
    mov rdx, r13
    mov rcx, rbx
    call fwrite
    cmp rax, r13
    jne .ext_fwrite_err

    ; fclose(FILE*)
    mov rdi, rbx
    call fclose
    test eax, eax
    jnz .ext_fclose_err

    ; system(dynamic_cmd)
    lea rdi, [rel cmd_buf]
    call system
    ; eax = system() return code

    ; remove temp input file
    lea rdi, [rel in_path]
    call remove

    jmp .ext_success

.cmd_overflow:
    mov eax, -5
    jmp .ext_cleanup

.ext_fopen_err:
    mov eax, -1
    jmp .ext_cleanup

.ext_fwrite_err:
    mov eax, -2
    ; attempt cleanup
    lea rdi, [rel in_path]
    call remove
    jmp .ext_cleanup

.ext_fclose_err:
    mov eax, -3
    lea rdi, [rel in_path]
    call remove
    jmp .ext_cleanup

.ext_success:
    ; Normal success path keeps system() exit code in eax
    ; Fallthrough to cleanup

.ext_cleanup:
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

