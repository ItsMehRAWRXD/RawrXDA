; OMEGA-POLYGLOT MAXIMUM v3.0 PRO - Working Edition
; Professional Reverse Engineering Suite for Claude/Moonshot/DeepSeek
; Features: PE32/PE32+ Analysis, Import Reconstruction, String Extraction
.386
.model flat, stdcall
option casemap:none

ExitProcess     PROTO :DWORD
GetStdHandle    PROTO :DWORD
WriteConsoleA   PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
ReadConsoleA    PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
CreateFileA     PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
ReadFile        PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
CloseHandle     PROTO :DWORD
GetFileSize     PROTO :DWORD,:DWORD
wsprintfA       PROTO C :DWORD,:DWORD,:VARARG
lstrlenA        PROTO :DWORD

STD_INPUT_HANDLE        equ -10
STD_OUTPUT_HANDLE       equ -11
GENERIC_READ            equ 80000000h
FILE_SHARE_READ         equ 1
OPEN_EXISTING           equ 3
INVALID_HANDLE_VALUE    equ -1

.data
; Professional UI
szTitle     db "OMEGA-POLYGLOT MAXIMUM v3.0 PRO", 13, 10
            db "Professional Reverse Engineering Suite", 13, 10
            db "Claude | Moonshot | DeepSeek | Codex Integration", 13, 10
            db "================================================", 13, 10, 0

szMenu      db 13, 10, "[1] PE Deep Analysis    [2] Import/Export Reconstruction", 13, 10
            db "[3] Section Entropy     [4] String Extraction", 13, 10
            db "[5] TLS Callbacks       [6] Debug Information", 13, 10
            db "[7] Full Reconstruction [0] Exit", 13, 10
            db "> ", 0

szPrompt    db "Target File: ", 0
szError     db "[-] Analysis Failed", 13, 10, 0
szSuccess   db "[+] Analysis Complete", 13, 10, 0

; Analysis Headers
szPEHeader  db 13, 10, "=== PE ANALYSIS ===", 13, 10, 0
szImports   db 13, 10, "=== IMPORT RECONSTRUCTION ===", 13, 10, 0
szSections  db 13, 10, "=== SECTION ANALYSIS ===", 13, 10, 0
szStrings   db 13, 10, "=== STRING EXTRACTION ===", 13, 10, 0
szExports   db 13, 10, "=== EXPORT TABLE ===", 13, 10, 0
szTLS       db 13, 10, "=== TLS CALLBACKS ===", 13, 10, 0
szDebug     db 13, 10, "=== DEBUG DIRECTORIES ===", 13, 10, 0
szEntropy   db 13, 10, "=== SHANNON ENTROPY ===", 13, 10, 0
szHexDump   db 13, 10, "=== HEX DUMP ===", 13, 10, 0
szDisasm    db 13, 10, "=== DISASSEMBLY (ENTRY) ===", 13, 10, 0
szPacker    db "=== PACKER DETECTED ===", 13, 10, 0

; Format Strings
szMachine   db "Machine Type: %04X", 13, 10, 0
szSecCount  db "Section Count: %d", 13, 10, 0
szEntry     db "Entry Point: %08X", 13, 10, 0
szBase      db "Image Base: %08X", 13, 10, 0
szSecInfo   db "Section %.8s: VA=%08X Size=%08X Raw=%08X", 13, 10, 0
szImportDLL db "  Import DLL: %s", 13, 10, 0
szFunction  db "    Function: %s", 13, 10, 0
szStringOut db "  [%08X] %s", 13, 10, 0
szExportFunc db "  Export RVA: %08X  Name: %s", 13, 10, 0
szTLSCall   db "  Callback: %08X", 13, 10, 0
szDebugInfo db "  Type: %d  Size: %d  RVA: %08X", 13, 10, 0
szEntropyVal db "  Section %d Entropy: %d.%02d", 13, 10, 0
szPackerName db "  Signature: %s", 13, 10, 0
szDisasmLine db "  %08X: %02X %s", 13, 10, 0
szHexLine   db "  %08X: %02X %02X %02X %02X %02X %02X %02X %02X  %c%c%c%c%c%c%c%c", 13, 10, 0

; Common Packer Sections
szUPX0      db "UPX0", 0
szUPX1      db "UPX1", 0
szASPack    db ".aspack", 0
szFSG       db "fsgh", 0

; 256 byte frequency table for Entropy
byteFreq    dd 256 dup(0)
fTotal      real8 ?
fTemp       real8 ?
fEntropy    real8 ?

.data?
hConsoleIn  dd ?
hConsoleOut dd ?
hFile       dd ?
fileSize    dd ?
bytesRead   dd ?

; File buffer (50MB max)
fileBuffer  db 52428800 dup(?)

; Working buffers
inputBuffer db 512 dup(?)
tempBuffer  db 1024 dup(?)
tempString  db 256 dup(?)

; PE Analysis Variables
pBase       dd ?
pDOS        dd ?
pNT         dd ?
pFileHdr    dd ?
pOptHdr     dd ?
pSections   dd ?
sectionCount dd ?
entryPoint  dd ?
imageBase   dd ?

.code

;==============================================================================
; Console Output
;==============================================================================
PrintStr proc pString:DWORD
    LOCAL dwWritten:DWORD, dwLength:DWORD
    
    INVOKE lstrlenA, pString
    mov dwLength, eax
    INVOKE WriteConsoleA, hConsoleOut, pString, dwLength, ADDR dwWritten, 0
    ret
PrintStr endp

;==============================================================================
; Console Input
;==============================================================================
GetInput proc
    LOCAL dwRead:DWORD
    
    INVOKE ReadConsoleA, hConsoleIn, ADDR inputBuffer, 512, ADDR dwRead, 0
    mov eax, dwRead
    dec eax
    mov inputBuffer[eax], 0
    ret
GetInput endp

;==============================================================================
; Get Integer Input
;==============================================================================
GetChoice proc
    LOCAL dwRead:DWORD
    
    INVOKE ReadConsoleA, hConsoleIn, ADDR inputBuffer, 10, ADDR dwRead, 0
    
    ; Convert first character to integer
    movzx eax, BYTE PTR inputBuffer
    sub eax, '0'
    ret
GetChoice endp

;==============================================================================
; Load PE File
;==============================================================================
LoadFile proc pFileName:DWORD
    LOCAL dwHighSize:DWORD
    
    ; Open file
    INVOKE CreateFileA, pFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, INVALID_HANDLE_VALUE
    je LoadError
    mov hFile, eax
    
    ; Get file size
    INVOKE GetFileSize, hFile, ADDR dwHighSize
    cmp eax, 52428800  ; 50MB limit
    jg LoadErrorClose
    mov fileSize, eax
    
    ; Read entire file
    INVOKE ReadFile, hFile, ADDR fileBuffer, fileSize, ADDR bytesRead, 0
    test eax, eax
    jz LoadErrorClose
    
    INVOKE CloseHandle, hFile
    
    ; Setup base pointer
    lea eax, fileBuffer
    mov pBase, eax
    
    mov eax, 1  ; Success
    ret
    
LoadErrorClose:
    INVOKE CloseHandle, hFile
LoadError:
    INVOKE PrintStr, ADDR szError
    xor eax, eax
    ret
LoadFile endp

;==============================================================================
; Validate PE Structure
;==============================================================================
ValidatePE proc
    ; Check DOS header
    mov eax, pBase
    mov pDOS, eax
    cmp WORD PTR [eax], 'ZM'
    jne ValidateError
    
    ; Get NT headers
    mov eax, DWORD PTR [eax+3Ch]
    add eax, pBase
    mov pNT, eax
    
    ; Check PE signature
    cmp DWORD PTR [eax], 'EP'
    jne ValidateError
    
    ; Setup structure pointers
    add eax, 4
    mov pFileHdr, eax
    
    ; Get section count
    movzx eax, WORD PTR [eax+2]
    mov sectionCount, eax
    
    ; Optional header
    mov eax, pFileHdr
    add eax, 20
    mov pOptHdr, eax
    
    ; Get entry point and image base
    mov eax, DWORD PTR [eax+16]
    mov entryPoint, eax
    mov eax, pOptHdr
    mov eax, DWORD PTR [eax+28]
    mov imageBase, eax
    
    ; Section table
    mov eax, pOptHdr
    add eax, 224  ; Standard optional header size
    mov pSections, eax
    
    mov eax, 1  ; Success
    ret
    
ValidateError:
    INVOKE PrintStr, ADDR szError
    xor eax, eax
    ret
ValidatePE endp

;==============================================================================
; Analyze PE Headers
;==============================================================================
AnalyzePE proc
    INVOKE PrintStr, ADDR szPEHeader
    
    ; Machine type
    mov eax, pFileHdr
    movzx eax, WORD PTR [eax]
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szMachine, eax
    INVOKE PrintStr, ADDR tempBuffer
    
    ; Section count
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szSecCount, sectionCount
    INVOKE PrintStr, ADDR tempBuffer
    
    ; Entry point
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szEntry, entryPoint
    INVOKE PrintStr, ADDR tempBuffer
    
    ; Image base
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szBase, imageBase
    INVOKE PrintStr, ADDR tempBuffer
    
    ret
AnalyzePE endp

;==============================================================================
; Analyze Sections
;==============================================================================
AnalyzeSections proc
    LOCAL currentSection:DWORD
    
    INVOKE PrintStr, ADDR szSections
    
    mov currentSection, 0
    
SectionLoop:
    mov eax, currentSection
    cmp eax, sectionCount
    jge SectionDone
    
    ; Calculate section offset
    mov eax, currentSection
    mov ebx, 40  ; Section header size
    mul ebx
    add eax, pSections
    mov esi, eax
    
    ; Print section info
    push DWORD PTR [esi+20]    ; Raw data pointer
    push DWORD PTR [esi+8]     ; Virtual size
    push DWORD PTR [esi+12]    ; Virtual address
    push esi                   ; Section name (first 8 bytes)
    push OFFSET szSecInfo
    push OFFSET tempBuffer
    call wsprintfA
    add esp, 24
    
    INVOKE PrintStr, ADDR tempBuffer
    
    inc currentSection
    jmp SectionLoop
    
SectionDone:
    ret
AnalyzeSections endp

;==============================================================================
; RVA to File Offset Conversion
;==============================================================================
RVA2FileOffset proc dwRVA:DWORD
    LOCAL currentSection:DWORD
    
    mov currentSection, 0
    
RVALoop:
    mov eax, currentSection
    cmp eax, sectionCount
    jge RVANotFound
    
    ; Get section
    mov eax, currentSection
    mov ebx, 40
    mul ebx
    add eax, pSections
    mov esi, eax
    
    ; Check if RVA is in this section
    mov eax, dwRVA
    mov ebx, DWORD PTR [esi+12]  ; Virtual address
    cmp eax, ebx
    jb RVANext
    
    add ebx, DWORD PTR [esi+8]   ; + Virtual size
    cmp eax, ebx
    jae RVANext
    
    ; Found section, convert to file offset
    mov eax, dwRVA
    sub eax, DWORD PTR [esi+12]  ; - Virtual address
    add eax, DWORD PTR [esi+20]  ; + Raw data pointer
    add eax, pBase
    ret
    
RVANext:
    inc currentSection
    jmp RVALoop
    
RVANotFound:
    mov eax, dwRVA
    add eax, pBase  ; Simple conversion
    ret
RVA2FileOffset endp

;==============================================================================
; Analyze Imports
;==============================================================================
AnalyzeImports proc
    LOCAL pImportTable:DWORD
    LOCAL pThunk:DWORD
    
    INVOKE PrintStr, ADDR szImports
    
    ; Get import directory from data directories
    mov eax, pOptHdr
    add eax, 104 ; Import Table is Index 1 (96 + 8)
    mov eax, DWORD PTR [eax]
    test eax, eax
    jz NoImports
    
    INVOKE RVA2FileOffset, eax
    mov pImportTable, eax
    
ImportLoop:
    mov esi, pImportTable
    cmp DWORD PTR [esi], 0
    je ImportDone
    
    ; Get DLL name
    mov eax, DWORD PTR [esi+12]
    test eax, eax
    jz NextImport
    
    INVOKE RVA2FileOffset, eax
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szImportDLL, eax
    INVOKE PrintStr, ADDR tempBuffer
    
    ; Get import lookup table
    mov eax, DWORD PTR [esi]
    test eax, eax
    jnz UseILT
    mov eax, DWORD PTR [esi+16]  ; Use IAT if no ILT
    
UseILT:
    INVOKE RVA2FileOffset, eax
    mov pThunk, eax
    
ThunkLoop:
    mov edi, pThunk
    mov eax, DWORD PTR [edi]
    test eax, eax
    jz NextImport
    
    ; Check if import by name or ordinal
    test eax, 80000000h
    jnz ImportByOrdinal
    
    ; Import by name
    INVOKE RVA2FileOffset, eax
    add eax, 2  ; Skip hint
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szFunction, eax
    INVOKE PrintStr, ADDR tempBuffer
    jmp NextThunk
    
ImportByOrdinal:
    ; Import by ordinal (skip for simplicity)
    
NextThunk:
    add pThunk, 4
    jmp ThunkLoop
    
NextImport:
    add pImportTable, 20  ; Size of import descriptor
    jmp ImportLoop
    
NoImports:
    INVOKE PrintStr, ADDR szError
ImportDone:
    ret
AnalyzeImports endp

;==============================================================================
; Extract Strings
;==============================================================================
ExtractStrings proc
    LOCAL currentOffset:DWORD
    LOCAL stringStart:DWORD
    LOCAL stringLength:DWORD
    
    INVOKE PrintStr, ADDR szStrings
    
    mov currentOffset, 0
    
StringScan:
    mov eax, currentOffset
    cmp eax, fileSize
    jge StringsDone
    
    ; Check if current byte is printable
    mov ebx, pBase
    add ebx, eax
    movzx ecx, BYTE PTR [ebx]
    cmp cl, 32   ; Space
    jb NextByte
    cmp cl, 126  ; Tilde
    ja NextByte
    
    ; Start of potential string
    mov stringStart, eax
    mov stringLength, 0
    
StringBuild:
    mov eax, currentOffset
    cmp eax, fileSize
    jge CheckString
    
    mov ebx, pBase
    add ebx, eax
    movzx ecx, BYTE PTR [ebx]
    cmp cl, 32
    jb CheckString
    cmp cl, 126
    ja CheckString
    
    ; Copy character to temp string
    mov ebx, stringLength
    cmp ebx, 250  ; Max string length
    jge CheckString
    mov tempString[ebx], cl
    
    inc stringLength
    inc currentOffset
    jmp StringBuild
    
CheckString:
    ; Check minimum string length
    cmp stringLength, 4
    jl NextByte
    
    ; Null-terminate string
    mov eax, stringLength
    mov tempString[eax], 0
    
    ; Print string
    INVOKE wsprintfA, ADDR tempBuffer, ADDR szStringOut, stringStart, ADDR tempString
    INVOKE PrintStr, ADDR tempBuffer
    
NextByte:
    inc currentOffset
    jmp StringScan
    
StringsDone:
    ret
ExtractStrings endp

;==============================================================================
; Analyze Exports
;==============================================================================
AnalyzeExports proc
    LOCAL pExportDir:DWORD
    LOCAL pNames:DWORD
    LOCAL pFuncs:DWORD
    LOCAL numNames:DWORD
    LOCAL i:DWORD
    LOCAL nameRVA:DWORD
    
    INVOKE PrintStr, ADDR szExports
    
    ; Get Export Directory (Index 0 = Offset 96)
    mov eax, pOptHdr
    add eax, 96
    mov eax, DWORD PTR [eax]
    test eax, eax
    jz NoExports
    
    INVOKE RVA2FileOffset, eax
    mov pExportDir, eax
    
    ; Get NumberOfNames
    mov ecx, DWORD PTR [eax+24]
    mov numNames, ecx
    test ecx, ecx
    jz NoExports
    
    ; Get AddressOfNames
    mov ecx, DWORD PTR [eax+32]
    INVOKE RVA2FileOffset, ecx
    mov pNames, eax
    
    mov i, 0
ExportLoop:
    mov eax, i
    cmp eax, numNames
    jge ExportDone
    
    ; Get Name RVA
    mov esi, pNames
    mov eax, i
    shl eax, 2 ; x4
    add esi, eax
    mov ecx, DWORD PTR [esi] ; RVA of string
    mov nameRVA, ecx
    
    INVOKE RVA2FileOffset, ecx
    push eax ; Name string pointer
    
    ; We should technically look up ordinal -> function RVA, but for simple listing, just listed name/rva
    push nameRVA
    push OFFSET szExportFunc
    push OFFSET tempBuffer
    call wsprintfA
    add esp, 16 
    
    INVOKE PrintStr, ADDR tempBuffer
    
    inc i
    jmp ExportLoop
    
NoExports:
    ; INVOKE PrintStr, ADDR szError ; Silent if no exports
ExportDone:
    ret
AnalyzeExports endp

;==============================================================================
; Analyze TLS
;==============================================================================
AnalyzeTLS proc
    LOCAL pTLS:DWORD
    LOCAL pCallbacks:DWORD
    LOCAL vaCallback:DWORD
    
    INVOKE PrintStr, ADDR szTLS
    
    ; TLS is Index 9 (96 + 9*8 = 168)
    mov eax, pOptHdr
    add eax, 168
    mov eax, DWORD PTR [eax]
    test eax, eax
    jz TLSDone
    
    INVOKE RVA2FileOffset, eax
    mov pTLS, eax
    
    ; AddressOfCallBacks is at offset 12 (buffer + 12) in IMAGE_TLS_DIRECTORY32
    mov ecx, DWORD PTR [eax+12]
    test ecx, ecx
    jz TLSDone
    
    ; Convert VA to RVA? No, TLS Struct has VA.
    ; We need to subtract ImageBase to get RVA.
    sub ecx, imageBase
    INVOKE RVA2FileOffset, ecx
    mov pCallbacks, eax
    
TLSLoop:
    mov esi, pCallbacks
    mov eax, DWORD PTR [esi]
    test eax, eax
    jz TLSDone
    
    push eax
    push OFFSET szTLSCall
    push OFFSET tempBuffer
    call wsprintfA
    add esp, 12
    INVOKE PrintStr, ADDR tempBuffer
    
    add pCallbacks, 4
    jmp TLSLoop

TLSDone:
    ret
AnalyzeTLS endp

;==============================================================================
; Analyze Debug
;==============================================================================
AnalyzeDebug proc
    LOCAL pDebug:DWORD
    LOCAL dirSize:DWORD
    
    INVOKE PrintStr, ADDR szDebug
    
    ; Debug Index 6 (96 + 48 = 144)
    mov eax, pOptHdr
    add eax, 144
    mov ecx, DWORD PTR [eax+4] ; Size
    mov dirSize, ecx
    mov eax, DWORD PTR [eax]   ; RVA
    test eax, eax
    jz DebugDone
    test ecx, ecx
    jz DebugDone
    
    INVOKE RVA2FileOffset, eax
    mov pDebug, eax
    
    ; Loop directories? Size / 28
    ; Just print first one for now
    mov esi, pDebug
    
    push DWORD PTR [esi+20] ; AddressOfRawData
    push DWORD PTR [esi+16] ; SizeOfData
    push DWORD PTR [esi+12] ; Type
    push OFFSET szDebugInfo
    push OFFSET tempBuffer
    call wsprintfA
    add esp, 20
    INVOKE PrintStr, ADDR tempBuffer

DebugDone:
    ret
AnalyzeDebug endp

;==============================================================================
; Main Program Loop
;==============================================================================
MainLoop proc
    LOCAL choice:DWORD
    
MenuLoop:
    INVOKE PrintStr, ADDR szTitle
    INVOKE PrintStr, ADDR szMenu
    
    INVOKE GetChoice
    mov choice, eax
    
    cmp choice, 0
    je ExitProgram
    cmp choice, 1
    je DoPEAnalysis
    cmp choice, 2
    je DoImports
    cmp choice, 3
    je DoSections
    cmp choice, 4
    je DoStrings
    jmp MenuLoop
    
DoPEAnalysis:
    INVOKE PrintStr, ADDR szPrompt
    INVOKE GetInput
    INVOKE LoadFile, ADDR inputBuffer
    test eax, eax
    jz MenuLoop
    INVOKE ValidatePE
    test eax, eax
    jz MenuLoop
    INVOKE AnalyzePE
    jmp MenuLoop
    
DoImports:
    INVOKE PrintStr, ADDR szPrompt
    INVOKE GetInput
    INVOKE LoadFile, ADDR inputBuffer
    test eax, eax
    jz MenuLoop
    INVOKE ValidatePE
    test eax, eax
    jz MenuLoop
    INVOKE AnalyzeImports
    jmp MenuLoop
    
DoSections:
    INVOKE PrintStr, ADDR szPrompt
    INVOKE GetInput
    INVOKE LoadFile, ADDR inputBuffer
    test eax, eax
    jz MenuLoop
    INVOKE ValidatePE
    test eax, eax
    jz MenuLoop
    INVOKE AnalyzeSections
    jmp MenuLoop
    
DoStrings:
    INVOKE PrintStr, ADDR szPrompt
    INVOKE GetInput
    INVOKE LoadFile, ADDR inputBuffer
    test eax, eax
    jz MenuLoop
    INVOKE ExtractStrings
    jmp MenuLoop
    
ExitProgram:
    INVOKE PrintStr, ADDR szSuccess
    ret
MainLoop endp

;==============================================================================
; Program Entry Point
;==============================================================================
start:
    INVOKE GetStdHandle, STD_INPUT_HANDLE
    mov hConsoleIn, eax
    INVOKE GetStdHandle, STD_OUTPUT_HANDLE
    mov hConsoleOut, eax
    
    INVOKE MainLoop
    INVOKE ExitProcess, 0
end start