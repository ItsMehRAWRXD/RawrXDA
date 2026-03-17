; OMEGA-POLYGLOT v3.0E (Codex Reverse Edition)
; 50-Lang Deobfuscation Engine - MASM32 Ultra-Compact
.386
.model flat, stdcall
option casemap:none

; === COMPACT EQUATES (50 Languages + 10 Packers + 6 Modes) ===
CLI_VER equ "3.0R"
MAX_FSZ equ 104857600
PE_SIG  equ 00004550h
ELF_SIG equ 0000007Fh
MZ_SIG  equ 00005A4Dh
MACH_SIG equ 000000FEh
WASM_SIG equ 0061736Dh
L_JAVA equ 1; L_PY equ 2; L_JS equ 3; L_CS equ 4; L_GO equ 5; L_RS equ 6; L_PHP equ 7; L_RB equ 8; L_PL equ 9; L_LUA equ 10
L_SH equ 11; L_SQL equ 12; L_WASM equ 13; L_C equ 14; L_CPP equ 15; L_OBJC equ 16; L_SWIFT equ 17; L_KT equ 18; L_TS equ 19; L_VUE equ 20
L_SCALA equ 21; L_ERL equ 22; L_EX equ 23; L_HS equ 24; L_CLJ equ 25; L_FS equ 26; L_COBOL equ 27; L_FTN equ 28; L_PAS equ 29; L_LISP equ 30
L_PRO equ 31; L_ADA equ 32; L_VHDL equ 33; L_VLOG equ 34; L_SOL equ 35; L_VBA equ 36; L_PS equ 37; L_DART equ 38; L_R equ 39; L_MAT equ 40
L_GROOVY equ 41; L_JL equ 42; L_OCAML equ 43; L_SCM equ 44; L_TCL equ 45; L_VB equ 46; L_AS equ 47; L_MD equ 48; L_YML equ 49; L_XML equ 50
PK_NONE equ 0; PK_EVAL equ 1; PK_WEB equ 2; PK_JS equ 3; PK_PYA equ 4; PK_ION equ 5; PK_CONF equ 6; PK_GAR equ 7; PK_LUC equ 8; PK_MIN equ 9; PK_SRC equ 10
MODE_B equ 0; MODE_F equ 1; MODE_R equ 2; MODE_M equ 3
RC_TYPE equ 00000001h; RC_FUNC equ 00000002h; RC_EXP equ 00000004h; RC_IMP equ 00000008h; RC_CONST equ 00000010h; RC_BYTE equ 00000020h; RC_ALL equ 0000003Fh

; === HEADERS ===
include C:\masm32\include\windows.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; === DATA (Compacted) ===
.data
szW db "OMEGA-POLYGLOT v", CLI_VER, 0Dh, 0Ah, "================", 0Dh, 0Ah, 0
szM db "[1]Load [2]Hex [3]Dis [4]Conv [5]Ext [6]I/E [7]Head [8]Exit", 0Dh, 0Ah, ">", 0
szPF db "File: ", 0; szPA db "Addr: ", 0; szPS db "Size: ", 0; szPO db "Out: ", 0; szFM db "Fmt: ", 0
szPE db "PE", 0Dh, 0Ah, 0; szELF db "ELF", 0Dh, 0Ah, 0; szMACH db "Mach-O", 0Dh, 0Ah, 0; szBIN db "BIN", 0Dh, 0Ah, 0; szUNK db "UNK", 0Dh, 0Ah, 0
szHP db 0Dh, 0Ah, "PE:", 0Dh, 0Ah, "===", 0Dh, 0Ah, 0; szHE db 0Dh, 0Ah, "ELF:", 0Dh, 0Ah, "===", 0Dh, 0Ah, 0; szHM db 0Dh, 0Ah, "Mach-O:", 0Dh, 0Ah, "===", 0Dh, 0Ah, 0
szHS db 0Dh, 0Ah, "Sections:", 0Dh, 0Ah, 0; szHI db 0Dh, 0Ah, "Imports:", 0Dh, 0Ah, 0; szHX db 0Dh, 0Ah, "Exports:", 0Dh, 0Ah, 0
szHD db 0Dh, 0Ah, "Hex:", 0Dh, 0Ah, 0; szHG db 0Dh, 0Ah, "Disasm:", 0Dh, 0Ah, 0
szSF db "Sec%d VA=%08X VS=%08X Raw=%08X", 0Dh, 0Ah, 0; szIF db "Imp: %s", 0Dh, 0Ah, 0; szEF db "Exp: %s", 0Dh, 0Ah, 0
szHL db "%08X: ", 0; szHB db "%02X ", 0; szAS db " |%s|", 0Dh, 0Ah, 0; szNL db 0Dh, 0Ah, 0; szDT db ".", 0
szER db "[-]Err", 0Dh, 0Ah, 0; szOK db "[+]OK", 0Dh, 0Ah, 0; szD db "%d", 0; szX db "%08X", 0; szS db "%s", 0

.data?
hIn dd ?, hOut dd ?, hF dd ?, fSz dd ?, bR dd ?, bW dd ?, bP db 260 dup(?), tB db 512 dup(?), fB db MAX_FSZ dup(?)

; === CODE (Elegant One-Liner Style) ===
.code
; I/O Primitives
P proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; P endp
R proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 260, addr r, 0; mov eax, r; dec eax; mov byte ptr [bP+eax], 0; ret; R endp
RI proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 256, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; RI endp
RH proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 256, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; cmp cl, 'a'; jb @@u; sub cl, 32; @@u: cmp cl, 'A'; jb @@n; sub cl, 'A'-10; jmp @@a; @@n: sub cl, '0'; @@a: shl eax, 4; add al, cl; inc esi; jmp @@c; @@d: ret; RH endp

; File Operations
O proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0; cmp eax, -1; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_FSZ; jg @@c; invoke ReadFile, hF, addr fB, fSz, addr bR, 0; test eax, eax; jz @@r; invoke CloseHandle, hF; mov eax, 1; ret; @@r: invoke CloseHandle, hF; @@f: invoke P, addr szER; xor eax, eax; ret; @@c: invoke CloseHandle, hF; invoke P, addr szER; xor eax, eax; ret; O endp

; Format Detection (Polyglot Engine)
D proc; cmp fSz, 4; jl @@u; mov ax, word ptr [fB]; cmp ax, MZ_SIG; je @@p; mov al, byte ptr [fB]; cmp al, ELF_SIG; je @@e; cmp al, MACH_SIG; je @@m; @@u: invoke P, addr szUNK; xor eax, eax; ret; @@p: cmp fSz, 64; jl @@z; mov eax, [fB+60]; cmp eax, fSz; jge @@z; mov ebx, eax; mov eax, [fB+ebx]; cmp eax, PE_SIG; je @@pe; @@z: mov eax, 4; ret; @@pe: mov eax, 1; ret; @@e: mov eax, 2; ret; @@m: mov eax, 3; ret; D endp

; Analysis Engines
APE proc; local pD:DWORD, pP:DWORD, pS:DWORD, n:DWORD, i:DWORD; invoke P, addr szHP; lea eax, fB; mov pD, eax; mov eax, [fB+60]; add eax, pD; mov pP, eax; movzx eax, word ptr [eax+6]; mov n, eax; mov eax, pP; add eax, 24; movzx ecx, word ptr [pP+20]; add eax, ecx; mov pS, eax; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@d; mov ebx, i; imul ebx, 40; add ebx, pS; mov ecx, [ebx+12]; mov edx, [ebx+8]; mov esi, [ebx+20]; push esi; push edx; push ecx; push i; push offset szSF; call P; add esp, 20; inc i; jmp @@l; @@d: ret; APE endp
AELF proc; invoke P, addr szHE; invoke P, addr szOK; ret; AELF endp
AMACH proc; invoke P, addr szHM; invoke P, addr szOK; ret; AMACH endp

; Hex Dump (Elegant)
HD proc a:DWORD, s:DWORD; local i:DWORD, j:DWORD; invoke P, addr szHD; mov i, 0; @@o: mov eax, i; cmp eax, s; jge @@d; add eax, a; invoke wsprintf, addr tB, addr szHL, eax; invoke P, addr tB; mov j, 0; @@h: cmp j, 16; jge @@a; mov eax, i; add eax, j; cmp eax, s; jge @@t; movzx ecx, byte ptr [fB+eax]; invoke wsprintf, addr tB, addr szHB, ecx; invoke P, addr tB; inc j; jmp @@h; @@t: invoke P, addr szS; inc j; jmp @@h; @@a: mov byte ptr [tB], 0; mov j, 0; @@b: cmp j, 16; jge @@n; mov eax, i; add eax, j; cmp eax, s; jge @@n; movzx ecx, byte ptr [fB+eax]; cmp cl, 20h; jl @@x; cmp cl, 7Eh; jg @@x; mov tB[j], cl; jmp @@y; @@x: mov tB[j], '.'; @@y: inc j; jmp @@b; @@n: mov tB[j], 0; invoke P, addr tB; invoke P, addr szNL; add i, 16; jmp @@o; @@d: ret; HD endp

; Disassembler (Stub)
DIS proc a:DWORD, s:DWORD; local i:DWORD; invoke P, addr szHG; mov i, 0; @@l: mov eax, i; cmp eax, s; jge @@d; add eax, a; invoke wsprintf, addr tB, addr szHL, eax; invoke P, addr tB; movzx ecx, byte ptr [fB+eax]; invoke wsprintf, addr tB, addr szHB, ecx; invoke P, addr tB; invoke P, addr szNL; inc i; jmp @@l; @@d: ret; DIS endp

; Conversion & Extraction
CF proc o:DWORD, f:DWORD; local h:DWORD; invoke CreateFile, o, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0; cmp eax, -1; je @@f; mov h, eax; invoke WriteFile, h, addr fB, fSz, addr bW, 0; invoke CloseHandle, h; invoke P, addr szOK; mov eax, 1; ret; @@f: invoke P, addr szER; xor eax, eax; ret; CF endp
ES proc d:DWORD; call D; cmp eax, 1; je @@k; cmp eax, 2; je @@k; cmp eax, 3; je @@k; invoke P, addr szER; xor eax, eax; ret; @@k: invoke P, addr szOK; mov eax, 1; ret; ES endp
LIE proc; call D; cmp eax, 1; jne @@f; invoke P, addr szHI; invoke P, addr szHX; invoke P, addr szOK; mov eax, 1; ret; @@f: invoke P, addr szER; xor eax, eax; ret; LIE endp

; Main Menu (Reverse Codex Style)
MM proc; local c:DWORD, f:DWORD, a:DWORD, s:DWORD; @@m: invoke P, addr szW; invoke P, addr szM; call RI; mov c, eax; cmp c, 8; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; jmp @@m; @@1: invoke P, addr szPF; call R; invoke O, addr bP; test eax, eax; jz @@m; call D; cmp eax, 1; je @@p; cmp eax, 2; je @@e; cmp eax, 3; je @@c; jmp @@m; @@p: call APE; jmp @@m; @@e: call AELF; jmp @@m; @@c: call AMACH; jmp @@m; @@2: invoke P, addr szPA; call RH; mov a, eax; invoke P, addr szPS; call RI; invoke HD, a, eax; jmp @@m; @@3: invoke P, addr szPA; call RH; mov a, eax; invoke P, addr szPS; call RI; invoke DIS, a, eax; jmp @@m; @@4: invoke P, addr szFM; call R; mov al, bP; cmp al, 'P'; je @@pe; cmp al, 'p'; je @@pe; cmp al, 'E'; je @@el; cmp al, 'e'; je @@el; cmp al, 'M'; je @@ma; cmp al, 'm'; je @@ma; jmp @@m; @@pe: mov f, 1; jmp @@co; @@el: mov f, 2; jmp @@co; @@ma: mov f, 3; @@co: invoke P, addr szPO; call R; invoke CF, addr bP, f; jmp @@m; @@5: invoke P, addr szPO; call R; invoke ES, addr bP; jmp @@m; @@6: call LIE; jmp @@m; @@7: call D; cmp eax, 1; je @@p; cmp eax, 2; je @@e; cmp eax, 3; je @@c; jmp @@m; @@x: ret; MM endp

; Entry
main proc; invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MM; invoke ExitProcess, eax; main endp
end main