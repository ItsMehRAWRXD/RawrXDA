; RawrXD GGUF Analyzer - Minimalist Working Version (64-bit)
; Focuses on successful file reading and header validation

OPTION CASEMAP:NONE

EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN ExitProcess:PROC
EXTERN GetLastError:PROC

; Constants
STD_OUTPUT_HANDLE   EQU -11
GENERIC_READ        EQU 80000000h
FILE_SHARE_READ     EQU 1
OPEN_EXISTING       EQU 3
INVALID_HANDLE_VALUE EQU -1
GGUF_MAGIC32        EQU 46554747h  ; "GGUF" in little-endian

.data

; Messages
msg_banner          DB "=== RawrXD GGUF Analyzer ===", 0Dh, 0Ah, 0
msg_opening_file    DB "Opening file...", 0Dh, 0Ah, 0
msg_file_opened     DB "File opened successfully", 0Dh, 0Ah, 0
msg_reading_header  DB "Reading header...", 0Dh, 0Ah, 0
msg_header_read     DB "Header read successfully", 0Dh, 0Ah, 0
msg_magic_ok        DB "Magic validated: ", 0
msg_version_ok      DB "Version: ", 0
msg_copy_done       DB "Path copy done", 0Dh, 0Ah, 0
msg_error_file      DB "ERROR: Failed to open file", 0Dh, 0Ah, 0
msg_error_read      DB "ERROR: Failed to read file", 0Dh, 0Ah, 0
msg_error_magic     DB "ERROR: Invalid GGUF magic", 0Dh, 0Ah, 0
msg_error_version   DB "ERROR: Invalid version", 0Dh, 0Ah, 0
msg_success         DB "Successfully processed GGUF file", 0Dh, 0Ah, 0
msg_newline         DB 0Dh, 0Ah, 0
msg_hex_prefix      DB "0x", 0

; File path
default_path        DB "D:\OllamaModels\BigDaddyG-Q2_K-ULTRA.gguf", 0
file_path           DB 260 DUP(0)

; Global handles and buffers
stdout_handle       DQ 0
file_handle         DQ 0
header_buffer       DB 16 DUP(0)
bytes_read_buffer   DQ 0
error_code_buffer   DQ 0
hex_buffer          DB 16 DUP(0)

.code

; Simple print string to stdout
PrintString PROC
    sub rsp, 48h
    mov r8, rcx              ; r8 = string pointer
    xor r9, r9
    xor eax, eax

PS_Len:
    cmp byte ptr [r8+r9], 0
    je PS_Write
    inc r9
    cmp r9, 4096
    jb PS_Len

PS_Write:
    mov rcx, [stdout_handle]
    mov rdx, r8
    mov r8d, r9d
    lea r9, [rsp+28h]        ; bytes written
    mov qword ptr [rsp+20h], 0
    call WriteFile
    add rsp, 48h
    ret
PrintString ENDP

; Print 64-bit unsigned integer as decimal
PrintUInt64 PROC
    sub rsp, 48h
    mov rax, rcx             ; rax = number to print
    lea r8, hex_buffer
    add r8, 15
    mov byte ptr [r8], 0
    
    mov r9, 10
    
PU_Loop:
    xor rdx, rdx
    div r9
    add dl, '0'
    dec r8
    mov [r8], dl
    cmp rax, 0
    jne PU_Loop
    
    mov rcx, r8
    call PrintString
    add rsp, 48h
    ret
PrintUInt64 ENDP

; Print 32-bit hex value
PrintHex32 PROC
    sub rsp, 48h
    mov [rsp+30h], rsi       ; Save RSI
    mov [rsp+38h], rbx       ; Save RBX
    
    mov ebx, ecx             ; ebx = value to print
    lea rsi, hex_buffer
    
    ; Build the string in hex_buffer
    mov ecx, 8               ; 8 hex digits
PH_BuildLoop:
    mov eax, ebx
    rol ebx, 4               ; Rotate left to get next nibble
    and eax, 0Fh
    cmp al, 10
    jl PH_Digit
    add al, 'A' - 10
    jmp PH_Store
PH_Digit:
    add al, '0'
PH_Store:
    mov [rsi], al
    inc rsi
    loop PH_BuildLoop
    
    mov byte ptr [rsi], 0    ; Null terminator
    
    ; Print "0x"
    lea rcx, msg_hex_prefix
    call PrintString
    
    ; Print the hex digits
    lea rcx, hex_buffer
    call PrintString
    
    mov rsi, [rsp+30h]       ; Restore RSI
    mov rbx, [rsp+38h]       ; Restore RBX
    add rsp, 48h
    ret
PrintHex32 ENDP

main PROC
    sub rsp, 0C8h            ; 200 bytes (192 + 8) to align RSP (16n+8 - 200 = 16k)
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [stdout_handle], rax
    
    ; Print banner
    lea rcx, msg_banner
    call PrintString
    ; -- REMOVED TEST EXIT --

    ; Use default path directly
    lea rcx, default_path

    ; Debug: path ready
    lea rcx, msg_copy_done
    call PrintString

    ; Print opening message
    lea rcx, msg_opening_file
    call PrintString

    ; Open file
    lea rcx, default_path
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je FileOpenError
    
    mov [file_handle], rax
    
    ; Print file opened
    lea rcx, msg_file_opened
    call PrintString
    
    ; Print reading header
    lea rcx, msg_reading_header
    call PrintString
    
    ; Read file header (16 bytes)
    mov rcx, [file_handle]
    lea rdx, header_buffer
    mov r8d, 16
    mov r9, offset bytes_read_buffer
    mov qword ptr [rsp+20h], 0
    call ReadFile
    
    test eax, eax
    jz FileReadError
    
    ; Print header read
    lea rcx, msg_header_read
    call PrintString
    
    ; Validate magic
    mov eax, dword ptr [header_buffer]
    cmp eax, GGUF_MAGIC32
    jne MagicError
    
    ; Print magic OK
    lea rcx, msg_magic_ok
    call PrintString
    mov ecx, dword ptr [header_buffer]
    call PrintHex32
    lea rcx, msg_newline
    call PrintString
    
    ; Check version (bytes 4-7)
    mov eax, dword ptr [header_buffer+4]
    cmp eax, 3
    jne VersionError
    
    lea rcx, msg_version_ok
    call PrintString
    mov ecx, dword ptr [header_buffer+4]
    call PrintUInt64
    lea rcx, msg_newline
    call PrintString
    
    ; Success
    lea rcx, msg_success
    call PrintString
    
    ; Close file
    mov rcx, [file_handle]
    call CloseHandle
    
    xor ecx, ecx
    call ExitProcess
    
FileOpenError:
    lea rcx, msg_error_file
    call PrintString
    mov ecx, 1
    call ExitProcess
    
FileReadError:
    lea rcx, msg_error_read
    call PrintString
    call GetLastError
    mov [error_code_buffer], rax
    mov rcx, rax
    call PrintUInt64
    mov ecx, 2
    call ExitProcess
    
MagicError:
    lea rcx, msg_error_magic
    call PrintString
    mov ecx, 3
    call ExitProcess
    
VersionError:
    lea rcx, msg_error_version
    call PrintString
    mov ecx, 4
    call ExitProcess
    
main ENDP

END
