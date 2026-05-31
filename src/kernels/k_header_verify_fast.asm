; k_header_verify_fast.asm
; Fast, extension-agnostic header verification for model artifacts.
; Accepts files by content signature, not by filename convention.

OPTION CASEMAP:NONE

RAWRXD_FMT_UNKNOWN        EQU 0
RAWRXD_FMT_GGUF           EQU 1
RAWRXD_FMT_SAFETENSORS    EQU 2
RAWRXD_FMT_GZIP_WRAPPED   EQU 3
RAWRXD_FMT_ZIP_CONTAINER  EQU 4

RAWRXD_FLAG_HEADER_VALID  EQU 00000001h
RAWRXD_FLAG_WRAPPED       EQU 00000002h

RAWRXD_MAGIC_GGUF         EQU 46554747h ; 'GGUF' little-endian
RAWRXD_MAGIC_ZIP          EQU 04034B50h ; 'PK\x03\x04'

.CODE

; rcx = file_base, rdx = file_size_bytes, r8 = out_format (DWORD*), r9 = out_flags (DWORD*)
; returns eax = 0 on recognized format, 57h on invalid/unknown
k_header_verify_fast PROC
    push rbx

    ; Defaults
    xor ebx, ebx
    mov g_last_status, 57h
    mov g_last_format, RAWRXD_FMT_UNKNOWN
    mov g_last_flags, 0

    test rcx, rcx
    jz invalid
    cmp rdx, 4
    jb invalid

    mov eax, DWORD PTR [rcx]
    mov g_last_magic, eax

    cmp eax, RAWRXD_MAGIC_GGUF
    je recognized_gguf

    cmp eax, RAWRXD_MAGIC_ZIP
    je recognized_zip

    ; GZIP wrapper heuristic: 1F 8B
    cmp BYTE PTR [rcx], 1Fh
    jne check_safetensors
    cmp BYTE PTR [rcx + 1], 8Bh
    jne check_safetensors
    mov ebx, RAWRXD_FMT_GZIP_WRAPPED
    mov eax, RAWRXD_FLAG_HEADER_VALID
    or eax, RAWRXD_FLAG_WRAPPED
    jmp recognized

check_safetensors:
    ; Safetensors heuristic:
    ; [0..7] = little-endian JSON header length, followed by '{"...'
    cmp rdx, 16
    jb invalid

    mov rax, QWORD PTR [rcx]
    test rax, rax
    jz invalid
    cmp rax, 01000000h ; 16MB header sanity cap
    ja invalid

    cmp BYTE PTR [rcx + 8], 7Bh ; '{'
    jne invalid
    cmp BYTE PTR [rcx + 9], 22h ; '"'
    jne invalid

    mov ebx, RAWRXD_FMT_SAFETENSORS
    mov eax, RAWRXD_FLAG_HEADER_VALID
    jmp recognized

recognized_gguf:
    mov ebx, RAWRXD_FMT_GGUF
    mov eax, RAWRXD_FLAG_HEADER_VALID
    jmp recognized

recognized_zip:
    mov ebx, RAWRXD_FMT_ZIP_CONTAINER
    mov eax, RAWRXD_FLAG_HEADER_VALID
    or eax, RAWRXD_FLAG_WRAPPED

recognized:
    mov g_last_format, ebx
    mov g_last_flags, eax
    mov g_last_status, 0

    test r8, r8
    jz no_out_format
    mov DWORD PTR [r8], ebx

no_out_format:
    test r9, r9
    jz done
    mov DWORD PTR [r9], eax

done:
    xor eax, eax
    pop rbx
    ret

invalid:
    test r8, r8
    jz no_out_format_invalid
    mov DWORD PTR [r8], RAWRXD_FMT_UNKNOWN

no_out_format_invalid:
    test r9, r9
    jz done_invalid
    mov DWORD PTR [r9], 0

done_invalid:
    mov eax, 57h
    pop rbx
    ret
k_header_verify_fast ENDP

; rcx = out_state (optional, 16 bytes minimum)
; [0..3] format, [4..7] flags, [8..11] status, [12..15] last_magic
k_header_verify_fast_get_state PROC
    test rcx, rcx
    jz state_done

    mov eax, g_last_format
    mov DWORD PTR [rcx + 0], eax
    mov eax, g_last_flags
    mov DWORD PTR [rcx + 4], eax
    mov eax, g_last_status
    mov DWORD PTR [rcx + 8], eax
    mov eax, g_last_magic
    mov DWORD PTR [rcx + 12], eax

state_done:
    xor eax, eax
    ret
k_header_verify_fast_get_state ENDP

.DATA
ALIGN 8
g_last_magic   DD 0
g_last_format  DD RAWRXD_FMT_UNKNOWN
g_last_flags   DD 0
g_last_status  DD 57h

END
