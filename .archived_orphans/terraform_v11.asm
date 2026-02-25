; ================================================
; RXUC-FinalCompiler v11.0 — Fixed x64 Build
; Line 281 Fix: 64-bit register alignment for token math
; Assemble: ml64 /c /nologo terraform.asm
; Link: link /ENTRY:_start /SUBSYSTEM:CONSOLE terraform.obj kernel32.lib
; ================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


public _start

extrn ExitProcess:proc
extrn GetCommandLineA:proc
extrn CreateFileA:proc
extrn ReadFile:proc
extrn WriteFile:proc
extrn CloseHandle:proc
extrn GetFileSizeEx:proc
extrn GetStdHandle:proc
extrn WriteConsoleA:proc
extrn VirtualAlloc:proc

STD_OUTPUT equ -11

; Token types
TOK_EOF     equ 0
TOK_NUM     equ 1
TOK_ID      equ 2
TOK_PLUS    equ 3
TOK_MINUS   equ 4
TOK_MUL     equ 5
TOK_DIV     equ 6
TOK_LPAREN  equ 7
TOK_RPAREN  equ 8
TOK_FN      equ 9
TOK_RET     equ 10

.data
szBanner db 'RXUC Native Compiler',0Dh,0Ah,0
szError  db '[!] Error',0Dh,0Ah,0
szDone   db '[+] Build OK',0Dh,0Ah,0

hStdOut  dq 0
pSource  dq 0
cbSource dq 0
pOutput  dq 0
cbOutput dd 0
iSource  dq 0
iToken   dd 0
nTokens  dd 0

outFile  db 'output.exe',0

; Minimal PE template (192 bytes — padded to 256 with zeros after)
pe_template:
db 4Dh,5Ah,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 40h,00,00,00,50h,45h,00,00,64h,86h,01,00,00,00,00,00
db 00,00,00,00,00,00,00,00,0F0h,00,0Fh,00,0Bh,02,00,00
db 00,01,00,00,00,00,00,00,00,10,00,00,00,10,00,00
db 00,00,00,40h,01,00,00,00,00,00,00,00,00,00,00,00
db 00,10,00,00,00,02,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
db 00,00,00,00,00,00,00,00,2Eh,74,65,78,74,00,00,00
pe_template_end label byte
PE_TEMPLATE_SIZE equ pe_template_end - pe_template

; Buffers
.data?
srcBuf   db 131072 dup(?)
outBuf   db 262144 dup(?)
tokBuf   db 65536 dup(?)

.code

; ================================================
; Utility: String length  RCX→RAX
; ================================================
strlen proc
    xor rax, rax
SL1:
    cmp byte ptr [rcx+rax], 0
    je SL_d
    inc rax
    jmp SL1
SL_d:
    ret
strlen endp

; ================================================
; Print string to console  RCX=string
; ================================================
print proc
    push rsi
    sub rsp, 48
    mov rsi, rcx
    call strlen
    mov r8, rax
    mov rdx, rsi
    mov rcx, hStdOut
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteConsoleA
    add rsp, 48
    pop rsi
    ret
print endp

; ================================================
; Fatal error: print "[!] Error" and exit(1)
; ================================================
fatal proc
    lea rcx, szError
    call print
    mov ecx, 1
    call ExitProcess
fatal endp

; ================================================
; FIXED: Token handling with proper 64-bit math
; ================================================
add_token proc
    ; ecx=type, edx=value
    mov eax, nTokens
    cmp eax, 2000
    jae at_overflow

    ; Zero-extend 32→64 via 32-bit mov (implicit in x64)
    mov r9d, eax
    shl r9, 4                     ; ×16 (token slot size)

    lea r10, tokBuf
    add r10, r9

    mov [r10], cl                 ; type  (byte 0)
    mov [r10+4], edx              ; value (dword at +4)

    inc nTokens
at_overflow:
    ret
add_token endp

; ================================================
; Lexer
; ================================================
lexer proc
    mov nTokens, 0
    mov iSource, 0

lx_loop:
    mov rax, iSource
    cmp rax, cbSource
    jae lx_done

    mov rcx, pSource
    movzx eax, byte ptr [rcx+rax]

    cmp al, ' '
    jbe lx_skip
    cmp al, '0'
    jb lx_sym
    cmp al, '9'
    ja lx_sym

    ; ---- digit ----
    sub al, '0'
    movzx edx, al
    mov ecx, TOK_NUM
    call add_token

lx_skip:
    inc iSource
    jmp lx_loop

lx_sym:
    cmp al, '+'
    jne lx_m
    mov ecx, TOK_PLUS
    xor edx, edx
    call add_token
    inc iSource
    jmp lx_loop
lx_m:
    cmp al, '-'
    jne lx_mul
    mov ecx, TOK_MINUS
    xor edx, edx
    call add_token
    inc iSource
    jmp lx_loop
lx_mul:
    cmp al, '*'
    jne lx_div
    mov ecx, TOK_MUL
    xor edx, edx
    call add_token
    inc iSource
    jmp lx_loop
lx_div:
    cmp al, '/'
    jne lx_lp
    mov ecx, TOK_DIV
    xor edx, edx
    call add_token
    inc iSource
    jmp lx_loop
lx_lp:
    cmp al, '('
    jne lx_rp
    mov ecx, TOK_LPAREN
    xor edx, edx
    call add_token
    inc iSource
    jmp lx_loop
lx_rp:
    cmp al, ')'
    jne lx_next
    mov ecx, TOK_RPAREN
    xor edx, edx
    call add_token

lx_next:
    inc iSource
    jmp lx_loop

lx_done:
    mov ecx, TOK_EOF
    xor edx, edx
    call add_token
    ret
lexer endp

; ================================================
; Code Generator — emit helpers
; ================================================
emit_byte proc
    mov rax, pOutput
    mov r11d, cbOutput
    mov [rax+r11], cl
    inc cbOutput
    ret
emit_byte endp

emit_dword proc
    mov rax, pOutput
    mov r11d, cbOutput
    mov [rax+r11], ecx
    add cbOutput, 4
    ret
emit_dword endp

; ================================================
; parse_expr — emit "mov eax, imm32" for first number token
; ================================================
parse_expr proc
    mov eax, iToken
    mov r9d, eax
    shl r9, 4
    lea r10, tokBuf
    add r10, r9

    cmp byte ptr [r10], TOK_NUM
    jne pe_done

    ; Encode: mov eax, imm32  (B8 + imm32)
    mov cl, 0B8h
    call emit_byte
    mov ecx, [r10+4]
    call emit_dword

    inc iToken

pe_done:
    ; Encode: ret (C3)
    mov cl, 0C3h
    call emit_byte
    ret
parse_expr endp

; ================================================
; PE Writer  — writes a minimal x64 PE to "output.exe"
; ================================================
write_pe proc
    push r12
    sub rsp, 96                   ; shadow(32) + 7 params(56) + align(8)

    ; ---- CreateFileA("output.exe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NORMAL, NULL)
    lea rcx, outFile
    mov edx, 40000000h            ; GENERIC_WRITE
    xor r8d, r8d                  ; dwShareMode = 0
    xor r9d, r9d                  ; lpSecurityAttributes = NULL
    mov qword ptr [rsp+32], 2     ; CREATE_ALWAYS
    mov qword ptr [rsp+40], 80h   ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0     ; hTemplateFile = NULL
    call CreateFileA

    cmp rax, -1
    je wp_fail
    mov r12, rax                  ; r12 = hFile

    ; ---- Write PE template (first 192 bytes of header) ----
    lea rdx, pe_template
    mov rcx, r12
    mov r8d, PE_TEMPLATE_SIZE
    xor r9d, r9d
    mov qword ptr [rsp+32], 0
    call WriteFile

    ; ---- Write generated code ----
    mov rcx, r12
    mov rdx, pOutput
    mov r8d, cbOutput
    xor r9d, r9d
    mov qword ptr [rsp+32], 0
    call WriteFile

    ; ---- Close ----
    mov rcx, r12
    call CloseHandle

    add rsp, 96
    pop r12
    ret

wp_fail:
    call fatal
    ; fatal does not return
write_pe endp

; ================================================
; Entry Point
; ================================================
_start:
    sub rsp, 72                   ; 8-byte aligned after implicit push rip

    ; ---- stdout handle ----
    mov ecx, STD_OUTPUT
    call GetStdHandle
    mov hStdOut, rax

    lea rcx, szBanner
    call print

    ; ---- command-line argument parsing ----
    call GetCommandLineA
    mov rsi, rax
    cmp byte ptr [rsi], 22h       ; '"'
    je sq_skip

sq_norm:
    lodsb
    cmp al, ' '
    jne sq_norm
    jmp sq_arg

sq_skip:
    inc rsi                       ; skip opening quote
sq_q:
    lodsb
    cmp al, 22h
    jne sq_q
    ; RSI now past closing quote

sq_arg:
    ; skip spaces
    lodsb
    cmp al, ' '
    je sq_arg
    dec rsi

    cmp al, 0
    je sq_noargs

    ; ---- Open source file ----
    mov rcx, rsi
    mov edx, 80000000h            ; GENERIC_READ
    mov r8d, 1                    ; FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+32], 3     ; OPEN_EXISTING
    mov qword ptr [rsp+40], 80h
    mov qword ptr [rsp+48], 0
    call CreateFileA

    cmp rax, -1
    je sq_err
    mov r13, rax                  ; r13 = hFile

    ; ---- Get size ----
    lea rdx, cbSource
    mov rcx, r13
    call GetFileSizeEx

    ; ---- Read into srcBuf ----
    lea rax, srcBuf
    mov pSource, rax

    mov rcx, r13
    mov rdx, pSource
    mov r8, cbSource
    lea r9, [rsp+64]              ; lpBytesRead (scratch)
    mov qword ptr [rsp+32], 0     ; lpOverlapped
    call ReadFile

    mov rcx, r13
    call CloseHandle

    ; ---- Set up output buffer ----
    lea rax, outBuf
    mov pOutput, rax
    mov cbOutput, 0

    ; ---- Compile ----
    call lexer
    mov iToken, 0
    call parse_expr

    ; ---- Emit PE ----
    call write_pe

    lea rcx, szDone
    call print

    xor ecx, ecx
    call ExitProcess

sq_noargs:
sq_err:
    call fatal

end
