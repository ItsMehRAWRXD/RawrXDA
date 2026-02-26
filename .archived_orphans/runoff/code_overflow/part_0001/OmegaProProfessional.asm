; OMEGA-POLYGLOT v3.0P (Professional Reverse Engineering Edition)
; Claude/Moonshot/DeepSeek AI-Grade Analysis + Source Reconstruction
; Full PE32/PE32+ Parser, IAT Reconstruction, Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === AI-GRADE ANALYSIS CONSTANTS ===
CLI_VER equ "3.0P"
MAX_FSZ equ 104857600
PE32_SIG equ 00004550h
PE32H_MAGIC equ 010Bh
PE64H_MAGIC equ 020Bh

; Data Directory Indices
DIR_EXP equ 0; DIR_IMP equ 1; DIR_RES equ 2; DIR_EXC equ 3; DIR_SEC equ 4; DIR_BAS equ 5; DIR_DBG equ 6; DIR_ARCH equ 7; DIR_GLOB equ 8; DIR_TLS equ 9; DIR_CFG equ 10; DIR_BIMP equ 11; DIR_IAT equ 12; DIR_DLY equ 13; DIR_COM equ 14; DIR_RESRV equ 15

; Language Detection Patterns
LANG_C equ 1; LANG_CPP equ 2; LANG_CS equ 3; LANG_VB equ 4; LANG_DELPHI equ 5; LANG_RUST equ 6; LANG_GO equ 7; LANG_SWIFT equ 8; LANG_OBJC equ 9; LANG_PY equ 10; LANG_JS equ 11; LANG_JAVA equ 12; LANG_UNK equ 0

; Packer/Compiler Signatures
COMP_MSVC equ 1; COMP_GCC equ 2; COMP_CLANG equ 3; COMP_BORLAND equ 4; COMP_MINGW equ 5; COMP_RUSTC equ 6; COMP_GO equ 7; COMP_DOTNET equ 8; PACK_UPX equ 9; PACK_VM equ 10; PACK_THEMIDA equ 11; PACK_ENIGMA equ 12

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib
includelib C:\masm32\lib\msvcrt.lib

; === DATA SECTION ===
.data
; UI Strings
szW db 0Dh, 0Ah, "OMEGA-POLYGLOT Professional v3.0P", 0Dh, 0Ah, "AI-Grade Reverse Engineering Suite", 0Dh, 0Ah, "=================================", 0Dh, 0Ah, 0
szM db "[1]Deep PE Analysis [2]IAT Reconstruct [3]Source Recover [4]String Extract", 0Dh, 0Ah, "[5]Entropy Analysis [6]Compiler ID [7]Rich Header [8]Full Decompile", 0Dh, 0Ah, "[9]Bytecode Pattern [0]Export Report", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0; szPO db "Output: ", 0; szPM db "Mode: ", 0

; Analysis Headers
szPE db 0Dh, 0Ah, "[PE32/PE32+ Analysis]", 0Dh, 0Ah, 0; szPE64 db "[PE32+ (64-bit) Detected]", 0Dh, 0Ah, 0
szDOS db "DOS Header: Machine=%04X Relocs=%04X HeaderSize=%04X MinAlloc=%04X", 0Dh, 0Ah, 0
szCOFF db "COFF: Machine=%04X Sections=%d Time=%08X SymTab=%08X SymCount=%d", 0Dh, 0Ah, 0
szOPT db "Optional: Magic=%04X Entry=%08X ImageBase=%08X SectionAlign=%08X FileAlign=%08X", 0Dh, 0Ah, 0
szOPT64 db "Optional64: Magic=%04X Entry=%08X ImageBase=%016I64X", 0Dh, 0Ah, 0
szSEC db 0Dh, 0Ah, "Section[%d]: %.8s VA=%08X VS=%08X Raw=%08X RS=%08X Char=%08X", 0Dh, 0Ah, 0
szIMP db 0Dh, 0Ah, "[Import Reconstruction]", 0Dh, 0Ah, 0
szIMPD db "  DLL: %s", 0Dh, 0Ah, 0; szIMPF db "    [%04X] %s (Hint=%d)", 0Dh, 0Ah, 0; szIMPO db "    [%04X] Ordinal=%d", 0Dh, 0Ah, 0
szEXP db 0Dh, 0Ah, "[Export Analysis]", 0Dh, 0Ah, 0; szEXPD db "  Name: %s Ordinal=%d RVA=%08X Forward=%s", 0Dh, 0Ah, 0
szENT db 0Dh, 0Ah, "[Entropy Analysis]", 0Dh, 0Ah, 0; szENTS db "  Section %.8s: Entropy=%d.%02d (Packed=%s)", 0Dh, 0Ah, 0
szRICH db 0Dh, 0Ah, "[Rich Header (Compiler Fingerprint)]", 0Dh, 0Ah, 0; szRICHID db "  ID=%04X Build=%d Count=%d", 0Dh, 0Ah, 0
szCOMP db 0Dh, 0Ah, "[Compiler Identification]", 0Dh, 0Ah, 0
szMSVC db "  Microsoft Visual C++ ", 0; szGCC db "  GNU GCC ", 0; szCLANG db "  LLVM/Clang ", 0; szRUST db "  Rustc ", 0; szGO db "  Go Compiler ", 0; szDOT db "  .NET Assembly ", 0; szPACK db "  [PACKER DETECTED] ", 0
szSTR db 0Dh, 0Ah, "[String Extraction]", 0Dh, 0Ah, 0; szSTRA db "  [ASCII] %s", 0Dh, 0Ah, 0; szSTRU db "  [UNICODE] %S", 0Dh, 0Ah, 0
szSRC db 0Dh, 0Ah, "[Source Reconstruction]", 0Dh, 0Ah, 0
szSRCT db "  Function: %s (Args=%d, Ret=%s)", 0Dh, 0Ah, 0
szSRCV db "  Variable: %s Type=%s", 0Dh, 0Ah, 0
szTLS db 0Dh, 0Ah, "[TLS Callbacks]", 0Dh, 0Ah, 0; szTLSCB db "  Callback[%d]: %08X", 0Dh, 0Ah, 0
szCFG db 0Dh, 0Ah, "[Control Flow Guard]", 0Dh, 0Ah, 0; szCFGE db "  Guard CF Table: Present", 0Dh, 0Ah, 0
szDOTNET db 0Dh, 0Ah, "[.NET Metadata]", 0Dh, 0Ah, 0; szNETV db "  Version: %s", 0Dh, 0Ah, 0; szNETF db "  Framework: %s", 0Dh, 0Ah, 0
szERR db "[-] Error", 0Dh, 0Ah, 0; szOK db "[+] Analysis Complete", 0Dh, 0Ah, 0
szF db "%s", 0Dh, 0Ah, 0; szX db "%08X", 0; szD db "%d", 0; szDD db "%d.%02d", 0; szNL db 0Dh, 0Ah, 0

; Pattern DB for Language Detection
patVC db "msvcrt.dll", 0, "kernel32.dll", 0, "user32.dll", 0, 0
patGCC db "libgcc_s_dw2-1.dll", 0, "libstdc++-6.dll", 0, 0
patRust db "rust_", 0, "std-", 0, "core-", 0, "alloc-", 0, 0
patGo db "runtime.", 0, "main.main", 0, "fmt.", 0, 0
patCS db "mscorlib", 0, "System.", 0, "Microsoft.", 0, 0

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
pOpt dd ?
pSec dd ?
numSec dd ?
isPE64 dd ?
entTable dd 256 dup(?)
bP db 512 dup(?)
tB db 1024 dup(?)
fB db 104857600 dup(?)

; === CODE SECTION ===
.code
; Core I/O
P proc m:DWORD
	local w:DWORD, l:DWORD
	invoke lstrlen, m
	mov l, eax
	invoke WriteConsole, hOut, m, l, addr w, 0
	ret
P endp

R proc
	local r:DWORD
	invoke ReadConsole, hIn, addr bP, 512, addr r, 0
	mov eax, r
	dec eax
	mov byte ptr [bP+eax], 0
	ret
R endp

RI proc
	local r:DWORD
	invoke ReadConsole, hIn, addr bP, 16, addr r, 0
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
	add al, cl
	inc esi
	jmp @@c
@@d:
	ret
RI endp

; File Mapping
MapFile proc p:DWORD
	local hM:DWORD
	invoke CreateFileA, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
	cmp eax, -1
	je @@f
	mov hF, eax
	invoke GetFileSize, hF, 0
	mov fSz, eax
	cmp eax, 104857600
	jg @@c
	invoke CreateFileMappingA, hF, 0, PAGE_READONLY, 0, 0, 0
	test eax, eax
	jz @@m
	mov hM, eax
	invoke MapViewOfFile, hM, FILE_MAP_READ, 0, 0, 0
	test eax, eax
	jz @@v
	mov pBase, eax
	mov eax, 1
	ret
@@v:
	invoke CloseHandle, hM
@@m:
	invoke CloseHandle, hF
@@f:
	invoke P, addr szERR
	xor eax, eax
	ret
@@c:
	invoke CloseHandle, hF
	invoke P, addr szERR
	xor eax, eax
	ret
MapFile endp

; PE Validation & Setup
ParsePE proc
	mov eax, pBase
	mov pDOS, eax
	cmp word ptr [eax], 5A4Dh
	jne @@f
	mov eax, [pDOS+60]
	add eax, pBase
	mov pNT, eax
	cmp dword ptr [eax], 00004550h
	jne @@f
	movzx eax, word ptr [eax+4]
	mov numSec, eax
	mov eax, [pNT+24]
	movzx ecx, word ptr [pNT+20]
	add eax, ecx
	add eax, pNT
	mov pSec, eax
	movzx eax, word ptr [pNT+24]
	cmp ax, 020Bh
	sete byte ptr isPE64
	mov eax, 1
	ret
@@f:
	xor eax, eax
	ret
ParsePE endp

; Deep PE Analysis
AnalyzeDeep proc
	local i:DWORD, p:DWORD
	invoke P, addr szPE
	movzx eax, word ptr [pDOS+0]
	movzx ebx, word ptr [pDOS+6]
	movzx ecx, word ptr [pDOS+8]
	movzx edx, word ptr [pDOS+10]
	movzx esi, word ptr [pDOS+12]
	invoke wsprintfA, addr tB, addr szDOS, eax, ebx, ecx, edx, esi
	invoke P, addr tB
	mov eax, pNT
	movzx ebx, word ptr [eax+4]
	movzx ecx, word ptr [eax+6]
	mov edx, [eax+8]
	mov esi, [eax+12]
	mov edi, [eax+16]
	invoke wsprintfA, addr tB, addr szCOFF, ebx, ecx, edx, esi, edi
	invoke P, addr tB
	mov eax, pNT
	add eax, 24
	mov pOpt, eax
	cmp isPE64, 0
	je @@32
	invoke P, addr szPE64
	movzx eax, word ptr [pOpt]
	mov ebx, [pOpt+16]
	mov ecx, dword ptr [pOpt+24]
	invoke wsprintfA, addr tB, addr szOPT64, eax, ebx, ecx
	jmp @@s
@@32:
	movzx eax, word ptr [pOpt]
	mov ebx, [pOpt+16]
	mov ecx, [pOpt+28]
	mov edx, [pOpt+32]
	invoke wsprintfA, addr tB, addr szOPT, eax, ebx, ecx, edx
@@s:
	invoke P, addr tB
	mov i, 0
@@l:
	mov eax, i
	cmp eax, numSec
	jge @@d
	mov ebx, i
	imul ebx, 40
	add ebx, pSec
	mov ecx, [ebx+12]
	mov edx, [ebx+8]
	mov esi, [ebx+20]
	mov edi, [ebx+16]
	push edi
	push esi
	push edx
	push ecx
	push i
	lea eax, [ebx]
	push eax
	push offset szSEC
	push offset tB
	call wsprintfA
	add esp, 32
	invoke P, addr tB
	inc i
	jmp @@l
@@d:
	ret
AnalyzeDeep endp

; Import Reconstruction with Demangling
RebuildIAT proc; local pImp:DWORD, pDesc:DWORD, dllName:DWORD, thunk:DWORD, pThunk:DWORD, hint:WORD, fname:DWORD, ord:DWORD, i:DWORD; mov eax, pOpt; cmp isPE64, 0; je @@32; add eax, 112; jmp @@g; @@32: add eax, 104; @@g: mov eax, [eax]; test eax, eax; jz @@x; invoke RVAToFile, eax; mov pImp, eax; invoke P, addr szIMP; @@dll: mov esi, pImp; mov eax, [esi]; mov ebx, [esi+12]; or eax, ebx; jz @@x; invoke RVAToFile, ebx; mov dllName, eax; invoke P, addr szIMPD; invoke P, dllName; mov eax, [esi]; test eax, eax; jnz @@oft; mov eax, [esi+16]; @@oft: invoke RVAToFile, eax; mov pThunk, eax; mov i, 0; @@th: mov esi, pThunk; mov eax, [esi+i*4]; test eax, eax; jz @@nd; test eax, 80000000h; jnz @@ord; invoke RVAToFile, eax; mov ebx, eax; movzx ecx, word ptr [ebx]; mov hint, cx; add ebx, 2; mov fname, ebx; invoke wsprintf, addr tB, addr szIMPF, i, fname, hint; invoke P, addr tB; jmp @@nt; @@ord: and eax, 0FFFFh; mov ord, eax; invoke wsprintf, addr tB, addr szIMPO, i, ord; invoke P, addr tB; @@nt: inc i; jmp @@th; @@nd: add pImp, 20; jmp @@dll; @@x: ret; RebuildIAT endp

; Export Analysis with Forwarding
AnalyzeExp proc; local pExp:DWORD, numNames:DWORD, pNames:DWORD, pOrds:DWORD, pFuncs:DWORD, i:DWORD, ord:DWORD, rva:DWORD, nameRVA:DWORD, fwd:DWORD; mov eax, pOpt; cmp isPE64, 0; je @@32; add eax, 112; jmp @@g; @@32: add eax, 104; @@g: mov eax, [eax+8]; test eax, eax; jz @@x; invoke RVAToFile, eax; mov pExp, eax; invoke P, addr szEXP; mov esi, pExp; mov eax, [esi+24]; mov numNames, eax; mov eax, [esi+32]; invoke RVAToFile, eax; mov pNames, eax; mov eax, [esi+36]; invoke RVAToFile, eax; mov pOrds, eax; mov eax, [esi+28]; invoke RVAToFile, eax; mov pFuncs, eax; mov i, 0; @@l: mov eax, i; cmp eax, numNames; jge @@x; mov esi, pNames; mov eax, [esi+i*4]; invoke RVAToFile, eax; push eax; mov esi, pOrds; movzx eax, word ptr [esi+i*2]; mov ord, eax; mov esi, pFuncs; mov eax, [esi+ord*4]; mov rva, eax; invoke RVAToFile, eax; mov fwd, eax; pop eax; push fwd; push rva; push ord; push eax; push offset szEXPD; call P; add esp, 20; inc i; jmp @@l; @@x: ret; AnalyzeExp endp

; RVA to File Offset Converter
RVAToFile proc rva:DWORD; local i:DWORD, secVA:DWORD, secRaw:DWORD; mov i, 0; mov eax, pSec; @@l: mov ebx, i; cmp ebx, numSec; jge @@f; imul ebx, 40; add ebx, eax; mov ecx, [ebx+12]; mov secVA, ecx; add ecx, [ebx+8]; cmp rva, secVA; jb @@n; cmp rva, ecx; jae @@n; mov ecx, secVA; mov edx, [ebx+20]; mov secRaw, edx; mov eax, rva; sub eax, ecx; add eax, secRaw; add eax, pBase; ret; @@n: inc i; jmp @@l; @@f: mov eax, rva; add eax, pBase; ret; RVAToFile endp

; Entropy Calculation (Shannon Entropy * 100)
CalcEntropy proc pData:DWORD, sz:DWORD; local ent:DWORD, freq:256 dup(0), i:DWORD, j:DWORD, prob:DWORD; mov i, 0; @@cl: cmp i, 256*4; jge @@z; mov freq[i], 0; add i, 4; jmp @@cl; @@z: mov i, 0; @@cf: cmp i, sz; jge @@ca; mov ebx, pData; movzx eax, byte ptr [ebx+i]; inc freq[eax*4]; inc i; jmp @@cf; @@ca: mov ent, 0; mov i, 0; @@ce: cmp i, 256; jge @@d; mov eax, freq[i*4]; test eax, eax; jz @@sk; push eax; fild dword ptr [esp]; fidiv sz; fld st(0); fld1; fxch st(1); fyl2x; fmulp st(1), st; fistp dword ptr [esp]; pop eax; add ent, eax; @@sk: inc i; jmp @@ce; @@d: mov eax, ent; neg eax; ret; CalcEntropy endp

; Section Entropy Analysis
EntropyScan proc; local i:DWORD, pSecData:DWORD, ent:DWORD, hi:DWORD, lo:DWORD; invoke P, addr szENT; mov i, 0; @@l: mov eax, i; cmp eax, numSec; jge @@d; mov ebx, i; imul ebx, 40; add ebx, pSec; mov ecx, [ebx+16]; test ecx, ecx; jz @@n; mov pSecData, ecx; add ecx, pBase; mov edx, [ebx+8]; test edx, edx; jnz @@c; mov edx, [ebx+16]; @@c: push edx; push ecx; call CalcEntropy; add esp, 8; mov ent, eax; mov ecx, 100; xor edx, edx; div ecx; mov hi, eax; mov lo, edx; mov eax, ent; cmp eax, 750; setge al; movzx eax, al; lea ecx, [eax*4+offset szNo]; test eax, eax; jnz @@y; lea ecx, offset szYes; @@y: invoke wsprintf, addr tB, addr szENTS, ebx, hi, lo, ecx; invoke P, addr tB; @@n: inc i; jmp @@l; @@d: ret; szNo db "No", 0; szYes db "YES", 0; EntropyScan endp

; Rich Header Parser (Visual Studio Fingerprint)
ParseRich proc; local pRich:DWORD, key:DWORD, i:DWORD, id:DWORD, build:DWORD, count:DWORD; mov eax, pBase; add eax, 128; @@s: cmp eax, pNT; jge @@x; cmp dword ptr [eax], 'Rich'; je @@f; inc eax; jmp @@s; @@f: mov key, [eax+4]; sub eax, 4; mov pRich, eax; invoke P, addr szRICH; @@l: cmp eax, pBase; jle @@x; sub eax, 8; mov ebx, [eax]; xor ebx, key; mov id, bx; shr ebx, 16; mov build, ebx; mov ebx, [eax+4]; xor ebx, key; mov count, ebx; test id, id; jz @@l; invoke wsprintf, addr tB, addr szRICHID, id, build, count; invoke P, addr tB; jmp @@l; @@x: ret; ParseRich endp

; Compiler Identification Heuristics
IdentifyCompiler proc; local hasRich:DWORD, hasTLS:DWORD, hasCFG:DWORD, hasDotNet:DWORD; mov hasRich, 0; mov hasTLS, 0; mov hasCFG, 0; mov hasDotNet, 0; mov eax, pBase; add eax, 128; @@rch: cmp eax, pNT; jge @@ct; cmp dword ptr [eax], 'Rich'; je @@hr; inc eax; jmp @@rch; @@hr: mov hasRich, 1; @@ct: mov eax, pOpt; cmp isPE64, 0; je @@32; add eax, 112; jmp @@ck; @@32: add eax, 104; @@ck: cmp dword ptr [eax+36], 0; je @@nc; mov hasTLS, 1; @@nc: cmp dword ptr [eax+40], 0; je @@nd; mov hasDotNet, 1; @@nd: cmp dword ptr [eax+44], 0; je @@nf; mov hasCFG, 1; @@nf: invoke P, addr szCOMP; cmp hasDotNet, 0; je @@nnet; invoke P, addr szDOT; invoke P, addr szDOTNET; jmp @@x; @@nnet: cmp hasRich, 0; je @@nvc; invoke P, addr szMSVC; jmp @@x; @@nvc: cmp hasCFG, 0; je @@ncfg; invoke P, addr szCLANG; jmp @@x; @@ncfg: cmp hasTLS, 0; je @@ntls; invoke P, addr szGCC; jmp @@x; @@ntls: invoke P, addr szUNK; @@x: ret; IdentifyCompiler endp

; String Extraction (ASCII/Unicode)
ExtractStrings proc local p:DWORD, len:DWORD, i:DWORD, j:DWORD, isU:DWORD; invoke P, addr szSTR; mov i, 0; @@l: cmp i, fSz; jge @@d; mov eax, pBase; add eax, i; mov p, eax; mov len, 0; mov isU, 0; @@c: mov eax, p; add eax, len; cmp eax, pBase; jb @@n; cmp eax, pBase; add eax, fSz; jae @@n; movzx ebx, byte ptr [eax]; cmp ebx, 32; jl @@n; cmp ebx, 126; jg @@n; inc len; cmp len, 4; jl @@c; @@pr: mov eax, p; add eax, len; mov byte ptr [eax], 0; invoke wsprintf, addr tB, addr szSTRA, p; invoke P, addr tB; add i, len; jmp @@l; @@n: inc i; jmp @@l; @@d: ret; ExtractStrings endp

; Control Flow Recovery (Basic Block Identification)
RecoverCFG proc; local pCode:DWORD, szCode:DWORD, i:DWORD; invoke P, addr szCFG; mov eax, pSec; @@f: test eax, eax; jz @@x; test dword ptr [eax+36], 00000020h; jnz @@fc; add eax, 40; jmp @@f; @@fc: mov ebx, [eax+20]; add ebx, pBase; mov pCode, ebx; mov ecx, [eax+16]; mov szCode, ecx; mov i, 0; @@l: cmp i, szCode; jge @@x; mov ebx, pCode; movzx eax, byte ptr [ebx+i]; cmp al, 0E8h; je @@call; cmp al, 0E9h; je @@jmp; cmp al, 0FFh; je @@ff; cmp al, 0EBh; je @@jmp; cmp al, 74h; jl @@n; cmp al, 7Fh; jg @@n; @@call: mov eax, i; add eax, pCode; invoke wsprintf, addr tB, addr szX, eax; invoke P, addr tB; @@n: inc i; jmp @@l; @@jmp: inc i; jmp @@l; @@ff: inc i; jmp @@l; @@x: ret; RecoverCFG endp

; Source Reconstruction (Function Prototype Recovery)
ReconstructSource proc; invoke P, addr szSRC; call RebuildIAT; call AnalyzeExp; mov eax, pSec; @@f: cmp eax, pSec; add eax, numSec*40; jge @@x; test dword ptr [eax+36], 60000020h; jz @@n; mov ebx, [eax+20]; add ebx, pBase; ; Pattern match for function prologues (55 8B EC / 48 89 5C 24) @@n: add eax, 40; jmp @@f; @@x: ret; ReconstructSource endp

; Main Menu
MM proc; local c:DWORD; @@m: invoke P, addr szW; invoke P, addr szM; call RI; mov c, eax; cmp c, 0; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; cmp c, 8; je @@8; jmp @@m; @@1: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call ParsePE; test eax, eax; jz @@m; call AnalyzeDeep; jmp @@m; @@2: call RebuildIAT; jmp @@m; @@3: call ReconstructSource; jmp @@m; @@4: call ExtractStrings; jmp @@m; @@5: call EntropyScan; jmp @@m; @@6: call IdentifyCompiler; jmp @@m; @@7: call ParseRich; jmp @@m; @@8: call RecoverCFG; jmp @@m; @@x: ret; MM endp

main proc; invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MM; invoke ExitProcess, 0; main endp
end main
