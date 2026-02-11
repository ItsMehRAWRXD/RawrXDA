; dumpbin_simple.asm - Simple PE Dumper
; Minimal version that compiles without errors

.386
.model flat, stdcall
option casemap:none

; ============================================================================
; INCLUDES
; ============================================================================
include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; STRUCTURES
; ============================================================================
IMAGE_DOS_HEADER STRUCT
    e_magic     WORD ?
    e_cblp      WORD ?
    e_cp        WORD ?
    e_crlc      WORD ?
    e_cparhdr   WORD ?
    e_minalloc  WORD ?
    e_maxalloc  WORD ?
    e_ss        WORD ?
    e_sp        WORD ?
    e_csum      WORD ?
    e_ip        WORD ?
    e_cs        WORD ?
    e_lfarlc    WORD ?
    e_ovno      WORD ?
    e_res       WORD 4 dup(?)
    e_oemid     WORD ?
    e_oeminfo   WORD ?
    e_res2      WORD 10 dup(?)
    e_lfanew    DWORD ?
IMAGE_DOS_HEADER ENDS

; ============================================================================
; DATA SECTION
; ============================================================================
.data
    szUsage         db "Usage: dumpbin_simple.exe <PE_file>",13,10,0
    szErrorOpen     db "[-] Failed to open file",13,10,0
    szErrorMap      db "[-] Failed to create file mapping",13,10,0
    szErrorView     db "[-] Failed to map view of file",13,10,0
    szErrorValid    db "[-] Not a valid PE file",13,10,0
    szSuccess       db "[+] PE Analysis Complete",13,10,0
    
    szHdrDOS        db 13,10,"=== DOS HEADER ===",13,10,0
    szHdrPE         db 13,10,"=== PE HEADER ===",13,10,0
    szHdrFile       db 13,10,"=== FILE HEADER ===",13,10,0
    szHdrOptional   db 13,10,"=== OPTIONAL HEADER ===",13,10,0
    szHdrSections   db 13,10,"=== SECTION HEADERS ===",13,10,0
    
    szMagic         db "Magic:                0x%04X",13,10,0
    szMachine       db "Machine:              0x%04X",13,10,0
    szSections      db "Number of Sections:   %d",13,10,0
    szTimeDate      db "Time Date Stamp:      0x%08X",13,10,0
    szCharacteristics db "Characteristics:      0x%04X",13,10,0
    
    szEntryPoint    db "Entry Point:          0x%08X",13,10,0
    szImageBase     db "Image Base:           0x%08X",13,10,0
    szImageSize     db "Image Size:           0x%08X",13,10,0
    szSubsystem     db "Subsystem:            0x%04X",13,10,0
    
    szSectionFmt    db 13,10,"Section: %s",13,10,0
    szVirtSize      db "  Virtual Size:       0x%08X",13,10,0
    szVirtAddr      db "  Virtual Address:    0x%08X",13,10,0
    szRawSize       db "  Raw Size:           0x%08X",13,10,0
    szRawAddr       db "  Raw Address:        0x%08X",13,10,0
    szSectionChars  db "  Characteristics:    0x%08X",13,10,0
    
    hFile           dd 0
    hMapping        dd 0
    pBase           dd 0
    fileSize        dd 0
    pDOS            dd 0
    pNT             dd 0
    pFileHdr        dd 0
    pOptHdr         dd 0
    pSectionHdr     dd 0
    numSections     dd 0

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ----------------------------------------------------------------------------
; printf wrapper - simplified
; ----------------------------------------------------------------------------
printf proc C fmt:DWORD, arg1:DWORD, arg2:DWORD, arg3:DWORD, arg4:DWORD
    LOCAL buffer[1024]:BYTE
    LOCAL written:DWORD
    
    ; Format string
    invoke wsprintf, addr buffer, fmt, arg1, arg2, arg3, arg4
    
    ; Write to stdout
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    invoke lstrlen, addr buffer
    invoke WriteFile, eax, addr buffer, eax, addr written, NULL
    
    ret
printf endp

; ----------------------------------------------------------------------------
; Print error and exit
; ----------------------------------------------------------------------------
ErrorExit proc msg:DWORD
    invoke printf, msg, 0, 0, 0, 0
    invoke ExitProcess, 1
    ret
ErrorExit endp

; ----------------------------------------------------------------------------
; Validate PE file
; ----------------------------------------------------------------------------
ValidatePE proc
    ; Check DOS signature
    mov esi, pDOS
    mov ax, [esi].IMAGE_DOS_HEADER.e_magic
    cmp ax, 5A4Dh                   ; "MZ"
    jne @invalid
    
    ; Check PE signature
    mov esi, pDOS
    mov eax, [esi].IMAGE_DOS_HEADER.e_lfanew
    add eax, pBase
    mov pNT, eax
    
    mov esi, eax
    mov eax, [esi]
    cmp eax, 00004550h              ; "PE\0\0"
    jne @invalid
    
    mov eax, 1
    ret
    
@invalid:
    xor eax, eax
    ret
ValidatePE endp

; ----------------------------------------------------------------------------
; Dump DOS Header
; ----------------------------------------------------------------------------
DumpDOSHeader proc
    invoke printf, offset szHdrDOS, 0, 0, 0, 0
    invoke printf, offset szMagic, 5A4Dh, 0, 0, 0
    invoke printf, offset szTimeDate, 0, 0, 0, 0
    ret
DumpDOSHeader endp

; ----------------------------------------------------------------------------
; Dump PE File Header
; ----------------------------------------------------------------------------
DumpPEFileHeader proc
    LOCAL machine:WORD
    LOCAL numSecs:WORD
    LOCAL timeDate:DWORD
    LOCAL chars:WORD
    
    invoke printf, offset szHdrFile, 0, 0, 0, 0
    
    ; Calculate offsets
    mov esi, pNT
    add esi, 4                      ; Skip "PE\0\0"
    mov pFileHdr, esi
    
    ; Load values into registers then variables
    mov ax, [esi+0]                 ; Machine
    mov machine, ax
    
    mov ax, [esi+2]                 ; NumberOfSections
    mov numSecs, ax
    movzx eax, ax
    mov numSections, eax
    
    mov eax, [esi+4]                ; TimeDateStamp
    mov timeDate, eax
    
    mov ax, [esi+18]                ; Characteristics
    mov chars, ax
    
    ; Print values
    invoke printf, offset szMachine, machine, 0, 0, 0
    invoke printf, offset szSections, numSecs, 0, 0, 0
    invoke printf, offset szTimeDate, timeDate, 0, 0, 0
    invoke printf, offset szCharacteristics, chars, 0, 0, 0
    
    ret
DumpPEFileHeader endp

; ----------------------------------------------------------------------------
; Dump Optional Header
; ----------------------------------------------------------------------------
DumpOptionalHeader proc
    LOCAL entryPoint:DWORD
    LOCAL imageBase:DWORD
    LOCAL imageSize:DWORD
    LOCAL subsystem:WORD
    
    invoke printf, offset szHdrOptional, 0, 0, 0, 0
    
    mov esi, pFileHdr
    add esi, 20                     ; Size of FILE_HEADER
    mov pOptHdr, esi
    
    ; Load values
    mov eax, [esi+16]               ; AddressOfEntryPoint
    mov entryPoint, eax
    
    mov eax, [esi+28]               ; ImageBase
    mov imageBase, eax
    
    mov eax, [esi+80]               ; SizeOfImage
    mov imageSize, eax
    
    mov ax, [esi+68]                ; Subsystem
    mov subsystem, ax
    
    invoke printf, offset szEntryPoint, entryPoint, 0, 0, 0
    invoke printf, offset szImageBase, imageBase, 0, 0, 0
    invoke printf, offset szImageSize, imageSize, 0, 0, 0
    invoke printf, offset szSubsystem, subsystem, 0, 0, 0
    
    ret
DumpOptionalHeader endp

; ----------------------------------------------------------------------------
; Dump Section Headers
; ----------------------------------------------------------------------------
DumpSectionHeaders proc
    LOCAL i:DWORD
    LOCAL vSize:DWORD
    LOCAL vAddr:DWORD
    LOCAL rSize:DWORD
    LOCAL rAddr:DWORD
    LOCAL chars:DWORD
    
    invoke printf, offset szHdrSections, 0, 0, 0, 0
    
    mov esi, pOptHdr
    add esi, 224                    ; Size of OPTIONAL_HEADER32
    mov pSectionHdr, esi
    
    mov i, 0
    
@next_section:
    mov eax, i
    cmp eax, numSections
    jge @done
    
    ; Print section name (8 bytes)
    invoke printf, offset szSectionFmt, esi, 0, 0, 0
    
    ; Load all values to registers first
    mov eax, [esi+8]                ; VirtualSize
    mov vSize, eax
    
    mov eax, [esi+12]               ; VirtualAddress
    mov vAddr, eax
    
    mov eax, [esi+16]               ; SizeOfRawData
    mov rSize, eax
    
    mov eax, [esi+20]               ; PointerToRawData
    mov rAddr, eax
    
    mov eax, [esi+36]               ; Characteristics
    mov chars, eax
    
    invoke printf, offset szVirtSize, vSize, 0, 0, 0
    invoke printf, offset szVirtAddr, vAddr, 0, 0, 0
    invoke printf, offset szRawSize, rSize, 0, 0, 0
    invoke printf, offset szRawAddr, rAddr, 0, 0, 0
    invoke printf, offset szSectionChars, chars, 0, 0, 0
    
    add esi, 40                     ; Next section header
    inc i
    jmp @next_section
    
@done:
    ret
DumpSectionHeaders endp

; ----------------------------------------------------------------------------
; Main entry point
; ----------------------------------------------------------------------------
main proc argc:DWORD, argv:PTR DWORD
    LOCAL fileName:DWORD
    
    ; Check arguments
    cmp argc, 2
    jge @have_args
    
    invoke printf, offset szUsage, 0, 0, 0, 0
    invoke ExitProcess, 1
    
@have_args:
    mov esi, argv
    add esi, 4                      ; argv[1]
    mov eax, [esi]
    mov fileName, eax
    
    ; Open file
    invoke CreateFile, fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @open_fail
    
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov fileSize, eax
    
    ; Create file mapping
    invoke CreateFileMapping, hFile, NULL, PAGE_READONLY, 0, 0, NULL
    test eax, eax
    jz @map_fail
    
    mov hMapping, eax
    
    ; Map view of file
    invoke MapViewOfFile, hMapping, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @view_fail
    
    mov pBase, eax
    mov pDOS, eax
    
    ; Validate PE
    call ValidatePE
    test eax, eax
    jz @invalid_pe
    
    ; Dump all headers
    call DumpDOSHeader
    call DumpPEFileHeader
    call DumpOptionalHeader
    call DumpSectionHeaders
    
    invoke printf, offset szSuccess, 0, 0, 0, 0
    
    ; Cleanup
    invoke UnmapViewOfFile, pBase
    invoke CloseHandle, hMapping
    invoke CloseHandle, hFile
    
    xor eax, eax
    ret
    
@open_fail:
    invoke ErrorExit, offset szErrorOpen
    ret
    
@map_fail:
    invoke CloseHandle, hFile
    invoke ErrorExit, offset szErrorMap
    ret
    
@view_fail:
    invoke CloseHandle, hMapping
    invoke CloseHandle, hFile
    invoke ErrorExit, offset szErrorView
    ret
    
@invalid_pe:
    invoke UnmapViewOfFile, pBase
    invoke CloseHandle, hMapping
    invoke CloseHandle, hFile
    invoke ErrorExit, offset szErrorValid
    ret
    
main endp

; ============================================================================
; ENTRY POINT
; ============================================================================
start:
    invoke GetCommandLine
    invoke CommandLineToArgvW, eax, offset argc
    
    invoke main, argc, offset argv
    
    invoke ExitProcess, eax

.data?
    argc    dd ?
    argv    dd ?

end start
