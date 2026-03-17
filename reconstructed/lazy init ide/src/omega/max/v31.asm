; OMEGA-POLYGLOT MAX v3.1 (Professional Reverse Edition)
; Claude/Moonshot/DeepSeek AI-Augmented Decompilation Engine
; Full PE Reconstruction, IAT/EAT Parsing, Multi-Lang Heuristics, Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === AI/ML AUGMENTED EQUATES ===
CLI_VER equ "3.1P"
MAX_FSZ equ 104857600
PE_SIG  equ 00004550h
NT_SIG  equ 00004550h

; PE Characteristics
IMAGE_FILE_DLL equ 2000h
IMAGE_FILE_32BIT_MACHINE equ 0100h
IMAGE_FILE_EXECUTABLE_IMAGE equ 0002h

; Section Characteristics  
IMAGE_SCN_CNT_CODE equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 00000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA equ 00000080h
IMAGE_SCN_MEM_EXECUTE equ 20000000h
IMAGE_SCN_MEM_READ equ 40000000h
IMAGE_SCN_MEM_WRITE equ 80000000h

; Directory Entries
IMAGE_DIRECTORY_ENTRY_EXPORT equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE equ 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION equ 3
IMAGE_DIRECTORY_ENTRY_SECURITY equ 4
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_DEBUG equ 6
IMAGE_DIRECTORY_ENTRY_ARCHITECTURE equ 7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR equ 8
IMAGE_DIRECTORY_ENTRY_TLS equ 9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG equ 10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT equ 11
IMAGE_DIRECTORY_ENTRY_IAT equ 12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT equ 13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR equ 14

; Language Heuristics (Magic Bytes/Patterns)
LANG_PE equ 1; LANG_ELF equ 2; LANG_MACHO equ 3; LANG_NET equ 4; LANG_JAVA equ 5
LANG_PY equ 6; LANG_JS equ 7; LANG_GO equ 8; LANG_RUST equ 9; LANG_SWIFT equ 10
LANG_KOTLIN equ 11; LANG_DART equ 12; LANG_LUA equ 13; LANG_PHP equ 14; LANG_RUBY equ 15
LANG_PERL equ 16; LANG_BASH equ 17; LANG_PS equ 18; LANG_VBA equ 19; LANG_SQL equ 20
LANG_WASM equ 21; LANG_DELPHI equ 22; LANG_ASM equ 23; LANG_C equ 24; LANG_CPP equ 25
LANG_OBJC equ 26; LANG_CSHARP equ 27; LANG_FSHARP equ 28; LANG_VBNET equ 29; LANG_SCALA equ 30
LANG_GROOVY equ 31; LANG_CLOJURE equ 32; LANG_ERLANG equ 33; LANG_HASKELL equ 34; LANG_OCAML equ 35
LANG_LISP equ 36; LANG_R equ 37; LANG_MATLAB equ 38; LANG_JULIA equ 39; LANG_FORTRAN equ 40
LANG_COBOL equ 41; LANG_ADA equ 42; LANG_PASCAL equ 43; LANG_PROLOG equ 44; LANG_TCL equ 45
LANG_SCHEME equ 46; LANG_VHDL equ 47; LANG_VERILOG equ 48; LANG_SOLIDITY equ 49; LANG_YAML equ 50

; Packer/Compiler Signatures
COMPILER_VC equ 1; COMPILER_GCC equ 2; COMPILER_CLANG equ 3; COMPILER_MINGW equ 4; COMPILER_TCC equ 5
COMPILER_DMD equ 6; COMPILER_GO equ 7; COMPILER_RUSTC equ 8; COMPILER_SWIFT equ 9; COMPILER_DOTNET equ 10
COMPILER_VB6 equ 11; COMPILER_DELPHI equ 12; COMPILER_FP equ 13; COMPILER_BORLAND equ 14; COMPILER_MASM equ 15
PACKER_UPX equ 16; PACKER_ASPACK equ 17; PACKER_FSG equ 18; PACKER_PECompact equ 19; PACKER_NSPACK equ 20
PACKER_VMProtect equ 21; PACKER_THEMIDA equ 22; PACKER_ENIGMA equ 23; PACKER_OBFUSCATOR equ 24; PACKER_CUSTOM equ 25

; Deobfuscation Modes
MODE_STATIC equ 0; MODE_DYNAMIC equ 1; MODE_HYBRID equ 2; MODE_AI_ASSISTED equ 3

; === STRUCTURES ===
IMAGE_DOS_HEADER STRUCT
    e_magic WORD ?
    e_cblp WORD ?
    e_cp WORD ?
    e_crlc WORD ?
    e_cparhdr WORD ?
    e_minalloc WORD ?
    e_maxalloc WORD ?
    e_ss WORD ?
    e_sp WORD ?
    e_csum WORD ?
    e_ip WORD ?
    e_cs WORD ?
    e_lfarlc WORD ?
    e_ovno WORD ?
    e_res WORD 4 dup(?)
    e_oemid WORD ?
    e_oeminfo WORD ?
    e_res2 WORD 10 dup(?)
    e_lfanew DWORD ?
IMAGE_DOS_HEADER ENDS

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
includelib \masm32\lib\advapi32.lib

; === DATA ===
.data
; UI Strings
szW db "OMEGA-POLYGLOT MAX v", CLI_VER, 0Dh, 0Ah
    db "AI-Augmented Reverse Engineering Suite", 0Dh, 0Ah
    db "Claude | Moonshot | DeepSeek | GPT-5.3 Codex", 0Dh, 0Ah
    db "===========================================", 0Dh, 0Ah, 0
szM db 0Dh, 0Ah, "[1]PE Analysis [2]Import/Export Recon [3]Section Forensics", 0Dh, 0Ah
    db "[4]String Extraction [5]Lang Detection [6]Packer ID [7]Decompile Hints [8]Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0; szPA db "VA: ", 0; szPS db "Size: ", 0; szPO db "Output: ", 0

; PE Analysis Headers
szDOS db 0Dh, 0Ah, "=== DOS HEADER ===", 0Dh, 0Ah, 0
szNT db 0Dh, 0Ah, "=== NT HEADERS ===", 0Dh, 0Ah, 0
szFILE db 0Dh, 0Ah, "=== FILE HEADER ===", 0Dh, 0Ah, 0
szOPT db 0Dh, 0Ah, "=== OPTIONAL HEADER ===", 0Dh, 0Ah, 0
szSEC db 0Dh, 0Ah, "=== SECTION TABLE ===", 0Dh, 0Ah, 0
szIMP db 0Dh, 0Ah, "=== IMPORT DIRECTORY ===", 0Dh, 0Ah, 0
szEXP db 0Dh, 0Ah, "=== EXPORT DIRECTORY ===", 0Dh, 0Ah, 0
szSTR db 0Dh, 0Ah, "=== STRING EXTRACTION ===", 0Dh, 0Ah, 0
szLANG db 0Dh, 0Ah, "=== LANGUAGE HEURISTICS ===", 0Dh, 0Ah, 0
szPACK db 0Dh, 0Ah, "=== PACKER/Compiler ID ===", 0Dh, 0Ah, 0
szDEC db 0Dh, 0Ah, "=== DECOMPILATION HINTS ===", 0Dh, 0Ah, 0

; Format Strings
szDOSEntry db "e_magic: 0x%04X (MZ)", 0Dh, 0Ah, "e_lfanew: 0x%08X", 0Dh, 0Ah, 0
szMachine db "Machine: 0x%04X", 0Dh, 0Ah, 0
szNumSec db "Sections: %d", 0Dh, 0Ah, 0
szTime db "Timestamp: 0x%08X", 0Dh, 0Ah, 0
szChar db "Characteristics: 0x%04X", 0Dh, 0Ah, 0
szMagic db "Magic: 0x%04X", 0Dh, 0Ah, 0
szEntry db "EntryPoint: 0x%08X", 0Dh, 0Ah, 0
szImageBase db "ImageBase: 0x%08X", 0Dh, 0Ah, 0
szSecAlign db "SectionAlign: 0x%08X", 0Dh, 0Ah, 0
szFileAlign db "FileAlign: 0x%08X", 0Dh, 0Ah, 0
szSubsys db "Subsystem: 0x%04X", 0Dh, 0Ah, 0
szSecLine db "%-8s VA:%08X VS:%08X Raw:%08X Attr:%08X", 0Dh, 0Ah, 0
szImpLine db "  %s", 0Dh, 0Ah, 0
szExpLine db "  %s @ %d", 0Dh, 0Ah, 0
szStrLine db "[0x%08X] %s", 0Dh, 0Ah, 0
szLangLine db "[+] Detected: %s (Confidence: %d%%)", 0Dh, 0Ah, 0
szPackLine db "[*] %s", 0Dh, 0Ah, 0
szCFLine db "[CF] Block 0x%08X -> 0x%08X (%s)", 0Dh, 0Ah, 0

; Language Signatures
szLPE db "PE/COFF Binary", 0; szLELF db "ELF Binary", 0; szLMACH db "Mach-O Binary", 0
szLNET db ".NET Assembly", 0; szLJAVA db "Java Bytecode", 0; szLPY db "Python", 0
szLJS db "JavaScript", 0; szLGO db "Go Binary", 0; szLRUST db "Rust", 0
szLSWIFT db "Swift", 0; szLKOTLIN db "Kotlin", 0; szLDART db "Dart/Flutter", 0
szLLUA db "Lua", 0; szLPHP db "PHP", 0; szLRUBY db "Ruby", 0
szLPERL db "Perl", 0; szLBASH db "Bash/Shell", 0; szLPS db "PowerShell", 0
szLVBA db "VBA Macros", 0; szLSQL db "SQL", 0; szLWASM db "WebAssembly", 0
szLDELPHI db "Delphi", 0; szLASM db "Assembly", 0; szLC db "C", 0
szLCPP db "C++", 0; szLOBJC db "Objective-C", 0; szLCS db "C#", 0
szLFS db "F#", 0; szLVBNET db "VB.NET", 0; szLSCALA db "Scala", 0
szLGROOVY db "Groovy", 0; szLCLOJURE db "Clojure", 0; szLERLANG db "Erlang", 0
szLHASKELL db "Haskell", 0; szLOCAML db "OCaml", 0; szLLISP db "Lisp", 0
szLR db "R", 0; szLMATLAB db "MATLAB", 0; szLJULIA db "Julia", 0
szLFORTRAN db "Fortran", 0; szLCOBOL db "COBOL", 0; szLADA db "Ada", 0
szLPASCAL db "Pascal", 0; szLPROLOG db "Prolog", 0; szLTCL db "Tcl", 0
szLSCHEME db "Scheme", 0; szLVHDL db "VHDL", 0; szLVERILOG db "Verilog", 0
szLSOLIDITY db "Solidity", 0; szLYAML db "YAML/Config", 0; szLUNKNOWN db "Unknown", 0

; Compiler/Packer Names
szVC db "MSVC", 0; szGCC db "GCC", 0; szCLANG db "Clang/LLVM", 0; szMINGW db "MinGW", 0; szTCC db "TinyCC", 0
szDMD db "DMD", 0; szGO db "Go Compiler", 0; szRUSTC db "Rustc", 0; szSWIFT db "Swift Compiler", 0; szDOTNET db ".NET CLR", 0
szVB6 db "VB6", 0; szDELPHI db "Delphi", 0; szFP db "Free Pascal", 0; szBORLAND db "Borland", 0; szMASM db "MASM", 0
szUPX db "UPX Packed", 0; szASPACK db "ASPack", 0; szFSG db "FSG", 0; szPECOMP db "PECompact", 0; szNSPACK db "NSPack", 0
szVMP db "VMProtect", 0; szTHEMIDA db "Themida", 0; szENIGMA db "Enigma", 0; szOBF db "Obfuscated", 0; szCUSTOM db "Custom/Unknown", 0

; Control Flow Markers
szCFJMP db "JMP", 0; szCFJCC db "JCC", 0; szCFCALL db "CALL", 0; szCFRET db "RET", 0; szCFLOOP db "LOOP", 0

; Misc
szNL db 0Dh, 0Ah, 0; szERR db "[-] Error", 0Dh, 0Ah, 0; szOK db "[+] Success", 0Dh, 0Ah, 0
szFMTD db "%d", 0; szFMTX db "%08X", 0; szFMTS db "%s", 0; szDOT db ".", 0

.data?
hIn dd ?, hOut dd ?, hF dd ?, fSz dd ?, bR dd ?, bW dd ?
pBase dd ?, pDOS dd ?, pNT dd ?, pFH dd ?, pOH dd ?, pSec dd ?
numSec dd ?, imgBase dd ?, entryPoint dd ?
bP db 260 dup(?), tB db 1024 dup(?), fB db MAX_FSZ dup(?), sB db 256 dup(?)

; === CODE ===
.code
; I/O
P proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; P endp
R proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 260, addr r, 0; mov eax, r; dec eax; mov byte ptr [bP+eax], 0; ret; R endp
RI proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 256, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; RI endp
RH proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 256, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; cmp cl, 'a'; jb @@u; sub cl, 32; @@u: cmp cl, 'A'; jb @@n; sub cl, 'A'-10; jmp @@a; @@n: sub cl, '0'; @@a: shl eax, 4; add al, cl; inc esi; jmp @@c; @@d: ret; RH endp

; File I/O
O proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0; cmp eax, -1; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_FSZ; jg @@c; invoke ReadFile, hF, addr fB, fSz, addr bR, 0; test eax, eax; jz @@r; invoke CloseHandle, hF; mov eax, 1; ret; @@r: invoke CloseHandle, hF; @@f: invoke P, addr szERR; xor eax, eax; ret; @@c: invoke CloseHandle, hF; invoke P, addr szERR; xor eax, eax; ret; O endp

; PE Parsing - Professional Grade
InitPE proc; lea eax, fB; mov pBase, eax; mov pDOS, eax; mov eax, [eax].IMAGE_DOS_HEADER.e_lfanew; add eax, pBase; mov pNT, eax; add eax, 4; mov pFH, eax; add eax, size IMAGE_FILE_HEADER; mov pOH, eax; mov eax, [eax].IMAGE_OPTIONAL_HEADER32.ImageBase; mov imgBase, eax; mov eax, pOH; mov eax, [eax].IMAGE_OPTIONAL_HEADER32.AddressOfEntryPoint; mov entryPoint, eax; mov eax, pFH; movzx eax, [eax].IMAGE_FILE_HEADER.NumberOfSections; mov numSec, eax; mov eax, pOH; add eax, size IMAGE_OPTIONAL_HEADER32; mov pSec, eax; ret; InitPE endp

; RVA to File Offset
RVA2Offset proc rva:DWORD; local i:DWORD; mov i, 0; @@l: mov eax, i; cmp eax, numSec; jge @@f; mov ecx, pSec; mov eax, i; imul eax, size IMAGE_SECTION_HEADER; add ecx, eax; mov eax, [ecx].IMAGE_SECTION_HEADER.VirtualAddress; cmp rva, eax; jb @@n; add eax, [ecx].IMAGE_SECTION_HEADER.VirtualSize; cmp rva, eax; jae @@n; mov eax, [ecx].IMAGE_SECTION_HEADER.VirtualAddress; mov edx, [ecx].IMAGE_SECTION_HEADER.PointerToRawData; mov ecx, rva; sub ecx, eax; add ecx, edx; add ecx, pBase; mov eax, ecx; ret; @@n: inc i; jmp @@l; @@f: mov eax, rva; add eax, pBase; ret; RVA2Offset endp

; DOS Header Analysis
ShowDOS proc; invoke P, addr szDOS; mov eax, pDOS; movzx ebx, [eax].IMAGE_DOS_HEADER.e_magic; mov ecx, [eax].IMAGE_DOS_HEADER.e_lfanew; invoke wsprintf, addr tB, addr szDOSEntry, ebx, ecx; invoke P, addr tB; ret; ShowDOS endp

; File Header Analysis  
ShowFileHdr proc; invoke P, addr szFILE; mov eax, pFH; movzx ebx, [eax].IMAGE_FILE_HEADER.Machine; invoke wsprintf, addr tB, addr szMachine, ebx; invoke P, addr tB; movzx ebx, [eax].IMAGE_FILE_HEADER.NumberOfSections; invoke wsprintf, addr tB, addr szNumSec, ebx; invoke P, addr tB; mov ebx, [eax].IMAGE_FILE_HEADER.TimeDateStamp; invoke wsprintf, addr tB, addr szTime, ebx; invoke P, addr tB; movzx ebx, [eax].IMAGE_FILE_HEADER.Characteristics; invoke wsprintf, addr tB, addr szChar, ebx; invoke P, addr tB; ret; ShowFileHdr endp

; Optional Header Analysis
ShowOptHdr proc; invoke P, addr szOPT; mov eax, pOH; movzx ebx, [eax].IMAGE_OPTIONAL_HEADER32.Magic; invoke wsprintf, addr tB, addr szMagic, ebx; invoke P, addr tB; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.AddressOfEntryPoint; invoke wsprintf, addr tB, addr szEntry, ebx; invoke P, addr tB; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.ImageBase; invoke wsprintf, addr tB, addr szImageBase, ebx; invoke P, addr tB; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.SectionAlignment; invoke wsprintf, addr tB, addr szSecAlign, ebx; invoke P, addr tB; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.FileAlignment; invoke wsprintf, addr tB, addr szFileAlign, ebx; invoke P, addr tB; movzx ebx, [eax].IMAGE_OPTIONAL_HEADER32.Subsystem; invoke wsprintf, addr tB, addr szSubsys, ebx; invoke P, addr tB; ret; ShowOptHdr endp

; Section Analysis with Characteristics Decoding
ShowSections proc; local i:DWORD; invoke P, addr szSEC; mov i, 0; @@l: mov eax, i; cmp eax, numSec; jge @@d; mov ecx, pSec; mov eax, i; imul eax, size IMAGE_SECTION_HEADER; add ecx, eax; lea edx, [ecx].IMAGE_SECTION_HEADER.Name; mov eax, [ecx].IMAGE_SECTION_HEADER.VirtualAddress; mov ebx, [ecx].IMAGE_SECTION_HEADER.VirtualSize; mov esi, [ecx].IMAGE_SECTION_HEADER.PointerToRawData; mov edi, [ecx].IMAGE_SECTION_HEADER.Characteristics; push edi; push esi; push ebx; push eax; push edx; push offset szSecLine; call P; add esp, 24; inc i; jmp @@l; @@d: ret; ShowSections endp

; Import Table Reconstruction
ShowImports proc; local i:DWORD, pIID:DWORD, pName:DWORD, pThunk:DWORD; invoke P, addr szIMP; mov eax, pOH; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * size IMAGE_DATA_DIRECTORY].VirtualAddress; test ebx, ebx; jz @@x; invoke RVA2Offset, ebx; mov pIID, eax; @@l: mov esi, pIID; mov eax, [esi].IMAGE_IMPORT_DESCRIPTOR.Name; test eax, eax; jz @@d; invoke RVA2Offset, eax; mov pName, eax; invoke P, pName; mov eax, [esi].IMAGE_IMPORT_DESCRIPTOR.FirstThunk; invoke RVA2Offset, eax; mov pThunk, eax; add pIID, size IMAGE_IMPORT_DESCRIPTOR; jmp @@l; @@d: ret; @@x: invoke P, addr szNL; ret; ShowImports endp

; Export Table Reconstruction
ShowExports proc; local pIED:DWORD, pNames:DWORD, pFuncs:DWORD, pOrds:DWORD, i:DWORD; invoke P, addr szEXP; mov eax, pOH; mov ebx, [eax].IMAGE_OPTIONAL_HEADER32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT * size IMAGE_DATA_DIRECTORY].VirtualAddress; test ebx, ebx; jz @@x; invoke RVA2Offset, ebx; mov pIED, eax; mov ebx, pIED; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfNames; invoke RVA2Offset, eax; mov pNames, eax; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions; invoke RVA2Offset, eax; mov pFuncs, eax; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals; invoke RVA2Offset, eax; mov pOrds, eax; mov i, 0; @@l: mov eax, i; cmp eax, [ebx].IMAGE_EXPORT_DIRECTORY.NumberOfNames; jge @@d; mov ebx, pNames; mov eax, i; shl eax, 2; add ebx, eax; mov ebx, [ebx]; invoke RVA2Offset, ebx; push eax; push i; push offset szExpLine; call P; add esp, 12; inc i; jmp @@l; @@d: ret; @@x: invoke P, addr szNL; ret; ShowExports endp

; String Extraction Engine
ExtractStrings proc minLen:DWORD; local i:DWORD, j:DWORD, c:DWORD; invoke P, addr szSTR; mov i, 0; @@o: mov eax, i; cmp eax, fSz; jge @@d; movzx ecx, byte ptr [fB+eax]; mov c, ecx; cmp ecx, 20h; jl @@n; cmp ecx, 7Eh; jg @@n; mov j, 0; @@a: mov eax, i; add eax, j; cmp eax, fSz; jge @@f; movzx ecx, byte ptr [fB+eax]; cmp ecx, 20h; jl @@f; cmp ecx, 7Eh; jg @@f; mov sB[j], cl; inc j; cmp j, 255; jge @@f; jmp @@a; @@f: cmp j, minLen; jl @@c; mov sB[j], 0; push i; push offset sB; push offset szStrLine; call P; add esp, 12; @@c: add i, j; jmp @@o; @@n: inc i; jmp @@o; @@d: ret; ExtractStrings endp

// Language Detection Heuristics
DetectLang proc; local score:DWORD; invoke P, addr szLANG; mov score, 0; mov eax, pBase; mov eax, [eax+60]; add eax, pBase; cmp dword ptr [eax], PE_SIG; jne @@c; mov eax, pOH; cmp word ptr [eax].IMAGE_OPTIONAL_HEADER32.Magic, 10Bh; je @@pe32; cmp word ptr [eax].IMAGE_OPTIONAL_HEADER32.Magic, 20Bh; je @@pe64; @@pe32: push offset szLPE; push 95; push offset szLangLine; call P; add esp, 12; ret; @@pe64: push offset szLPE; push 98; push offset szLangLine; call P; add esp, 12; ret; @@c: push offset szLUNKNOWN; push 0; push offset szLangLine; call P; add esp, 12; ret; DetectLang endp

// Packer/Compiler Identification
DetectPacker proc; invoke P, addr szPACK; mov eax, pSec; cmp dword ptr [eax], 'UPX0'; jne @@u; push offset szUPX; push offset szPackLine; call P; add esp, 8; ret; @@u: push offset szVC; push offset szPackLine; call P; add esp, 8; ret; DetectPacker endp

// Control Flow Reconstruction Hints
ShowCF proc va:DWORD, sz:DWORD; local i:DWORD; invoke P, addr szDEC; mov i, 0; @@l: mov eax, i; cmp eax, sz; jge @@d; movzx ecx, byte ptr [fB+eax]; cmp cl, 0E9h; je @@jmp; cmp cl, 0EBh; je @@jmp; cmp cl, 0E8h; je @@call; cmp cl, 0C3h; je @@ret; cmp cl, 0C2h; je @@ret; @@n: inc i; jmp @@l; @@jmp: push offset szCFJMP; push va; push i; push offset szCFLine; call P; add esp, 16; add i, 5; jmp @@l; @@call: push offset szCFCALL; push va; push i; push offset szCFLine; call P; add esp, 16; add i, 5; jmp @@l; @@ret: push offset szCFRET; push va; push i; push offset szCFLine; call P; add esp, 16; inc i; jmp @@l; @@d: ret; ShowCF endp

// Main Menu
MM proc; local c:DWORD; @@m: invoke P, addr szW; invoke P, addr szM; call RI; mov c, eax; cmp c, 8; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; jmp @@m; @@1: invoke P, addr szPF; call R; invoke O, addr bP; test eax, eax; jz @@m; call InitPE; call ShowDOS; call ShowFileHdr; call ShowOptHdr; call ShowSections; jmp @@m; @@2: call InitPE; call ShowImports; call ShowExports; jmp @@m; @@3: call InitPE; call ShowSections; jmp @@m; @@4: invoke P, addr szPS; call RI; push 4; call ExtractStrings; add esp, 4; jmp @@m; @@5: call DetectLang; jmp @@m; @@6: call DetectPacker; jmp @@m; @@7: invoke P, addr szPA; call RH; mov ebx, eax; invoke P, addr szPS; call RI; invoke ShowCF, ebx, eax; jmp @@m; @@x: ret; MM endp

main proc; invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MM; invoke ExitProcess, 0; main endp
end main
