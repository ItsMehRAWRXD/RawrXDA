; ============================================================================
; masm_solo_compiler_dual.asm
; Universal MASM64 Compiler - Generates x86 (PE32) or x64 (PE32+) Executables
; Host: x64 Windows (ML64) | Targets: x86 (IA32) or x64 (AMD64)
; Build: ml64 /c masm_solo_compiler_dual.asm && link /subsystem:console /entry:main kernel32.lib user32.lib
; Usage: compiler.exe input.asm output.exe [--x86]
; ============================================================================

includelib kernel32.lib
includelib user32.lib

; Architecture Constants
ARCH_X86	equ 0
ARCH_X64	equ 1

; PE Format Constants
PE32_MAGIC	equ 010Bh
PE32P_MAGIC	equ 020Bh

; Windows Subsystems
SUBSYSTEM_WINDOWS_CUI	equ 3
SUBSYSTEM_WINDOWS_GUI	equ 2
IMAGE_FILE_MACHINE_I386	equ 014Ch
IMAGE_FILE_MACHINE_AMD64 equ 08664h

; Characteristics
IMAGE_FILE_EXECUTABLE_IMAGE	equ 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE	equ 0020h
IMAGE_FILE_32BIT_MACHINE	equ 0100h

extrn GetCommandLineA:proc, GetStdHandle:proc, WriteFile:proc
extrn CreateFileA:proc, ReadFile:proc, CloseHandle:proc, GetFileSize:proc
extrn ExitProcess:proc, lstrlenA:proc, lstrcmpiA:proc, wsprintfA:proc

; ============================================================================
; .CONST
; ============================================================================
.const
compiler_name		db "[RawrXD] Universal MASM Compiler v2.0 (x86/x64)", 0
compiler_copyright	db "(C) 2026 RawrXD Project - Dual Architecture Edition", 0

usage_msg		db "Usage: compiler <input.asm> <output.exe> [--x86]", 13, 10
			db "  Default: x64 (AMD64) target", 13, 10
			db "  --x86:   x86 (IA32) target", 13, 10, 0

opt_x86			db "--x86", 0

; Token Types
TOK_EOF		equ 0
TOK_IDENT	equ 1
TOK_NUMBER	equ 2
TOK_REG		equ 3
TOK_INSTR	equ 4
TOK_DIR		equ 5

; x86 Instructions
x86_mov_rm_r	 db 089h, 0C0h, 0, 1
x86_mov_r_imm32	 db 0B8h, 000h, 4, 1
x86_push_r	 db 050h, 000h, 0, 1
x86_pop_r	 db 058h, 000h, 0, 1
x86_add_rm_r	 db 001h, 0C0h, 0, 1
x86_sub_rm_r	 db 029h, 0C0h, 0, 1
x86_xor_rm_r	 db 031h, 0C0h, 0, 1
x86_call_rel32	 db 0E8h, 000h, 4, 1
x86_ret		 db 0C3h, 000h, 0, 1
x86_nop		 db 090h, 000h, 0, 1

; x64 Instructions
x64_rex_w	 equ 048h
x64_mov_rm_r	 db 089h, 0C0h, 0, 2
x64_mov_r_imm64	 db 0B8h, 000h, 8, 2
x64_push_r	 db 050h, 000h, 0, 2
x64_pop_r	 db 058h, 000h, 0, 2
x64_add_rm_r	 db 001h, 0C0h, 0, 2
x64_sub_rm_r	 db 029h, 0C0h, 0, 2
x64_xor_rm_r	 db 031h, 0C0h, 0, 2
x64_call_rel32	 db 0E8h, 000h, 4, 2
x64_ret		 db 0C3h, 000h, 0, 2
x64_nop		 db 090h, 000h, 0, 2

; Register Tables
reg_x86_names	 db "eax", 0, "ecx", 0, "edx", 0, "ebx", 0
		 db "esp", 0, "ebp", 0, "esi", 0, "edi", 0, 0
reg_x86_ids	 db 0, 1, 2, 3, 4, 5, 6, 7

reg_x64_names	 db "rax", 0, "rcx", 0, "rdx", 0, "rbx", 0
		 db "rsp", 0, "rbp", 0, "rsi", 0, "rdi", 0
		 db "r8", 0, "r9", 0, "r10", 0, "r11", 0
		 db "r12", 0, "r13", 0, "r14", 0, "r15", 0, 0
reg_x64_ids	 db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15

; Mnemonics
mnemonic_nop	 db "nop", 0
mnemonic_ret	 db "ret", 0
mnemonic_mov	 db "mov", 0
mnemonic_push	 db "push", 0
mnemonic_pop	 db "pop", 0
mnemonic_add	 db "add", 0
mnemonic_sub	 db "sub", 0
mnemonic_xor	 db "xor", 0
mnemonic_call	 db "call", 0

; PE Section Names
sect_name_text	 db ".text", 0, 0, 0
sect_name_data	 db ".data", 0, 0, 0

str_target_x86	db 13, 10, "[Target Architecture: x86 (IA32)]", 13, 10, 0
str_target_x64	db 13, 10, "[Target Architecture: x64 (AMD64)]", 13, 10, 0

; ============================================================================
; .DATA?
; ============================================================================
.data?
g_target_arch	 dd ?
g_is_64bit	 db ?

; File I/O
g_hInput	 dq ?
g_hOutput	 dq ?
g_file_size	 dq ?

; Buffers
MAX_SRC		equ 01000000h
MAX_TOK		equ 0100000h
MAX_CODE	equ 01000000h
MAX_DATA	equ 01000000h
MAX_PE		equ 02000000h

g_source	db MAX_SRC dup(?)
g_tokens	db MAX_TOK * 32 dup(?)
g_code		db MAX_CODE dup(?)
g_data		db MAX_DATA dup(?)
g_pe		db MAX_PE dup(?)

; Code Generation State
g_code_ptr	dq ?
g_data_ptr	dq ?
g_pe_ptr	dq ?
g_entry_point	dd ?

; Command Line
g_infile	db 260 dup(?)
g_outfile	db 260 dup(?)

; Temp
g_temp		db 1024 dup(?)

; ============================================================================
; CODE
; ============================================================================
.code
option win64:save

; Helper: Check if string equals --x86
check_arg_x86 proc
	push rsi
	push rdi
	mov rsi, rcx
	lea rdi, opt_x86
cmp_loop:
	mov al, [rsi]
	mov bl, [rdi]
	cmp al, bl
	jne not_equal
	test al, al
	jz equal
	inc rsi
	inc rdi
	jmp cmp_loop
not_equal:
	mov rax, 0
	pop rdi
	pop rsi
	ret
equal:
	mov rax, 1
	pop rdi
	pop rsi
	ret
check_arg_x86 endp

; Helper: Parse command line
parse_args proc
	push rbp
	mov rbp, rsp
	sub rsp, 32

	call GetCommandLineA
	mov rsi, rax

	; Skip exe name
skip1:
	lodsb
	cmp al, ' '
	jne skip1
skip_space1:
	cmp byte ptr [rsi], ' '
	jne got_space1
	inc rsi
	jmp skip_space1
got_space1:

	; Copy input filename
	lea rdi, g_infile
cpy_in:
	lodsb
	cmp al, ' '
	je in_done
	cmp al, 0
	je no_args
	stosb
	jmp cpy_in
in_done:
	mov byte ptr [rdi], 0

	; Skip spaces
skip_space2:
	cmp byte ptr [rsi], ' '
	jne got_space2
	inc rsi
	jmp skip_space2
got_space2:

	; Check for --x86 before output filename
	mov rcx, rsi
	call check_arg_x86
	test rax, rax
	jz not_x86_switch
	mov g_target_arch, ARCH_X86
	; Skip --x86
	add rsi, 5
skip_sw:
	cmp byte ptr [rsi], ' '
	jne got_sw
	inc rsi
	jmp skip_sw
got_sw:
not_x86_switch:

	; Copy output filename
	lea rdi, g_outfile
cpy_out:
	lodsb
	cmp al, ' '
	je out_done
	cmp al, 0
	je out_done
	stosb
	jmp cpy_out
out_done:
	mov byte ptr [rdi], 0

	; Check remaining args for --x86
chk_rem:
	lodsb
	cmp al, 0
	je args_done
	cmp al, '-'
	jne chk_rem
	dec rsi
	mov rcx, rsi
	call check_arg_x86
	test rax, rax
	jz chk_rem
	mov g_target_arch, ARCH_X86

args_done:
	mov rax, 1
	jmp parse_done

no_args:
	xor rax, rax

parse_done:
	add rsp, 32
	pop rbp
	ret
parse_args endp

; ============================================================================
; PE32 (x86) Writer
; ============================================================================
write_pe32 proc
	push rbp
	mov rbp, rsp
	sub rsp, 80

	mov r12, rcx
	mov r13, rdx

	; Align sizes
	mov rax, r12
	add rax, 01FFh
	and rax, -0200h
	mov r14, rax

	mov rax, r13
	add rax, 01FFh
	and rax, -0200h
	mov r15, rax

	; Clear header area
	lea rdi, g_pe
	xor rax, rax
	mov rcx, 128
	rep stosq

	; DOS Header
	lea rdi, g_pe
	mov word ptr [rdi], 05A4Dh
	mov dword ptr [rdi+03Ch], 080h

	; PE Signature
	lea rdi, g_pe+80h
	mov dword ptr [rdi], 00004550h

	; COFF Header
	lea rdi, g_pe+84h
	mov word ptr [rdi], IMAGE_FILE_MACHINE_I386
	mov word ptr [rdi+2], 1
	mov dword ptr [rdi+4], 0
	mov dword ptr [rdi+8], 0
	mov dword ptr [rdi+12], 0
	mov word ptr [rdi+16], 0E0h
	mov word ptr [rdi+18], IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_32BIT_MACHINE

	; Optional Header PE32
	lea rdi, g_pe+98h
	mov word ptr [rdi], PE32_MAGIC
	mov byte ptr [rdi+2], 0Eh
	mov byte ptr [rdi+3], 0
	mov dword ptr [rdi+4], r14d
	mov dword ptr [rdi+8], r15d
	mov dword ptr [rdi+12], 0
	mov dword ptr [rdi+16], 01000h
	mov dword ptr [rdi+20], 01000h
	mov dword ptr [rdi+24], 0
	mov dword ptr [rdi+28], 0400000h
	mov dword ptr [rdi+32], 01000h
	mov dword ptr [rdi+36], 0200h
	mov word ptr [rdi+40], 6
	mov word ptr [rdi+42], 0
	mov word ptr [rdi+44], 0
	mov word ptr [rdi+46], 0
	mov word ptr [rdi+48], 6
	mov word ptr [rdi+50], 0
	mov dword ptr [rdi+52], 0
	mov eax, 01000h
	add eax, r14d
	cmp r13, 0
	je no_data32
	add eax, 01000h
no_data32:
	mov dword ptr [rdi+56], eax
	mov dword ptr [rdi+60], 0200h
	mov dword ptr [rdi+64], 0
	mov word ptr [rdi+68], SUBSYSTEM_WINDOWS_CUI
	mov word ptr [rdi+70], 0
	mov dword ptr [rdi+72], 00100000h
	mov dword ptr [rdi+76], 01000h
	mov dword ptr [rdi+80], 00100000h
	mov dword ptr [rdi+84], 01000h
	mov dword ptr [rdi+88], 0
	mov dword ptr [rdi+92], 16

	; Zero data directories
	lea rdi, g_pe+98h+096h
	xor rax, rax
	mov rcx, 16
zerodir32:
	mov qword ptr [rdi], rax
	add rdi, 8
	loop zerodir32

	; Section Header .text
	lea rdi, g_pe+184h
	mov dword ptr [rdi], 074657874h
	mov dword ptr [rdi+4], 0
	mov dword ptr [rdi+8], r12d
	mov dword ptr [rdi+12], 01000h
	mov dword ptr [rdi+16], r14d
	mov dword ptr [rdi+20], 0200h
	mov dword ptr [rdi+24], 0
	mov dword ptr [rdi+28], 0
	mov word ptr [rdi+32], 0
	mov word ptr [rdi+34], 0
	mov dword ptr [rdi+36], 060000020h

	; Copy code to 0200h
	lea rsi, g_code
	lea rdi, g_pe+200h
	mov rcx, r12
	rep movsb

	; Calculate total size
	mov rax, 0200h
	add rax, r14
	cmp r13, 0
	je done_pe32
done_pe32:
	mov g_pe_ptr, rax

	add rsp, 80
	pop rbp
	ret
write_pe32 endp

; ============================================================================
; PE32+ (x64) Writer
; ============================================================================
write_pe64 proc
	push rbp
	mov rbp, rsp
	sub rsp, 80

	mov r12, rcx
	mov r13, rdx

	; Align
	mov rax, r12
	add rax, 01FFh
	and rax, -0200h
	mov r14, rax

	mov rax, r13
	add rax, 01FFh
	and rax, -0200h
	mov r15, rax

	; Clear 1KB header
	lea rdi, g_pe
	xor rax, rax
	mov rcx, 128
	rep stosq

	; DOS Header
	lea rdi, g_pe
	mov word ptr [rdi], 05A4Dh
	mov dword ptr [rdi+03Ch], 080h

	; PE Sig
	lea rdi, g_pe+80h
	mov dword ptr [rdi], 00004550h

	; COFF
	lea rdi, g_pe+84h
	mov word ptr [rdi], IMAGE_FILE_MACHINE_AMD64
	mov word ptr [rdi+2], 1
	mov word ptr [rdi+16], 0F0h
	mov word ptr [rdi+18], IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE

	; Optional Header PE32+
	lea rdi, g_pe+98h
	mov word ptr [rdi], PE32P_MAGIC
	mov byte ptr [rdi+2], 0Eh
	mov qword ptr [rdi+24], 0140000000h

	; Copy code
	lea rsi, g_code
	lea rdi, g_pe+200h
	mov rcx, r12
	rep movsb

	mov rax, 0200h
	add rax, r14
	mov g_pe_ptr, rax

	add rsp, 80
	pop rbp
	ret
write_pe64 endp

; ============================================================================
; Unified PE Writer
; ============================================================================
generate_pe proc
	push rbp
	mov rbp, rsp

	mov rcx, g_code_ptr
	mov rdx, g_data_ptr

	cmp g_target_arch, ARCH_X86
	jne do_pe64
	call write_pe32
	jmp pe_done
do_pe64:
	call write_pe64
pe_done:

	pop rbp
	ret
generate_pe endp

; ============================================================================
; x86 Code Generation
; ============================================================================
emit_x86_prologue proc
	; push ebp / mov ebp, esp
	mov byte ptr [g_code], 055h
	mov byte ptr [g_code+1], 08Bh
	mov byte ptr [g_code+2], 0ECh
	mov g_code_ptr, 3
	ret
emit_x86_prologue endp

emit_x86_epilogue proc
	; mov esp, ebp / pop ebp / ret
	mov rax, g_code_ptr
	mov byte ptr [g_code+rax], 08Bh
	mov byte ptr [g_code+rax+1], 0E4h
	mov byte ptr [g_code+rax+2], 05Dh
	mov byte ptr [g_code+rax+3], 0C3h
	add g_code_ptr, 4
	ret
emit_x86_epilogue endp

; ============================================================================
; x64 Code Generation
; ============================================================================
emit_x64_prologue proc
	; push rbp / mov rbp, rsp
	mov byte ptr [g_code], 055h
	mov word ptr [g_code+1], 0E58948h
	mov g_code_ptr, 4
	ret
emit_x64_prologue endp

emit_x64_epilogue proc
	mov rax, g_code_ptr
	mov word ptr [g_code+rax], 0EC8948h
	mov byte ptr [g_code+rax+2], 05Dh
	mov byte ptr [g_code+rax+3], 0C3h
	add g_code_ptr, 5
	ret
emit_x64_epilogue endp

; ============================================================================
; Main
; ============================================================================
main proc
	push rbp
	mov rbp, rsp
	sub rsp, 48

	; Default to x64
	mov g_target_arch, ARCH_X64

	call parse_args
	test rax, rax
	jz show_usage

	; Print banner and target
	lea rcx, compiler_name
	call print_string
	
	cmp g_target_arch, ARCH_X86
	jne target_64
	lea rcx, str_target_x86
	jmp print_target
target_64:
	lea rcx, str_target_x64
print_target:
	call print_string

	; Compile
	call compile_source

	; Write output
	call write_output_file

	xor rcx, rcx
	call ExitProcess

show_usage:
	lea rcx, usage_msg
	call print_string
	mov rcx, 1
	call ExitProcess

main endp

; Placeholder functions
compile_source proc
	cmp g_target_arch, ARCH_X86
	jne cs_64
	call emit_x86_prologue
	call emit_x86_epilogue
	jmp cs_done
cs_64:
	call emit_x64_prologue
	call emit_x64_epilogue
cs_done:
	ret
compile_source endp

write_output_file proc
	; Generate PE
	call generate_pe

	; Create file
	lea rcx, g_outfile
	mov rdx, 40000000h
	xor r8, r8
	xor r9, r9
	mov qword ptr [rsp+32], 2
	mov qword ptr [rsp+40], 80h
	call CreateFileA
	mov g_hOutput, rax

	; Write
	mov rcx, g_hOutput
	lea rdx, g_pe
	mov r8, g_pe_ptr
	lea r9, [rsp+40]
	mov qword ptr [rsp+32], 0
	call WriteFile

	; Close
	mov rcx, g_hOutput
	call CloseHandle
	ret
write_output_file endp

print_string proc
	push rbp
	mov rbp, rsp
	sub rsp, 48
	mov rdx, rcx
	mov rcx, -11
	call GetStdHandle
	mov rcx, rax
	mov r8, rdx
	call lstrlenA
	mov r8, rax
	lea r9, [rsp+40]
	mov qword ptr [rsp+32], 0
	call WriteFile
	add rsp, 48
	pop rbp
	ret
print_string endp

end
