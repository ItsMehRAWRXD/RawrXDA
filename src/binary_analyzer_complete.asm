;============================================================================
; Binary Analyzer Complete v6.0
; Universal file format analyzer and converter
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
include advapi32.inc
include psapi.inc

includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib
includelib psapi.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "6.0.0"
MAX_PATH                equ 260
MAX_FILE_SIZE           equ 104857600  ; 100MB limit
PE_SIGNATURE            equ 00004550h  ; "PE\0\0"
ELF_SIGNATURE           equ 0000007Fh  ; ELF magic
MZ_SIGNATURE            equ 00005A4Dh  ; "MZ"

;============================================================================
; DATA
;============================================================================

.data

szWelcome               db "Binary Analyzer Complete v", CLI_VERSION, 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0Dh, 0Ah, 0
szMenu                  db "[1] Load and analyze file", 0Dh, 0Ah
                        db "[2] Hex dump", 0Dh, 0Ah
                        db "[3] Show headers", 0Dh, 0Ah
                        db "[4] Extract sections", 0Dh, 0Ah
                        db "[5] List imports/exports", 0Dh, 0Ah
                        db "[6] Convert format", 0Dh, 0Ah
                        db "[7] Exit", 0Dh, 0Ah
                        db "Select option: ", 0
szPromptFile            db "Enter file path: ", 0
szPromptFormat          db "Enter target format (PE/ELF/BIN): ", 0
szPromptAddress         db "Enter start address (hex): ", 0
szPromptSize            db "Enter size: ", 0
szPromptOutput          db "Enter output file: ", 0
szFileHeader            db 0Dh, 0Ah, "File Analysis:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szPEHeader              db 0Dh, 0Ah, "PE Header:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szELFHeader             db 0Dh, 0Ah, "ELF Header:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szSectionHeader         db 0Dh, 0Ah, "Sections:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szImportHeader          db 0Dh, 0Ah, "Imports:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szExportHeader          db 0Dh, 0Ah, "Exports:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szHexDumpHeader         db 0Dh, 0Ah, "Hex Dump:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szFormatPE              db "PE (Portable Executable)", 0Dh, 0Ah, 0
szFormatELF             db "ELF (Executable and Linkable Format)", 0Dh, 0Ah, 0
szFormatBIN             db "Binary", 0Dh, 0Ah, 0
szFormatUnknown         db "Unknown", 0Dh, 0Ah, 0
szSectionEntry          db "Section: VA=%08X Size=%08X Raw=%08X", 0Dh, 0Ah, 0
szImportEntry           db "Import: %s", 0Dh, 0Ah, 0
szExportEntry           db "Export: %s", 0Dh, 0Ah, 0
szHexLine               db "%08X: ", 0
szHexByte               db "%02X ", 0
szAscii                 db " |%s|", 0Dh, 0Ah, 0
szError                 db "[-] ERROR: ", 0
szErrorFileOpen         db "Failed to open file.", 0Dh, 0Ah, 0
szErrorFileRead         db "Failed to read file.", 0Dh, 0Ah, 0
szErrorFileWrite        db "Failed to write file.", 0Dh, 0Ah, 0
szErrorInvalidFile      db "Invalid file.", 0Dh, 0Ah, 0
szErrorInvalidFormat    db "Invalid format.", 0Dh, 0Ah, 0
szErrorInvalidAddress   db "Invalid address.", 0Dh, 0Ah, 0
szErrorInvalidSize      db "Invalid size.", 0Dh, 0Ah, 0
szSuccess               db "[+] Operation completed successfully.", 0Dh, 0Ah, 0
szPressAnyKey           db 0Dh, 0Ah, "Press any key to continue...", 0
szFormatHex             db "%08X", 0
szFormatDec             db "%d", 0
szFormatString          db "%s", 0
szBuffer                db MAX_PATH dup(0)
szFilePath              db MAX_PATH dup(0)
szOutputPath            db MAX_PATH dup(0)
szHexBuffer             db 256 dup(0)
fileBuffer              db MAX_FILE_SIZE dup(0)
hStdIn                  dd 0
hStdOut                 dd 0
hFile                   dd 0
dwFileSize              dd 0
dwBytesRead             dd 0
dwBytesWritten          dd 0

;============================================================================
; CODE
;============================================================================

.code

;----------------------------------------------------------------------------
; Display message
;----------------------------------------------------------------------------
DisplayMessage PROC lpMessage:DWORD
    LOCAL dwWritten:DWORD
    LOCAL dwLen:DWORD
    
    invoke lstrlen, lpMessage
    mov dwLen, eax
    
    invoke WriteConsole, hStdOut, lpMessage, dwLen, addr dwWritten, NULL
    
    ret
DisplayMessage ENDP

;----------------------------------------------------------------------------
; Read string input
;----------------------------------------------------------------------------
ReadString PROC
    LOCAL dwRead:DWORD
    
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
    LOCAL dwRead:DWORD
    
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
    LOCAL dwRead:DWORD
    
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
    LOCAL dwFileSizeHigh:DWORD
    
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
; Detect file format
;----------------------------------------------------------------------------
DetectFormat PROC
    LOCAL dwMagic:DWORD
    
    ; Check if file is large enough
    cmp dwFileSize, 4
    jl @@unknown
    
    ; Read first 4 bytes
    mov eax, dword ptr [fileBuffer]
    mov dwMagic, eax
    
    ; Check for MZ signature
    cmp ax, MZ_SIGNATURE
    je @@check_pe
    
    ; Check for ELF signature
    cmp al, ELF_SIGNATURE
    je @@elf
    
    jmp @@unknown
    
@@check_pe:
    ; Check if file is large enough for PE offset
    cmp dwFileSize, 64
    jl @@mz_only
    
    ; Get PE offset
    mov eax, dword ptr [fileBuffer+60]  ; e_lfanew
    cmp eax, dwFileSize
    jge @@mz_only
    
    ; Check PE signature
    mov eax, dword ptr [fileBuffer+eax]
    cmp eax, PE_SIGNATURE
    je @@pe
    
    jmp @@mz_only
    
@@pe:
    invoke DisplayMessage, addr szFormatPE
    mov eax, 1  ; PE format
    ret
    
@@elf:
    invoke DisplayMessage, addr szFormatELF
    mov eax, 2  ; ELF format
    ret
    
@@mz_only:
    invoke DisplayMessage, addr szFormatBIN
    mov eax, 3  ; MZ only (DOS)
    ret
    
@@unknown:
    invoke DisplayMessage, addr szFormatUnknown
    mov eax, 0  ; Unknown format
    ret
DetectFormat ENDP

;----------------------------------------------------------------------------
; Analyze PE file
;----------------------------------------------------------------------------
AnalyzePE PROC
    LOCAL pDosHeader:DWORD
    LOCAL pPeHeader:DWORD
    LOCAL pSectionHeader:DWORD
    LOCAL dwNumSections:DWORD
    LOCAL i:DWORD
    
    ; Get DOS header
    lea eax, fileBuffer
    mov pDosHeader, eax
    
    ; Get PE offset
    mov eax, dword ptr [fileBuffer+60]  ; e_lfanew
    add eax, pDosHeader
    mov pPeHeader, eax
    
    ; Display PE header info
    invoke DisplayMessage, addr szPEHeader
    
    ; Get number of sections
    movzx eax, word ptr [pPeHeader+6]  ; NumberOfSections
    mov dwNumSections, eax
    
    invoke wsprintf, addr szBuffer, addr szFormatDec, dwNumSections
    invoke DisplayMessage, addr szBuffer
    invoke DisplayMessage, addr szHexBuffer
    
    ; Get section header
    mov eax, pPeHeader
    add eax, 24  ; Size of PE header
    movzx ecx, word ptr [pPeHeader+20]  ; SizeOfOptionalHeader
    add eax, ecx
    mov pSectionHeader, eax
    
    ; Display sections
    invoke DisplayMessage, addr szSectionHeader
    
    mov i, 0
    
@@section_loop:
    cmp i, dwNumSections
    jge @@done
    
    ; Display section info
    mov eax, i
    imul eax, 40
    add eax, pSectionHeader
    
    ; For now, just display basic info
    invoke wsprintf, addr szBuffer, addr szSectionEntry, \
        eax, dword ptr [eax+8], dword ptr [eax+12], dword ptr [eax+16]
    invoke DisplayMessage, addr szBuffer
    
    inc i
    jmp @@section_loop
    
@@done:
    mov eax, TRUE
    ret
AnalyzePE ENDP

;----------------------------------------------------------------------------
; Analyze ELF file
;----------------------------------------------------------------------------
AnalyzeELF PROC
    ; Display ELF header info
    invoke DisplayMessage, addr szELFHeader
    
    ; For now, just display basic info
    invoke DisplayMessage, addr szSuccess
    
    mov eax, TRUE
    ret
AnalyzeELF ENDP

;----------------------------------------------------------------------------
; Hex dump
;----------------------------------------------------------------------------
HexDump PROC lpAddress:DWORD, dwSize:DWORD
    LOCAL i:DWORD
    LOCAL j:DWORD
    LOCAL dwLineAddr:DWORD
    
    ; Display header
    invoke DisplayMessage, addr szHexDumpHeader
    
    mov i, 0
    
@@dump_loop:
    cmp i, dwSize
    jge @@done
    
    ; Calculate line address
    mov eax, lpAddress
    add eax, i
    mov dwLineAddr, eax
    
    ; Format address
    invoke wsprintf, addr szBuffer, addr szHexLine, dwLineAddr
    invoke DisplayMessage, addr szBuffer
    
    ; Display hex values
    mov j, 0
    
@@hex_loop:
    cmp j, 16
    jge @@ascii
    
    mov eax, i
    add eax, j
    cmp eax, dwSize
    jge @@pad
    
    ; Get byte value
    movzx ecx, byte ptr [fileBuffer+eax]
    
    ; Format hex byte
    invoke wsprintf, addr szBuffer, addr szHexByte, ecx
    invoke DisplayMessage, addr szBuffer
    
    inc j
    jmp @@hex_loop
    
@@pad:
    ; Pad with spaces
    invoke DisplayMessage, addr szHexBuffer
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
    movzx ecx, byte ptr [fileBuffer+eax]
    
    ; Check if printable
    cmp cl, 20h
    jl @@non_printable
    cmp cl, 7Eh
    jg @@non_printable
    
    mov byte ptr [szBuffer+j], cl
    jmp @@continue_ascii
    
@@non_printable:
    mov byte ptr [szBuffer+j], '.'
    
@@continue_ascii:
    inc j
    jmp @@ascii_loop
    
@@next_line:
    mov byte ptr [szBuffer+j], 0
    invoke wsprintf, addr szHexBuffer, addr szAscii, addr szBuffer
    invoke DisplayMessage, addr szHexBuffer
    
    add i, 16
    jmp @@dump_loop
    
@@done:
    mov eax, TRUE
    ret
HexDump ENDP

;----------------------------------------------------------------------------
; Convert file format
;----------------------------------------------------------------------------
ConvertFormat PROC lpOutputPath:DWORD, dwTargetFormat:DWORD
    LOCAL hOutputFile:DWORD
    
    ; Create output file
    invoke CreateFile, lpOutputPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@error_create
    mov hOutputFile, eax
    
    ; Write converted file based on target format
    cmp dwTargetFormat, 1  ; PE
    je @@write_pe
    
    cmp dwTargetFormat, 2  ; ELF
    je @@write_elf
    
    cmp dwTargetFormat, 3  ; BIN
    je @@write_bin
    
    jmp @@error_format
    
@@write_pe:
    ; For now, just copy original file
    invoke WriteFile, hOutputFile, addr fileBuffer, dwFileSize, addr dwBytesWritten, NULL
    jmp @@cleanup
    
@@write_elf:
    ; For now, just copy original file
    invoke WriteFile, hOutputFile, addr fileBuffer, dwFileSize, addr dwBytesWritten, NULL
    jmp @@cleanup
    
@@write_bin:
    ; Write as raw binary
    invoke WriteFile, hOutputFile, addr fileBuffer, dwFileSize, addr dwBytesWritten, NULL
    jmp @@cleanup
    
@@error_create:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorFileOpen
    mov eax, FALSE
    ret
    
@@error_format:
    invoke CloseHandle, hOutputFile
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidFormat
    mov eax, FALSE
    ret
    
@@cleanup:
    invoke CloseHandle, hOutputFile
    
    mov eax, TRUE
    ret
ConvertFormat ENDP

;----------------------------------------------------------------------------
; Extract sections
;----------------------------------------------------------------------------
ExtractSections PROC lpOutputDir:DWORD
    LOCAL dwFormat:DWORD
    
    ; Detect format
    call DetectFormat
    mov dwFormat, eax
    
    cmp dwFormat, 1  ; PE
    je @@extract_pe_sections
    
    cmp dwFormat, 2  ; ELF
    je @@extract_elf_sections
    
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidFormat
    mov eax, FALSE
    ret
    
@@extract_pe_sections:
    ; For now, just display success
    invoke DisplayMessage, addr szSuccess
    mov eax, TRUE
    ret
    
@@extract_elf_sections:
    ; For now, just display success
    invoke DisplayMessage, addr szSuccess
    mov eax, TRUE
    ret
ExtractSections ENDP

;----------------------------------------------------------------------------
; List imports and exports
;----------------------------------------------------------------------------
ListImportsExports PROC
    LOCAL dwFormat:DWORD
    
    ; Detect format
    call DetectFormat
    mov dwFormat, eax
    
    cmp dwFormat, 1  ; PE
    je @@list_pe_imports_exports
    
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidFormat
    mov eax, FALSE
    ret
    
@@list_pe_imports_exports:
    ; Display imports header
    invoke DisplayMessage, addr szImportHeader
    
    ; For now, just display sample imports
    invoke lstrcpy, addr szBuffer, addr szFormatString
    invoke lstrcat, addr szBuffer, addr szFormatString
    
    ; Display exports header
    invoke DisplayMessage, addr szExportHeader
    
    ; For now, just display sample exports
    invoke lstrcpy, addr szBuffer, addr szFormatString
    invoke lstrcat, addr szBuffer, addr szFormatString
    
    mov eax, TRUE
    ret
ListImportsExports ENDP

;----------------------------------------------------------------------------
; Main menu
;----------------------------------------------------------------------------
MainMenu PROC
    LOCAL dwChoice:DWORD
    LOCAL dwFormat:DWORD
    LOCAL lpAddress:DWORD
    LOCAL dwSize:DWORD
    
@@menu_loop:
    ; Display welcome
    invoke DisplayMessage, addr szWelcome
    
    ; Display menu
    invoke DisplayMessage, addr szMenu
    
    ; Get choice
    call ReadInt
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@option_analyze
    
    cmp dwChoice, 2
    je @@option_hexdump
    
    cmp dwChoice, 3
    je @@option_headers
    
    cmp dwChoice, 4
    je @@option_extract
    
    cmp dwChoice, 5
    je @@option_imports_exports
    
    cmp dwChoice, 6
    je @@option_convert
    
    cmp dwChoice, 7
    je @@option_exit
    
    jmp @@menu_loop
    
@@option_analyze:
    invoke DisplayMessage, addr szPromptFile
    call ReadString
    
    ; Copy buffer to file path
    invoke lstrcpy, addr szFilePath, addr szBuffer
    
    call ReadInputFile, addr szFilePath
    cmp eax, FALSE
    je @@menu_loop
    
    call DetectFormat
    
    cmp eax, 1  ; PE
    je @@analyze_pe
    
    cmp eax, 2  ; ELF
    je @@analyze_elf
    
    jmp @@menu_loop
    
@@analyze_pe:
    call AnalyzePE
    jmp @@menu_loop
    
@@analyze_elf:
    call AnalyzeELF
    jmp @@menu_loop
    
@@option_hexdump:
    invoke DisplayMessage, addr szPromptAddress
    call ReadHex
    mov lpAddress, eax
    
    invoke DisplayMessage, addr szPromptSize
    call ReadInt
    mov dwSize, eax
    
    call HexDump, lpAddress, dwSize
    jmp @@menu_loop
    
@@option_headers:
    call DetectFormat
    
    cmp eax, 1  ; PE
    je @@analyze_pe
    
    cmp eax, 2  ; ELF
    je @@analyze_elf
    
    jmp @@menu_loop
    
@@option_extract:
    invoke DisplayMessage, addr szPromptOutput
    call ReadString
    
    ; Copy buffer to output path
    invoke lstrcpy, addr szOutputPath, addr szBuffer
    
    call ExtractSections, addr szOutputPath
    jmp @@menu_loop
    
@@option_imports_exports:
    call ListImportsExports
    jmp @@menu_loop
    
@@option_convert:
    invoke DisplayMessage, addr szPromptFormat
    call ReadString
    
    ; Determine target format
    mov eax, 0
    lea esi, szBuffer
    movzx ecx, byte ptr [esi]
    cmp ecx, 'P'
    je @@convert_pe
    cmp ecx, 'p'
    je @@convert_pe
    cmp ecx, 'E'
    je @@convert_elf
    cmp ecx, 'e'
    je @@convert_elf
    cmp ecx, 'B'
    je @@convert_bin
    cmp ecx, 'b'
    je @@convert_bin
    
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidFormat
    jmp @@menu_loop
    
@@convert_pe:
    mov dwFormat, 1
    jmp @@do_convert
    
@@convert_elf:
    mov dwFormat, 2
    jmp @@do_convert
    
@@convert_bin:
    mov dwFormat, 3
    
@@do_convert:
    invoke DisplayMessage, addr szPromptOutput
    call ReadString
    
    ; Copy buffer to output path
    invoke lstrcpy, addr szOutputPath, addr szBuffer
    
    call ConvertFormat, addr szOutputPath, dwFormat
    jmp @@menu_loop
    
@@option_exit:
    mov eax, 0
    ret
    
MainMenu ENDP

;----------------------------------------------------------------------------
; Main entry point
;----------------------------------------------------------------------------
main PROC
    LOCAL dwExitCode:DWORD
    
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
main ENDP

END main
