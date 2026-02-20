; OMEGA-POLYGLOT MAXIMUM v3.0 PRO
; Claude/Moonshot/DeepSeek Hybrid Reverse Engineering Engine
; Professional PE Analysis + Source Reconstruction + Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === PROFESSIONAL HEADERS ===
include \masm32\include\windows.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; === ENTERPRISE CONSTANTS ===
CLI_VER equ "3.0 PRO"
MAX_FSIZE equ 104857600
PE_SIG equ 00004550h
ELF_SIG equ 0000007Fh
MZ_SIG equ 00005A4Dh
MACH_SIG equ 000000FEh

; PE Characteristics Flags
IMAGE_FILE_DLL equ 2000h
IMAGE_FILE_EXECUTABLE_IMAGE equ 0002h
IMAGE_FILE_32BIT_MACHINE equ 0100h
IMAGE_FILE_SYSTEM equ 1000h

; Section Characteristics
IMAGE_SCN_CNT_CODE equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 00000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA equ 00000080h
IMAGE_SCN_MEM_EXECUTE equ 20000000h
IMAGE_SCN_MEM_READ equ 40000000h
IMAGE_SCN_MEM_WRITE equ 80000000h

; === DATA SECTION ===
.data
; UI Strings
szBanner db "OMEGA-POLYGLOT MAXIMUM v", CLI_VER, 0Dh, 0Ah
         db "Professional Reverse Engineering Suite", 0Dh, 0Ah
         db "Claude/Moonshot/DeepSeek Hybrid Engine", 0Dh, 0Ah
         db "========================================", 0Dh, 0Ah, 0
szMenu db "[1]Deep PE Analysis [2]Source Reconstruct [3]Import/Export Map", 0Dh, 0Ah
       db "[4]Section Analyzer [5]String Extract [6]Hex View [7]Control Flow", 0Dh, 0Ah
       db "[8]Export Reconstructed C [9]Full Dump [0]Exit", 0Dh, 0Ah, ">", 0
szPromptFile db "Target: ", 0
szPromptOut db "Output: ", 0
szDotNet db "[!] .NET Assembly Detected - IL Analysis Recommended", 0Dh, 0Ah, 0
szPacked db "[!] Possible Packer/Compressor Detected", 0Dh, 0Ah, 0

; Analysis Headers
szPESig db 0Dh, 0Ah, "=== PE SIGNATURE ANALYSIS ===", 0Dh, 0Ah, 0
szPEHdr db 0Dh, 0Ah, "=== PE HEADER ===", 0Dh, 0Ah, 0
szFileHdr db 0Dh, 0Ah, "=== FILE HEADER ===", 0Dh, 0Ah, 0
szOptHdr db 0Dh, 0Ah, "=== OPTIONAL HEADER ===", 0Dh, 0Ah, 0
szSecHdr db 0Dh, 0Ah, "=== SECTION TABLE ===", 0Dh, 0Ah, 0
szImpHdr db 0Dh, 0Ah, "=== IMPORT DIRECTORY ===", 0Dh, 0Ah, 0
szExpHdr db 0Dh, 0Ah, "=== EXPORT DIRECTORY ===", 0Dh, 0Ah, 0
szStrHdr db 0Dh, 0Ah, "=== STRING EXTRACTION ===", 0Dh, 0Ah, 0
szCFHdr db 0Dh, 0Ah, "=== CONTROL FLOW ANALYSIS ===", 0Dh, 0Ah, 0
szReconHdr db 0Dh, 0Ah, "=== SOURCE RECONSTRUCTION ===", 0Dh, 0Ah, 0

; Format Strings
szFmtMachine db "Machine:              0x%04X (%s)", 0Dh, 0Ah, 0
szFmtSections db "Number of Sections:   %d", 0Dh, 0Ah, 0
szFmtTime db "Time Stamp:           0x%08X", 0Dh, 0Ah, 0
szFmtChars db "Characteristics:      0x%04X [%s]", 0Dh, 0Ah, 0
szFmtEntry db "Entry Point:          0x%08X", 0Dh, 0Ah, 0
szFmtImageBase db "Image Base:           0x%08X", 0Dh, 0Ah, 0
szFmtImageSize db "Image Size:           0x%08X", 0Dh, 0Ah, 0
szFmtSubsys db "Subsystem:            0x%04X (%s)", 0Dh, 0Ah, 0
szFmtDllChars db "DLL Characteristics:  0x%04X", 0Dh, 0Ah, 0
szFmtSecName db 0Dh, 0Ah, "Section: %.8s", 0Dh, 0Ah, 0
szFmtSecVA db "  Virtual Address:    0x%08X", 0Dh, 0Ah, 0
szFmtSecVS db "  Virtual Size:       0x%08X", 0Dh, 0Ah, 0
szFmtSecRaw db "  Raw Address:        0x%08X", 0Dh, 0Ah, 0
szFmtSecRawSize db "  Raw Size:           0x%08X", 0Dh, 0Ah, 0
szFmtSecChars db "  Characteristics:    0x%08X [%s]", 0Dh, 0Ah, 0
szFmtImpDLL db 0Dh, 0Ah, "DLL: %s", 0Dh, 0Ah, 0
szFmtImpFunc db "  %08X  %s (Hint: %d)", 0Dh, 0Ah, 0
szFmtImpOrd db "  %08X  Ordinal: %d", 0Dh, 0Ah, 0
szFmtExpName db "Export: %s (Ordinal: %d, RVA: 0x%08X)", 0Dh, 0Ah, 0
szFmtString db "[0x%08X] %s", 0Dh, 0Ah, 0
szFmtReconFunc db 0Dh, 0Ah, "// Function at 0x%08X", 0Dh, 0Ah, "void func_%08X() {", 0Dh, 0Ah, 0
szFmtReconEnd db "}", 0Dh, 0Ah, 0
szFmtCFBlock db "Basic Block @ 0x%08X - 0x%08X", 0Dh, 0Ah, 0
szFmtAPI db "    API Call to 0x%08X", 0Dh, 0Ah, 0
szCharExec db "EXECUTE", 0
szCharRead db "READ", 0
szCharWrite db "WRITE", 0
szCharCode db "CODE", 0
szCharData db "DATA", 0
szCharInit db "INITIALIZED", 0
szCharUninit db "UNINITIALIZED", 0
szCharDLL db "DLL", 0
szCharEXE db "EXE", 0
szChar32 db "32BIT", 0
szCharSys db "SYSTEM", 0

; Subsystem Names
szSubsysWin db "WINDOWS_GUI", 0
szSubsysCon db "WINDOWS_CUI", 0
szSubsysSys db "NATIVE", 0
szSubsysPos db "POSIX", 0
szSubsysEfi db "EFI", 0
szSubsysXbo db "XBOX", 0
szSubsysUnk db "UNKNOWN", 0

; Machine Types
szMachineI386 db "i386", 0
szMachineAMD64 db "AMD64", 0
szMachineARM db "ARM", 0
szMachineARM64 db "ARM64", 0
szMachineUnk db "UNKNOWN", 0

; Separators
szNewLine db 0Dh, 0Ah, 0
szTab db "  ", 0
szNull db 0

; === BSS SECTION ===
.data?
hStdIn dd ?
hStdOut dd ?
hFile dd ?
hOutFile dd ?
dwFileSize dd ?
dwBase dd ?
dwPEOffset dd ?
dwNumSections dd ?
dwEntryPoint dd ?
dwImageBase dd ?
dwImportDir dd ?
dwExportDir dd ?
dwSectionTable dd ?
bBuffer db 512 dup(?)
bFile db MAX_FSIZE dup(?)
bOutPath db 260 dup(?)

; === CODE SECTION ===
.code

; === UTILITY FUNCTIONS ===
Print proc lpString:DWORD
    local dwWritten:DWORD, dwLen:DWORD
    invoke lstrlen, lpString
    mov dwLen, eax
    invoke WriteConsole, hStdOut, lpString, dwLen, addr dwWritten, NULL
    ret
Print endp

PrintFmt proc fmt:DWORD, args:VARARG
    local buffer[512]:BYTE, dwWritten:DWORD
    invoke wvsprintf, addr buffer, fmt, addr args
    invoke Print, addr buffer
    ret
PrintFmt endp

ReadInput proc
    local dwRead:DWORD
    invoke ReadConsole, hStdIn, addr bBuffer, 260, addr dwRead, NULL
    mov eax, dwRead
    dec eax
    mov byte ptr [bBuffer+eax], 0
    ret
ReadInput endp

ReadInt proc
    call ReadInput
    xor eax, eax
    lea esi, bBuffer
    @@conv:
        movzx ecx, byte ptr [esi]
        cmp cl, 0Dh
        je @@done
        cmp cl, 0
        je @@done
        sub cl, '0'
        cmp cl, 9
        ja @@done
        imul eax, 10
        add al, cl
        inc esi
        jmp @@conv
    @@done:
    ret
ReadInt endp

ReadHex proc
    call ReadInput
    xor eax, eax
    lea esi, bBuffer
    @@conv:
        movzx ecx, byte ptr [esi]
        cmp cl, 0Dh
        je @@done
        cmp cl, 0
        je @@done
        cmp cl, 'a'
        jb @@up
        sub cl, 32
        @@up:
        cmp cl, 'A'
        jb @@num
        sub cl, 'A'-10
        jmp @@add
        @@num:
        sub cl, '0'
        @@add:
        shl eax, 4
        add al, cl
        inc esi
        jmp @@conv
    @@done:
    ret
ReadHex endp

; === FILE OPERATIONS ===
LoadFile proc lpPath:DWORD
    local dwHigh:DWORD
    invoke CreateFile, lpPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@fail
    mov hFile, eax
    invoke GetFileSize, hFile, addr dwHigh
    mov dwFileSize, eax
    cmp eax, MAX_FSIZE
    jg @@toobig
    invoke ReadFile, hFile, addr bFile, dwFileSize, addr dwHigh, NULL
    test eax, eax
    jz @@fail
    invoke CloseHandle, hFile
    mov eax, TRUE
    ret
@@toobig:
    invoke CloseHandle, hFile
@@fail:
    mov eax, FALSE
    ret
LoadFile endp

SaveOutput proc lpPath:DWORD
    invoke CreateFile, lpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@fail
    mov hOutFile, eax
    mov eax, TRUE
    ret
@@fail:
    xor eax, eax
    ret
SaveOutput endp

WriteOut proc lpData:DWORD, dwLen:DWORD
    local dwWritten:DWORD
    cmp hOutFile, 0
    je @@console
    invoke WriteFile, hOutFile, lpData, dwLen, addr dwWritten, NULL
    ret
@@console:
    invoke Print, lpData
    ret
WriteOut endp

; === PE ANALYSIS ENGINE ===
RVAToOffset proc dwRVA:DWORD
    local i:DWORD, pSec:DWORD
    mov i, 0
    mov eax, dwNumSections
    test eax, eax
    jz @@fail
    @@loop:
        cmp i, eax
        jge @@fail
        mov ecx, i
        imul ecx, 28h
        add ecx, dwSectionTable
        mov pSec, ecx
        mov ebx, dwRVA
        mov edx, [ecx+0Ch]  ; VirtualAddress
        cmp ebx, edx
        jb @@next
        add edx, [ecx+08h]  ; VirtualSize
        cmp ebx, edx
        jae @@next
        sub ebx, [ecx+0Ch]
        add ebx, [ecx+014h] ; PointerToRawData
        add ebx, dwBase
        mov eax, ebx
        ret
    @@next:
        inc i
        jmp @@loop
    @@fail:
    xor eax, eax
    ret
RVAToOffset endp

ParsePE proc
    mov eax, dwBase
    cmp word ptr [eax], 'ZM'
    jne @@fail
    mov eax, [eax+3Ch]
    cmp eax, dwFileSize
    jge @@fail
    mov dwPEOffset, eax
    add eax, dwBase
    cmp dword ptr [eax], 'EP'
    jne @@fail
    mov eax, TRUE
    ret
@@fail:
    xor eax, eax
    ret
ParsePE endp

AnalyzeFileHeader proc
    local pFileHdr:DWORD, wMachine:WORD, wSections:WORD, dwTime:DWORD, wChars:WORD
    mov eax, dwPEOffset
    add eax, dwBase
    add eax, 4
    mov pFileHdr, eax
    
    movzx eax, word ptr [eax]   ; Machine
    mov wMachine, ax
    movzx eax, word ptr [pFileHdr+2] ; NumberOfSections
    mov wSections, ax
    mov dwNumSections, eax
    mov eax, [pFileHdr+4]       ; TimeDateStamp
    mov dwTime, eax
    movzx eax, word ptr [pFileHdr+12] ; Characteristics
    mov wChars, ax
    
    invoke Print, addr szFileHdr
    
    ; Machine type
    lea ebx, szMachineUnk
    cmp wMachine, 14Ch
    jne @@chk64
    lea ebx, szMachineI386
    jmp @@printMach
@@chk64:
    cmp wMachine, 8664h
    jne @@chkArm
    lea ebx, szMachineAMD64
    jmp @@printMach
@@chkArm:
    cmp wMachine, 1C0h
    jne @@printMach
    lea ebx, szMachineARM
@@printMach:
    invoke PrintFmt, addr szFmtMachine, wMachine, ebx
    
    invoke PrintFmt, addr szFmtSections, wSections
    invoke PrintFmt, addr szFmtTime, dwTime
    
    ; Characteristics analysis
    mov ecx, offset szNull
    test wChars, IMAGE_FILE_DLL
    jz @@chkExe
    mov ecx, offset szCharDLL
    jmp @@printChar
@@chkExe:
    test wChars, IMAGE_FILE_EXECUTABLE_IMAGE
    jz @@printChar
    mov ecx, offset szCharEXE
@@printChar:
    invoke PrintFmt, addr szFmtChars, wChars, ecx
    
    ret
AnalyzeFileHeader endp

AnalyzeOptionalHeader proc
    local pOptHdr:DWORD, dwEntry:DWORD, dwImgBase:DWORD, dwImgSize:DWORD
    local wSubsys:WORD, wDllChars:WORD
    
    mov eax, dwPEOffset
    add eax, dwBase
    add eax, 18h
    mov pOptHdr, eax
    
    mov eax, [pOptHdr+10h]  ; AddressOfEntryPoint
    mov dwEntry, eax
    mov dwEntryPoint, eax
    mov eax, [pOptHdr+1Ch]  ; ImageBase
    mov dwImageBase, eax
    mov eax, [pOptHdr+38h]  ; SizeOfImage
    mov dwImgSize, eax
    movzx eax, word ptr [pOptHdr+40h] ; Subsystem
    mov wSubsys, ax
    movzx eax, word ptr [pOptHdr+42h] ; DllCharacteristics
    mov wDllChars, ax
    
    ; Import/Export directories
    mov eax, [pOptHdr+68h]  ; Import Directory RVA
    mov dwImportDir, eax
    mov eax, [pOptHdr+70h]  ; Export Directory RVA
    mov dwExportDir, eax
    
    invoke Print, addr szOptHdr
    invoke PrintFmt, addr szFmtEntry, dwEntry
    invoke PrintFmt, addr szFmtImageBase, dwImageBase
    invoke PrintFmt, addr szFmtImageSize, dwImgSize
    
    ; Subsystem name
    lea ebx, szSubsysUnk
    cmp wSubsys, 1
    jne @@chkCon
    lea ebx, szSubsysSys
    jmp @@printSub
@@chkCon:
    cmp wSubsys, 2
    jne @@chkWin
    lea ebx, szSubsysWin
    jmp @@printSub
@@chkWin:
    cmp wSubsys, 3
    jne @@chkPos
    lea ebx, szSubsysCon
    jmp @@printSub
@@chkPos:
    cmp wSubsys, 7
    jne @@printSub
    lea ebx, szSubsysPos
@@printSub:
    invoke PrintFmt, addr szFmtSubsys, wSubsys, ebx
    invoke PrintFmt, addr szFmtDllChars, wDllChars
    
    ; Detect .NET
    mov eax, [pOptHdr+70h+88h] ; COM Descriptor Directory
    test eax, eax
    jz @@noNet
    invoke Print, addr szDotNet
@@noNet:
    
    ret
AnalyzeOptionalHeader endp

AnalyzeSections proc
    local i:DWORD, pSec:DWORD, dwChars:DWORD
    
    mov eax, dwPEOffset
    add eax, dwBase
    add eax, 18h
    movzx ecx, word ptr [eax+10h] ; SizeOfOptionalHeader
    add eax, 14h
    add eax, ecx
    mov dwSectionTable, eax
    
    invoke Print, addr szSecHdr
    
    mov i, 0
    @@loop:
        mov eax, i
        cmp eax, dwNumSections
        jge @@done
        
        mov ecx, i
        imul ecx, 28h
        add ecx, dwSectionTable
        mov pSec, ecx
        
        ; Section name (8 bytes)
        invoke PrintFmt, addr szFmtSecName, ecx
        
        mov eax, [ecx+0Ch]  ; VirtualAddress
        invoke PrintFmt, addr szFmtSecVA, eax
        mov eax, [ecx+08h]  ; VirtualSize
        invoke PrintFmt, addr szFmtSecVS, eax
        mov eax, [ecx+014h] ; PointerToRawData
        invoke PrintFmt, addr szFmtSecRaw, eax
        mov eax, [ecx+010h] ; SizeOfRawData
        invoke PrintFmt, addr szFmtSecRawSize, eax
        
        ; Characteristics decoding
        mov eax, [ecx+024h]
        mov dwChars, eax
        mov ebx, offset szNull
        
        test eax, IMAGE_SCN_CNT_CODE
        jz @@chkData
        mov ebx, offset szCharCode
        jmp @@printChars
    @@chkData:
        test eax, IMAGE_SCN_CNT_INITIALIZED_DATA
        jz @@chkUninit
        mov ebx, offset szCharData
        jmp @@printChars
    @@chkUninit:
        test eax, IMAGE_SCN_CNT_UNINITIALIZED_DATA
        jz @@printChars
        mov ebx, offset szCharUninit
        
    @@printChars:
        invoke PrintFmt, addr szFmtSecChars, dwChars, ebx
        
        inc i
        jmp @@loop
    @@done:
    ret
AnalyzeSections endp

; === IMPORT/EXPORT ANALYSIS ===
AnalyzeImports proc
    local pImpDesc:DWORD, dwRVA:DWORD, pThunk:DWORD, pName:DWORD, dwHint:DWORD
    
    cmp dwImportDir, 0
    je @@done
    
    invoke RVAToOffset, dwImportDir
    test eax, eax
    jz @@done
    mov pImpDesc, eax
    
    invoke Print, addr szImpHdr
    
    @@dllLoop:
        mov esi, pImpDesc
        mov eax, [esi]      ; OriginalFirstThunk
        mov ebx, [esi+0Ch]  ; Name RVA
        test eax, eax
        jnz @@hasImports
        test ebx, ebx
        jz @@done
        
    @@hasImports:
        ; DLL Name
        push eax
        invoke RVAToOffset, ebx
        mov pName, eax
        invoke PrintFmt, addr szFmtImpDLL, eax
        pop eax
        
        ; Thunk table
        test eax, eax
        jnz @@useOFT
        mov eax, [esi+10h]  ; FirstThunk
        
    @@useOFT:
        invoke RVAToOffset, eax
        mov pThunk, eax
        
        @@funcLoop:
            mov esi, pThunk
            mov eax, [esi]
            test eax, eax
            jz @@nextDLL
            
            test eax, 80000000h
            jnz @@byOrdinal
            
            ; By name
            push eax
            invoke RVAToOffset, eax
            mov esi, eax
            movzx eax, word ptr [esi] ; Hint
            mov dwHint, eax
            add esi, 2                ; Name
            invoke PrintFmt, addr szFmtImpFunc, pThunk, esi, dwHint
            pop eax
            jmp @@nextFunc
            
        @@byOrdinal:
            and eax, 0FFFFh
            invoke PrintFmt, addr szFmtImpOrd, pThunk, eax
            
        @@nextFunc:
            add pThunk, 4
            jmp @@funcLoop
            
    @@nextDLL:
        add pImpDesc, 14h
        jmp @@dllLoop
        
    @@done:
    ret
AnalyzeImports endp

AnalyzeExports proc
    local pExpDir:DWORD, dwNumNames:DWORD, pNames:DWORD, pOrdinals:DWORD, pFuncs:DWORD, i:DWORD
    
    cmp dwExportDir, 0
    je @@done
    
    invoke RVAToOffset, dwExportDir
    test eax, eax
    jz @@done
    mov pExpDir, eax
    
    invoke Print, addr szExpHdr
    
    mov esi, pExpDir
    mov eax, [esi+18h]  ; NumberOfNames
    mov dwNumNames, eax
    mov eax, [esi+20h]  ; AddressOfNames
    invoke RVAToOffset, eax
    mov pNames, eax
    mov eax, [esi+24h]  ; AddressOfNameOrdinals
    invoke RVAToOffset, eax
    mov pOrdinals, eax
    mov eax, [esi+1Ch]  ; AddressOfFunctions
    invoke RVAToOffset, eax
    mov pFuncs, eax
    
    mov i, 0
    @@loop:
        mov eax, i
        cmp eax, dwNumNames
        jge @@done
        
        mov esi, pNames
        mov eax, [esi+eax*4]
        invoke RVAToOffset, eax
        mov ebx, eax        ; Function name
        
        mov esi, pOrdinals
        mov eax, i
        movzx eax, word ptr [esi+eax*2]
        movzx ecx, ax       ; Ordinal
        
        mov esi, pFuncs
        mov eax, [esi+eax*4] ; Function RVA
        
        invoke PrintFmt, addr szFmtExpName, ebx, ecx, eax
        
        inc i
        jmp @@loop
        
    @@done:
    ret
AnalyzeExports endp

; === STRING EXTRACTION ===
ExtractStrings proc dwMinLen:DWORD
    local i:DWORD, j:DWORD, dwStart:DWORD, bIsString:DWORD
    
    invoke Print, addr szStrHdr
    
    mov i, 0
    @@loop:
        cmp i, dwFileSize
        jge @@done
        
        ; Check if printable ASCII
        movzx eax, byte ptr [bFile+i]
        cmp al, 20h
        jb @@next
        cmp al, 7Eh
        ja @@next
        
        ; Start of string
        mov dwStart, i
        mov j, 0
        
    @@strLoop:
        mov eax, i
        cmp eax, dwFileSize
        jge @@checkLen
        movzx eax, byte ptr [bFile+i]
        cmp al, 20h
        jb @@checkLen
        cmp al, 7Eh
        ja @@checkLen
        inc i
        inc j
        jmp @@strLoop
        
    @@checkLen:
        cmp j, dword ptr dwMinLen
        jb @@next
        mov byte ptr [bFile+i], 0
        invoke PrintFmt, addr szFmtString, dwStart, addr bFile+dwStart
        
    @@next:
        inc i
        jmp @@loop
        
    @@done:
    ret
ExtractStrings endp

; === CONTROL FLOW ANALYSIS ===
AnalyzeControlFlow proc
    local dwEPOffset:DWORD, i:DWORD, bPrevWasRet:BYTE
    
    cmp dwEntryPoint, 0
    je @@done
    
    invoke RVAToOffset, dwEntryPoint
    test eax, eax
    jz @@done
    mov dwEPOffset, eax
    
    invoke Print, addr szCFHdr
    invoke PrintFmt, addr szFmtCFBlock, dwEntryPoint, dwEntryPoint
    
    ; Simple heuristic: look for function prologues (55 8B EC) and returns (C3)
    mov i, 0
    mov bPrevWasRet, 1
    
    @@loop:
        mov eax, i
        cmp eax, dwFileSize
        jge @@done
        
        cmp bPrevWasRet, 1
        jne @@chkRet
        
        ; Check for push ebp / mov ebp,esp pattern
        cmp word ptr [bFile+i], 0EC8Bh
        jne @@chkRet
        cmp byte ptr [bFile+i-2], 55h
        jne @@chkRet
        
        ; New function found
        mov eax, i
        sub eax, 2
        add eax, dwImageBase
        invoke PrintFmt, addr szFmtCFBlock, eax, eax
        
    @@chkRet:
        mov bPrevWasRet, 0
        cmp byte ptr [bFile+i], 0C3h
        jne @@next
        mov bPrevWasRet, 1
        
    @@next:
        inc i
        jmp @@loop
        
    @@done:
    ret
AnalyzeControlFlow endp

; === SOURCE RECONSTRUCTION ===
ReconstructSource proc
    local i:DWORD, dwFuncAddr:DWORD
    
    cmp dwEntryPoint, 0
    je @@done
    
    invoke Print, addr szReconHdr
    
    ; Entry point as main
    mov eax, dwImageBase
    add eax, dwEntryPoint
    invoke PrintFmt, addr szFmtReconFunc, eax, eax
    
    ; Simple pattern matching for API calls
    mov i, 0
    @@loop:
        mov eax, i
        cmp eax, dwFileSize
        jge @@endFunc
        
        ; Look for CALL instruction (E8) or indirect CALL (FF 15)
        cmp byte ptr [bFile+i], 0E8h
        je @@foundCall
        cmp word ptr [bFile+i], 15FFh
        je @@foundAPICall
        jmp @@next
        
    @@foundCall:
        ; Relative call - calculate target
        mov eax, i
        add eax, 5
        add eax, dwImageBase
        invoke PrintFmt, offset szFmtString, eax, offset szTab
        jmp @@next
        
    @@foundAPICall:
        mov eax, [bFile + i + 2]  ; Get IAT VA
        invoke PrintFmt, addr szFmtAPI, eax
        
    @@next:
        inc i
        jmp @@loop
        
    @@endFunc:
    invoke Print, addr szFmtReconEnd
    
    @@done:
    ret
ReconstructSource endp

; === HEX DUMP ===
HexDump proc dwStart:DWORD, dwLen:DWORD
    local i:DWORD, j:DWORD, bLine[16]:BYTE
    
    mov i, 0
    @@loop:
        mov eax, i
        cmp eax, dwLen
        jge @@done
        
        ; Address
        mov eax, dwStart
        add eax, i
        invoke PrintFmt, addr szHL, eax
        
        ; Hex bytes
        mov j, 0
        @@hex:
            cmp j, 16
            jge @@ascii
            mov eax, i
            add eax, j
            cmp eax, dwLen
            jge @@pad
            movzx eax, byte ptr [bFile+eax]
            invoke PrintFmt, addr szHB, eax
            jmp @@nexth
        @@pad:
            invoke Print, offset szS
        @@nexth:
            inc j
            jmp @@hex
            
        @@ascii:
            invoke Print, offset szS
            mov j, 0
        @@asc:
            cmp j, 16
            jge @@nextLine
            mov eax, i
            add eax, j
            cmp eax, dwLen
            jge @@nextLine
            movzx eax, byte ptr [bFile+eax]
            cmp al, 20h
            jb @@dot
            cmp al, 7Eh
            ja @@dot
            mov bLine[j], al
            jmp @@nexta
        @@dot:
            mov bLine[j], '.'
        @@nexta:
            inc j
            jmp @@asc
            
        @@nextLine:
        mov bLine[j], 0
        invoke PrintFmt, addr szAS, addr bLine
        invoke Print, offset szNewLine
        
        add i, 16
        jmp @@loop
        
    @@done:
    ret
HexDump endp

; === FULL DUMP ===
FullAnalysis proc
    call ParsePE
    test eax, eax
    jz @@fail
    
    call AnalyzeFileHeader
    call AnalyzeOptionalHeader
    call AnalyzeSections
    call AnalyzeImports
    call AnalyzeExports
    call AnalyzeControlFlow
    call ExtractStrings, 4
    call ReconstructSource
    ret
@@fail:
    invoke Print, offset szS
    ret
FullAnalysis endp

; === MAIN MENU ===
MainMenu proc
    local choice:DWORD, dwAddr:DWORD, dwSize:DWORD
@@show:
    invoke Print, addr szBanner
    invoke Print, addr szMenu
    call ReadInt
    mov choice, eax
    
    cmp choice, 0
    je @@exit
    
    cmp choice, 9
    je @@full
    
    invoke Print, offset szPF
    call ReadInput
    invoke LoadFile, addr bBuffer
    test eax, eax
    jz @@show
    
    mov dwBase, offset bFile
    call ParsePE
    test eax, eax
    jz @@show
    
    cmp choice, 1
    je @@deep
    cmp choice, 2
    je @@recon
    cmp choice, 3
    je @@imp
    cmp choice, 4
    je @@sec
    cmp choice, 5
    je @@str
    cmp choice, 6
    je @@hex
    cmp choice, 7
    je @@cf
    cmp choice, 8
    je @@export
    
    jmp @@show
    
@@deep:
    call AnalyzeFileHeader
    call AnalyzeOptionalHeader
    call AnalyzeSections
    jmp @@show
    
@@recon:
    call ReconstructSource
    jmp @@show
    
@@imp:
    call AnalyzeImports
    call AnalyzeExports
    jmp @@show
    
@@sec:
    call AnalyzeSections
    jmp @@show
    
@@str:
    invoke Print, offset szS
    call ReadInt
    call ExtractStrings, eax
    jmp @@show
    
@@hex:
    invoke Print, offset szPA
    call ReadHex
    mov dwAddr, eax
    invoke Print, offset szPS
    call ReadInt
    call HexDump, dwAddr, eax
    jmp @@show
    
@@cf:
    call AnalyzeControlFlow
    jmp @@show
    
@@export:
    invoke Print, offset szPO
    call ReadInput
    invoke SaveOutput, addr bBuffer
    call FullAnalysis
    cmp hOutFile, 0
    je @@show
    invoke CloseHandle, hOutFile
    mov hOutFile, 0
    jmp @@show
    
@@full:
    invoke Print, offset szPF
    call ReadInput
    invoke LoadFile, addr bBuffer
    test eax, eax
    jz @@show
    mov dwBase, offset bFile
    call FullAnalysis
    jmp @@show
    
@@exit:
    ret
MainMenu endp

; === ENTRY POINT ===
start:
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    call MainMenu
    invoke ExitProcess, 0
end start
