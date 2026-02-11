; OMEGA-POLYGLOT MAXIMUM v3.0 PRO - Professional Reverse Engineering Suite
; Claude/Moonshot/DeepSeek Enhanced PE Analysis Tool
.386
.model flat, stdcall
option casemap:none

; Professional Constants
CLI_VER             equ "3.0 PRO"
MAX_FILE_SIZE       equ 104857600    ; 100MB max

; PE Magic Numbers
PE32_MAGIC          equ 010Bh
PE32P_MAGIC         equ 020Bh

; Section Characteristics
SCN_CNT_CODE        equ 000000020h
SCN_CNT_INIT_DATA   equ 000000040h
SCN_CNT_UNINIT_DATA equ 000000080h
SCN_MEM_EXECUTE     equ 020000000h
SCN_MEM_READ        equ 040000000h
SCN_MEM_WRITE       equ 080000000h
SCN_MEM_DISCARDABLE equ 002000000h
SCN_MEM_SHARED      equ 010000000h

; Directory Indices
DIR_EXPORT          equ 0
DIR_IMPORT          equ 1
DIR_RESOURCE        equ 2
DIR_BASERELOC       equ 5
DIR_DEBUG           equ 6
DIR_TLS             equ 9

include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc

includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

.data
; UI Strings
szWelcome   db "OMEGA-POLYGLOT MAXIMUM v", CLI_VER, 0Dh, 0Ah
            db "Professional Reverse Engineering Suite", 0Dh, 0Ah
            db "Claude | Moonshot | DeepSeek | Codex", 0Dh, 0Ah
            db "========================================", 0Dh, 0Ah, 0

szMenu      db "[1]PE Analysis [2]Import/Export [3]Sections [4]Strings [5]TLS [6]Debug [0]Exit", 0Dh, 0Ah
            db ">", 0

szPrompt    db "Target File: ", 0
szError     db "[-] Error processing file", 0Dh, 0Ah, 0
szSuccess   db "[+] Analysis complete", 0Dh, 0Ah, 0

; Analysis Headers
szPEHeader  db 0Dh, 0Ah, "=== PE ANALYSIS ===", 0Dh, 0Ah, 0
szImports   db 0Dh, 0Ah, "=== IMPORTS ===", 0Dh, 0Ah, 0
szExports   db 0Dh, 0Ah, "=== EXPORTS ===", 0Dh, 0Ah, 0
szSections  db 0Dh, 0Ah, "=== SECTIONS ===", 0Dh, 0Ah, 0
szStrings   db 0Dh, 0Ah, "=== STRINGS ===", 0Dh, 0Ah, 0
szTLS       db 0Dh, 0Ah, "=== TLS ===", 0Dh, 0Ah, 0
szDebug     db 0Dh, 0Ah, "=== DEBUG ===", 0Dh, 0Ah, 0

; Format strings
szFormat    db "%s", 0
szHex       db "%08X", 0
szDecimal   db "%d", 0
szNewLine   db 0Dh, 0Ah, 0
szSpace     db " ", 0

; Analysis result formats
szMachine       db "Machine: %04X", 0Dh, 0Ah, 0
szSectionCount  db "Sections: %d", 0Dh, 0Ah, 0
szEntryPoint    db "Entry Point: %08X", 0Dh, 0Ah, 0
szImageBase     db "Image Base: %08X", 0Dh, 0Ah, 0
szSubsystem     db "Subsystem: %04X", 0Dh, 0Ah, 0

szSectionInfo   db "Section: %s", 0Dh, 0Ah
                db "  VA: %08X  Size: %08X", 0Dh, 0Ah
                db "  Raw: %08X  RawSize: %08X", 0Dh, 0Ah
                db "  Characteristics: %08X", 0Dh, 0Ah, 0

szImportDLL     db "DLL: %s", 0Dh, 0Ah, 0
szImportFunc    db "  Function: %s", 0Dh, 0Ah, 0
szImportOrd     db "  Ordinal: %d", 0Dh, 0Ah, 0

szStringFound   db "[STR] %08X: %s", 0Dh, 0Ah, 0

; Characteristic names
szExecute   db "EXECUTE ", 0
szRead      db "READ ", 0
szWrite     db "WRITE ", 0
szCode      db "CODE ", 0
szData      db "DATA ", 0

.data?
; Handles
hStdIn      dd ?
hStdOut     dd ?
hFile       dd ?

; File data
dwFileSize  dd ?
dwBytesRead dd ?
pFileBase   dd ?

; PE structure pointers
pDosHeader  dd ?
pNtHeaders  dd ?
pFileHeader dd ?
pOptHeader  dd ?
pSections   dd ?

; PE information
dwSectionCount dd ?
dwEntryPoint   dd ?
dwImageBase    dd ?
dwSubsystem    dd ?
bIsPE32Plus    dd ?

; Buffers
szInput     db 512 dup(?)
szTemp      db 1024 dup(?)
pbFileData  db MAX_FILE_SIZE dup(?)
szSecName   db 16 dup(?)

.code

;==============================================================================
; Console output procedure
;==============================================================================
PrintString proc pString:DWORD
    local dwWritten:DWORD, dwLength:DWORD
    
    invoke lstrlen, pString
    mov dwLength, eax
    invoke WriteConsole, hStdOut, pString, dwLength, addr dwWritten, NULL
    ret
PrintString endp

;==============================================================================
; Console input procedure
;==============================================================================
GetInput proc
    local dwRead:DWORD
    
    invoke ReadConsole, hStdIn, addr szInput, 512, addr dwRead, NULL
    mov eax, dwRead
    dec eax
    mov byte ptr [szInput+eax], 0
    ret
GetInput endp

;==============================================================================
; Get integer input
;==============================================================================
GetInteger proc
    local dwRead:DWORD
    
    invoke ReadConsole, hStdIn, addr szInput, 32, addr dwRead, NULL
    xor eax, eax
    lea esi, szInput
    
@@convert:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@done
    sub cl, '0'
    cmp cl, 9
    ja @@done
    imul eax, 10
    add eax, ecx
    inc esi
    jmp @@convert
    
@@done:
    ret
GetInteger endp

;==============================================================================
; Load and validate PE file
;==============================================================================
LoadPEFile proc pFileName:DWORD
    local dwHighSize:DWORD
    
    ; Open file
    invoke CreateFile, pFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, addr dwHighSize
    cmp eax, MAX_FILE_SIZE
    jg @@error_close
    mov dwFileSize, eax
    
    ; Read file
    invoke ReadFile, hFile, addr pbFileData, dwFileSize, addr dwBytesRead, NULL
    test eax, eax
    jz @@error_close
    
    invoke CloseHandle, hFile
    
    ; Validate PE
    lea eax, pbFileData
    mov pFileBase, eax
    
    ; Check DOS header
    mov pDosHeader, eax
    cmp word ptr [eax], 'ZM'
    jne @@error
    
    ; Get NT headers
    mov eax, [eax+3Ch]
    add eax, pFileBase
    mov pNtHeaders, eax
    cmp dword ptr [eax], 'EP'
    jne @@error
    
    ; Setup pointers
    add eax, 4
    mov pFileHeader, eax
    
    movzx ecx, word ptr [eax+2]  ; NumberOfSections
    mov dwSectionCount, ecx
    
    add eax, 20  ; sizeof(IMAGE_FILE_HEADER)
    mov pOptHeader, eax
    
    ; Check PE type
    cmp word ptr [eax], PE32_MAGIC
    je @@is_pe32
    cmp word ptr [eax], PE32P_MAGIC
    je @@is_pe32plus
    jmp @@error
    
@@is_pe32:
    mov bIsPE32Plus, 0
    mov eax, [eax+28]  ; AddressOfEntryPoint
    mov dwEntryPoint, eax
    mov eax, pOptHeader
    mov eax, [eax+52]  ; ImageBase
    mov dwImageBase, eax
    mov eax, pOptHeader
    movzx eax, word ptr [eax+68]  ; Subsystem
    mov dwSubsystem, eax
    mov eax, pOptHeader
    add eax, 224  ; PE32 optional header size
    jmp @@set_sections
    
@@is_pe32plus:
    mov bIsPE32Plus, 1
    mov eax, [eax+24]  ; AddressOfEntryPoint
    mov dwEntryPoint, eax
    mov eax, pOptHeader
    mov eax, [eax+48]  ; ImageBase (lower 32 bits)
    mov dwImageBase, eax
    mov eax, pOptHeader
    movzx eax, word ptr [eax+68]  ; Subsystem
    mov dwSubsystem, eax
    mov eax, pOptHeader
    add eax, 240  ; PE32+ optional header size
    
@@set_sections:
    mov pSections, eax
    mov eax, 1  ; Success
    ret
    
@@error_close:
    invoke CloseHandle, hFile
@@error:
    invoke PrintString, addr szError
    xor eax, eax
    ret
LoadPEFile endp

;==============================================================================
; RVA to File Offset conversion
;==============================================================================
RVA2FileOffset proc dwRVA:DWORD
    local dwSection:DWORD
    
    mov dwSection, 0
    mov ecx, dwSectionCount
    test ecx, ecx
    jz @@not_found
    
@@loop:
    push ecx
    mov eax, dwSection
    imul eax, 40  ; sizeof(IMAGE_SECTION_HEADER)
    add eax, pSections
    
    mov ebx, [eax+12]  ; VirtualAddress
    cmp dwRVA, ebx
    jb @@next
    
    add ebx, [eax+8]   ; VirtualSize
    cmp dwRVA, ebx
    jae @@next
    
    ; Found section
    mov eax, [eax+20]  ; PointerToRawData
    mov ebx, dwRVA
    sub ebx, [eax+12-20]  ; VirtualAddress
    add eax, ebx
    add eax, pFileBase
    pop ecx
    ret
    
@@next:
    inc dwSection
    pop ecx
    loop @@loop
    
@@not_found:
    mov eax, dwRVA
    add eax, pFileBase
    ret
RVA2FileOffset endp

;==============================================================================
; Print section characteristics
;==============================================================================
PrintCharacteristics proc dwChars:DWORD
    mov eax, dwChars
    
    test eax, SCN_MEM_EXECUTE
    jz @@1
    invoke PrintString, addr szExecute
@@1:
    test eax, SCN_MEM_READ
    jz @@2
    invoke PrintString, addr szRead
@@2:
    test eax, SCN_MEM_WRITE
    jz @@3
    invoke PrintString, addr szWrite
@@3:
    test eax, SCN_CNT_CODE
    jz @@4
    invoke PrintString, addr szCode
@@4:
    test eax, SCN_CNT_INIT_DATA
    jz @@5
    invoke PrintString, addr szData
@@5:
    ret
PrintCharacteristics endp

;==============================================================================
; Analyze PE headers
;==============================================================================
AnalyzePEHeaders proc
    invoke PrintString, addr szPEHeader
    
    ; Machine type
    mov eax, pFileHeader
    movzx eax, word ptr [eax]
    invoke wsprintf, addr szTemp, addr szMachine, eax
    invoke PrintString, addr szTemp
    
    ; Section count
    invoke wsprintf, addr szTemp, addr szSectionCount, dwSectionCount
    invoke PrintString, addr szTemp
    
    ; Entry point
    invoke wsprintf, addr szTemp, addr szEntryPoint, dwEntryPoint
    invoke PrintString, addr szTemp
    
    ; Image base
    invoke wsprintf, addr szTemp, addr szImageBase, dwImageBase
    invoke PrintString, addr szTemp
    
    ; Subsystem
    invoke wsprintf, addr szTemp, addr szSubsystem, dwSubsystem
    invoke PrintString, addr szTemp
    
    ret
AnalyzePEHeaders endp

;==============================================================================
; Analyze sections
;==============================================================================
AnalyzeSections proc
    local dwCurrentSection:DWORD
    
    invoke PrintString, addr szSections
    mov dwCurrentSection, 0
    
@@loop:
    mov eax, dwCurrentSection
    cmp eax, dwSectionCount
    jge @@done
    
    ; Get section pointer
    imul eax, 40
    add eax, pSections
    mov esi, eax
    
    ; Copy section name
    push edi
    lea edi, szSecName
    mov ecx, 8
    push esi
    rep movsb
    pop esi
    mov byte ptr [edi], 0
    pop edi
    
    ; Print section info
    push [esi+36]    ; Characteristics
    push [esi+16]    ; SizeOfRawData
    push [esi+20]    ; PointerToRawData
    push [esi+8]     ; VirtualSize
    push [esi+12]    ; VirtualAddress
    push offset szSecName
    push offset szSectionInfo
    push offset szTemp
    call wsprintf
    add esp, 32
    
    invoke PrintString, addr szTemp
    
    ; Print characteristics
    invoke PrintCharacteristics, [esi+36]
    invoke PrintString, addr szNewLine
    
    inc dwCurrentSection
    jmp @@loop
    
@@done:
    ret
AnalyzeSections endp

;==============================================================================
; Analyze imports
;==============================================================================
AnalyzeImports proc
    local pImportDesc:DWORD
    local pThunk:DWORD
    
    invoke PrintString, addr szImports
    
    ; Get import directory
    mov eax, pOptHeader
    add eax, 112  ; Import directory offset
    mov eax, [eax]
    test eax, eax
    jz @@no_imports
    
    invoke RVA2FileOffset, eax
    mov pImportDesc, eax
    
@@import_loop:
    mov esi, pImportDesc
    cmp dword ptr [esi], 0
    je @@done
    
    ; Get DLL name
    mov eax, [esi+12]
    test eax, eax
    jz @@next_import
    
    invoke RVA2FileOffset, eax
    invoke wsprintf, addr szTemp, addr szImportDLL, eax
    invoke PrintString, addr szTemp
    
    ; Get thunk table
    mov eax, [esi]
    test eax, eax
    jnz @@use_ilt
    mov eax, [esi+16]
@@use_ilt:
    invoke RVA2FileOffset, eax
    mov pThunk, eax
    
@@thunk_loop:
    mov edi, pThunk
    mov eax, [edi]
    test eax, eax
    jz @@next_import
    
    test eax, 80000000h
    jnz @@by_ordinal
    
    ; By name
    invoke RVA2FileOffset, eax
    add eax, 2
    invoke wsprintf, addr szTemp, addr szImportFunc, eax
    invoke PrintString, addr szTemp
    jmp @@next_thunk
    
@@by_ordinal:
    and eax, 0FFFFh
    invoke wsprintf, addr szTemp, addr szImportOrd, eax
    invoke PrintString, addr szTemp
    
@@next_thunk:
    add pThunk, 4
    jmp @@thunk_loop
    
@@next_import:
    add pImportDesc, 20
    jmp @@import_loop
    
@@no_imports:
    invoke PrintString, addr szError
@@done:
    ret
AnalyzeImports endp

;==============================================================================
; Extract strings
;==============================================================================
ExtractStrings proc
    local dwOffset:DWORD
    local dwLength:DWORD
    local dwStart:DWORD
    
    invoke PrintString, addr szStrings
    mov dwOffset, 0
    
@@scan_loop:
    mov eax, dwOffset
    cmp eax, dwFileSize
    jge @@done
    
    ; Check for printable character
    mov ebx, pFileBase
    movzx ecx, byte ptr [ebx+eax]
    cmp cl, 20h
    jb @@next_byte
    cmp cl, 7Eh
    ja @@next_byte
    
    ; Found start of potential string
    mov dwStart, eax
    mov dwLength, 0
    
@@string_loop:
    mov eax, dwStart
    add eax, dwLength
    cmp eax, dwFileSize
    jge @@check_length
    
    mov ebx, pFileBase
    movzx ecx, byte ptr [ebx+eax]
    cmp cl, 20h
    jb @@check_length
    cmp cl, 7Eh
    ja @@check_length
    
    ; Store character in temp buffer
    mov ebx, dwLength
    cmp ebx, 1000
    jge @@check_length
    mov szTemp[ebx], cl
    inc dwLength
    jmp @@string_loop
    
@@check_length:
    cmp dwLength, 4
    jl @@skip_string
    
    ; Null terminate and print
    mov eax, dwLength
    mov szTemp[eax], 0
    
    invoke wsprintf, addr szInput, addr szStringFound, dwStart, addr szTemp
    invoke PrintString, addr szInput
    
@@skip_string:
    mov eax, dwStart
    add eax, dwLength
    mov dwOffset, eax
    jmp @@scan_loop
    
@@next_byte:
    inc dwOffset
    jmp @@scan_loop
    
@@done:
    ret
ExtractStrings endp

;==============================================================================
; Main menu
;==============================================================================
MainLoop proc
    local dwChoice:DWORD
    
@@menu_loop:
    invoke PrintString, addr szWelcome
    invoke PrintString, addr szMenu
    
    invoke GetInteger
    mov dwChoice, eax
    
    cmp dwChoice, 0
    je @@exit
    cmp dwChoice, 1
    je @@pe_analysis
    cmp dwChoice, 2
    je @@imports
    cmp dwChoice, 3
    je @@sections
    cmp dwChoice, 4
    je @@strings
    jmp @@menu_loop
    
@@pe_analysis:
    invoke PrintString, addr szPrompt
    invoke GetInput
    invoke LoadPEFile, addr szInput
    test eax, eax
    jz @@menu_loop
    invoke AnalyzePEHeaders
    jmp @@menu_loop
    
@@imports:
    invoke PrintString, addr szPrompt
    invoke GetInput
    invoke LoadPEFile, addr szInput
    test eax, eax
    jz @@menu_loop
    invoke AnalyzeImports
    jmp @@menu_loop
    
@@sections:
    invoke PrintString, addr szPrompt
    invoke GetInput
    invoke LoadPEFile, addr szInput
    test eax, eax
    jz @@menu_loop
    invoke AnalyzeSections
    jmp @@menu_loop
    
@@strings:
    invoke PrintString, addr szPrompt
    invoke GetInput
    invoke LoadPEFile, addr szInput
    test eax, eax
    jz @@menu_loop
    invoke ExtractStrings
    jmp @@menu_loop
    
@@exit:
    ret
MainLoop endp

;==============================================================================
; Program entry point
;==============================================================================
start:
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    invoke MainLoop
    invoke ExitProcess, 0
end start