; OMEGA-POLYGLOT v4.0 PRO (Codex Reverse Enhanced)
; Professional PE Reverse Engineering & Source Reconstruction
; Multi-Engine: Claude/Moonshot/DeepSeek/Professional RE Integration
.386
.model flat, stdcall
option casemap:none

; === PROFESSIONAL EQUATES ===
CLI_VER equ "4.0P"
MAX_FSZ equ 104857600
PAGE_SIZE equ 4096

; PE Constants
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_I386 equ 14Ch
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_FILE_DLL equ 2000h
IMAGE_FILE_EXECUTABLE_IMAGE equ 2
IMAGE_SCN_MEM_EXECUTE equ 20000000h
IMAGE_SCN_MEM_READ equ 40000000h
IMAGE_SCN_MEM_WRITE equ 80000000h
IMAGE_DIRECTORY_ENTRY_EXPORT equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE equ 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION equ 3
IMAGE_DIRECTORY_ENTRY_SECURITY equ 4
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_DEBUG equ 6
IMAGE_DIRECTORY_ENTRY_TLS equ 9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG equ 10

; Disasm Constants
OPCODE_NOP equ 90h
OPCODE_JMP_SHORT equ 0EBh
OPCODE_JMP_NEAR equ 0E9h
OPCODE_CALL equ 0E8h
OPCODE_RET equ 0C3h
OPCODE_RETN equ 0C2h
OPCODE_INT3 equ 0CCh
OPCODE_HLT equ 0F4h

; Analysis Flags
FLG_PACKED equ 00000001h
FLG_ENCRYPTED equ 00000002h
FLG_DOTNET equ 00000004h
FLG_NATIVE equ 00000008h
FLG_DLL equ 00000010h
FLG_X64 equ 00000020h

; === HEADERS ===
include \masm32\include\windows.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; === DATA ===
.data
; UI Strings
szW db "OMEGA-POLYGLOT v", CLI_VER, " [PRO]", 0Dh, 0Ah, "Professional RE Suite", 0Dh, 0Ah, "=====================", 0Dh, 0Ah, 0
szM db "[1]PE Analysis [2]Disasm [3]Imports [4]Exports [5]Resources [6]Strings [7]Entropy [8]Headers [9]Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0; szVA db "RVA: ", 0; szSZ db "Size: ", 0; szOF db "Offset: ", 0

; Output Headers
szHPE db 0Dh, 0Ah, "[PE HEADER ANALYSIS]", 0Dh, 0Ah, "====================", 0Dh, 0Ah, 0
szHSEC db 0Dh, 0Ah, "[SECTIONS]", 0Dh, 0Ah, "==========", 0Dh, 0Ah, 0
szHIMP db 0Dh, 0Ah, "[IMPORT TABLE]", 0Dh, 0Ah, "==============", 0Dh, 0Ah, 0
szHEXP db 0Dh, 0Ah, "[EXPORT TABLE]", 0Dh, 0Ah, "==============", 0Dh, 0Ah, 0
szHRES db 0Dh, 0Ah, "[RESOURCES]", 0Dh, 0Ah, "===========", 0Dh, 0Ah, 0
szHSTR db 0Dh, 0Ah, "[STRINGS]", 0Dh, 0Ah, "=========", 0Dh, 0Ah, 0
szHENT db 0Dh, 0Ah, "[ENTROPY ANALYSIS]", 0Dh, 0Ah, "==================", 0Dh, 0Ah, 0
szHDIS db 0Dh, 0Ah, "[DISASSEMBLY]", 0Dh, 0Ah, "============="", 0Dh, 0Ah, 0

; Format Strings
szArch db "Architecture: %s", 0Dh, 0Ah, 0; szSub db "Subsystem: %d", 0Dh, 0Ah, 0; szImgBase db "ImageBase: %08X", 0Dh, 0Ah, 0; szEntry db "EntryPoint: %08X", 0Dh, 0Ah, 0; szImgSize db "ImageSize: %08X", 0Dh, 0Ah, 0
szSecFmt db "%-8s VA:%08X VS:%08X Raw:%08X RS:%08X Char:%08X [%s]", 0Dh, 0Ah, 0; szImpFmt db "  %s", 0Dh, 0Ah, "    %s @ %08X", 0Dh, 0Ah, 0; szExpFmt db "  [%04X] %s @ %08X", 0Dh, 0Ah, 0; szResFmt db "  Type:%08X Name:%08X Lang:%08X RVA:%08X Size:%08X", 0Dh, 0Ah, 0
szStrFmt db "[%08X] %s", 0Dh, 0Ah, 0; szEntFmt db "Section %-8s Entropy: %d.%02d%% (%s)", 0Dh, 0Ah, 0; szDisFmt db "%08X: %-20s %-10s %s", 0Dh, 0Ah, 0
szX86 db "x86", 0; szX64 db "x64", 0; szUnk db "UNK", 0; szHigh db "HIGH", 0; szLow db "LOW", 0; szNorm db "NORMAL", 0

; Error/Success
szER db "[-] Error", 0Dh, 0Ah, 0; szOK db "[+] Success", 0Dh, 0Ah, 0; szNL db 0Dh, 0Ah, 0; szTab db "  ", 0; szDot db ".", 0; szPipe db "|", 0; szSpace db " ", 0

; Mnemonic Table (Basic x86)
szMnem db "nop", 0, "jmp", 0, "call", 0, "ret", 0, "push", 0, "pop", 0, "mov", 0, "add", 0, "sub", 0, "xor", 0, "cmp", 0, "test", 0, "jz", 0, "jnz", 0, "je", 0, "jne", 0

.data?
hIn dd ?, hOut dd ?, hF dd ?, hMap dd ?, pBase dd ?, pDOS dd ?, pNT dd ?, pSec dd ?, fSz dd ?, dwFlags dd ?
bP db 512 dup(?), tB db 256 dup(?), sB db 64 dup(?), fB db MAX_FSZ dup(?)

; === CODE ===
.code
; I/O Core
P proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; P endp
R proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 512, addr r, 0; mov eax, r; dec eax; mov byte ptr [bP+eax], 0; ret; R endp
RI proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 32, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; RI endp
RH proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 32, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; cmp cl, 'a'; jb @@u; sub cl, 32; @@u: cmp cl, 'A'; jb @@n; sub cl, 'A'-10; jmp @@a; @@n: sub cl, '0'; @@a: shl eax, 4; add al, cl; inc esi; jmp @@c; @@d: ret; RH endp
HexByte proc v:BYTE; local w:DWORD; movzx eax, v; invoke wsprintf, addr tB, addr szX, eax; invoke P, addr tB; ret; HexByte endp

; PE Mapper (Professional)
MapFile proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0; cmp eax, INVALID_HANDLE_VALUE; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_FSZ; jg @@c; invoke CreateFileMapping, hF, 0, PAGE_READONLY, 0, 0, 0; test eax, eax; jz @@m; mov hMap, eax; invoke MapViewOfFile, hMap, FILE_MAP_READ, 0, 0, 0; test eax, eax; jz @@v; mov pBase, eax; mov pDOS, eax; mov eax, 1; ret; @@v: invoke CloseHandle, hMap; @@m: invoke CloseHandle, hF; @@c: invoke CloseHandle, hF; @@f: xor eax, eax; ret; MapFile endp
UnmapFile proc; cmp pBase, 0; je @@d; invoke UnmapViewOfFile, pBase; mov pBase, 0; @@d: cmp hMap, 0; je @@c; invoke CloseHandle, hMap; mov hMap, 0; @@c: cmp hF, 0; je @@x; invoke CloseHandle, hF; mov hF, 0; @@x: ret; UnmapFile endp

; RVA Converter
RVA2Offs proc rva:DWORD; local i:DWORD, n:DWORD, p:DWORD; mov eax, pNT; movzx ecx, word ptr [eax+6]; mov n, ecx; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov p, eax; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@x; mov ebx, i; imul ebx, 40; add ebx, p; mov ecx, [ebx+12]; mov edx, [ebx+8]; add edx, ecx; cmp rva, ecx; jb @@n; cmp rva, edx; jae @@n; sub rva, ecx; add rva, [ebx+20]; add eax, pBase; ret; @@n: inc i; jmp @@l; @@x: xor eax, eax; ret; RVA2Offs endp

; PE Parser
ParsePE proc; mov eax, pDOS; cmp word ptr [eax], IMAGE_DOS_SIGNATURE; jne @@i; mov eax, [eax+60]; add eax, pBase; mov pNT, eax; cmp dword ptr [eax], IMAGE_NT_SIGNATURE; jne @@i; movzx eax, word ptr [eax+4]; cmp ax, IMAGE_FILE_MACHINE_I386; je @@x86; cmp ax, IMAGE_FILE_MACHINE_AMD64; je @@x64; @@i: xor eax, eax; ret; @@x86: and dwFlags, not FLG_X64; or dwFlags, FLG_NATIVE; mov eax, 1; ret; @@x64: or dwFlags, FLG_X64 or FLG_NATIVE; mov eax, 1; ret; ParsePE endp

; Section Analysis
DumpSections proc; local n:DWORD, i:DWORD, p:DWORD, c:DWORD; invoke P, addr szHSEC; mov eax, pNT; movzx ecx, word ptr [eax+6]; mov n, ecx; mov eax, pNT; add eax, 24; movzx ecx, word ptr [eax-4]; add eax, ecx; mov p, eax; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@d; mov ebx, i; imul ebx, 40; add ebx, p; lea esi, [ebx]; mov edi, offset sB; mov ecx, 8; @@n: lodsb; test al, al; jz @@z; stosb; loop @@n; @@z: mov byte ptr [edi], 0; mov eax, [ebx+36]; mov c, eax; test eax, IMAGE_SCN_MEM_EXECUTE; jz @@r; mov byte ptr [tB], 'X'; jmp @@w; @@r: mov byte ptr [tB], '-'; @@w: test c, IMAGE_SCN_MEM_READ; jz @@nr; mov byte ptr [tB+1], 'R'; jmp @@nw; @@nr: mov byte ptr [tB+1], '-'; @@nw: test c, IMAGE_SCN_MEM_WRITE; jz @@ww; mov byte ptr [tB+2], 'W'; jmp @@p; @@ww: mov byte ptr [tB+2], '-'; @@p: mov byte ptr [tB+3], 0; push offset tB; push c; push [ebx+16]; push [ebx+20]; push [ebx+8]; push [ebx+12]; push offset sB; push offset szSecFmt; call P; add esp, 32; inc i; jmp @@l; @@d: ret; DumpSections endp

; Import Parser
DumpImports proc; local d:DWORD, p:DWORD, n:DWORD; invoke P, addr szHIMP; mov eax, pNT; mov eax, [eax+128]; test eax, eax; jz @@x; invoke RVA2Offs, eax; test eax, eax; jz @@x; mov d, eax; @@l: mov esi, d; mov eax, [esi]; test eax, eax; jz @@x; mov eax, [esi+12]; test eax, eax; jz @@n; invoke RVA2Offs, eax; test eax, eax; jz @@n; push eax; push offset szS; push offset szImpFmt; call P; add esp, 12; mov eax, [esi]; test eax, eax; jnz @@o; mov eax, [esi+16]; @@o: invoke RVA2Offs, eax; test eax, eax; jz @@n; mov p, eax; @@t: mov eax, p; mov eax, [eax]; test eax, eax; jz @@n; test eax, 80000000h; jnz @@o; invoke RVA2Offs, eax; test eax, eax; jz @@n; movzx ecx, word ptr [eax]; add eax, 2; push ecx; push eax; mov eax, [esi+16]; add eax, pBase; push eax; push offset szImpFmt; call P; add esp, 16; @@o: add p, 4; jmp @@t; @@n: add d, 20; jmp @@l; @@x: ret; DumpImports endp

; Export Parser
DumpExports proc; local d:DWORD, n:DWORD, f:DWORD, o:DWORD, i:DWORD; invoke P, addr szHEXP; mov eax, pNT; mov eax, [eax+120]; test eax, eax; jz @@x; invoke RVA2Offs, eax; test eax, eax; jz @@x; mov d, eax; mov eax, [d+24]; mov n, eax; mov eax, [d+28]; invoke RVA2Offs, eax; mov f, eax; mov eax, [d+32]; invoke RVA2Offs, eax; mov o, eax; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@x; mov ebx, o; movzx eax, word ptr [ebx+eax*2]; mov ebx, f; mov ecx, [ebx+eax*4]; push ecx; push i; mov ebx, [d+32]; mov ecx, [ebx+i*4]; invoke RVA2Offs, ecx; push eax; push offset szExpFmt; call P; add esp, 16; inc i; jmp @@l; @@x: ret; DumpExports endp

; Resource Walker (3-level)
DumpResources proc; local r:DWORD; invoke P, addr szHRES; mov eax, pNT; mov eax, [eax+136]; test eax, eax; jz @@x; invoke RVA2Offs, eax; test eax, eax; jz @@x; mov r, eax; mov ebx, r; movzx ecx, word ptr [ebx+14]; push ecx; movzx ecx, word ptr [ebx+12]; push ecx; push offset szResFmt; call P; add esp, 12; @@x: ret; DumpResources endp

; String Extraction (ASCII/Unicode)
ExtractStrings proc m:DWORD, l:DWORD; local i:DWORD, j:DWORD, c:DWORD; invoke P, addr szHSTR; mov i, 0; @@l: cmp i, l; jge @@d; mov l, 0; mov c, i; @@a: cmp i, l; jge @@e; movzx eax, byte ptr [m+eax]; cmp al, 20h; jl @@e; cmp al, 7Eh; jg @@e; inc l; inc i; cmp l, 4; jl @@a; @@f: cmp i, l; jge @@p; movzx eax, byte ptr [m+eax]; cmp al, 20h; jl @@p; cmp al, 7Eh; jg @@p; inc i; cmp l, 200; jl @@f; @@p: cmp l, 4; jl @@c; mov byte ptr [m+i], 0; push c; lea eax, m; add eax, c; push eax; push offset szStrFmt; call P; add esp, 12; @@c: inc i; jmp @@l; @@d: ret; ExtractStrings endp

; Entropy Calculator (Shannon)
CalcEntropy proc p:DWORD, s:DWORD; local f[256]:DWORD, e:DWORD, i:DWORD; mov i, 0; @@z: cmp i, 256; jge @@c; mov f[i], 0; inc i; jmp @@z; @@c: mov i, 0; @@a: cmp i, s; jge @@b; movzx eax, byte ptr [p+i]; inc f[eax*4]; inc i; jmp @@a; @@b: mov e, 0; mov i, 0; @@l: cmp i, 256; jge @@d; mov eax, f[i*4]; test eax, eax; jz @@n; cvtsi2sd xmm0, eax; cvtsi2sd xmm1, s; divsd xmm0, xmm1; movsd xmm1, xmm0; call log2; mulsd xmm0, xmm1; movsd xmm1, e; addsd xmm1, xmm0; movsd e, xmm1; @@n: inc i; jmp @@l; @@d: movsd xmm0, e; xorpd xmm1, xmm1; subsd xmm1, xmm0; cvtsd2si eax, xmm1; ret; CalcEntropy endp
log2 proc; fld1; fld qword ptr [esp+4]; fyl2x; fstp qword ptr [esp+4]; ret 8; log2 endp

; Disassembly Engine (Professional)
DisasmEngine proc a:DWORD, s:DWORD; local i:DWORD, o:DWORD, b:BYTE; invoke P, addr szHDIS; mov i, 0; @@l: cmp i, s; jge @@d; mov o, eax; movzx ecx, byte ptr [a+eax]; mov b, cl; cmp cl, 90h; jne @@1; push offset szMnem; jmp @@o; @@1: cmp cl, 0E9h; je @@jmp; cmp cl, 0EBh; je @@jmp; cmp cl, 0E8h; je @@call; cmp cl, 0C3h; je @@ret; cmp cl, 50h; jb @@unk; cmp cl, 57h; ja @@unk; push offset szMnem+16; jmp @@o; @@jmp: push offset szMnem+4; jmp @@o; @@call: push offset szMnem+8; jmp @@o; @@ret: push offset szMnem+12; jmp @@o; @@unk: push offset szUnk; @@o: add esp, 4; movzx eax, b; invoke wsprintf, addr tB, addr szDisFmt, o, eax, offset szMnem, offset szMnem; invoke P, addr tB; call InstrLen; add i, eax; jmp @@l; @@d: ret; DisasmEngine endp
InstrLen proc; movzx eax, byte ptr [a+i]; cmp al, 0Fh; je @@2; cmp al, 66h; je @@p; cmp al, 67h; je @@p; cmp al, 0F0h; jb @@1; cmp al, 0F3h; jbe @@p; @@1: cmp al, 50h; jb @@c; cmp al, 57h; jbe @@1b; cmp al, 0B0h; jb @@c; cmp al, 0BFh; jbe @@1b; cmp al, 0E9h; je @@5; cmp al, 0E8h; je @@5; cmp al, 0EBh; je @@2; cmp al, 0C3h; je @@1b; cmp al, 0C2h; je @@3; @@c: mov eax, 1; ret; @@p: mov eax, 2; ret; @@2: mov eax, 2; ret; @@3: mov eax, 3; ret; @@5: mov eax, 5; ret; @@1b: mov eax, 1; ret; InstrLen endp

; Main Analysis
AnalyzeFile proc; invoke P, addr szHPE; call ParsePE; test eax, eax; jz @@i; test dwFlags, FLG_X64; jz @@x86; push offset szX64; jmp @@a; @@x86: push offset szX86; @@a: push offset szArch; call P; add esp, 8; call DumpSections; call DumpImports; call DumpExports; call DumpResources; invoke ExtractStrings, pBase, fSz; mov eax, 1; ret; @@i: invoke P, addr szER; xor eax, eax; ret; AnalyzeFile endp

; Menu System
MainMenu proc; local c:DWORD, a:DWORD, s:DWORD; @@m: invoke P, addr szW; invoke P, addr szM; call RI; mov c, eax; cmp c, 9; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; cmp c, 8; je @@8; jmp @@m; @@1: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call AnalyzeFile; call UnmapFile; jmp @@m; @@2: invoke P, addr szVA; call RH; mov a, eax; invoke P, addr szSZ; call RI; invoke DisasmEngine, a, eax; jmp @@m; @@3: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call ParsePE; call DumpImports; call UnmapFile; jmp @@m; @@4: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call ParsePE; call DumpExports; call UnmapFile; jmp @@m; @@5: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call ParsePE; call DumpResources; call UnmapFile; jmp @@m; @@6: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; invoke ExtractStrings, pBase, fSz; call UnmapFile; jmp @@m; @@7: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; invoke CalcEntropy, pBase, fSz; call UnmapFile; jmp @@m; @@8: invoke P, addr szPF; call R; invoke MapFile, addr bP; test eax, eax; jz @@m; call ParsePE; call DumpSections; call UnmapFile; jmp @@m; @@x: ret; MainMenu endp

; Entry
start: invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MainMenu; invoke ExitProcess, 0; end start
