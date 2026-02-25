;============================================================================
; Simple DumpBin Replacement v6.0
; Basic file analysis tool
;============================================================================

.386
.model flat, stdcall
option casemap :none

;============================================================================
; INCLUDES
;============================================================================

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "6.0.0"
MAX_PATH                equ 260
MAX_FILE_SIZE           equ 10485760   ; 10MB limit
PE_SIGNATURE            equ 00004550h  ; "PE\0\0"
MZ_SIGNATURE            equ 00005A4Dh  ; "MZ"

;============================================================================
; DATA
;============================================================================

.data

szWelcome               db "DumpBin Replacement v", CLI_VERSION, 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0Dh, 0Ah, 0
szMenu                  db "[1] Load file", 0Dh, 0Ah
                        db "[2] Hex dump", 0Dh, 0Ah
                        db "[3] Show PE header", 0Dh, 0Ah
                        db "[4] Exit", 0Dh, 0Ah
                        db "Select option: ", 0
szPromptFile            db "Enter file path: ", 0
szPromptAddress         db "Enter start offset (hex): ", 0
szPromptSize            db "Enter size: ", 0
szFileLoaded            db "[+] File loaded successfully. Size: ", 0
szPEHeader              db 0Dh, 0Ah, "PE Header Found:", 0Dh, 0Ah, 0
szNoPE                  db "[-] No PE header found.", 0Dh, 0Ah, 0
szHexDumpHeader         db 0Dh, 0Ah, "Hex Dump:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szError                 db "[-] ERROR: ", 0
szErrorFileOpen         db "Failed to open file.", 0Dh, 0Ah, 0
szErrorFileRead         db "Failed to read file.", 0Dh, 0Ah, 0
szErrorInvalidFile      db "Invalid file.", 0Dh, 0Ah, 0
szErrorInvalidSize      db "File too large.", 0Dh, 0Ah, 0
szErrorInvalidAddress   db "Invalid address.", 0Dh, 0Ah, 0
szPressAnyKey           db 0Dh, 0Ah, "Press any key to continue...", 0
szNewLine               db 0Dh, 0Ah, 0
szFormatHex             db "%08X ", 0
szFormatDec             db "%d", 0
szFormatByte            db "%02X ", 0
szFormatAscii           db "%c", 0
szBuffer                db MAX_PATH dup(0)
szFilePath              db MAX_PATH dup(0)
fileBuffer              db MAX_FILE_SIZE dup(0)
hStdIn                  dd 0
hStdOut                 dd 0
hFile                   dd 0
dwFileSize              dd 0
dwBytesRead             dd 0

;============================================================================
; CODE
;============================================================================

.code

;----------------------------------------------------------------------------
; Display message
;----------------------------------------------------------------------------
DisplayMessage PROC lpMessage:DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwLen :DWORD
    
    invoke lstrlen, lpMessage
    mov dwLen, eax
    
    invoke WriteConsole, hStdOut, lpMessage, dwLen, addr dwWritten, NULL
    
    ret
DisplayMessage ENDP

;----------------------------------------------------------------------------
; Read string input
;----------------------------------------------------------------------------
ReadString PROC
    LOCAL dwRead :DWORD
    
    invoke ReadConsole, hStdIn, addr szBuffer, 256, addr dwRead, NULL
    
    ; Remove newline
    mov eax, dwRead
    dec eax
    mov byte ptr [szBuffer+eax], 0
    
    ret
ReadString ENDP

;----------------------------------------------------------------------------
; Read integer input
;----------------------------------------------------------------------------
ReadInt PROC
    LOCAL dwRead :DWORD
    
    invoke ReadConsole, hStdIn, addr szBuffer, 256, addr dwRead, NULL
    
    ; Convert string to integer
    mov eax, 0
    lea esi, szBuffer
    
@@convert_loop:
    movzx ecx, byte ptr [esi]
    cmp ecx, 0Dh
    je @@done
    cmp ecx, 0
    je @@done
    
    sub ecx, '0'
    cmp ecx, 9
    ja @@invalid
    
    imul eax, eax, 10
    add eax, ecx
    inc esi
    jmp @@convert_loop
    
@@invalid:
    mov eax, -1
    
@@done:
    ret
ReadInt ENDP

;----------------------------------------------------------------------------
; Read hex input
;----------------------------------------------------------------------------
ReadHex PROC
    LOCAL dwRead :DWORD
    
    invoke ReadConsole, hStdIn, addr szBuffer, 256, addr dwRead, NULL
    
    ; Convert hex string to integer
    mov eax, 0
    lea esi, szBuffer
    
@@convert_loop:
    movzx ecx, byte ptr [esi]
    cmp ecx, 0Dh
    je @@done
    cmp ecx, 0
    je @@done
    
    ; Convert hex digit
    cmp ecx, '0'
    jb @@invalid
    cmp ecx, '9'
    jbe @@is_digit
    
    cmp ecx, 'A'
    jb @@invalid
    cmp ecx, 'F'
    jbe @@is_upper
    
    cmp ecx, 'a'
    jb @@invalid
    cmp ecx, 'f'
    ja @@invalid
    
    sub ecx, 'a'
    add ecx, 10
    jmp @@continue
    
@@is_upper:
    sub ecx, 'A'
    add ecx, 10
    jmp @@continue
    
@@is_digit:
    sub ecx, '0'
    
@@continue:
    shl eax, 4
    add eax, ecx
    inc esi
    jmp @@convert_loop
    
@@invalid:
    mov eax, 0
    
@@done:
    ret
ReadHex ENDP

;----------------------------------------------------------------------------
; Open and read file
;----------------------------------------------------------------------------
ReadInputFile PROC lpFilePath:DWORD
    LOCAL dwFileSizeHigh :DWORD
    
    ; Open file
    invoke CreateFile, lpFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@error_open
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, addr dwFileSizeHigh
    cmp eax, -1
    je @@error_size
    mov dwFileSize, eax
    
    ; Check if file is too large
    cmp eax, MAX_FILE_SIZE
    jg @@error_large
    
    ; Read file
    invoke ReadFile, hFile, addr fileBuffer, dwFileSize, addr dwBytesRead, NULL
    cmp eax, 0
    je @@error_read
    
    ; Close file
    invoke CloseHandle, hFile
    
    mov eax, TRUE
    ret
    
@@error_open:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorFileOpen
    mov eax, FALSE
    ret
    
@@error_size:
    invoke CloseHandle, hFile
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidFile
    mov eax, FALSE
    ret
    
@@error_large:
    invoke CloseHandle, hFile
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidSize
    mov eax, FALSE
    ret
    
@@error_read:
    invoke CloseHandle, hFile
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorFileRead
    mov eax, FALSE
    ret
ReadInputFile ENDP

;----------------------------------------------------------------------------
; Detect PE file
;----------------------------------------------------------------------------
DetectPE PROC
    ; Check if file is large enough
    cmp dwFileSize, 64
    jl @@no_pe
    
    ; Check MZ signature
    mov ax, word ptr [fileBuffer]
    cmp ax, MZ_SIGNATURE
    jne @@no_pe
    
    ; Get PE offset
    mov eax, dword ptr [fileBuffer+60]  ; e_lfanew
    cmp eax, dwFileSize
    jge @@no_pe
    
    ; Check PE signature
    mov eax, dword ptr [fileBuffer+eax]
    cmp eax, PE_SIGNATURE
    jne @@no_pe
    
    mov eax, TRUE
    ret
    
@@no_pe:
    mov eax, FALSE
    ret
DetectPE ENDP

;----------------------------------------------------------------------------
; Show PE header info
;----------------------------------------------------------------------------
ShowPEHeader PROC
    LOCAL pPeHeader :DWORD
    LOCAL dwNumSections :DWORD
    
    ; Get PE offset
    mov eax, dword ptr [fileBuffer+60]  ; e_lfanew
    add eax, offset fileBuffer
    mov pPeHeader, eax
    
    ; Display header
    invoke DisplayMessage, addr szPEHeader
    
    ; Get number of sections
    movzx eax, word ptr [pPeHeader+6]  ; NumberOfSections
    mov dwNumSections, eax
    
    invoke DisplayMessage, addr szFormatDec
    invoke DisplayMessage, addr szNewLine
    
    mov eax, TRUE
    ret
ShowPEHeader ENDP

;----------------------------------------------------------------------------
; Hex dump
;----------------------------------------------------------------------------
HexDump PROC dwOffset:DWORD, dwSize:DWORD
    LOCAL i :DWORD
    LOCAL j :DWORD
    LOCAL bChar :BYTE
    
    ; Check bounds
    mov eax, dwOffset
    add eax, dwSize
    cmp eax, dwFileSize
    jg @@error_bounds
    
    ; Display header
    invoke DisplayMessage, addr szHexDumpHeader
    
    mov i, 0
    
@@dump_loop:
    cmp i, dwSize
    jge @@done
    
    ; Display offset
    mov eax, i
    add eax, dwOffset
    invoke wsprintf, addr szBuffer, addr szFormatHex, eax
    invoke DisplayMessage, addr szBuffer
    
    ; Display hex values
    mov j, 0
    
@@hex_loop:
    cmp j, 16
    jge @@ascii
    
    mov eax, i
    add eax, j
    cmp eax, dwSize
    jge @@ascii
    
    ; Get byte value
    movzx ecx, byte ptr [fileBuffer+eax+dwOffset]
    
    ; Format hex byte
    invoke wsprintf, addr szBuffer, addr szFormatByte, ecx
    invoke DisplayMessage, addr szBuffer
    
    inc j
    jmp @@hex_loop
    
@@ascii:
    ; Display ASCII
    invoke DisplayMessage, addr szHexBuffer
    mov j, 0
    
@@ascii_loop:
    cmp j, 16
    jge @@next_line
    
    mov eax, i
    add eax, j
    cmp eax, dwSize
    jge @@next_line
    
    ; Get byte value
    movzx ecx, byte ptr [fileBuffer+eax+dwOffset]
    
    ; Check if printable
    cmp cl, 20h
    jl @@non_printable
    cmp cl, 7Eh
    jg @@non_printable
    
    mov bChar, cl
    jmp @@print_char
    
@@non_printable:
    mov bChar, '.'
    
@@print_char:
    invoke wsprintf, addr szBuffer, addr szFormatAscii, bChar
    invoke DisplayMessage, addr szBuffer
    
    inc j
    jmp @@ascii_loop
    
@@next_line:
    invoke DisplayMessage, addr szNewLine
    
    add i, 16
    jmp @@dump_loop
    
@@done:
    mov eax, TRUE
    ret
    
@@error_bounds:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidAddress
    mov eax, FALSE
    ret
HexDump ENDP

;----------------------------------------------------------------------------
; Main menu
;----------------------------------------------------------------------------
MainMenu PROC
    LOCAL dwChoice :DWORD
    LOCAL dwOffset :DWORD
    LOCAL dwSize :DWORD
    
@@menu_loop:
    ; Display welcome
    invoke DisplayMessage, addr szWelcome
    
    ; Display menu
    invoke DisplayMessage, addr szMenu
    
    ; Get choice
    call ReadInt
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@option_load
    
    cmp dwChoice, 2
    je @@option_hexdump
    
    cmp dwChoice, 3
    je @@option_peheader
    
    cmp dwChoice, 4
    je @@option_exit
    
    jmp @@menu_loop
    
@@option_load:
    invoke DisplayMessage, addr szPromptFile
    call ReadString
    
    ; Copy buffer to file path
    invoke lstrcpy, addr szFilePath, addr szBuffer
    
    call ReadInputFile, addr szFilePath
    cmp eax, FALSE
    je @@menu_loop
    
    invoke DisplayMessage, addr szFileLoaded
    invoke DisplayMessage, addr szFormatDec
    invoke DisplayMessage, addr szNewLine
    invoke DisplayMessage, addr szPressAnyKey
    call ReadString
    
    jmp @@menu_loop
    
@@option_hexdump:
    invoke DisplayMessage, addr szPromptAddress
    call ReadHex
    mov dwOffset, eax
    
    invoke DisplayMessage, addr szPromptSize
    call ReadInt
    mov dwSize, eax
    
    call HexDump, dwOffset, dwSize
    invoke DisplayMessage, addr szPressAnyKey
    call ReadString
    
    jmp @@menu_loop
    
@@option_peheader:
    call DetectPE
    cmp eax, FALSE
    je @@no_pe
    
    call ShowPEHeader
    jmp @@menu_loop
    
@@no_pe:
    invoke DisplayMessage, addr szNoPE
    invoke DisplayMessage, addr szPressAnyKey
    call ReadString
    jmp @@menu_loop
    
@@option_exit:
    mov eax, 0
    ret
    
MainMenu ENDP

;----------------------------------------------------------------------------
; Main entry point
;----------------------------------------------------------------------------
main PROC
    LOCAL dwExitCode :DWORD
    
    ; Get standard input/output handles
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    ; Run main menu
    call MainMenu
    mov dwExitCode, eax
    
    ; Exit
    invoke ExitProcess, dwExitCode
    ret
main ENDP

END main
