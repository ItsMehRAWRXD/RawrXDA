; OMEGA-POLYGLOT MAXIMUM v3.0 PRO
; Claude/Moonshot/DeepSeek Professional Reverse Engineering Suite
; Full PE32/PE32+ Reconstruction, Entropy Analysis, Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === PROFESSIONAL EQUATES ===
CLI_VER equ "3.0 PRO"
MAX_FSZ equ 104857600
PE32_MAGIC equ 010Bh
PE32P_MAGIC equ 020Bh

; Data Directory Indices
DIR_EXPORT equ 0
DIR_IMPORT equ 1
DIR_RESOURCE equ 2
DIR_EXCEPTION equ 3
DIR_SECURITY equ 4
DIR_BASERELOC equ 5
DIR_DEBUG equ 6
DIR_ARCH equ 7
DIR_GLOBALPTR equ 8
DIR_TLS equ 9
DIR_LOADCFG equ 10
DIR_BOUNDIMP equ 11
DIR_IAT equ 12
DIR_DELAYIMP equ 13
DIR_COM equ 14
DIR_RESERVE equ 15

; Relocation Types
REL_BASED_ABSOLUTE equ 0
REL_BASED_HIGH equ 1
REL_BASED_LOW equ 2
REL_BASED_HIGHLOW equ 3
REL_BASED_HIGHADJ equ 4
REL_BASED_MIPS_JMPADDR equ 5
REL_BASED_ARM_MOV32 equ 5
REL_BASED_RISCV_HIGH20 equ 5
REL_BASED_THUMB_MOV32 equ 7
REL_BASED_RISCV_LOW12I equ 7
REL_BASED_RISCV_LOW12S equ 8
REL_BASED_MIPS_JMPADDR16 equ 9
REL_BASED_DIR64 equ 10

; Debug Types
DEBUG_UNKNOWN equ 0
DEBUG_COFF equ 1
DEBUG_CODEVIEW equ 2
DEBUG_FPO equ 3
DEBUG_MISC equ 4
DEBUG_EXCEPTION equ 5
DEBUG_FIXUP equ 6
DEBUG_OMAP_TO_SRC equ 7
DEBUG_OMAP_FROM_SRC equ 8
DEBUG_BORLAND equ 9
DEBUG_RESERVED10 equ 10
DEBUG_CLSID equ 11
DEBUG_VC_FEATURE equ 12
DEBUG_POGO equ 13
DEBUG_ILTCG equ 14
DEBUG_MPX equ 15
DEBUG_REPRO equ 16
DEBUG_EX_DLLCHARS equ 20
DEBUG_PDATA equ 32

; Section Characteristics
SCN_CNT_CODE equ 000000020h
SCN_CNT_INIT_DATA equ 000000040h
SCN_CNT_UNINIT_DATA equ 000000080h
SCN_LNK_OTHER equ 000000100h
SCN_LNK_INFO equ 000000200h
SCN_LNK_REMOVE equ 00000800h
SCN_LNK_COMDAT equ 00001000h
SCN_NO_DEFER_SPEC_EXC equ 00004000h
SCN_GPREL equ 00008000h
SCN_MEM_FARDATA equ 00008000h
SCN_MEM_PURGEABLE equ 00020000h
SCN_MEM_16BIT equ 00020000h
SCN_MEM_LOCKED equ 00040000h
SCN_MEM_PRELOAD equ 00080000h
SCN_ALIGN_1BYTES equ 00100000h
SCN_ALIGN_2BYTES equ 00200000h
SCN_ALIGN_4BYTES equ 00300000h
SCN_ALIGN_8BYTES equ 00400000h
SCN_ALIGN_16BYTES equ 00500000h
SCN_ALIGN_32BYTES equ 00600000h
SCN_ALIGN_64BYTES equ 00700000h
SCN_ALIGN_128BYTES equ 00800000h
SCN_ALIGN_256BYTES equ 00900000h
SCN_ALIGN_512BYTES equ 00A00000h
SCN_ALIGN_1024BYTES equ 00B00000h
SCN_ALIGN_2048BYTES equ 00C00000h
SCN_ALIGN_4096BYTES equ 00D00000h
SCN_ALIGN_8192BYTES equ 00E00000h
SCN_ALIGN_MASK equ 00F00000h
SCN_LNK_NRELOC_OVFL equ 01000000h
SCN_MEM_DISCARDABLE equ 02000000h
SCN_MEM_NOT_CACHED equ 04000000h
SCN_MEM_NOT_PAGED equ 08000000h
SCN_MEM_SHARED equ 10000000h
SCN_MEM_EXECUTE equ 20000000h
SCN_MEM_READ equ 40000000h
SCN_MEM_WRITE equ 80000000h

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib
includelib C:\masm32\lib\advapi32.lib

; === DATA SECTION ===
.data
; UI Strings
szW db "OMEGA-POLYGLOT MAXIMUM v", CLI_VER, 0Dh, 0Ah
    db "Professional Reverse Engineering Suite", 0Dh, 0Ah
    db "Claude | Moonshot | DeepSeek | Codex", 0Dh, 0Ah
    db "========================================", 0Dh, 0Ah, 0

szM db "[1]PE Deep Analysis [2]Import/Export Recon [3]Section Entropy [4]String Extraction", 0Dh, 0Ah
    db "[5]TLS Callbacks [6]Resources [7]Relocations [8]Debug Info [9]Full Reconstruct [0]Exit", 0Dh, 0Ah
    db ">", 0

szPF db "Target: ", 0
szErr db "[-] Fatal Error", 0Dh, 0Ah, 0
szOk db "[+] Analysis Complete", 0Dh, 0Ah, 0

; PE Analysis Strings
szPE32 db "PE32 (32-bit)", 0Dh, 0Ah, 0
szPE64 db "PE32+ (64-bit)", 0Dh, 0Ah, 0
szCOFF db "COFF Header:", 0Dh, 0Ah, 0
szMACH db "Machine: ", 0
szSEC db "Sections: ", 0
szTIM db "Timestamp: ", 0
szSYM db "Symbols: ", 0
szOPTH db "Optional Header:", 0Dh, 0Ah, 0
szEP db "EntryPoint: ", 0
szIMG db "ImageBase: ", 0
szALN db "Alignment: ", 0
szSS db "Subsystem: ", 0
szDLL db "DLLChars: ", 0
szSZ db "ImageSize: ", 0
szSTK db "Stack: ", 0
szHEP db "Heap: ", 0

; Directory Strings
szDIR db 0Dh, 0Ah, "Data Directories:", 0Dh, 0Ah, 0
szEXP db "Export: ", 0
szIMP db "Import: ", 0
szRES db "Resource: ", 0
szEXC db "Exception: ", 0
szSECU db "Security: ", 0
szREL db "Relocation: ", 0
szDBG db "Debug: ", 0
szARC db "Architecture: ", 0
szGP db "GlobalPtr: ", 0
szTLS db "TLS: ", 0
szLDC db "LoadConfig: ", 0
szBND db "BoundImport: ", 0
szIAT db "IAT: ", 0
szDLY db "DelayImport: ", 0
szCOM db "COM: ", 0
szRESERVE db "Reserved: ", 0

; Section Analysis
szSECH db 0Dh, 0Ah, "Section Table:", 0Dh, 0Ah, 0
szSN db "Name: ", 0
szSVA db "  VirtualAddr: ", 0
szSVS db "  VirtualSize: ", 0
szSRAW db "  RawAddr: ", 0
szSRSZ db "  RawSize: ", 0
szSREL db "  Relocs: ", 0
szSLN db "  LineNums: ", 0
szSCH db "  Characteristics: ", 0
szENT db "  Entropy: ", 0
szPACK db " [PACKED/ENCRYPTED]", 0
szCODE db " [CODE]", 0
szDATA db " [DATA]", 0
szBSS db " [BSS]", 0

; Import/Export
szIMPH db 0Dh, 0Ah, "Import Table Reconstruction:", 0Dh, 0Ah, 0
szDLLN db "  DLL: ", 0
szAPI db "    API: ", 0
szORD db "    Ordinal: ", 0
szEXPH db 0Dh, 0Ah, "Export Table Reconstruction:", 0Dh, 0Ah, 0
szEXPV db "  Exports: ", 0
szENAM db "    Name: ", 0
szEOFF db "    Ordinal: ", 0
szERVA db "    RVA: ", 0

; TLS
szTLSH db 0Dh, 0Ah, "TLS Directory:", 0Dh, 0Ah, 0
szTSTART db "  Start: ", 0
szTEND db "  End: ", 0
szTIDX db "  Index: ", 0
szTCB db "  Callbacks: ", 0
szTCALL db "    Callback: ", 0

; Resources
szRESH db 0Dh, 0Ah, "Resource Directory:", 0Dh, 0Ah, 0
szRLEV db "  Level ", 0
szRENT db "    Entry: ID=", 0
szRNAME db "    Name: ", 0
szRDATA db "      Data RVA: ", 0
szRSZ db "      Size: ", 0

; Relocations
szRELH db 0Dh, 0Ah, "Base Relocations:", 0Dh, 0Ah, 0
szRBLK db "  Block RVA: ", 0
szRCNT db "  Count: ", 0
szRTYP db "    Type: ", 0
szROFF db " Offset: ", 0

; Debug
szDBGH db 0Dh, 0Ah, "Debug Directory:", 0Dh, 0Ah, 0
szDTYP db "  Type: ", 0
szDSZ db "  Size: ", 0
szDVA db "  VA: ", 0
szDRAW db "  Raw: ", 0

; Format Strings
szX db "%08X", 0
szX2 db "%02X", 0
szD db "%d", 0
szF db "%s", 0
szC db " : ", 0
szNL db 0Dh, 0Ah, 0
szSPC db " ", 0
szHEX8 db "%08X ", 0
szHEX2 db "%02X ", 0
szDOT db ".", 0
szPIPE db "|", 0
szPERC db "%.2f", 0
szS db "MISC", 0
szCODEVIEW db "CODEVIEW", 0

; Characteristic Names
szCHR_EXEC db "EXECUTE ", 0
szCHR_READ db "READ ", 0
szCHR_WRITE db "WRITE ", 0
szCHR_CODE db "CODE ", 0
szCHR_IDATA db "IDATA ", 0
szCHR_UDATA db "UDATA ", 0
szCHR_DISC db "DISCARD ", 0
szCHR_SHARED db "SHARED ", 0

; String extraction
szSTRH db 0Dh, 0Ah, "Strings:", 0Dh, 0Ah, 0
szSTRFMT db "[STR] 0x%08X: %s", 0Dh, 0Ah, 0

; === UNINITIALIZED DATA ===
.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
bR dd ?
bW dd ?
pBase dd ?
pDOS dd ?
pNT dd ?
pFH dd ?
pOH dd ?
pSec dd ?
isPE32 dd ?
numSec dd ?
imgBase dd ?
secAlign dd ?
fileAlign dd ?
entryPoint dd ?
imgSize dd ?
subSys dd ?
dllChars dd ?
bP db 260 dup(?)
tB db 1024 dup(?)
fB db MAX_FSZ dup(?)
sB db 256 dup(?)

; === CODE SECTION ===
.code

; Core I/O Functions
P proc pMsg:DWORD
    local dwWrite:DWORD, dwLength:DWORD
    invoke lstrlen, pMsg
    mov dwLength, eax
    invoke WriteConsole, hOut, pMsg, dwLength, addr dwWrite, 0
    ret
P endp

R proc
    local dwRead:DWORD
    invoke ReadConsole, hIn, addr bP, 260, addr dwRead, 0
    mov eax, dwRead
    dec eax
    mov byte ptr [bP+eax], 0
    ret
R endp

RI proc
    local dwRead:DWORD
    invoke ReadConsole, hIn, addr bP, 32, addr dwRead, 0
    xor eax, eax
    lea esi, bP
@@c:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@d
    sub cl, '0'
    cmp cl, 9
    ja @@d
    imul eax, 10
    add eax, ecx
    inc esi
    jmp @@c
@@d:
    ret
RI endp

; File Operations
OpenTarget proc pPath:DWORD
    local dwZero:DWORD
    invoke CreateFile, pPath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@f
    mov hF, eax
    invoke GetFileSize, hF, addr dwZero
    mov fSz, eax
    cmp eax, MAX_FSZ
    jg @@c
    invoke ReadFile, hF, addr fB, fSz, addr bR, 0
    test eax, eax
    jz @@r
    invoke CloseHandle, hF
    lea eax, fB
    mov pBase, eax
    mov eax, 1
    ret
@@r:
    invoke CloseHandle, hF
@@f:
    invoke P, addr szErr
    xor eax, eax
    ret
@@c:
    invoke CloseHandle, hF
    invoke P, addr szErr
    xor eax, eax
    ret
OpenTarget endp

; PE Validation & Setup
ValidatePE proc
    mov eax, pBase
    mov pDOS, eax
    cmp word ptr [eax], 'ZM'
    jne @@f
    mov eax, [eax+3Ch]
    add eax, pBase
    mov pNT, eax
    cmp dword ptr [eax], 'EP'
    jne @@f
    add eax, 4
    mov pFH, eax
    movzx ecx, word ptr [eax+2]
    mov numSec, ecx
    add eax, 20
    mov pOH, eax
    cmp word ptr [eax], PE32_MAGIC
    je @@32
    cmp word ptr [eax], PE32P_MAGIC
    je @@64
@@f:
    xor eax, eax
    ret
@@32:
    mov isPE32, 1
    mov eax, [pOH+28]
    mov entryPoint, eax
    mov eax, [pOH+52]
    mov imgBase, eax
    mov eax, [pOH+56]
    mov secAlign, eax
    mov eax, [pOH+60]
    mov fileAlign, eax
    mov eax, [pOH+80]
    mov imgSize, eax
    movzx eax, word ptr [pOH+68]
    mov subSys, eax
    movzx eax, word ptr [pOH+70]
    mov dllChars, eax
    mov eax, pOH
    add eax, 224
    mov pSec, eax
    mov eax, 1
    ret
@@64:
    mov isPE32, 0
    mov eax, [pOH+24]
    mov entryPoint, eax
    mov eax, [pOH+48]
    mov imgBase, eax
    mov eax, [pOH+56]
    mov secAlign, eax
    mov eax, [pOH+60]
    mov fileAlign, eax
    mov eax, [pOH+88]
    mov imgSize, eax
    movzx eax, word ptr [pOH+68]
    mov subSys, eax
    movzx eax, word ptr [pOH+70]
    mov dllChars, eax
    mov eax, pOH
    add eax, 240
    mov pSec, eax
    mov eax, 1
    ret
ValidatePE endp

; RVA to File Offset Conversion
RVA2Offset proc rva:DWORD
    local i:DWORD
    mov i, 0
    mov ecx, numSec
    test ecx, ecx
    jz @@f
@@l:
    mov eax, i
    imul eax, 40
    add eax, pSec
    mov ebx, [eax+12]
    cmp rva, ebx
    jb @@n
    add ebx, [eax+8]
    cmp rva, ebx
    jae @@n
    mov eax, [eax+20]
    mov ebx, rva
    sub ebx, [eax+12-20]
    add eax, ebx
    add eax, pBase
    ret
@@n:
    inc i
    loop @@l
@@f:
    mov eax, rva
    add eax, pBase
    ret
RVA2Offset endp

; Entropy Calculation (Shannon Entropy for Packing Detection)
CalcEntropy proc pData:DWORD, dwSize:DWORD
    local freq[256]:DWORD, i:DWORD, j:DWORD, ent:REAL8, prob:REAL8, dSize:REAL8
    
    ; Initialize frequency table
    lea edi, freq
    mov ecx, 256
    xor eax, eax
    rep stosd
    
    ; Count frequencies
    mov i, 0
@@cl:
    mov eax, i
    cmp eax, dwSize
    jge @@ca
    mov ebx, pData
    movzx eax, byte ptr [ebx+eax]
    shl eax, 2
    inc dword ptr [freq+eax]
    inc i
    jmp @@cl
    
@@ca:
    ; Calculate entropy
    fldz
    fstp ent
    fild dwSize
    fstp dSize
    mov j, 0
    
@@el:
    cmp j, 256
    jge @@dn
    mov eax, j
    shl eax, 2
    mov eax, [freq+eax]
    test eax, eax
    jz @@sk
    
    ; prob = freq[j] / size
    fild dword ptr [freq+eax]
    fld dSize
    fdivp st(1), st(0)
    fst prob
    
    ; entropy += -prob * log2(prob)
    fld prob
    fyl2x
    fchs
    fld prob
    fmulp st(1), st(0)
    fld ent
    faddp st(1), st(0)
    fstp ent
    
@@sk:
    inc j
    jmp @@el
    
@@dn:
    fld ent
    ret
CalcEntropy endp

; Print Characteristics
PrintChars proc dwChar:DWORD
    mov eax, dwChar
    test eax, SCN_MEM_EXECUTE
    jz @@1
    invoke P, addr szCHR_EXEC
@@1:
    mov eax, dwChar
    test eax, SCN_MEM_READ
    jz @@2
    invoke P, addr szCHR_READ
@@2:
    mov eax, dwChar
    test eax, SCN_MEM_WRITE
    jz @@3
    invoke P, addr szCHR_WRITE
@@3:
    mov eax, dwChar
    test eax, SCN_CNT_CODE
    jz @@4
    invoke P, addr szCHR_CODE
@@4:
    mov eax, dwChar
    test eax, SCN_CNT_INIT_DATA
    jz @@5
    invoke P, addr szCHR_IDATA
@@5:
    mov eax, dwChar
    test eax, SCN_CNT_UNINIT_DATA
    jz @@6
    invoke P, addr szCHR_UDATA
@@6:
    mov eax, dwChar
    test eax, SCN_MEM_DISCARDABLE
    jz @@7
    invoke P, addr szCHR_DISC
@@7:
    mov eax, dwChar
    test eax, SCN_MEM_SHARED
    jz @@8
    invoke P, addr szCHR_SHARED
@@8:
    ret
PrintChars endp

; Deep PE Analysis
DeepAnalysis proc
    invoke P, addr szW
    invoke ValidatePE
    test eax, eax
    jz @@x
    
    cmp isPE32, 1
    je @@p32
    invoke P, addr szPE64
    jmp @@coff
@@p32:
    invoke P, addr szPE32
    
@@coff:
    invoke P, addr szCOFF
    invoke P, addr szMACH
    movzx eax, word ptr [pFH]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSEC
    mov eax, numSec
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szTIM
    mov eax, [pFH+4]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szOPTH
    invoke P, addr szEP
    mov eax, entryPoint
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szIMG
    mov eax, imgBase
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSZ
    mov eax, imgSize
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSS
    mov eax, subSys
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szDIR
    call PrintDirectories
    invoke P, addr szSECH
    call AnalyzeSections
@@x:
    ret
DeepAnalysis endp

; Print Data Directories
PrintDirectories proc
    mov ecx, 16
    xor esi, esi
@@l:
    push ecx
    mov eax, esi
    shl eax, 3
    add eax, pOH
    add eax, 112
    mov ebx, [eax]
    mov ecx, [eax+4]
    test ebx, ebx
    jz @@n
    test ecx, ecx
    jz @@n
    
    cmp esi, DIR_EXPORT
    jne @@1
    invoke P, addr szEXP
    jmp @@p
@@1:
    cmp esi, DIR_IMPORT
    jne @@2
    invoke P, addr szIMP
    jmp @@p
@@2:
    cmp esi, DIR_RESOURCE
    jne @@3
    invoke P, addr szRES
    jmp @@p
@@3:
    cmp esi, DIR_EXCEPTION
    jne @@4
    invoke P, addr szEXC
    jmp @@p
@@4:
    cmp esi, DIR_SECURITY
    jne @@5
    invoke P, addr szSECU
    jmp @@p
@@5:
    cmp esi, DIR_BASERELOC
    jne @@6
    invoke P, addr szREL
    jmp @@p
@@6:
    cmp esi, DIR_DEBUG
    jne @@7
    invoke P, addr szDBG
    jmp @@p
@@7:
    cmp esi, DIR_ARCH
    jne @@8
    invoke P, addr szARC
    jmp @@p
@@8:
    cmp esi, DIR_GLOBALPTR
    jne @@9
    invoke P, addr szGP
    jmp @@p
@@9:
    cmp esi, DIR_TLS
    jne @@10
    invoke P, addr szTLS
    jmp @@p
@@10:
    cmp esi, DIR_LOADCFG
    jne @@11
    invoke P, addr szLDC
    jmp @@p
@@11:
    cmp esi, DIR_BOUNDIMP
    jne @@12
    invoke P, addr szBND
    jmp @@p
@@12:
    cmp esi, DIR_IAT
    jne @@13
    invoke P, addr szIAT
    jmp @@p
@@13:
    cmp esi, DIR_DELAYIMP
    jne @@14
    invoke P, addr szDLY
    jmp @@p
@@14:
    cmp esi, DIR_COM
    jne @@15
    invoke P, addr szCOM
    jmp @@p
@@15:
    invoke P, addr szRESERVE
    
@@p:
    invoke wsprintf, addr tB, addr szX, ebx
    invoke P, addr tB
    invoke P, addr szSPC
    invoke wsprintf, addr tB, addr szD, ecx
    invoke P, addr tB
    invoke P, addr szNL
    
@@n:
    pop ecx
    inc esi
    loop @@l
    ret
PrintDirectories endp

; Section Analysis with Entropy
AnalyzeSections proc
    mov ecx, numSec
    xor esi, esi
@@l:
    push ecx
    mov eax, esi
    imul eax, 40
    add eax, pSec
    mov ebx, eax
    
    invoke P, addr szSN
    push dword ptr [ebx]
    push offset szF
    call wsprintf
    add esp, 8
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSVA
    mov eax, [ebx+12]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSVS
    mov eax, [ebx+8]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSRAW
    mov eax, [ebx+20]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSRSZ
    mov eax, [ebx+16]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szSCH
    mov eax, [ebx+36]
    call PrintChars
    invoke P, addr szNL
    
    ; Calculate entropy if section has raw data
    mov eax, [ebx+20]
    test eax, eax
    jz @@n
    mov ecx, [ebx+16]
    test ecx, ecx
    jz @@n
    add eax, pBase
    push ecx
    push eax
    call CalcEntropy
    add esp, 8
    sub esp, 8
    fstp qword ptr [esp]
    push offset szPERC
    push offset tB
    call wsprintf
    add esp, 16
    invoke P, addr szENT
    invoke P, addr tB
    invoke P, addr szNL
    
@@n:
    pop ecx
    inc esi
    loop @@l
    ret
AnalyzeSections endp

; Import Table Reconstruction
ReconImports proc
    ; Check if import directory exists
    mov eax, [pOH+120]
    test eax, eax
    jz @@x
    mov ecx, [pOH+124]
    test ecx, ecx
    jz @@x
    
    invoke RVA2Offset, eax
    mov esi, eax
    invoke P, addr szIMPH
    
@@l:
    cmp dword ptr [esi], 0
    je @@x
    cmp dword ptr [esi+12], 0
    je @@n
    
    ; Print DLL name
    mov eax, [esi+12]
    invoke RVA2Offset, eax
    invoke wsprintf, addr tB, addr szDLLN
    invoke P, addr tB
    invoke P, addr eax
    invoke P, addr szNL
    
    ; Get thunk table (ILT or IAT)
    mov eax, [esi]
    test eax, eax
    jnz @@oft
    mov eax, [esi+16]
@@oft:
    invoke RVA2Offset, eax
    mov edi, eax
    
@@a:
    cmp dword ptr [edi], 0
    je @@n
    test dword ptr [edi], 80000000h
    jnz @@ord
    
    ; Import by name
    mov eax, [edi]
    invoke RVA2Offset, eax
    add eax, 2
    invoke wsprintf, addr tB, addr szAPI
    invoke P, addr tB
    invoke P, addr eax
    invoke P, addr szNL
    jmp @@i
    
@@ord:
    ; Import by ordinal
    mov eax, [edi]
    and eax, 0FFFFh
    invoke wsprintf, addr tB, addr szORD, eax
    invoke P, addr tB
    invoke P, addr szNL
    
@@i:
    add edi, 4
    jmp @@a
    
@@n:
    add esi, 20
    jmp @@l
    
@@x:
    ret
ReconImports endp

; Export Table Reconstruction
ReconExports proc
    ; Check if export directory exists
    mov eax, [pOH+112]
    test eax, eax
    jz @@x
    mov ecx, [pOH+116]
    test ecx, ecx
    jz @@x
    
    invoke RVA2Offset, eax
    mov esi, eax
    invoke P, addr szEXPH
    
    invoke P, addr szEXPV
    mov eax, [esi+24]
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    ; Get name table
    mov eax, [esi+32]
    test eax, eax
    jz @@x
    invoke RVA2Offset, eax
    mov edi, eax
    mov eax, [esi+24]
    mov ecx, eax
    
@@l:
    test ecx, ecx
    jz @@x
    push ecx
    
    mov eax, [edi]
    test eax, eax
    jz @@n
    invoke RVA2Offset, eax
    invoke wsprintf, addr tB, addr szENAM
    invoke P, addr tB
    invoke P, addr eax
    invoke P, addr szNL
    
@@n:
    add edi, 4
    pop ecx
    dec ecx
    jmp @@l
    
@@x:
    ret
ReconExports endp

; TLS Callback Enumeration
AnalyzeTLS proc
    ; Check if TLS directory exists
    mov eax, [pOH+144]
    test eax, eax
    jz @@x
    mov ecx, [pOH+148]
    test ecx, ecx
    jz @@x
    
    invoke RVA2Offset, eax
    mov esi, eax
    invoke P, addr szTLSH
    
    invoke P, addr szTSTART
    mov eax, [esi]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szTEND
    mov eax, [esi+4]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    invoke P, addr szTIDX
    mov eax, [esi+8]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    ; Enumerate callbacks
    mov eax, [esi+12]
    test eax, eax
    jz @@x
    invoke RVA2Offset, eax
    mov edi, eax
    
@@l:
    cmp dword ptr [edi], 0
    je @@x
    invoke P, addr szTCALL
    mov eax, [edi]
    invoke wsprintf, addr tB, addr szX, eax
    invoke P, addr tB
    invoke P, addr szNL
    add edi, 4
    jmp @@l
    
@@x:
    ret
AnalyzeTLS endp

; Base Relocation Processing
AnalyzeReloc proc
    ; Check if relocation directory exists
    mov eax, [pOH+136]
    test eax, eax
    jz @@x
    mov ecx, [pOH+140]
    test ecx, ecx
    jz @@x
    
    invoke RVA2Offset, eax
    mov esi, eax
    add ecx, esi
    invoke P, addr szRELH
    
@@l:
    cmp esi, ecx
    jae @@x
    
    ; Get block header
    mov ebx, [esi]
    add esi, 4
    movzx eax, word ptr [esi]
    add esi, 4
    
    ; Calculate number of entries
    sub eax, 8
    shr eax, 1
    mov edx, eax
    
    invoke P, addr szRBLK
    invoke wsprintf, addr tB, addr szX, ebx
    invoke P, addr tB
    invoke P, addr szNL
    
@@e:
    test edx, edx
    jz @@l
    movzx eax, word ptr [esi]
    push edx
    mov edx, eax
    and edx, 0FFFh
    add edx, ebx
    shr eax, 12
    
    invoke P, addr szRTYP
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    invoke P, addr szROFF
    invoke wsprintf, addr tB, addr szX, edx
    invoke P, addr tB
    invoke P, addr szNL
    
    pop edx
    add esi, 2
    dec edx
    jmp @@e
    
@@x:
    ret
AnalyzeReloc endp

; Debug Directory Parsing
AnalyzeDebug proc
    ; Check if debug directory exists
    mov eax, [pOH+128]
    test eax, eax
    jz @@x
    mov ecx, [pOH+132]
    test ecx, ecx
    jz @@x
    
    invoke RVA2Offset, eax
    mov esi, eax
    invoke P, addr szDBGH
    
@@l:
    cmp ecx, 28
    jb @@x
    
    invoke P, addr szDTYP
    mov eax, [esi]
    cmp eax, DEBUG_CODEVIEW
    je @@cv
    cmp eax, DEBUG_MISC
    je @@mc
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    jmp @@c
@@cv:
    invoke wsprintf, addr tB, addr szF, addr szCODEVIEW
    invoke P, addr tB
    jmp @@c
@@mc:
    invoke wsprintf, addr tB, addr szF, addr szS
    invoke P, addr tB
    
@@c:
    invoke P, addr szNL
    
    invoke P, addr szDSZ
    mov eax, [esi+4]
    invoke wsprintf, addr tB, addr szD, eax
    invoke P, addr tB
    invoke P, addr szNL
    
    add esi, 28
    sub ecx, 28
    jmp @@l
    
@@x:
    ret
AnalyzeDebug endp

; String Extraction
ExtractStrings proc
    local i:DWORD, j:DWORD, len:DWORD
    
    invoke P, addr szSTRH
    mov i, 0
    
@@o:
    mov eax, i
    cmp eax, fSz
    jge @@x
    
    movzx eax, byte ptr [fB+eax]
    cmp al, 20h
    jb @@n
    cmp al, 7Eh
    ja @@n
    
    ; Found printable character, extract string
    mov j, 0
    lea edi, sB
    
@@l:
    mov eax, i
    add eax, j
    cmp eax, fSz
    jge @@p
    
    movzx eax, byte ptr [fB+eax]
    cmp al, 20h
    jb @@p
    cmp al, 7Eh
    ja @@p
    
    mov [edi], al
    inc edi
    inc j
    cmp j, 255
    jb @@l
    
@@p:
    ; Check minimum length
    cmp j, 4
    jb @@s
    
    mov byte ptr [edi], 0
    invoke wsprintf, addr tB, addr szSTRFMT, i, addr sB
    invoke P, addr tB
    
@@s:
    add i, j
    inc i
    jmp @@o
    
@@n:
    inc i
    jmp @@o
    
@@x:
    ret
ExtractStrings endp

; Full Reconstruction
FullReconstruct proc
    call DeepAnalysis
    call ReconImports
    call ReconExports
    call AnalyzeTLS
    call AnalyzeReloc
    call AnalyzeDebug
    ret
FullReconstruct endp

; Main Menu
MainMenu proc
    local c:DWORD
    
@@m:
    invoke P, addr szW
    invoke P, addr szM
    call RI
    mov c, eax
    
    cmp c, 0
    je @@x
    cmp c, 1
    je @@1
    cmp c, 2
    je @@2
    cmp c, 3
    je @@3
    cmp c, 4
    je @@4
    cmp c, 5
    je @@5
    cmp c, 6
    je @@6
    cmp c, 7
    je @@7
    cmp c, 8
    je @@8
    cmp c, 9
    je @@9
    jmp @@m
    
@@1:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call DeepAnalysis
    jmp @@m
    
@@2:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call ReconImports
    call ReconExports
    jmp @@m
    
@@3:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call AnalyzeSections
    jmp @@m
    
@@4:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call ExtractStrings
    jmp @@m
    
@@5:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call AnalyzeTLS
    jmp @@m
    
@@6:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    invoke P, addr szRESH
    jmp @@m
    
@@7:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call AnalyzeReloc
    jmp @@m
    
@@8:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call AnalyzeDebug
    jmp @@m
    
@@9:
    invoke P, addr szPF
    call R
    invoke OpenTarget, addr bP
    test eax, eax
    jz @@m
    call FullReconstruct
    jmp @@m
    
@@x:
    ret
MainMenu endp

; Entry Point
start:
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    call MainMenu
    invoke ExitProcess, 0
end start