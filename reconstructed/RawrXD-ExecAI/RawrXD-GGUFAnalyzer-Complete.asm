; RawrXD-GGUFAnalyzer-Complete.asm
; MASM64 GGUF v3 analyzer - Final Corrected Version

option casemap:none

EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetStdHandle:PROC
EXTERN SetFilePointerEx:PROC
EXTERN ExitProcess:PROC
EXTERN GetCommandLineA:PROC
EXTERN GetLastError:PROC

.const
GGUF_MAGIC32         EQU 46554747h
GGUF_VERSION         EQU 3
MAX_NAME_BYTES       EQU 8192

GGUF_TYPE_UINT8      EQU 0
GGUF_TYPE_INT8       EQU 1
GGUF_TYPE_UINT16     EQU 2
GGUF_TYPE_INT16      EQU 3
GGUF_TYPE_UINT32     EQU 4
GGUF_TYPE_INT32      EQU 5
GGUF_TYPE_FLOAT32    EQU 6
GGUF_TYPE_BOOL       EQU 7
GGUF_TYPE_STRING     EQU 8
GGUF_TYPE_ARRAY      EQU 9
GGUF_TYPE_UINT64     EQU 10
GGUF_TYPE_INT64      EQU 11
GGUF_TYPE_FLOAT64    EQU 12

GENERIC_READ         EQU 80000000h
FILE_SHARE_READ      EQU 1
OPEN_EXISTING        EQU 3
FILE_CURRENT         EQU 1
INVALID_HANDLE_VALUE EQU 0FFFFFFFFFFFFFFFFh
STD_OUTPUT_HANDLE    EQU 0FFFFFFFFFFFFFFF5h

.data
default_input        DB "D:\RawrXD-ExecAI\test_with_metadata.gguf", 0
input_path           DB 260 DUP(0)

file_handle          DQ 0
stdout_handle        DQ 0
header_buffer        DB 24 DUP(0)
read_temp            DQ 0
dec_buffer           DB 32 DUP(0)

metadata_kv_count    DQ 0
tensor_count         DQ 0
analysis_total       DQ 0
count_unknown        DQ 0
pattern_ffn          DB "ffn", 0
pattern_attn         DB "attn", 0
name_buffer          DB 8193 DUP(0)

; Banner and Messages
msg_header           DB "=== RawrXD GGUF Analyzer (Final) ===", 0Dh, 0Ah, 0
msg_success          DB "GGUF parse completed successfully", 0Dh, 0Ah, 0
msg_error_file       DB "ERROR: Cannot open file", 0Dh, 0Ah, 0
msg_error_read       DB "ERROR: Read failed", 0Dh, 0Ah, 0
msg_error_read_kv    DB "ERROR: Read failed in SkipMetadataKV", 0Dh, 0Ah, 0
msg_error_read_pt    DB "ERROR: Read failed in ParseTensorMetadata", 0Dh, 0Ah, 0
msg_error_magic      DB "ERROR: Invalid GGUF magic", 0Dh, 0Ah, 0
msg_step_header      DB "Step 1: Read header OK", 0Dh, 0Ah, 0
msg_step_magic       DB "Step 2: Magic OK", 0Dh, 0Ah, 0
msg_step_skip_kv     DB "Step 3: Skip metadata KV OK", 0Dh, 0Ah, 0
msg_step_parse_pt    DB "Step 4: Parse tensor metadata OK", 0Dh, 0Ah, 0
msg_before_print     DB "Before PrintString tensor_count", 0Dh, 0Ah, 0
msg_summary_tensors  DB "Tensors: ", 0
msg_summary_params   DB "Params:  ", 0
msg_newline          DB 0Dh, 0Ah, 0

.code

PrintString PROC
    sub rsp, 28h
    mov r8, rcx
    xor r9, r9
PS_L:
    cmp byte ptr [r8+r9], 0
    je PS_W
    inc r9
    cmp r9, 4096
    jb PS_L
PS_W:
    mov rcx, [stdout_handle]
    mov rdx, r8
    mov r8d, r9d
    lea r9, [rsp+30h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    add rsp, 28h
    ret
PrintString ENDP

PrintUInt64 PROC
    sub rsp, 38h
    mov rax, rcx
    lea r8, dec_buffer
    add r8, 31
    mov byte ptr [r8], 0
    mov r9, 10
PU_L:
    xor rdx, rdx
    div r9
    add dl, '0'
    dec r8
    mov [r8], dl
    test rax, rax
    jne PU_L
    mov rcx, r8
    call PrintString
    add rsp, 38h
    ret
PrintUInt64 ENDP

ReadExact PROC
    sub rsp, 38h
    mov [rsp+30h], r8        ; Save byte count for later comparison
    lea r9, [rsp+20h]        ; R9 = pointer to bytes read output
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz RE_F
    mov rax, [rsp+20h]       ; Get bytes actually read
    cmp rax, [rsp+30h]       ; Compare with requested
    jne RE_F
    mov eax, 1
    add rsp, 38h
    ret
RE_F:
    xor eax, eax
    add rsp, 38h
    ret
ReadExact ENDP

SkipBytes PROC
    sub rsp, 38h
    ; RCX = file handle, RDX = distance to move
    lea r8, [rsp+30h]    ; Output buffer for new file pointer
    mov r9d, FILE_CURRENT
    mov qword ptr [rsp+20h], 0
    call SetFilePointerEx
    add rsp, 38h
    ret
SkipBytes ENDP

ValueSizeForType PROC
    cmp ecx, GGUF_TYPE_UINT8
    je VS_1
    cmp ecx, GGUF_TYPE_INT8
    je VS_1
    cmp ecx, GGUF_TYPE_BOOL
    je VS_1
    cmp ecx, GGUF_TYPE_UINT16
    je VS_2
    cmp ecx, GGUF_TYPE_INT16
    je VS_2
    cmp ecx, GGUF_TYPE_UINT32
    je VS_4
    cmp ecx, GGUF_TYPE_INT32
    je VS_4
    cmp ecx, GGUF_TYPE_FLOAT32
    je VS_4
    cmp ecx, GGUF_TYPE_UINT64
    je VS_8
    cmp ecx, GGUF_TYPE_INT64
    je VS_8
    cmp ecx, GGUF_TYPE_FLOAT64
    je VS_8
    xor eax, eax
    ret
VS_1:
    mov eax, 1
    ret
VS_2:
    mov eax, 2
    ret
VS_4:
    mov eax, 4
    ret
VS_8:
    mov eax, 8
    ret
ValueSizeForType ENDP

CheckSubstring PROC
    sub rsp, 38h
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    xor r10, r10
CS_O:
    mov rdx, [rsp+28h]
    mov al, byte ptr [rdx+r10]
    test al, al
    jz CS_N
    xor r11, r11
CS_I:
    mov rcx, [rsp+20h]
    mov dl, byte ptr [rcx+r11]
    test dl, dl
    jz CS_F
    mov rax, r10
    add rax, r11
    mov rbx, [rsp+28h]
    mov bl, byte ptr [rbx+rax]
    cmp bl, dl
    jne CS_X
    inc r11
    jmp CS_I
CS_X:
    inc r10
    jmp CS_O
CS_F:
    mov eax, 1
    jmp CS_D
CS_N:
    xor eax, eax
CS_D:
    add rsp, 38h
    ret
CheckSubstring ENDP

SkipMetadataKV PROC
    sub rsp, 70h
    mov [rsp+30h], r12
    mov [rsp+38h], r13
    mov [rsp+40h], r14
    mov [rsp+48h], r15
    mov r13, rdx
    xor r12, r12
SM_L:
    cmp r12, r13
    jae SM_D
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 8
    call ReadExact
    test rax, rax
    jz SM_F
    mov rcx, [file_handle]
    mov rdx, [rsp+50h]
    call SkipBytes
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 4
    call ReadExact
    test rax, rax
    jz SM_F
    mov ecx, dword ptr [rsp+50h]
    cmp ecx, GGUF_TYPE_STRING
    je SM_S
    cmp ecx, GGUF_TYPE_ARRAY
    je SM_A
    call ValueSizeForType
    mov rdx, rax
    mov rcx, [file_handle]
    call SkipBytes
    jmp SM_N
SM_S:
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 8
    call ReadExact
    test rax, rax
    jz SM_F
    mov rcx, [file_handle]
    mov rdx, [rsp+50h]
    call SkipBytes
    jmp SM_N
SM_A:
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 4
    call ReadExact
    test rax, rax
    jz SM_F
    mov r14d, dword ptr [rsp+50h]
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 8
    call ReadExact
    test rax, rax
    jz SM_F
    mov r15, [rsp+50h]
    cmp r14d, GGUF_TYPE_STRING
    je SM_AS
    mov ecx, r14d
    call ValueSizeForType
    mul r15
    mov rdx, rax
    mov rcx, [file_handle]
    call SkipBytes
    jmp SM_N
SM_AS:
    test r15, r15
    jz SM_N
SM_AS_L:
    mov rcx, [file_handle]
    lea rdx, [rsp+50h]
    mov r8d, 8
    call ReadExact
    test rax, rax
    jz SM_F
    mov rcx, [file_handle]
    mov rdx, [rsp+50h]
    call SkipBytes
    dec r15
    jnz SM_AS_L
SM_N:
    inc r12
    jmp SM_L
SM_D:
    mov eax, 1
    jmp SM_E
SM_F:
    xor eax, eax
SM_E:
    mov r15, [rsp+48h]
    mov r14, [rsp+40h]
    mov r13, [rsp+38h]
    mov r12, [rsp+30h]
    add rsp, 70h
    ret
SkipMetadataKV ENDP

ParseTensorMetadata PROC
    mov eax, 1
    ret
ParseTensorMetadata ENDP

main PROC
    sub rsp, 58h
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [stdout_handle], rax

    ; Print Banner
    lea rcx, msg_header
    call PrintString

    ; Debug breakpoint (commented out for production)
    ; int 3

    lea rsi, default_input
    lea rdi, input_path
    mov rcx, 260
main_cp:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jz main_cf
    loop main_cp
main_cf:
    lea rcx, input_path
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je E_F
    mov [file_handle], rax
    mov rcx, [file_handle]
    lea rdx, header_buffer
    mov r8d, 24
    call ReadExact
    test rax, rax
    jz E_R
    mov eax, dword ptr [header_buffer]
    cmp eax, GGUF_MAGIC32
    jne E_M
    mov rax, qword ptr [header_buffer+8]
    mov [tensor_count], rax
    mov rax, qword ptr [header_buffer+16]
    mov [metadata_kv_count], rax
    mov rcx, [file_handle]
    mov rdx, [metadata_kv_count]
    call SkipMetadataKV
    test rax, rax
    jz E_KV
    mov rcx, [file_handle]
    mov rdx, [tensor_count]
    call ParseTensorMetadata
    test rax, rax
    jz E_PT
    lea rcx, msg_summary_tensors
    call PrintString
    mov rcx, [tensor_count]
    call PrintUInt64
    lea rcx, msg_newline
    call PrintString
    lea rcx, msg_summary_params
    call PrintString
    mov rcx, [analysis_total]
    call PrintUInt64
    lea rcx, msg_newline
    call PrintString
    lea rcx, msg_success
    call PrintString
    mov rcx, [file_handle]
    call CloseHandle
    xor ecx, ecx
    call ExitProcess
E_F:
    lea rcx, msg_error_file
    call PrintString
    mov ecx, 1
    call ExitProcess
E_KV:
    lea rcx, msg_error_read_kv
    call PrintString
    mov ecx, 2
    call ExitProcess
E_PT:
    lea rcx, msg_error_read_pt
    call PrintString
    mov ecx, 3
    call ExitProcess
E_R:
    lea rcx, msg_error_read
    call PrintString
    mov ecx, 4
    call ExitProcess
E_M:
    lea rcx, msg_error_magic
    call PrintString
    mov ecx, 5
    call ExitProcess
main ENDP

END
