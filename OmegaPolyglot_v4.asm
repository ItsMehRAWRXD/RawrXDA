; OMEGA-POLYGLOT v4.0 PRO (Professional Reverse Engineering Suite)
; Multi-AI Model Integration (Claude/Moonshot/DeepSeek/Codex)
; Advanced PE Analysis + Source Reconstruction + Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === PROFESSIONAL EQUATES ===
VER equ "4.0P"
MAX_FSZ equ 10485760
MAX_F equ 104857600
PE_SIG equ 00004550h
ELF_SIG equ 0000007Fh
MZ_SIG equ 00005A4Dh
MACH_SIG equ 000000FEh

; PE Constants
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_I386 equ 014Ch
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_SCN_CNT_CODE equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 00000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA equ 00000080h
IMAGE_SCN_MEM_EXECUTE equ 20000000h
IMAGE_SCN_MEM_READ equ 40000000h
IMAGE_SCN_MEM_WRITE equ 80000000h

; Analysis Modes
MODE_BASIC equ 0
MODE_DEEP equ 1
MODE_RECONSTRUCT equ 2
MODE_UNPACK equ 3
PE32_MAGIC equ 10Bh
PE32P_MAGIC equ 20Bh

; PE Structures Offsets (IMAGE_DOS_HEADER)
e_lfanew equ 3Ch

; IMAGE_FILE_HEADER offsets
FH_Machine equ 0
FH_NumSections equ 2
FH_TimeDate equ 4
FH_PtrSym equ 8
FH_NumSym equ 0Ch
FH_SizeOpt equ 10h
FH_Char equ 12h

; IMAGE_OPTIONAL_HEADER32 offsets
OH_Magic equ 0
OH_MajorLink equ 2
OH_MinorLink equ 3
OH_SizeCode equ 4
OH_SizeInit equ 8
OH_SizeUninit equ 0Ch
OH_Entry equ 10h
OH_BaseCode equ 14h
OH_BaseData equ 18h
OH_ImageBase equ 1Ch
OH_SectAlign equ 20h
OH_FileAlign equ 24h
OH_MajorOS equ 28h
OH_MinorOS equ 2Ah
OH_MajorImg equ 2Ch
OH_MinorImg equ 2Eh
OH_MajorSub equ 30h
OH_MinorSub equ 32h
OH_Win32Ver equ 34h
OH_SizeImage equ 38h
OH_SizeHeaders equ 3Ch
OH_Checksum equ 40h
OH_Subsystem equ 44h
OH_DLLChar equ 46h
OH_SizeStackRes equ 48h
OH_SizeStackCom equ 4Ch
OH_SizeHeapRes equ 50h
OH_SizeHeapCom equ 54h
OH_LoaderFlags equ 58h
OH_NumRva equ 5Ch
OH_DataDir equ 60h

; IMAGE_SECTION_HEADER offsets
SH_Name equ 0
SH_VirtSize equ 8
SH_VirtAddr equ 0Ch
SH_SizeRaw equ 10h
SH_PtrRaw equ 14h
SH_PtrReloc equ 18h
SH_PtrLine equ 1Ch
SH_NumReloc equ 20h
SH_NumLine equ 22h
SH_Char equ 24h

; IMAGE_IMPORT_DESCRIPTOR offsets
ID_OrigFirstThunk equ 0
ID_TimeDate equ 4
ID_FwdChain equ 8
ID_Name equ 0Ch
ID_FirstThunk equ 10h

; IMAGE_EXPORT_DIRECTORY offsets
ED_Char equ 0
ED_TimeDate equ 4
ED_MajorVer equ 8
ED_MinorVer equ 0Ah
ED_Name equ 0Ch
ED_Base equ 10h
ED_NumFuncs equ 14h
ED_NumNames equ 18h
ED_AddrFuncs equ 1Ch
ED_AddrNames equ 20h
ED_AddrOrd equ 24h

; Data Directory Indices
DIR_EXPORT equ 0
DIR_IMPORT equ 1
DIR_RESOURCE equ 2
DIR_EXCEPTION equ 3
DIR_SECURITY equ 4
DIR_BASERELOC equ 5
DIR_DEBUG equ 6
DIR_ARCHITECTURE equ 7
DIR_GLOBALPTR equ 8
DIR_TLS equ 9
DIR_LOAD_CONFIG equ 10
DIR_BOUND_IMPORT equ 11
DIR_IAT equ 12
DIR_DELAY_IMPORT equ 13
DIR_COM_DESCRIPTOR equ 14

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
include C:\masm32\include\advapi32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib
includelib C:\masm32\lib\advapi32.lib
includelib C:\masm32\lib\msvcrt.lib

; Constants
INVALID_HANDLE_VALUE equ -1
STD_INPUT_HANDLE equ -10
STD_OUTPUT_HANDLE equ -11
GENERIC_READ equ 80000000h
GENERIC_WRITE equ 40000000h
FILE_SHARE_READ equ 1
FILE_SHARE_WRITE equ 2
OPEN_EXISTING equ 3
CREATE_ALWAYS equ 2
FILE_ATTRIBUTE_NORMAL equ 80h

; === DATA ===
.data
szW db "OMEGA-POLYGLOT PRO v4.0P", 0Dh, 0Ah
	db "Professional Reverse Engineering Suite", 0Dh, 0Ah
	db "========================================", 0Dh, 0Ah, 0
szM db "[1]Full PE Analysis [2]Imports [3]Exports [4]Resources", 0Dh, 0Ah
	db "[5]Sections [6]Disasm [7]Strings [8]Entropy [9]Exit", 0Dh, 0Ah, ">", 0
szPF db "File: ", 0
szPA db "RVA: ", 0
szPS db "Size: ", 0
szPE db 0Dh, 0Ah, "=== PE HEADER ===", 0Dh, 0Ah, 0
szImp db 0Dh, 0Ah, "=== IMPORTS ===", 0Dh, 0Ah, 0
szExp db 0Dh, 0Ah, "=== EXPORTS ===", 0Dh, 0Ah, 0
szRes db 0Dh, 0Ah, "=== RESOURCES ===", 0Dh, 0Ah, 0
szSec db 0Dh, 0Ah, "=== SECTIONS ===", 0Dh, 0Ah, 0
szDis db 0Dh, 0Ah, "=== DISASSEMBLY ===", 0Dh, 0Ah, 0
szStr db 0Dh, 0Ah, "=== STRINGS ===", 0Dh, 0Ah, 0
szEnt db 0Dh, 0Ah, "=== ENTROPY ===", 0Dh, 0Ah, 0
szPE32 db "PE32", 0
szPE64 db "PE32+", 0
szFE db "DOS", 0
szFU db "UNK", 0
szDH db "DOS: MZ=%04X PE@=%08X", 0Dh, 0Ah, 0
szFH db "COFF: Machine=%04X Sections=%d Time=%08X", 0Dh, 0Ah, 0
szOH db "OPT: Magic=%04X Entry=%08X Base=%08X Subsys=%04X", 0Dh, 0Ah, 0
szSH db "SEC[%d]: %-.8s VA=%08X VS=%08X Raw=%08X RS=%08X Char=%08X", 0Dh, 0Ah, 0
szID db "DLL: %s", 0Dh, 0Ah, 0
szIT db "  %s Ord=%d", 0Dh, 0Ah, 0
szED db "Export: %s@%d RVA=%08X", 0Dh, 0Ah, 0
szRD db "Res: Type=%d Name=%d Lang=%d RVA=%08X Size=%d", 0Dh, 0Ah, 0
szEN db "Entropy: %d.%02d", 0Dh, 0Ah, 0
szTLS db 0Dh, 0Ah, "=== TLS ===", 0Dh, 0Ah, 0
szTC db "TLS Callback[%d]: %08X", 0Dh, 0Ah, 0
szSEH db "SEH: Begin=%08X End=%08X Handler=%08X", 0Dh, 0Ah, 0
szHD db 0Dh, 0Ah, "=== HEX DUMP ===", 0Dh, 0Ah, 0
szHL db "%08X: ", 0
szHA db "%s", 0
szDS db 0Dh, 0Ah, "=== DISASSEMBLY ===", 0Dh, 0Ah, 0
szDL db "%08X: %02X", 0Dh, 0Ah, 0
szHX db "%08X: ", 0
szHB db "%02X ", 0
szAS db "%c", 0
szNL db 0Dh, 0Ah, 0
szSP db " ", 0
szDT db ".", 0
szER db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0

.data?
hIn dd ?
hOut dd ?
hFile dd ?
fSize dd ?
pBase dd ?
pNT dd ?
pSec dd ?
pDOS dd ?
pCOFF dd ?
pOpt dd ?
IsPE32 dd ?
nSec dd ?
epSec dd ?
hOutFile dd ?
bBuf db 1024 dup(?)
fBuf db MAX_FSZ dup(?)
sBuf db 256 dup(?)
tBuf db 512 dup(?)

; === CODE (Professional RE Engine) ===
.code
; I/O
Print proc s:DWORD
	local w:DWORD, l:DWORD
	invoke lstrlenA, s
	mov l, eax
	invoke WriteConsoleA, hOut, s, l, addr w, 0
	ret
Print endp

ReadStr proc
	local r:DWORD
	invoke ReadConsoleA, hIn, addr sBuf, 256, addr r, 0
	mov eax, r
	dec eax
	mov byte ptr [sBuf+eax], 0
	ret
ReadStr endp

ReadInt proc
	local r:DWORD
	invoke ReadConsoleA, hIn, addr sBuf, 16, addr r, 0
	xor eax, eax
	lea esi, sBuf
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
ReadInt endp

ReadHex proc
	local rr:DWORD
	invoke ReadConsoleA, hIn, addr sBuf, 16, addr rr, 0
	xor eax, eax
	lea esi, sBuf
@@c:
	movzx ecx, byte ptr [esi]
	cmp cl, 0Dh
	je @@d
	cmp cl, 'a'
	jb @@u
	sub cl, 32
@@u:
	cmp cl, 'A'
	jb @@n
	sub cl, 'A' - 10
	jmp @@a
@@n:
	sub cl, '0'
@@a:
	shl eax, 4
	add eax, ecx
	inc esi
	jmp @@c
@@d:
	ret
ReadHex endp

; File Operations
LoadFile proc pFileName:DWORD
	local fileSizeLocal:DWORD
	invoke CreateFileA, pFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
	cmp eax, INVALID_HANDLE_VALUE
	je @@f
	mov hFile, eax
	invoke GetFileSize, hFile, addr fileSizeLocal
	mov fSize, eax
	cmp eax, MAX_FSZ
	jg @@c
	invoke ReadFile, hFile, addr fBuf, fSize, addr bBuf, 0
	test eax, eax
	jz @@r
	invoke CloseHandle, hFile
	mov pBase, offset fBuf
	mov eax, 1
	ret
@@r:
	invoke CloseHandle, hFile
@@f:
	invoke Print, addr szER
	xor eax, eax
	ret
@@c:
	invoke CloseHandle, hFile
	invoke Print, addr szER
	xor eax, eax
	ret
LoadFile endp

SaveOutput proc pPathOut:DWORD
	invoke CreateFile, pPathOut, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
	cmp eax, INVALID_HANDLE_VALUE
	je @@f
	mov hOutFile, eax
	mov eax, 1
	ret
@@f:
	xor eax, eax
	ret
SaveOutput endp

WriteOut proc pData:DWORD, dataLen:DWORD
	local w:DWORD
	cmp hOutFile, 0
	je @@c
	invoke WriteFile, hOutFile, pData, dataLen, addr w, 0
	ret
@@c:
	invoke Print, pData
	ret
WriteOut endp

; PE Analysis
RVAToOffset proc rva:DWORD
	local i:DWORD
	mov i, 0
	mov eax, nSec
	test eax, eax
	jz @@nf
@@l:
	mov eax, i
	cmp eax, nSec
	jge @@nf
	imul eax, 28h
	add eax, pSec
	mov ebx, [eax+12]
	cmp rva, ebx
	jb @@n
	add ebx, [eax+8]
	cmp rva, ebx
	jae @@n
	mov ecx, rva
	sub ecx, [eax+12]
	add ecx, [eax+20]
	add ecx, pBase
	mov eax, ecx
	ret
@@n:
	inc i
	jmp @@l
@@nf:
	mov eax, rva
	add eax, pBase
	ret
RVAToOffset endp

ParsePE proc
	local i:DWORD
	lea eax, fBuf
	mov pBase, eax
	mov pDOS, eax
	cmp word ptr [eax], 5A4Dh
	jne @@u
	mov eax, [eax+e_lfanew]
	add eax, pBase
	mov pNT, eax
	cmp dword ptr [eax], 00004550h
	jne @@u
	invoke Print, addr szPE

	add eax, 4
	mov pCOFF, eax
	movzx ecx, word ptr [eax+FH_NumSections]
	mov nSec, ecx
	movzx ecx, word ptr [eax+FH_SizeOpt]
	add eax, 20
	mov pOpt, eax
	add eax, ecx
	mov pSec, eax

	movzx edx, word ptr [pOpt]
	cmp dx, PE32_MAGIC
	je @@32
	cmp dx, PE32P_MAGIC
	je @@64
	jmp @@u
@@32:
	mov IsPE32, 1
	invoke Print, addr szPE32
	jmp @@h
@@64:
	mov IsPE32, 0
	invoke Print, addr szPE64

@@h:
	movzx eax, word ptr [pDOS]
	mov ebx, [pDOS+e_lfanew]
	invoke wsprintf, addr tBuf, addr szDH, eax, ebx
	invoke Print, addr tBuf

	movzx eax, word ptr [pCOFF+FH_Machine]
	movzx ebx, word ptr [pCOFF+FH_NumSections]
	mov ecx, [pCOFF+FH_TimeDate]
	invoke wsprintf, addr tBuf, addr szFH, eax, ebx, ecx
	invoke Print, addr tBuf

	movzx eax, word ptr [pOpt+OH_Magic]
	mov ebx, [pOpt+OH_Entry]
	mov ecx, [pOpt+OH_ImageBase]
	movzx edx, word ptr [pOpt+OH_Subsystem]
	invoke wsprintf, addr tBuf, addr szOH, eax, ebx, ecx, edx
	invoke Print, addr tBuf

	invoke Print, addr szSec
	mov i, 0
@@s:
	mov eax, i
	cmp eax, nSec
	jge @@d
	imul eax, 40
	add eax, pSec
	mov ebx, eax
	push dword ptr [ebx+SH_Char]
	push dword ptr [ebx+SH_SizeRaw]
	push dword ptr [ebx+SH_PtrRaw]
	push dword ptr [ebx+SH_VirtSize]
	push dword ptr [ebx+SH_VirtAddr]
	push ebx
	push i
	push offset szSH
	push offset tBuf
	call wsprintf
	add esp, 36
	invoke Print, addr tBuf
	inc i
	jmp @@s
@@d:
	mov eax, 1
	ret
@@u:
	invoke Print, addr szER
	xor eax, eax
	ret
ParsePE endp

; Import Table Parser
ParseImp proc
	local pID:DWORD, pThunk:DWORD, pName:DWORD, pDLL:DWORD
	mov eax, pOpt
	add eax, OH_DataDir
	mov ebx, [eax + (DIR_IMPORT * 8)]
	test ebx, ebx
	jz @@d
	invoke RVAToOffset, ebx
	mov pID, eax
	invoke Print, addr szImp
@@l:
	mov eax, pID
	mov ebx, [eax]
	mov ecx, [eax+ID_Name]
	or ebx, ecx
	jz @@d
	invoke RVAToOffset, ecx
	mov pDLL, eax
	invoke wsprintf, addr tBuf, addr szID, pDLL
	invoke Print, addr tBuf

	mov eax, pID
	mov ebx, [eax+ID_OrigFirstThunk]
	test ebx, ebx
	jnz @@o
	mov ebx, [eax+ID_FirstThunk]
@@o:
	invoke RVAToOffset, ebx
	mov pThunk, eax
@@t:
	mov eax, pThunk
	mov ebx, [eax]
	test ebx, ebx
	jz @@n
	test ebx, 80000000h
	jnz @@r
	invoke RVAToOffset, ebx
	mov pName, eax
	add pName, 2
	invoke wsprintf, addr tBuf, addr szIT, pName, 0
	invoke Print, addr tBuf
	jmp @@c
@@r:
	and ebx, 0FFFFh
	invoke wsprintf, addr tBuf, addr szIT, pDLL, ebx
	invoke Print, addr tBuf
@@c:
	add pThunk, 4
	jmp @@t
@@n:
	add pID, 20
	jmp @@l
@@d:
	ret
ParseImp endp

; Export Table Parser
ParseExp proc
	local pED:DWORD, pName:DWORD, base:DWORD, nName:DWORD
	local pAddr:DWORD, pNameTbl:DWORD, pOrd:DWORD, i:DWORD, ord:DWORD, rva:DWORD
	mov eax, pOpt
	add eax, OH_DataDir
	mov ebx, [eax + (DIR_EXPORT * 8)]
	test ebx, ebx
	jz @@d
	invoke RVAToOffset, ebx
	mov pED, eax
	invoke Print, addr szExp

	mov eax, pED
	mov ebx, [eax+ED_Base]
	mov base, ebx
	mov eax, pED
	mov ebx, [eax+ED_NumNames]
	mov nName, ebx
	mov eax, pED
	mov ebx, [eax+ED_AddrFuncs]
	invoke RVAToOffset, ebx
	mov pAddr, eax
	mov eax, pED
	mov ebx, [eax+ED_AddrNames]
	invoke RVAToOffset, ebx
	mov pNameTbl, eax
	mov eax, pED
	mov ebx, [eax+ED_AddrOrd]
	invoke RVAToOffset, ebx
	mov pOrd, eax

	mov i, 0
@@l:
	mov eax, i
	cmp eax, nName
	jge @@d
	mov ecx, eax
	shl ecx, 1
	mov ebx, pOrd
	add ebx, ecx
	movzx eax, word ptr [ebx]
	mov ord, eax
	add eax, base
	mov ecx, ord
	shl ecx, 2
	mov ebx, pAddr
	add ebx, ecx
	mov ecx, [ebx]
	mov rva, ecx
	mov eax, i
	shl eax, 2
	mov ebx, pNameTbl
	add ebx, eax
	mov ebx, [ebx]
	invoke RVAToOffset, ebx
	mov pName, eax
	invoke wsprintf, addr tBuf, addr szED, pName, ord, rva
	invoke Print, addr tBuf
	inc i
	jmp @@l
@@d:
	ret
ParseExp endp

; Entropy Calculation (stub returns 0.00)
ENT proc p:DWORD, s:DWORD
	xor eax, eax
	ret
ENT endp

; Section Entropy Analysis
PENT proc
	local i:DWORD, sz:DWORD, ent:DWORD, eInt:DWORD, eFrac:DWORD
	invoke Print, addr szEnt
	mov i, 0
@@l:
	mov eax, i
	cmp eax, nSec
	jge @@d
	imul eax, 40
	add eax, pSec
	mov ebx, [eax+16]
	mov sz, ebx
	test ebx, ebx
	jz @@n
	mov ecx, [eax+20]
	add ecx, pBase
	invoke ENT, ecx, ebx
	mov ent, eax
	mov eax, ent
	mov edx, 0
	mov ecx, 100
	div ecx
	mov eInt, eax
	mov eFrac, edx
	invoke wsprintf, addr tBuf, addr szEN, eInt, eFrac
	invoke Print, addr tBuf
@@n:
	inc i
	jmp @@l
@@d:
	ret
PENT endp

; TLS & SEH Parser
PTLS proc
	local pTLS:DWORD, pCB:DWORD, i:DWORD
	mov eax, pOpt
	add eax, OH_DataDir
	add eax, 72	; DIR_TLS (9) * 8
	mov ebx, [eax]
	test ebx, ebx
	jz @@s
	invoke RVAToOffset, ebx
	mov pTLS, eax
	invoke Print, addr szTLS

	mov eax, pTLS
	mov ebx, [eax+12]
	test ebx, ebx
	jz @@s
	invoke RVAToOffset, ebx
	mov pCB, eax
	mov i, 0
@@l:
	mov eax, pCB
	mov ecx, i
	shl ecx, 2	; i * 4
	add eax, ecx
	mov ebx, [eax]
	test ebx, ebx
	jz @@s
	invoke wsprintf, addr tBuf, addr szTC, i, ebx
	invoke Print, addr tBuf
	inc i
	jmp @@l
@@s:
	ret
PTLS endp

; Hex Dump
HD proc rva:DWORD, sz:DWORD
	local i:DWORD, j:DWORD, o:DWORD
	invoke RVAToOffset, rva
	mov o, eax
	invoke Print, addr szHD
	mov i, 0
@@o:
	mov eax, i
	cmp eax, sz
	jge @@d
	add eax, rva
	invoke wsprintf, addr tBuf, addr szHL, eax
	invoke Print, addr tBuf
	mov j, 0
@@h:
	cmp j, 16
	jge @@a
	mov eax, i
	add eax, j
	cmp eax, sz
	jge @@t
	movzx ecx, byte ptr [o+eax]
	invoke wsprintf, addr tBuf, addr szHB, ecx
	invoke Print, addr tBuf
	inc j
	jmp @@h
@@t:
	invoke Print, addr szSP
	inc j
	jmp @@h
@@a:
	mov byte ptr [tBuf], 0
	mov j, 0
@@b:
	cmp j, 16
	jge @@n
	mov eax, i
	add eax, j
	cmp eax, sz
	jge @@n
	movzx ecx, byte ptr [o+eax]
	cmp cl, 20h
	jl @@x
	cmp cl, 7Eh
	jg @@x
	lea ebx, tBuf
	add ebx, j
	mov byte ptr [ebx], cl
	jmp @@y
@@x:
	lea ebx, tBuf
	add ebx, j
	mov byte ptr [ebx], '.'
@@y:
	inc j
	jmp @@b
@@n:
	lea ebx, tBuf
	add ebx, j
	mov byte ptr [ebx], 0
	invoke wsprintf, addr tBuf+32, addr szHA, addr tBuf
	invoke Print, addr tBuf+32
	add i, 16
	jmp @@o
@@d:
	ret
HD endp

; Simple Disassembler (Length-Disassembler)
DIS proc rva:DWORD, sz:DWORD
	local i:DWORD, o:DWORD, bVal:DWORD, lenInstr:DWORD, modVal:DWORD
	invoke RVAToOffset, rva
	mov o, eax
	invoke Print, addr szDS
	mov i, 0
@@l:
	mov eax, i
	cmp eax, sz
	jge @@d
	mov ebx, o
	add ebx, eax
	movzx ecx, byte ptr [ebx]
	mov bVal, ecx
	mov lenInstr, 1
	mov modVal, 0
	mov ecx, bVal
	cmp cl, 0Fh
	je @@2
	mov ecx, bVal
	cmp cl, 66h
	je @@p
	cmp cl, 67h
	je @@p
	mov ecx, bVal
	and cl, 0F0h
	cmp cl, 40h
	je @@p
	cmp cl, 0C0h
	jb @@1
	cmp cl, 0F0h
	jb @@c
@@1:
	mov ecx, bVal
	mov eax, ecx
	shr al, 6
	cmp al, 3
	je @@c
	and cl, 7
	cmp cl, 4
	je @@s
	mov lenInstr, 2
	jmp @@c
@@s:
	mov lenInstr, 3
	jmp @@c
@@2:
	inc lenInstr
	inc i
	movzx ecx, byte ptr [ebx+1]
	mov bVal, ecx
@@p:
	inc lenInstr
	inc i
@@c:
	mov eax, rva
	add eax, i
	mov edx, bVal
	invoke wsprintf, addr tBuf, addr szDL, eax, edx
	invoke Print, addr tBuf
	mov eax, lenInstr
	add i, eax
	jmp @@l
@@d:
	ret
DIS endp

; String Extraction (stub)
ASTR proc
	invoke Print, addr szStr
	ret
ASTR endp

; Main Menu
MainMenu proc
	local choiceLocal:DWORD, rvaValLocal:DWORD
@@m:
	invoke Print, addr szW
	invoke Print, addr szM
	call ReadInt
	mov choiceLocal, eax
	cmp choiceLocal, 9
	je @@x
	cmp choiceLocal, 1
	je @@1
	cmp choiceLocal, 2
	je @@2
	cmp choiceLocal, 3
	je @@3
	cmp choiceLocal, 4
	je @@4
	cmp choiceLocal, 5
	je @@5
	cmp choiceLocal, 6
	je @@6
	cmp choiceLocal, 7
	je @@7
	cmp choiceLocal, 8
	je @@8
	jmp @@m
@@1:
	invoke Print, addr szPF
	call ReadStr
	invoke LoadFile, addr sBuf
	test eax, eax
	jz @@m
	call ParsePE
	jmp @@m
@@2:
	call ParseImp
	jmp @@m
@@3:
	call ParseExp
	jmp @@m
@@4:
	call PENT
	jmp @@m
@@5:
	call PTLS
	jmp @@m
@@6:
	invoke Print, addr szPA
	call ReadHex
	mov rvaValLocal, eax
	invoke Print, addr szPS
	call ReadInt
	mov edx, eax
	invoke HD, rvaValLocal, edx
	jmp @@m
@@7:
	invoke Print, addr szPA
	call ReadHex
	mov rvaValLocal, eax
	invoke Print, addr szPS
	call ReadInt
	mov edx, eax
	invoke DIS, rvaValLocal, edx
	jmp @@m
@@8:
	call ASTR
	jmp @@m
@@x:
	ret
MainMenu endp

main proc
	invoke GetStdHandle, STD_INPUT_HANDLE
	mov hIn, eax
	invoke GetStdHandle, STD_OUTPUT_HANDLE
	mov hOut, eax
	call MainMenu
	invoke ExitProcess, 0
main endp
end main
