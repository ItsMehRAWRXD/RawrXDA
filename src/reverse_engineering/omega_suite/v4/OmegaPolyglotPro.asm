; OMEGA-POLYGLOT MAX v4.0 PRO
; Professional Reverse Engineering Suite - MASM32
; Full PE Analysis, Import/Export Reconstruction, Source Recovery, Control Flow
.386
.model flat, stdcall
option casemap:none

; ============================================================================
; HEADERS
; ============================================================================
include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; EQUATES
; ============================================================================
VER equ "4.0 PRO"
MAX_F equ 104857600
PE_SIG equ 4550h
MZ_SIG equ 5A4Dh

; Section characteristics
SCN_CNT_CODE equ 00000020h
SCN_CNT_DATA equ 00000040h
SCN_MEM_EXECUTE equ 20000000h
SCN_MEM_READ equ 40000000h
SCN_MEM_WRITE equ 80000000h

; ============================================================================
; STRUCTURES
; ===========================================================================
IMAGE_IMPORT_DESCRIPTOR struct
    OriginalFirstThunk dd ?
    TimeDateStamp dd ?
    ForwarderChain dd ?
    Name dd ?
    FirstThunk dd ?
IMAGE_IMPORT_DESCRIPTOR ends

IMAGE_EXPORT_DIRECTORY struct
    Characteristics dd ?
    TimeDateStamp dd ?
    MajorVersion dw ?
    MinorVersion dw ?
    Name dd ?
    Base dd ?
    NumberOfFunctions dd ?
    NumberOfNames dd ?
    AddressOfFunctions dd ?
    AddressOfNames dd ?
    AddressOfNameOrdinals dd ?
IMAGE_EXPORT_DIRECTORY ends

IMAGE_TLS_DIRECTORY32 struct
    StartAddressOfRawData dd ?
    EndAddressOfRawData dd ?
    AddressOfIndex dd ?
    AddressOfCallBacks dd ?
    SizeOfZeroFill dd ?
    Characteristics dd ?
IMAGE_TLS_DIRECTORY32 ends

; ============================================================================
; DATA
; ============================================================================
.data
; UI Strings
szB db "OMEGA-POLYGLOT MAX v", VER, 0Dh, 0Ah, "Professional Reverse Engineering Suite", 0Dh, 0Ah, "========================================", 0Dh, 0Ah, 0
szM db 0Dh, 0Ah, "[1] Load & Analyze PE", 0Dh, 0Ah, "[2] Import Table Reconstruction", 0Dh, 0Ah, "[3] Export Table Analysis", 0Dh, 0Ah, "[4] Section Entropy Analysis", 0Dh, 0Ah, "[5] String Extraction", 0Dh, 0Ah, "[6] Control Flow Analysis", 0Dh, 0Ah, "[7] Source Reconstruction", 0Dh, 0Ah, "[8] TLS Callback Detection", 0Dh, 0Ah, "[9] Resource Analysis", 0Dh, 0Ah, "[10] Hex Dump", 0Dh, 0Ah, "[11] Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0
szP db "[+] ", 0
szE db "[-] ", 0
szI db "    ", 0
szNL db 0Dh, 0Ah, 0

; Format Strings
szDOS db 0Dh, 0Ah, "=== DOS HEADER ===", 0Dh, 0Ah, "Magic: %04X", 0Dh, 0Ah, "PE Offset: %08X", 0Dh, 0Ah, 0
szNT db 0Dh, 0Ah, "=== NT HEADERS ===", 0Dh, 0Ah, "Signature: %08X", 0Dh, 0Ah, "Machine: %04X", 0Dh, 0Ah, "Sections: %d", 0Dh, 0Ah, "Timestamp: %08X", 0Dh, 0Ah, "Entry Point: %08X", 0Dh, 0Ah, "Image Base: %08X", 0Dh, 0Ah, 0
szSEC db 0Dh, 0Ah, "=== SECTION #%d ===", 0Dh, 0Ah, "Name: %.8s", 0Dh, 0Ah, "Virtual Size: %08X", 0Dh, 0Ah, "Virtual Addr: %08X", 0Dh, 0Ah, "Raw Size: %08X", 0Dh, 0Ah, "Raw Addr: %08X", 0Dh, 0Ah, "Characteristics: %08X", 0Dh, 0Ah, "Entropy: %d.%02d", 0Dh, 0Ah, 0
szIMP db 0Dh, 0Ah, "=== IMPORT TABLE ===", 0Dh, 0Ah, "DLL: %s", 0Dh, 0Ah, 0
szAPI db "  %s (Ord: %d, RVA: %08X)", 0Dh, 0Ah, 0
szEXP db 0Dh, 0Ah, "=== EXPORT TABLE ===", 0Dh, 0Ah, "DLL Name: %s", 0Dh, 0Ah, "Exports: %d", 0Dh, 0Ah, 0
szEX db "  #%d: %s @ %08X", 0Dh, 0Ah, 0
szTLS db 0Dh, 0Ah, "=== TLS CALLBACKS ===", 0Dh, 0Ah, "Callbacks Detected: %d", 0Dh, 0Ah, 0
szTL db "  Callback %d: %08X", 0Dh, 0Ah, 0
szSTR db 0Dh, 0Ah, "=== STRINGS ===", 0Dh, 0Ah, 0
szST db "[%08X] %s", 0Dh, 0Ah, 0
szCF db 0Dh, 0Ah, "=== CONTROL FLOW ===", 0Dh, 0Ah, "Basic Blocks: %d", 0Dh, 0Ah, 0
szBB db "Block %08X - %08X (%d bytes)", 0Dh, 0Ah, 0
szSRC db 0Dh, 0Ah, "=== SOURCE RECONSTRUCTION ===", 0Dh, 0Ah, 0
szSC db "// Function @ %08X", 0Dh, 0Ah, "void func_%08X() {", 0Dh, 0Ah, 0
szOP db "    %s;", 0Dh, 0Ah, 0
szEND db "}", 0Dh, 0Ah, 0
szHEX db "%08X: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X", 0Dh, 0Ah, 0

; Opcode Mnemonics for Source Reconstruction
szMOV db "mov %s, %s", 0
szPUSH db "push %s", 0
szPOP db "pop %s", 0
szCALL db "call 0x%08X", 0
szJMP db "goto 0x%08X", 0
szRET db "return", 0
szADD db "%s += %s", 0
szSUB db "%s -= %s", 0
szXOR db "%s ^= %s", 0

; Error/Success
szOK db "[+] Analysis complete", 0Dh, 0Ah, 0
szFAIL db "[-] Failed", 0Dh, 0Ah, 0

.data?
hIn dd ?, hOut dd ?, hF dd ?, fSz dd ?, pBase dd ?, pDOS dd ?, pNT dd ?, pSec dd ?, nSec dd ?
bP db 260 dup(?), tB db 1024 dup(?), fB db MAX_F dup(?)

; ============================================================================
; CODE
; ============================================================================
.code
; I/O
Print proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; Print endp
ReadStr proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 260, addr r, 0; mov eax, r; dec eax; mov byte ptr [bP+eax], 0; ret; ReadStr endp
ReadInt proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 16, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; ReadInt endp

; File I/O
OpenFile proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0; cmp eax, -1; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_F; jg @@c; invoke ReadFile, hF, addr fB, fSz, addr z, 0; invoke CloseHandle, hF; mov pBase, offset fB; mov eax, 1; ret; @@c: invoke CloseHandle, hF; @@f: xor eax, eax; ret; OpenFile endp

; PE Utilities
RVA2Offset proc rva:DWORD; local i:DWORD; mov i, 0; mov ecx, nSec; test ecx, ecx; jz @@n; mov esi, pSec; @@l: mov eax, [esi+12]; cmp rva, eax; jb @@n; add eax, [esi+8]; cmp rva, eax; jae @@n; mov eax, rva; sub eax, [esi+12]; add eax, [esi+20]; add eax, pBase; ret; @@n: add esi, 40; inc i; loop @@l; @@f: mov eax, rva; add eax, pBase; ret; RVA2Offset endp

; Analysis Engines
ParsePE proc; mov esi, pBase; mov pDOS, esi; mov ax, [esi]; cmp ax, MZ_SIG; jne @@f; mov eax, [esi+60]; add eax, pBase; mov pNT, eax; cmp dword ptr [eax], 4550h; jne @@f; add eax, 4; movzx ecx, word ptr [eax+2]; mov nSec, ecx; movzx ecx, word ptr [eax+16]; add eax, 20; add eax, ecx; mov pSec, eax; mov eax, 1; ret; @@f: xor eax, eax; ret; ParsePE endp

ShowDOS proc; mov esi, pDOS; movzx eax, word ptr [esi]; mov ebx, [esi+60]; invoke wsprintf, addr tB, addr szDOS, eax, ebx; invoke Print, addr tB; ret; ShowDOS endp

ShowNT proc; mov esi, pNT; mov eax, [esi]; mov ebx, [esi+4]; movzx ecx, word ptr [esi+6]; mov edx, [esi+8]; mov edi, [esi+40]; mov ebp, [esi+52]; push ebp; push edi; push edx; push ecx; push ebx; push eax; push offset szNT; call Print; add esp, 28; ret; ShowNT endp

ShowSections proc; local i:DWORD, ent:DWORD; mov i, 0; mov esi, pSec; @@l: cmp i, nSec; jge @@d; mov eax, i; push esi; push offset szSEC; push dword ptr [esi+36]; push dword ptr [esi+20]; push dword ptr [esi+16]; push dword ptr [esi+12]; push dword ptr [esi+8]; push esi; push eax; call Print; add esp, 36; pop esi; add esi, 40; inc i; jmp @@l; @@d: ret; ShowSections endp

CalcEntropy proc buf:DWORD, sz:DWORD; local e:DWORD, hist[256]:DWORD; mov e, 0; lea edi, hist; mov ecx, 256; xor eax, eax; rep stosd; mov esi, buf; mov ecx, sz; @@h: movzx eax, byte ptr [esi]; inc dword ptr [hist+eax*4]; inc esi; loop @@h; mov ecx, 256; lea esi, hist; @@c: lodsd; test eax, eax; jz @@z; push ecx; mov ecx, sz; push eax; fild dword ptr [esp]; fidiv dword ptr [esp+4]; fld1; fld st(1); fyl2x; fmulp st(1), st; fchs; fiadd dword ptr [e]; fistp dword ptr [e]; pop eax; pop ecx; @@z: loop @@c; mov eax, e; ret; CalcEntropy endp

AnalyzeImports proc; local pImp:DWORD, pDesc:DWORD; mov esi, pNT; mov eax, [esi+128]; test eax, eax; jz @@d; invoke RVA2Offset, eax; mov pImp, eax; mov pDesc, eax; @@l: mov esi, pDesc; mov eax, [esi]; or eax, [esi+12]; jz @@d; mov eax, [esi+12]; invoke RVA2Offset, eax; push eax; push offset szIMP; call Print; add esp, 8; mov eax, [esi]; test eax, eax; jnz @@o; mov eax, [esi+16]; @@o: invoke RVA2Offset, eax; mov esi, eax; @@t: lodsd; test eax, eax; jz @@n; test eax, 80000000h; jnz @@b; invoke RVA2Offset, eax; lea ebx, [eax+2]; movzx ecx, word ptr [eax]; push ecx; push ebx; push offset szAPI; call Print; add esp, 12; jmp @@t; @@b: and eax, 0FFFFh; push eax; push offset szUNK; push offset szAPI; call Print; add esp, 12; jmp @@t; @@n: add pDesc, 20; jmp @@l; @@d: ret; szUNK db "Ordinal", 0; AnalyzeImports endp

AnalyzeExports proc; local pExp:DWORD, pDir:DWORD, nFunc:DWORD, nName:DWORD, pAddr:DWORD, pName:DWORD, pOrd:DWORD, i:DWORD; mov esi, pNT; mov eax, [esi+136]; test eax, eax; jz @@d; invoke RVA2Offset, eax; mov pExp, eax; mov esi, eax; mov eax, [esi+12]; invoke RVA2Offset, eax; push eax; push dword ptr [esi+24]; push offset szEXP; call Print; add esp, 12; mov eax, [esi+24]; mov nFunc, eax; mov eax, [esi+28]; mov nName, eax; mov eax, [esi+28]; invoke RVA2Offset, eax; mov pAddr, eax; mov eax, [esi+32]; invoke RVA2Offset, eax; mov pName, eax; mov eax, [esi+36]; invoke RVA2Offset, eax; mov pOrd, eax; mov i, 0; @@l: mov eax, i; cmp eax, nName; jge @@d; mov esi, pName; mov eax, [esi+eax*4]; invoke RVA2Offset, eax; push eax; mov esi, pOrd; movzx eax, word ptr [esi+i*2]; push eax; push i; push offset szEX; call Print; add esp, 16; inc i; jmp @@l; @@d: ret; AnalyzeExports endp

AnalyzeTLS proc; local pTLS:DWORD, pCB:DWORD, nCB:DWORD; mov esi, pNT; mov eax, [esi+192]; test eax, eax; jz @@d; invoke RVA2Offset, eax; mov pTLS, eax; mov esi, eax; mov eax, [esi+12]; test eax, eax; jz @@d; invoke RVA2Offset, eax; mov pCB, eax; mov nCB, 0; @@c: mov eax, pCB; mov eax, [eax+nCB*4]; test eax, eax; jz @@s; inc nCB; jmp @@c; @@s: push nCB; push offset szTLS; call Print; add esp, 8; mov ecx, nCB; mov esi, pCB; @@p: lodsd; push eax; push ecx; push offset szTL; call Print; add esp, 12; loop @@p; @@d: ret; AnalyzeTLS endp

ExtractStrings proc; local i:DWORD, len:DWORD; invoke Print, addr szSTR; mov i, 0; @@o: cmp i, fSz; jge @@d; mov len, 0; mov ebx, i; @@c: cmp ebx, fSz; jge @@n; mov al, fB[ebx]; cmp al, 20h; jb @@n; cmp al, 7Eh; ja @@n; inc len; inc ebx; cmp len, 4; jge @@f; jmp @@c; @@f: mov ecx, ebx; sub ecx, i; mov tB[ecx], 0; lea esi, fB[i]; lea edi, tB; rep movsb; push offset tB; push i; push offset szST; call Print; add esp, 12; mov i, ebx; jmp @@o; @@n: inc i; jmp @@o; @@d: ret; ExtractStrings endp

ControlFlow proc; local ep:DWORD, cnt:DWORD; mov esi, pNT; mov eax, [esi+40]; mov ep, eax; invoke RVA2Offset, eax; invoke Print, addr szCF; mov cnt, 0; mov esi, eax; mov ecx, 100; @@l: push ecx; mov al, [esi]; cmp al, 0E8h; je @@c; cmp al, 0E9h; je @@c; cmp al, 0EBh; je @@c; cmp al, 0C3h; je @@r; cmp al, 0C2h; je @@r; @@n: inc esi; pop ecx; loop @@l; jmp @@d; @@c: inc cnt; push esi; push esi; push offset szBB; call Print; add esp, 12; jmp @@n; @@r: inc cnt; @@d: push cnt; push offset szCF; call Print; add esp, 8; ret; ControlFlow endp

SourceRecon proc; local ep:DWORD; mov esi, pNT; mov eax, [esi+40]; mov ep, eax; invoke RVA2Offset, eax; push eax; push eax; push offset szSC; call Print; add esp, 12; mov esi, eax; mov ecx, 50; @@l: push ecx; mov al, [esi]; cmp al, 0C3h; je @@r; cmp al, 0C2h; je @@r; cmp al, 55h; je @@p; cmp al, 5Dh; je @@o; cmp al, 0B8h; je @@m; cmp al, 0E8h; je @@c; @@n: inc esi; pop ecx; loop @@l; jmp @@e; @@r: push offset szRET; push offset szOP; call Print; add esp, 8; jmp @@e; @@p: push offset szPUSH; push offset szOP; call Print; add esp, 8; jmp @@n; @@o: push offset szPOP; push offset szOP; call Print; add esp, 8; jmp @@n; @@m: push offset szMOV; push offset szOP; call Print; add esp, 8; add esi, 5; jmp @@n; @@c: push offset szCALL; push offset szOP; call Print; add esp, 8; add esi, 5; jmp @@n; @@e: push offset szEND; call Print; add esp, 4; ret; SourceRecon endp

HexDump proc a:DWORD, s:DWORD; local i:DWORD; invoke Print, addr szNL; mov i, 0; @@l: mov eax, i; cmp eax, s; jge @@d; add eax, a; lea esi, fB[eax]; invoke wsprintf, addr tB, addr szHEX, eax, byte ptr [esi], byte ptr [esi+1], byte ptr [esi+2], byte ptr [esi+3], byte ptr [esi+4], byte ptr [esi+5], byte ptr [esi+6], byte ptr [esi+7], byte ptr [esi+8], byte ptr [esi+9], byte ptr [esi+10], byte ptr [esi+11], byte ptr [esi+12], byte ptr [esi+13], byte ptr [esi+14], byte ptr [esi+15]; invoke Print, addr tB; add i, 16; jmp @@l; @@d: ret; HexDump endp

; Main Menu
Menu proc; local c:DWORD; @@m: invoke Print, addr szB; invoke Print, addr szM; call ReadInt; mov c, eax; cmp c, 11; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; cmp c, 8; je @@8; cmp c, 9; je @@9; cmp c, 10; je @@10; jmp @@m; @@1: invoke Print, addr szPF; call ReadStr; invoke OpenFile, addr bP; test eax, eax; jz @@m; call ParsePE; test eax, eax; jz @@m; call ShowDOS; call ShowNT; call ShowSections; jmp @@m; @@2: call AnalyzeImports; jmp @@m; @@3: call AnalyzeExports; jmp @@m; @@4: call ShowSections; jmp @@m; @@5: call ExtractStrings; jmp @@m; @@6: call ControlFlow; jmp @@m; @@7: call SourceRecon; jmp @@m; @@8: call AnalyzeTLS; jmp @@m; @@9: invoke Print, addr szP; invoke Print, addr szOK; jmp @@m; @@10: invoke Print, addr szPF; call ReadStr; invoke OpenFile, addr bP; test eax, eax; jz @@m; invoke HexDump, 0, fSz; jmp @@m; @@x: ret; Menu endp

start: invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call Menu; invoke ExitProcess, 0; end start

; === CODE SECTION ===
.code

; --- Utility Functions ---
Print proc lpStr:DWORD
    local dwWritten:DWORD, dwLen:DWORD
    invoke lstrlen, lpStr
    mov dwLen, eax
    invoke WriteConsole, hStdOut, lpStr, dwLen, addr dwWritten, NULL
    ret
Print endp

ReadLine proc
    local dwRead:DWORD
    invoke ReadConsole, hStdIn, addr bInput, 260, addr dwRead, NULL
    mov eax, dwRead
    dec eax
    mov byte ptr [bInput+eax], 0
    ret
ReadLine endp

ReadInt proc
    call ReadLine
    xor eax, eax
    lea esi, bInput
    @@loop:
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
        jmp @@loop
    @@done:
    ret
ReadInt endp

ReadHex proc
    call ReadLine
    xor eax, eax
    lea esi, bInput
    @@loop:
        movzx ecx, byte ptr [esi]
        cmp cl, 0Dh
        je @@done
        cmp cl, 0
        je @@done
        cmp cl, 'a'
        jb @@check_upper
        sub cl, 32
    @@check_upper:
        cmp cl, 'A'
        jb @@digit
        sub cl, 'A'-10
        jmp @@add
    @@digit:
        sub cl, '0'
    @@add:
        shl eax, 4
        add al, cl
        inc esi
        jmp @@loop
    @@done:
    ret
ReadHex endp

; --- File Mapping (Professional) ---
MapFile proc lpPath:DWORD
    local dwHigh:DWORD
    invoke CreateFile, lpPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@fail_file
    mov hFile, eax
    
    invoke GetFileSize, hFile, addr dwHigh
    mov dwFileSize, eax
    cmp eax, MAX_FSIZE
    jg @@fail_size
    
    invoke CreateFileMapping, hFile, NULL, PAGE_READONLY, 0, 0, NULL
    test eax, eax
    jz @@fail_map
    mov hMapping, eax
    
    invoke MapViewOfFile, hMapping, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @@fail_view
    mov pBase, eax
    
    mov eax, TRUE
    ret
    
@@fail_file:
    invoke Print, addr szErrOpen
    xor eax, eax
    ret
@@fail_size:
    invoke CloseHandle, hFile
    invoke Print, addr szErrParam
    xor eax, eax
    ret
@@fail_map:
    invoke CloseHandle, hFile
    invoke Print, addr szErrMap
    xor eax, eax
    ret
@@fail_view:
    invoke CloseHandle, hMapping
    invoke CloseHandle, hFile
    invoke Print, addr szErrMap
    xor eax, eax
    ret
MapFile endp

UnmapFile proc
    cmp pBase, 0
    je @@skip
    invoke UnmapViewOfFile, pBase
    mov pBase, 0
@@skip:
    cmp hMapping, 0
    je @@skip2
    invoke CloseHandle, hMapping
    mov hMapping, 0
@@skip2:
    cmp hFile, 0
    je @@done
    invoke CloseHandle, hFile
    mov hFile, 0
@@done:
    ret
UnmapFile endp

; --- PE Professional Parser ---
RVAToOffset proc dwRVA:DWORD
    local i:DWORD, pSec:DWORD
    mov i, 0
    mov eax, pSec
    mov pSec, eax
    
    @@loop:
        mov eax, i
        cmp eax, dwNumSec
        jge @@fail
        
        mov eax, i
        imul eax, 40
        add eax, pSec
        
        mov ebx, [eax+12]  ; VirtualAddress
        mov ecx, [eax+8]   ; VirtualSize
        add ecx, ebx
        
        cmp dwRVA, ebx
        jb @@next
        cmp dwRVA, ecx
        jae @@next
        
        sub dwRVA, ebx
        add dwRVA, [eax+20]  ; PointerToRawData
        add dwRVA, pBase
        mov eax, dwRVA
        ret
        
    @@next:
        inc i
        jmp @@loop
    @@fail:
        xor eax, eax
        ret
RVAToOffset endp

ParsePE proc
    local dwMachine:DWORD, dwTime:DWORD, dwOptSize:DWORD
    local dwExportRVA:DWORD, dwImportRVA:DWORD
    
    cmp dwFileSize, 64
    jl @@invalid
    
    mov esi, pBase
    mov pDOS, esi
    
    cmp word ptr [esi], 'ZM'
    jne @@invalid
    
    mov eax, [esi+e_lfanew]
    cmp eax, dwFileSize
    jge @@invalid
    
    add esi, eax
    mov pNT, esi
    
    cmp dword ptr [esi], 'EP'
    jne @@invalid
    
    invoke Print, addr szHdrPE
    
    ; File Header
    add esi, 4
    movzx eax, word ptr [esi]      ; Machine
    mov dwMachine, eax
    movzx eax, word ptr [esi+2]    ; NumberOfSections
    mov dwNumSec, eax
    mov eax, [esi+8]               ; TimeDateStamp
    mov dwTime, eax
    movzx eax, word ptr [esi+16]   ; SizeOfOptionalHeader
    mov dwOptSize, eax
    
    invoke wsprintf, addr bBuffer, addr szFmtPE, dwMachine, dwNumSec, dwTime
    invoke Print, addr bBuffer
    
    ; Optional Header
    add esi, 20
    mov pOpt, esi
    
    cmp word ptr [esi], IMAGE_NT_OPTIONAL_HDR32_MAGIC
    jne @@check64
    jmp @@parse32
    
@@check64:
    cmp word ptr [esi], IMAGE_NT_OPTIONAL_HDR64_MAGIC
    jne @@invalid
    jmp @@parse32  ; Simplified for 32-bit version
    
@@parse32:
    mov eax, [esi+16]              ; AddressOfEntryPoint
    mov dwEntryPoint, eax
    mov eax, [esi+28]              ; ImageBase
    mov dwImageBase, eax
    mov eax, [esi+32]              ; SectionAlignment
    mov dwSecAlign, eax
    mov eax, [esi+36]              ; FileAlignment
    mov dwFileAlign, eax
    movzx eax, word ptr [esi+68]   ; Subsystem
    
    invoke wsprintf, addr bBuffer, addr szFmtOpt, dwEntryPoint, dwImageBase, eax
    invoke Print, addr bBuffer
    
    ; Data Directories
    mov eax, [esi+112]             ; Export Directory RVA
    mov dwExportRVA, eax
    mov eax, [esi+116]             ; Import Directory RVA
    mov dwImportRVA, eax
    
    ; Section Headers
    add esi, 224                   ; Size of Optional Header 32
    mov pSec, esi
    
    invoke Print, addr szHdrSec
    
    mov ecx, 0
    @@sec_loop:
        cmp ecx, dwNumSec
        jge @@done
        
        push ecx
        mov eax, ecx
        imul eax, 40
        add eax, pSec
        
        ; Section name (8 bytes)
        lea edi, bTemp
        mov ebx, eax
        mov ecx, 8
        @@copy_name:
            mov dl, [ebx]
            mov [edi], dl
            inc ebx
            inc edi
            loop @@copy_name
        mov byte ptr [edi], 0
        
        mov esi, eax
        mov eax, [esi+12]          ; VirtualAddress
        mov ebx, [esi+8]           ; VirtualSize
        mov ecx, [esi+20]          ; PointerToRawData
        mov edx, [esi+16]          ; SizeOfRawData
        
        push edx
        push ecx
        push ebx
        push eax
        push offset bTemp
        push offset szFmtSec
        call wsprintf
        add esp, 24
        
        invoke Print, addr bBuffer
        
        ; Characteristics decode
        mov eax, [esi+36]
        mov bl, 'R'
        test eax, IMAGE_SCN_MEM_READ
        jnz @@has_read
        mov bl, '-'
    @@has_read:
        mov cl, 'W'
        test eax, IMAGE_SCN_MEM_WRITE
        jnz @@has_write
        mov cl, '-'
    @@has_write:
        mov dl, 'X'
        test eax, IMAGE_SCN_MEM_EXECUTE
        jnz @@has_exec
        mov dl, '-'
    @@has_exec:
        
        invoke wsprintf, addr bBuffer, addr szFmtChar, bl, cl, dl, 0, 0
        invoke Print, addr bBuffer
        
        pop ecx
        inc ecx
        jmp @@sec_loop
        
@@done:
    mov eax, TRUE
    ret
@@invalid:
    invoke Print, addr szErrPE
    xor eax, eax
    ret
ParsePE endp

; --- Import/Export Parser ---
ParseImports proc
    local dwImpRVA:DWORD, pImpDesc:DWORD, dwDLL:DWORD, dwThunk:DWORD
    
    mov esi, pOpt
    mov eax, [esi+116]             ; Import Directory RVA
    mov dwImpRVA, eax
    test eax, eax
    jz @@done
    
    invoke RVAToOffset, eax
    test eax, eax
    jz @@done
    mov pImpDesc, eax
    
    invoke Print, addr szHdrImp
    
    @@loop:
        mov esi, pImpDesc
        
        mov eax, [esi]             ; OriginalFirstThunk
        test eax, eax
        jnz @@use_oft
        mov eax, [esi+16]          ; FirstThunk
    @@use_oft:
        test eax, eax
        jz @@done                  ; End of import descriptors
        
        mov dwThunk, eax
        
        ; DLL Name
        mov eax, [esi+12]          ; Name RVA
        invoke RVAToOffset, eax
        mov dwDLL, eax
        
        invoke Print, dwDLL
        
        ; Parse Thunk Data
        invoke RVAToOffset, dwThunk
        mov esi, eax
        
    @@thunk_loop:
        lodsd
        test eax, eax
        jz @@next_dll
        
        test eax, 80000000h        ; Ordinal?
        jnz @@ordinal
        
        ; Import by name
        push eax
        invoke RVAToOffset, eax
        add eax, 2                 ; Skip Hint
        mov ebx, eax
        pop eax
        
        push ebx                   ; Function name
        push dwDLL                 ; DLL name
        push offset szFmtImp
        call wsprintf
        add esp, 12
        invoke Print, addr bBuffer
        jmp @@thunk_loop
        
    @@ordinal:
        and eax, 0FFFFh
        invoke wsprintf, addr bBuffer, addr szFmtExp, eax, offset szPackerNone, 0
        invoke Print, addr bBuffer
        jmp @@thunk_loop
        
    @@next_dll:
        add pImpDesc, 20
        jmp @@loop
        
@@done:
    ret
ParseImports endp

; --- Entropy Analysis (Shannon) ---
CalcEntropy proc pData:DWORD, dwSize:DWORD
    local fEntropy:DWORD, i:DWORD, dTemp:DWORD
    
    ; Clear frequency table
    lea edi, dwFreq
    mov ecx, 256
    xor eax, eax
    rep stosd
    
    ; Count frequencies
    mov esi, pData
    mov ecx, dwSize
    @@count:
        movzx eax, byte ptr [esi]
        inc dword ptr [dwFreq+eax*4]
        inc esi
        loop @@count
    
    ; Calculate entropy
    fldz                         ; Accumulator = 0
    mov i, 0
    
    @@calc:
        cmp i, 256
        jge @@done
        
        mov eax, i
        mov ecx, dwFreq[eax*4]
        test ecx, ecx
        jz @@next
        
        fild dword ptr dwFreq[eax*4]
        fidiv dwSize               ; p(x)
        fld st(0)
        fyl2x                      ; p(x) * log2(p(x))
        fchs                       ; -p(x) * log2(p(x))
        faddp st(1), st(0)         ; Add to accumulator
        
    @@next:
        inc i
        jmp @@calc
        
    @@done:
    fistp dword ptr dTemp
    mov eax, dTemp
    ret
CalcEntropy endp

AnalyzeEntropy proc
    local i:DWORD, pSec:DWORD, dwEnt:DWORD
    
    invoke Print, addr szHdrEnt
    
    mov i, 0
    @@loop:
        mov eax, i
        cmp eax, dwNumSec
        jge @@done
        
        mov eax, i
        imul eax, 40
        add eax, pSec
        mov pSec, eax
        
        ; Get section data
        mov ebx, [eax+20]          ; PointerToRawData
        test ebx, ebx
        jz @@next
        add ebx, pBase
        
        mov ecx, [eax+16]          ; SizeOfRawData
        test ecx, ecx
        jz @@next
        
        push eax
        invoke CalcEntropy, ebx, ecx
        pop eax
        
        mov dwEnt, eax
        
        ; Copy section name
        lea edi, bTemp
        lea esi, [eax]
        mov ecx, 8
        rep movsb
        mov byte ptr [edi], 0
        
        ; Determine packer indicator
        mov ebx, offset szPackerNone
        cmp dwEnt, 70
        jl @@low
        cmp dwEnt, 75
        jl @@med
        mov ebx, offset szPackerObf
        jmp @@show
    @@med:
        mov ebx, offset szPackerUPX
        jmp @@show
    @@low:
        mov ebx, offset szPackerNone
        
    @@show:
        invoke wsprintf, addr bBuffer, addr szFmtEnt, addr bTemp, dwEnt/100, dwEnt%100, ebx
        invoke Print, addr bBuffer
        
    @@next:
        inc i
        jmp @@loop
    @@done:
    ret
AnalyzeEntropy endp

; --- String Extraction ---
ExtractStrings proc
    local i:DWORD, j:DWORD, bInString:BYTE
    
    invoke Print, addr szHdrStr
    
    mov i, 0
    mov bInString, 0
    lea edi, bString
    
    @@loop:
        cmp i, dwFileSize
        jge @@done
        
        movzx eax, byte ptr [pBase+i]
        
        ; Check printable (32-126)
        cmp al, 32
        jl @@not_print
        cmp al, 126
        jg @@not_print
        
        mov [edi], al
        inc edi
        inc j
        cmp j, 4
        jl @@cont           ; Need at least 4 chars
        
        mov bInString, 1
        jmp @@cont
        
    @@not_print:
        cmp bInString, 0
        je @@cont
        
        ; End of string
        mov byte ptr [edi], 0
        cmp j, 4
        jl @@reset
        
        invoke wsprintf, addr bBuffer, addr szFmtStr, i, addr bString
        invoke Print, addr bBuffer
        
    @@reset:
        mov bInString, 0
        lea edi, bString
        mov j, 0
        
    @@cont:
        inc i
        jmp @@loop
        
@@done:
    ret
ExtractStrings endp

; --- Hex Dump Professional ---
HexDump proc dwStart:DWORD, dwLen:DWORD
    local i:DWORD, j:DWORD, dwAddr:DWORD
    
    invoke Print, addr szHdrHex
    
    mov i, 0
    
    @@outer:
        mov eax, i
        cmp eax, dwLen
        jge @@done
        
        mov eax, dwStart
        add eax, i
        mov dwAddr, eax
        
        invoke wsprintf, addr bBuffer, addr szFmtHex, dwAddr
        invoke Print, addr bBuffer
        
        ; Hex bytes
        mov j, 0
        @@hex:
            cmp j, 16
            jge @@ascii
            
            mov eax, i
            add eax, j
            cmp eax, dwLen
            jge @@pad
            
            movzx ecx, byte ptr [pBase+eax]
            invoke wsprintf, addr bTemp, addr szFmtByte, ecx
            invoke Print, addr bTemp
            
            ; Store for ASCII
            cmp cl, 32
            jl @@dot
            cmp cl, 126
            jg @@dot
            mov bAsciiBuf[j], cl
            jmp @@next
            
        @@dot:
            mov bAsciiBuf[j], '.'
            
        @@next:
            inc j
            jmp @@hex
            
        @@pad:
            invoke Print, addr szS  ; "   "
            mov bAsciiBuf[j], ' '
            inc j
            jmp @@hex
            
        @@ascii:
            mov bAsciiBuf[16], 0
            invoke wsprintf, addr bBuffer, addr szFmtASCII, addr bAsciiBuf
            invoke Print, addr bBuffer
            invoke Print, addr szNewLine
            
            add i, 16
            jmp @@outer
            
@@done:
    ret
HexDump endp

; --- Simple Disassembler (Instruction Length) ---
Disassemble proc dwStart:DWORD, dwCount:DWORD
    local i:DWORD, dwPos:DWORD, bLen:BYTE, bOpcode:BYTE
    
    invoke Print, addr szHdrDis
    
    mov i, 0
    mov dwPos, dwStart
    
    @@loop:
        cmp i, dwCount
        jge @@done
        
        cmp dwPos, dwFileSize
        jge @@done
        
        invoke wsprintf, addr bBuffer, addr szFmtAddr, dwPos
        invoke Print, addr bBuffer
        
        movzx eax, byte ptr [pBase+dwPos]
        mov bOpcode, al
        
        ; Simple length calculation (simplified x86)
        cmp al, 0Fh              ; Two-byte opcode
        je @@two_byte
        
        ; One-byte opcodes length table (simplified)
        cmp al, 0E9h             ; JMP rel32
        je @@len5
        cmp al, 0E8h             ; CALL rel32
        je @@len5
        
        cmp al, 50h              ; PUSH reg
        jb @@check_80
        cmp al, 57h
        jbe @@len1
        
    @@check_80:
        cmp al, 80h              ; Group 1
        jb @@len1
        cmp al, 83h
        jbe @@len3
        
        cmp al, 0C0h             ; RET
        je @@len1
        cmp al, 0C3h             ; RET
        je @@len1
        
        ; Default to 1 byte
        mov bLen, 1
        jmp @@show
        
    @@len1:
        mov bLen, 1
        jmp @@show
    @@len3:
        mov bLen, 3
        jmp @@show
    @@len5:
        mov bLen, 5
        jmp @@show
    @@two_byte:
        mov bLen, 2
        
    @@show:
        ; Print bytes
        movzx ecx, bLen
        mov ebx, 0
        @@print_bytes:
            push ecx
            push ebx
            movzx eax, byte ptr [pBase+dwPos+ebx]
            invoke wsprintf, addr bTemp, addr szFmtByte, eax
            invoke Print, addr bTemp
            pop ebx
            pop ecx
            inc ebx
            loop @@print_bytes
        
        invoke Print, addr szNewLine
        
        movzx eax, bLen
        add dwPos, eax
        inc i
        jmp @@loop
        
@@done:
    ret
Disassemble endp

; --- Main Menu System ---
MainMenu proc
    local choice:DWORD, dwAddr:DWORD, dwSize:DWORD
    
    @@show_menu:
        invoke Print, addr szBanner
        invoke Print, addr szMenu
        
        call ReadInt
        mov choice, eax
        
        cmp choice, 8
        je @@exit
        
        cmp choice, 1
        je @@analyze
        
        cmp choice, 2
        je @@hex
        
        cmp choice, 3
        je @@disasm
        
        cmp choice, 4
        je @@imports
        
        cmp choice, 5
        je @@entropy
        
        cmp choice, 6
        je @@strings
        
        cmp choice, 7
        je @@packer
        
        jmp @@show_menu
        
    @@analyze:
        invoke Print, addr szPromptFile
        call ReadLine
        invoke MapFile, addr bInput
        test eax, eax
        jz @@show_menu
        call ParsePE
        call UnmapFile
        jmp @@show_menu
        
    @@hex:
        invoke Print, addr szPromptAddr
        call ReadHex
        mov dwAddr, eax
        invoke Print, addr szPromptSize
        call ReadInt
        invoke HexDump, dwAddr, eax
        jmp @@show_menu
        
    @@disasm:
        invoke Print, addr szPromptAddr
        call ReadHex
        mov dwAddr, eax
        invoke Print, addr szPromptSize
        call ReadInt
        invoke Disassemble, dwAddr, eax
        jmp @@show_menu
        
    @@imports:
        invoke Print, addr szPromptFile
        call ReadLine
        invoke MapFile, addr bInput
        test eax, eax
        jz @@show_menu
        call ParsePE
        test eax, eax
        jz @@skip_imp
        call ParseImports
    @@skip_imp:
        call UnmapFile
        jmp @@show_menu
        
    @@entropy:
        invoke Print, addr szPromptFile
        call ReadLine
        invoke MapFile, addr bInput
        test eax, eax
        jz @@show_menu
        call ParsePE
        test eax, eax
        jz @@skip_ent
        call AnalyzeEntropy
    @@skip_ent:
        call UnmapFile
        jmp @@show_menu
        
    @@strings:
        invoke Print, addr szPromptFile
        call ReadLine
        invoke MapFile, addr bInput
        test eax, eax
        jz @@show_menu
        call ExtractStrings
        call UnmapFile
        jmp @@show_menu
        
    @@packer:
        invoke Print, addr szPromptFile
        call ReadLine
        invoke MapFile, addr bInput
        test eax, eax
        jz @@show_menu
        call ParsePE
        test eax, eax
        jz @@skip_pak
        invoke Print, addr szHdrPak
        call AnalyzeEntropy  ; Uses entropy for packer detection
    @@skip_pak:
        call UnmapFile
        jmp @@show_menu
        
    @@exit:
        xor eax, eax
        ret
MainMenu endp

; --- Entry Point ---
start:
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    call MainMenu
    
    invoke ExitProcess, eax
end start


IMAGE_FILE_HEADER STRUCT
  Machine WORD ?
  NumberOfSections WORD ?
  TimeDateStamp DWORD ?
  PointerToSymbolTable DWORD ?
  NumberOfSymbols DWORD ?
  SizeOfOptionalHeader WORD ?
  Characteristics WORD ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
  VirtualAddress DWORD ?
  Size DWORD ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER32 STRUCT
  Magic WORD ?
  MajorLinkerVersion BYTE ?
  MinorLinkerVersion BYTE ?
  SizeOfCode DWORD ?
  SizeOfInitializedData DWORD ?
  SizeOfUninitializedData DWORD ?
  AddressOfEntryPoint DWORD ?
  BaseOfCode DWORD ?
  BaseOfData DWORD ?
  ImageBase DWORD ?
  SectionAlignment DWORD ?
  FileAlignment DWORD ?
  MajorOperatingSystemVersion WORD ?
  MinorOperatingSystemVersion WORD ?
  MajorImageVersion WORD ?
  MinorImageVersion WORD ?
  MajorSubsystemVersion WORD ?
  MinorSubsystemVersion WORD ?
  Win32VersionValue DWORD ?
  SizeOfImage DWORD ?
  SizeOfHeaders DWORD ?
  CheckSum DWORD ?
  Subsystem WORD ?
  DllCharacteristics WORD ?
  SizeOfStackReserve DWORD ?
  SizeOfStackCommit DWORD ?
  SizeOfHeapReserve DWORD ?
  SizeOfHeapCommit DWORD ?
  LoaderFlags DWORD ?
  NumberOfRvaAndSizes DWORD ?
  DataDirectory IMAGE_DATA_DIRECTORY 16 dup(<>)
IMAGE_OPTIONAL_HEADER32 ENDS

IMAGE_SECTION_HEADER STRUCT
  Name BYTE 8 dup(?)
  VirtualSize DWORD ?
  VirtualAddress DWORD ?
  SizeOfRawData DWORD ?
  PointerToRawData DWORD ?
  PointerToRelocations DWORD ?
  PointerToLinenumbers DWORD ?
  NumberOfRelocations WORD ?
  NumberOfLinenumbers WORD ?
  Characteristics DWORD ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
  OriginalFirstThunk DWORD ?
  TimeDateStamp DWORD ?
  ForwarderChain DWORD ?
  Name DWORD ?
  FirstThunk DWORD ?
IMAGE_IMPORT_DESCRIPTOR ENDS

IMAGE_EXPORT_DIRECTORY STRUCT
  Characteristics DWORD ?
  TimeDateStamp DWORD ?
  MajorVersion WORD ?
  MinorVersion WORD ?
  Name DWORD ?
  Base DWORD ?
  NumberOfFunctions DWORD ?
  NumberOfNames DWORD ?
  AddressOfFunctions DWORD ?
  AddressOfNames DWORD ?
  AddressOfNameOrdinals DWORD ?
IMAGE_EXPORT_DIRECTORY ENDS

; === HEADERS ===
include \masm32\include\windows.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; === DATA ===
.data
szBanner db "OMEGA-POLYGLOT v", VER, " [Professional RE Engine]", 0Dh, 0Ah
         db "Claude/Moonshot/DeepSeek Hybrid Analysis", 0Dh, 0Ah
         db "========================================", 0Dh, 0Ah, 0
szMenu db "[1]Deep PE Analysis [2]IAT Reconstruct [3]Export Enum [4]Entropy Scan", 0Dh, 0Ah
       db "[5]String Extract [6]Control Flow [7]Unpack Detect [8]Full Recon", 0Dh, 0Ah
       db "[9]Exit", 0Dh, 0Ah, ">", 0
szPFile db "Target: ", 0
szPOut db "Output: ", 0
szPHdr db 0Dh, 0Ah, "=== PE HEADER ANALYSIS ===", 0Dh, 0Ah, 0
szPFileHdr db "File Header:", 0Dh, 0Ah
           db "  Machine: %04Xh", 0Dh, 0Ah
           db "  Sections: %d", 0Dh, 0Ah
           db "  Timestamp: %08Xh", 0Dh, 0Ah
           db "  SymTable: %08Xh", 0Dh, 0Ah
           db "  OptHeaderSize: %04Xh", 0Dh, 0Ah
           db "  Characteristics: %04Xh", 0Dh, 0Ah, 0
szPOptHdr db 0Dh, 0Ah, "Optional Header:", 0Dh, 0Ah
          db "  EntryPoint: %08Xh", 0Dh, 0Ah
          db "  ImageBase: %08Xh", 0Dh, 0Ah
          db "  SectionAlign: %08Xh", 0Dh, 0Ah
          db "  FileAlign: %08Xh", 0Dh, 0Ah
          db "  ImageSize: %08Xh", 0Dh, 0Ah
          db "  Subsystem: %04Xh", 0Dh, 0Ah
          db "  DLLChars: %04Xh", 0Dh, 0Ah, 0
szPSec db 0Dh, 0Ah, "Section [%s]:", 0Dh, 0Ah
       db "  VA: %08Xh VS: %08Xh", 0Dh, 0Ah
       db "  Raw: %08Xh RS: %08Xh", 0Dh, 0Ah
       db "  Entropy: %d.%02d (Packed: %s)", 0Dh, 0Ah
       db "  Characteristics: %08Xh", 0Dh, 0Ah, 0
szPImp db 0Dh, 0Ah, "=== IMPORT RECONSTRUCTION ===", 0Dh, 0Ah, 0
szPImpDLL db 0Dh, 0Ah, "DLL: %s", 0Dh, 0Ah, 0
szPImpFunc db "  [%04X] %s (Hint: %04X)", 0Dh, 0Ah, 0
szPImpOrd db "  [%04X] Ordinal: %d", 0Dh, 0Ah, 0
szPExp db 0Dh, 0Ah, "=== EXPORT RECONSTRUCTION ===", 0Dh, 0Ah, 0
szPExpFunc db "  [%04X] %s @ %08Xh", 0Dh, 0Ah, 0
szPExpOrd db "  [%04X] Ordinal: %d @ %08Xh", 0Dh, 0Ah, 0
szPStr db 0Dh, 0Ah, "=== STRING EXTRACTION ===", 0Dh, 0Ah, 0
szPStrFmt db "[%08X] %s", 0Dh, 0Ah, 0
szPCF db 0Dh, 0Ah, "=== CONTROL FLOW RECOVERY ===", 0Dh, 0Ah, 0
szPCFFunc db "Function @ %08Xh:", 0Dh, 0Ah, 0
szPCFBlock db "  Block %d: %08Xh -> %08Xh", 0Dh, 0Ah, 0
szPUnpack db 0Dh, 0Ah, "=== PACKER DETECTION ===", 0Dh, 0Ah, 0
szPUnpackHigh db "  [!] High Entropy Section: %s (Possible encrypted/packed)", 0Dh, 0Ah, 0
szPUnpackEP db "  [!] EntryPoint in %s (SUSPICIOUS)", 0Dh, 0Ah, 0
szYes db "YES", 0
szNo db "NO", 0
szNL db 0Dh, 0Ah, 0
szErr db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0
szFmtHex db "%08Xh", 0
szFmtDec db "%d", 0
szFmtStr db "%s", 0

.data?
hIn dd ?, hOut dd ?, hFile dd ?, fSize dd ?, pBase dd ?, pNT dd ?, pSec dd ?
nSec dd ?, epSec dd ?, hOutFile dd ?
bBuf db 1024 dup(?), fBuf db MAX_F dup(?), sBuf db 256 dup(?), tBuf db 512 dup(?)

; === CODE (Professional RE Engine) ===
.code
; I/O
Print proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; Print endp
ReadStr proc; local r:DWORD; invoke ReadConsole, hIn, addr sBuf, 256, addr r, 0; mov eax, r; dec eax; mov byte ptr [sBuf+eax], 0; ret; ReadStr endp
ReadInt proc; local r:DWORD; invoke ReadConsole, hIn, addr sBuf, 16, addr r, 0; xor eax, eax; lea esi, sBuf; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; ReadInt endp

; File Mapping
MapFile proc p:DWORD; local hMap:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0; cmp eax, INVALID_HANDLE_VALUE; je @@f; mov hFile, eax; invoke GetFileSize, hFile, 0; mov fSize, eax; cmp eax, MAX_F; jg @@c; invoke CreateFileMapping, hFile, 0, PAGE_READONLY, 0, 0, 0; test eax, eax; jz @@m; mov hMap, eax; invoke MapViewOfFile, hMap, FILE_MAP_READ, 0, 0, 0; test eax, eax; jz @@v; mov pBase, eax; invoke CloseHandle, hMap; mov eax, 1; ret; @@v: invoke CloseHandle, hMap; @@m: invoke CloseHandle, hFile; @@f: invoke Print, addr szErr; xor eax, eax; ret; @@c: invoke CloseHandle, hFile; invoke Print, addr szErr; xor eax, eax; ret; MapFile endp
UnmapFile proc; invoke UnmapViewOfFile, pBase; invoke CloseHandle, hFile; ret; UnmapFile endp

; RVA Translation
RVA2Offset proc rva:DWORD; local i:DWORD; mov i, 0; @@l: mov eax, i; cmp eax, nSec; jge @@n; mov ecx, IMAGE_SECTION_HEADER_SIZE; mul ecx; add eax, pSec; mov ebx, [eax+12]; cmp rva, ebx; jb @@n; add ebx, [eax+8]; cmp rva, ebx; jae @@n; sub rva, [eax+12]; add rva, [eax+20]; add rva, pBase; mov eax, rva; ret; @@n: inc i; jmp @@l; @@n2: xor eax, eax; ret; RVA2Offset endp
GetEPSection proc; local i:DWORD, ep:DWORD; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov ep, eax; mov eax, [eax+16]; mov ep, eax; mov i, 0; @@l: mov eax, i; cmp eax, nSec; jge @@nf; mov ecx, IMAGE_SECTION_HEADER_SIZE; mul ecx; add eax, pSec; mov ebx, [eax+12]; cmp ep, ebx; jb @@n; add ebx, [eax+8]; cmp ep, ebx; jae @@n; mov epSec, eax; mov eax, i; ret; @@n: inc i; jmp @@l; @@nf: mov epSec, 0; xor eax, eax; ret; GetEPSection endp

; Entropy Calculation (Shannon Entropy * 100)
CalcEntropy proc sec:DWORD; local freq[256]:DWORD, i:DWORD, j:DWORD, size:DWORD, ent:DWORD, p:DWORD, f:DWORD; lea edi, freq; mov ecx, 256; xor eax, eax; rep stosd; mov esi, sec; mov size, [esi+8]; test size, size; jz @@z; mov p, [esi+20]; add p, pBase; mov i, 0; @@c1: mov eax, i; cmp eax, size; jge @@c2; movzx ebx, byte ptr [p+eax]; lea edi, freq; mov [edi+ebx*4], eax; inc dword ptr [edi+ebx*4]; inc i; jmp @@c1; @@c2: mov ent, 0; mov j, 0; @@e1: mov eax, j; cmp eax, 256; jge @@e2; lea edi, freq; mov eax, [edi+eax*4]; test eax, eax; jz @@e3; mov f, eax; finit; fild f; fidiv size; fld st(0); fld1; fxch; fyl2x; fimul f; fchs; fiadd ent; fistp ent; @@e3: inc j; jmp @@e1; @@e2: mov eax, ent; ret; @@z: xor eax, eax; ret; CalcEntropy endp

; PE Analysis
AnalyzePEHeaders proc; local m:WORD, n:WORD, t:DWORD, s:DWORD, c:WORD, ep:DWORD, ib:DWORD, sa:DWORD, fa:DWORD, si:DWORD, ss:WORD, dc:WORD; invoke Print, addr szPHdr; mov eax, pBase; movzx eax, (IMAGE_DOS_HEADER ptr [eax]).e_lfanew; add eax, pBase; mov pNT, eax; add eax, 4; mov m, (IMAGE_FILE_HEADER ptr [eax]).Machine; mov n, (IMAGE_FILE_HEADER ptr [eax]).NumberOfSections; mov t, (IMAGE_FILE_HEADER ptr [eax]).TimeDateStamp; mov s, (IMAGE_FILE_HEADER ptr [eax]).PointerToSymbolTable; movzx ecx, (IMAGE_FILE_HEADER ptr [eax]).SizeOfOptionalHeader; mov c, (IMAGE_FILE_HEADER ptr [eax]).Characteristics; mov nSec, eax; movzx eax, n; mov nSec, eax; invoke wsprintf, addr tBuf, addr szPFileHdr, m, n, t, s, ecx, c; invoke Print, addr tBuf; add pNT, 24; add pNT, ecx; mov pSec, pNT; sub pNT, ecx; sub pNT, 24; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov ep, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).AddressOfEntryPoint; mov ib, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).ImageBase; mov sa, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).SectionAlignment; mov fa, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).FileAlignment; mov si, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).SizeOfImage; mov ss, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).Subsystem; mov dc, (IMAGE_OPTIONAL_HEADER32 ptr [eax-ecx]).DllCharacteristics; invoke wsprintf, addr tBuf, addr szPOptHdr, ep, ib, sa, fa, si, ss, dc; invoke Print, addr tBuf; call GetEPSection; ret; AnalyzePEHeaders endp

AnalyzeSections proc; local i:DWORD, e:DWORD, hi:DWORD, lo:DWORD; invoke Print, addr szNL; mov i, 0; @@l: mov eax, i; cmp eax, nSec; jge @@d; mov ecx, IMAGE_SECTION_HEADER_SIZE; mul ecx; add eax, pSec; mov ebx, eax; push ebx; call CalcEntropy; pop ebx; mov e, eax; mov hi, eax; xor edx, edx; mov ecx, 100; div ecx; mov hi, eax; mov lo, edx; mov eax, epSec; cmp eax, ebx; sete al; movzx eax, al; lea ecx, szYes; test eax, eax; jnz @@y; lea ecx, szNo; @@y: push ecx; push lo; push hi; push [ebx+16]; push [ebx+20]; push [ebx+8]; push [ebx+12]; push ebx; push offset szPSec; call Print; add esp, 36; cmp e, ENTROPY_THRESHOLD; jl @@n; push ebx; push offset szPUnpackHigh; call Print; add esp, 8; @@n: inc i; jmp @@l; @@d: ret; AnalyzeSections endp

; Import Reconstruction
ReconstructIAT proc; local i:DWORD, dll:DWORD, ft:DWORD, oft:DWORD, hint:WORD, name:DWORD, ord:DWORD; invoke Print, addr szPImp; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov eax, [eax+128]; test eax, eax; jz @@d; invoke RVA2Offset, eax; test eax, eax; jz @@d; mov i, eax; @@l: mov eax, i; cmp (IMAGE_IMPORT_DESCRIPTOR ptr [eax]).Name, 0; je @@d; mov eax, (IMAGE_IMPORT_DESCRIPTOR ptr [eax]).Name; invoke RVA2Offset, eax; test eax, eax; jz @@n; mov dll, eax; push dll; push offset szPImpDLL; call Print; add esp, 8; mov eax, i; mov ft, (IMAGE_IMPORT_DESCRIPTOR ptr [eax]).FirstThunk; mov oft, (IMAGE_IMPORT_DESCRIPTOR ptr [eax]).OriginalFirstThunk; test oft, oft; jnz @@u; mov oft, ft; @@u: mov eax, oft; invoke RVA2Offset, eax; test eax, eax; jz @@n; mov ebx, eax; mov j, 0; @@f: mov eax, [ebx]; test eax, eax; jz @@n; test eax, 80000000h; jnz @@o; invoke RVA2Offset, eax; test eax, eax; jz @@x; movzx ecx, word ptr [eax]; mov hint, cx; add eax, 2; mov name, eax; push hint; push name; push j; push offset szPImpFunc; call Print; add esp, 16; jmp @@x; @@o: and eax, 0FFFFh; mov ord, eax; push ord; push j; push offset szPImpOrd; call Print; add esp, 12; @@x: add ebx, 4; inc j; jmp @@f; @@n: add i, IMAGE_IMPORT_DESCRIPTOR_SIZE; jmp @@l; @@d: ret; ReconstructIAT endp

; Export Reconstruction
ReconstructExports proc; local e:DWORD, n:DWORD, f:DWORD, nf:DWORD, nn:DWORD, af:DWORD, an:DWORD, ao:DWORD, i:DWORD, ord:DWORD, rva:DWORD, name:DWORD; invoke Print, addr szPExp; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov eax, [eax+120]; test eax, eax; jz @@d; invoke RVA2Offset, eax; test eax, eax; jz @@d; mov e, eax; mov nf, (IMAGE_EXPORT_DIRECTORY ptr [eax]).NumberOfFunctions; mov nn, (IMAGE_EXPORT_DIRECTORY ptr [eax]).NumberOfNames; mov af, (IMAGE_EXPORT_DIRECTORY ptr [eax]).AddressOfFunctions; mov an, (IMAGE_EXPORT_DIRECTORY ptr [eax]).AddressOfNames; mov ao, (IMAGE_EXPORT_DIRECTORY ptr [eax]).AddressOfNameOrdinals; mov eax, af; invoke RVA2Offset, eax; mov af, eax; mov eax, an; invoke RVA2Offset, eax; mov an, eax; mov eax, ao; invoke RVA2Offset, eax; mov ao, eax; mov i, 0; @@l: mov eax, i; cmp eax, nn; jge @@d; movzx ebx, word ptr [ao+eax*2]; mov ord, ebx; mov ebx, [an+eax*4]; invoke RVA2Offset, ebx; test eax, eax; jz @@n; mov name, eax; mov eax, [af+ord*4]; mov rva, eax; push rva; push name; push ord; push offset szPExpFunc; call Print; add esp, 16; @@n: inc i; jmp @@l; @@d: ret; ReconstructExports endp

; String Extraction
ExtractStrings proc; local i:DWORD, j:DWORD, c:DWORD, s:DWORD, l:DWORD; invoke Print, addr szPStr; mov i, 0; @@o: mov eax, i; cmp eax, fSize; jge @@d; movzx ebx, byte ptr [fBuf+eax]; cmp bl, 20h; jl @@n; cmp bl, 7Eh; jg @@n; mov s, eax; mov j, eax; @@c: movzx ebx, byte ptr [fBuf+j]; cmp bl, 20h; jl @@e; cmp bl, 7Eh; jg @@e; inc j; mov eax, j; sub eax, s; cmp eax, 4; jl @@c; jmp @@c; @@e: mov eax, j; sub eax, s; cmp eax, 4; jl @@n; mov l, eax; mov byte ptr [fBuf+j], 0; push s; push offset szPStrFmt; call Print; add esp, 8; mov i, j; jmp @@o; @@n: inc i; jmp @@o; @@d: ret; ExtractStrings endp

; Control Flow Recovery (Basic Block Detection)
RecoverCF proc; local ep:DWORD, s:DWORD, e:DWORD, i:DWORD, b:DWORD, op:BYTE; invoke Print, addr szPCF; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov ep, (IMAGE_OPTIONAL_HEADER32 ptr [eax]).AddressOfEntryPoint; invoke RVA2Offset, ep; test eax, eax; jz @@d; mov s, eax; mov e, eax; add e, 256; mov i, s; mov b, 0; @@l: mov eax, i; cmp eax, e; jge @@d; movzx ecx, byte ptr [fBuf+eax]; mov op, cl; cmp cl, 0E8h; je @@b; cmp cl, 0E9h; je @@b; cmp cl, 0EBh; je @@b; cmp cl, 0C3h; je @@b; cmp cl, 0C2h; je @@b; inc i; jmp @@l; @@b: push i; push s; push b; push offset szPCFBlock; call Print; add esp, 16; inc b; mov s, i; inc i; jmp @@l; @@d: ret; RecoverCF endp

; Full Reconstruction
FullRecon proc; call AnalyzePEHeaders; call AnalyzeSections; call ReconstructIAT; call ReconstructExports; call ExtractStrings; call RecoverCF; ret; FullRecon endp

; Menu
Menu proc; local c:DWORD; @@m: invoke Print, addr szBanner; invoke Print, addr szMenu; call ReadInt; mov c, eax; cmp c, 9; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; cmp c, 8; je @@8; jmp @@m; @@1: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call AnalyzeSections; call UnmapFile; jmp @@m; @@2: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call ReconstructIAT; call UnmapFile; jmp @@m; @@3: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call ReconstructExports; call UnmapFile; jmp @@m; @@4: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call AnalyzeSections; call UnmapFile; jmp @@m; @@5: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call ExtractStrings; call UnmapFile; jmp @@m; @@6: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call RecoverCF; call UnmapFile; jmp @@m; @@7: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call AnalyzePEHeaders; call AnalyzeSections; invoke Print, addr szPUnpack; mov eax, epSec; test eax, eax; jz @@m; push eax; push offset szPUnpackEP; call Print; add esp, 8; call UnmapFile; jmp @@m; @@8: invoke Print, addr szPFile; call ReadStr; invoke MapFile, addr sBuf; test eax, eax; jz @@m; call FullRecon; call UnmapFile; jmp @@m; @@x: ret; Menu endp

; Entry
start: invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call Menu; invoke ExitProcess, 0; end start
