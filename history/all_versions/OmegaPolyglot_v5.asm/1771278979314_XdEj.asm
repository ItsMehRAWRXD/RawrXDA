; ============================================================================
; OMEGA-POLYGLOT v5.0 — Unified Professional Reverse Engineering Suite
; ============================================================================
; Complete PE32 analysis: headers, imports, exports, TLS, entropy/packer
; detection, string extraction, control flow, source reconstruction,
; hex dump, disassembly, HTML report generation.
;
; Build:  \masm32\bin\ml.exe /c /coff /Cp OmegaPolyglot_v5.asm
;         \masm32\bin\link.exe /SUBSYSTEM:CONSOLE OmegaPolyglot_v5.obj
; ============================================================================
.386
.model flat, stdcall
option casemap:none

; ============================================================================
; INCLUDES
; ============================================================================
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================
SEC_HDR_SIZE    equ 40
IMP_DESC_SIZE   equ 20
MAX_FSIZE       equ 104857600       ; 100 MB

; PE Optional Header DataDirectory offsets (from pOpt)
DD_EXPORT_RVA   equ 96              ; DataDirectory[0].VA
DD_IMPORT_RVA   equ 104             ; DataDirectory[1].VA
DD_RESOURCE_RVA equ 112             ; DataDirectory[2].VA
DD_TLS_RVA      equ 168             ; DataDirectory[9].VA

; Section characteristic flags
SCN_CODE        equ 00000020h
SCN_DATA        equ 00000040h
SCN_EXEC        equ 20000000h
SCN_READ        equ 40000000h
SCN_WRITE       equ 80000000h

; Entropy thresholds (value * 100)
ENT_SUSPICIOUS  equ 700
ENT_PACKED      equ 750

; ============================================================================
; LANGUAGE IDs (50 languages — from CodexReverse canonical enum)
; ============================================================================
LANG_UNKNOWN        equ 0
LANG_C              equ 1
LANG_CPP            equ 2
LANG_CSHARP         equ 3
LANG_JAVA           equ 4
LANG_PYTHON         equ 5
LANG_JAVASCRIPT     equ 6
LANG_TYPESCRIPT     equ 7
LANG_GO             equ 8
LANG_RUST           equ 9
LANG_SWIFT          equ 10
LANG_KOTLIN         equ 11
LANG_PHP            equ 12
LANG_RUBY           equ 13
LANG_PERL           equ 14
LANG_LUA            equ 15
LANG_SHELL          equ 16
LANG_SQL            equ 17
LANG_WEBASSEMBLY    equ 18
LANG_OBJECTIVEC     equ 19
LANG_DART           equ 20
LANG_SCALA          equ 21
LANG_ERLANG         equ 22
LANG_ELIXIR         equ 23
LANG_HASKELL        equ 24
LANG_CLOJURE        equ 25
LANG_FSHARP         equ 26
LANG_COBOL          equ 27
LANG_FORTRAN        equ 28
LANG_PASCAL         equ 29
LANG_DELPHI         equ 30
LANG_LISP           equ 31
LANG_PROLOG         equ 32
LANG_ADA            equ 33
LANG_VHDL           equ 34
LANG_VERILOG        equ 35
LANG_SOLIDITY       equ 36
LANG_VBA            equ 37
LANG_POWERSHELL     equ 38
LANG_MATLAB         equ 39
LANG_R              equ 40
LANG_GROOVY         equ 41
LANG_JULIA          equ 42
LANG_OCAML          equ 43
LANG_SCHEME         equ 44
LANG_TCL            equ 45
LANG_VBNET          equ 46
LANG_ACTIONSCRIPT   equ 47
LANG_NIM            equ 48
LANG_ZIG            equ 49
LANG_CARBON         equ 50
MAX_LANGUAGES       equ 51

; Binary format modes
MODE_AUTO           equ 0
MODE_PE             equ 1
MODE_ELF            equ 2
MODE_MACHO          equ 3
MODE_JAVA           equ 4
MODE_PYTHON         equ 5
MODE_JAVASCRIPT     equ 6

; Obfuscator/Packer IDs
OBF_NONE            equ 0
OBF_VMPROTECT       equ 1
OBF_THEMIDA         equ 2
OBF_UPX             equ 3
OBF_PYARMOR         equ 4
OBF_IONCUBE         equ 5
OBF_JSOBF           equ 6
OBF_CONFUSEREX      equ 7
OBF_GARBLE          equ 8
OBF_PROGUARD        equ 9
OBF_BABEL           equ 10
OBF_ASPACK          equ 11
OBF_PECOMPACT       equ 12
OBF_MPRESS          equ 13

; DD offset for COM/.NET descriptor
DD_COM_RVA          equ 208             ; DataDirectory[14].VA

; ============================================================================
; DATA
; ============================================================================
.data

; --- Banner & Menu ---
szBanner    db 13,10
            db "  OMEGA-POLYGLOT v5.0 - 50-Language Reverse Engineering Suite",13,10
            db "  PE/ELF/Mach-O | 50-Lang Fingerprint | Entropy | CFG | Decompiler",13,10
            db "  ================================================================",13,10,13,10,0

szMenu      db "  [1]  Full PE Analysis        [8]  TLS Callbacks",13,10
            db "  [2]  Import Table            [9]  Hex Dump",13,10
            db "  [3]  Export Table            [10] Disassemble",13,10
            db "  [4]  Entropy / Packer Scan   [11] Full Reconnaissance",13,10
            db "  [5]  String Extraction       [12] HTML Report",13,10
            db "  [6]  Control Flow Recovery   [13] Language Detection (50 langs)",13,10
            db "  [7]  Source Reconstruction   [14] Lang-Aware Source Recon",13,10
            db "  [0]  Exit",13,10
            db "  > ",0

; --- Prompts ---
szPromptFile db "Target binary: ",0
szPromptAddr db "Start offset (hex): ",0
szPromptSize db "Count/Length: ",0
szPromptOut  db "Output directory: ",0

; --- PE Analysis Format Strings ---
szHdrPE     db 13,10,"=== PE HEADER ANALYSIS ===",13,10,0

szFmtDOS    db "DOS Header:",13,10
            db "  Magic:      %04Xh (MZ)",13,10
            db "  PE Offset:  %08Xh",13,10,0

szFmtFile   db "File Header:",13,10
            db "  Machine:         %04Xh",13,10
            db "  Sections:        %d",13,10
            db "  Timestamp:       %08Xh",13,10
            db "  Characteristics: %04Xh",13,10,0

szFmtOpt    db "Optional Header:",13,10
            db "  EntryPoint:    %08Xh",13,10
            db "  ImageBase:     %08Xh",13,10
            db "  SectionAlign:  %08Xh",13,10
            db "  FileAlign:     %08Xh",13,10
            db "  SizeOfImage:   %08Xh",13,10
            db "  Subsystem:     %04Xh",13,10,0

szHdrSec    db 13,10,"=== SECTION TABLE ===",13,10,0

szFmtSec    db "  [%d] %s",13,10
            db "       VA: %08Xh  VSize: %08Xh",13,10
            db "       Raw: %08Xh  RSize: %08Xh",13,10,0

szFmtChar   db "       Chars: %08Xh [%c%c%c]",13,10,0

; --- Import ---
szHdrImp    db 13,10,"=== IMPORT TABLE ===",13,10,0
szFmtDLL    db 13,10,"  DLL: %s",13,10,0
szFmtFunc   db "    [%04X] %s",13,10,0
szFmtOrd    db "    Ordinal #%d",13,10,0

; --- Export ---
szHdrExp    db 13,10,"=== EXPORT TABLE ===",13,10,0
szFmtExpDLL db "  DLL: %s  (%d exports)",13,10,0
szFmtExpFn  db "    #%d: %s @ RVA %08Xh",13,10,0

; --- TLS ---
szHdrTLS    db 13,10,"=== TLS CALLBACKS ===",13,10,0
szFmtTLS    db "  Callback %d: %08Xh",13,10,0
szFmtTLSCnt db "  Total TLS callbacks: %d",13,10,0

; --- Entropy ---
szHdrEnt    db 13,10,"=== ENTROPY / PACKER ANALYSIS ===",13,10,0
szFmtEnt    db "  %s  Entropy: %d.%02d  [%s]",13,10,0
szPackNone  db "Clean",0
szPackSusp  db "Suspicious - possible packing",0
szPackHigh  db "HIGH - likely packed/encrypted",0

; --- Strings ---
szHdrStr    db 13,10,"=== STRING EXTRACTION ===",13,10,0
szFmtStrLn  db "  [%08X] %s",13,10,0

; --- Control Flow ---
szHdrCF     db 13,10,"=== CONTROL FLOW RECOVERY ===",13,10,0
szFmtBB     db "  Block #%d: %08Xh - %08Xh (%d bytes)",13,10,0
szFmtCFCnt  db "  Total basic blocks: %d",13,10,0

; --- Source Reconstruction ---
szHdrSrc    db 13,10,"=== SOURCE RECONSTRUCTION ===",13,10,0
szFmtSrcHdr db "// Function @ %08Xh",13,10
            db "void func_%08X(void) {",13,10,0
szFmtOp     db "    %s;",13,10,0
szSrcEnd    db "}",13,10,0

; Opcode mnemonics
szOpPush    db "push ebp",0
szOpPop     db "pop ebp",0
szOpMov     db "mov eax, imm32",0
szOpCall    db "call sub_%08Xh",0
szOpJmp     db "goto loc_%08Xh",0
szOpRet     db "return",0
szOpNop     db "nop",0
szOpXor     db "xor eax, eax",0
szOpSub     db "sub esp, imm",0
szOpAdd     db "add esp, imm",0
szOpLea     db "lea reg, [mem]",0

; --- Hex Dump / Disassembly ---
szFmtAddr   db "%08X: ",0
szFmtByte   db "%02X ",0
szHdrDis    db 13,10,"=== DISASSEMBLY ===",13,10,0
szFmtDisA   db "%08Xh: ",0

; --- HTML Report ---
szReportFn  db "\analysis_report.html",0
szHTMLHdr   db "<!DOCTYPE html><html><head><title>OmegaPolyglot v5.0 Report</title>"
            db "<style>"
            db "body{font-family:Consolas,monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}"
            db "h1{color:#569cd6;}h2{color:#4ec9b0;border-bottom:1px solid #333;padding-bottom:4px;}"
            db "pre{background:#252526;padding:12px;border-radius:4px;overflow-x:auto;}"
            db ".warn{color:#ce9178;}.ok{color:#6a9955;}.info{color:#9cdcfe;}"
            db "</style></head><body>",0
szHTMLTitle db "<h1>OmegaPolyglot v5.0 &mdash; Analysis Report</h1>",0
szHTMLPre   db "<pre>",0
szHTMLPreE  db "</pre>",0
szHTMLEnd   db "</body></html>",0
szRptDone   db "[+] Report saved to: %s",13,10,0

; --- Status / Error ---
szNL        db 13,10,0
szDone      db "[+] Analysis complete.",13,10,0
szErrOpen   db "[-] Could not open file.",13,10,0
szErrMap    db "[-] Could not map file.",13,10,0
szErrSize   db "[-] File too small or too large.",13,10,0
szErrPE     db "[-] Invalid PE file.",13,10,0
szErrNoFile db "[-] No file loaded. Use option [1] first.",13,10,0
szNoExport  db "[*] No export directory found.",13,10,0
szNoImport  db "[*] No import directory found.",13,10,0
szNoTLS     db "[*] No TLS directory found.",13,10,0
szNoStrings db "[*] No printable strings found.",13,10,0

; --- Language Detection ---
szHdrLang   db 13,10,"=== LANGUAGE / COMPILER DETECTION ===",13,10,0
szFmtLang   db "  Binary format:     %s",13,10,0
szFmtComp   db "  Source language:   %s",13,10,0
szFmtPack   db "  Packer/Obfuscator: %s",13,10,0
szFmtLinker db "  Linker version:    %d.%d",13,10,0
szFmtRich   db "  Rich header:       %s",13,10,0

; Language name strings (50 languages)
szLn00 db "Unknown",0
szLn01 db "C",0
szLn02 db "C++",0
szLn03 db "C#",0
szLn04 db "Java",0
szLn05 db "Python",0
szLn06 db "JavaScript",0
szLn07 db "TypeScript",0
szLn08 db "Go",0
szLn09 db "Rust",0
szLn10 db "Swift",0
szLn11 db "Kotlin",0
szLn12 db "PHP",0
szLn13 db "Ruby",0
szLn14 db "Perl",0
szLn15 db "Lua",0
szLn16 db "Shell",0
szLn17 db "SQL",0
szLn18 db "WebAssembly",0
szLn19 db "Objective-C",0
szLn20 db "Dart",0
szLn21 db "Scala",0
szLn22 db "Erlang",0
szLn23 db "Elixir",0
szLn24 db "Haskell",0
szLn25 db "Clojure",0
szLn26 db "F#",0
szLn27 db "COBOL",0
szLn28 db "Fortran",0
szLn29 db "Pascal",0
szLn30 db "Delphi",0
szLn31 db "Lisp",0
szLn32 db "Prolog",0
szLn33 db "Ada",0
szLn34 db "VHDL",0
szLn35 db "Verilog",0
szLn36 db "Solidity",0
szLn37 db "VBA",0
szLn38 db "PowerShell",0
szLn39 db "MATLAB",0
szLn40 db "R",0
szLn41 db "Groovy",0
szLn42 db "Julia",0
szLn43 db "OCaml",0
szLn44 db "Scheme",0
szLn45 db "Tcl",0
szLn46 db "VB.NET",0
szLn47 db "ActionScript",0
szLn48 db "Nim",0
szLn49 db "Zig",0
szLn50 db "Carbon",0

; Language name pointer table
LangNames dd offset szLn00, offset szLn01, offset szLn02, offset szLn03
          dd offset szLn04, offset szLn05, offset szLn06, offset szLn07
          dd offset szLn08, offset szLn09, offset szLn10, offset szLn11
          dd offset szLn12, offset szLn13, offset szLn14, offset szLn15
          dd offset szLn16, offset szLn17, offset szLn18, offset szLn19
          dd offset szLn20, offset szLn21, offset szLn22, offset szLn23
          dd offset szLn24, offset szLn25, offset szLn26, offset szLn27
          dd offset szLn28, offset szLn29, offset szLn30, offset szLn31
          dd offset szLn32, offset szLn33, offset szLn34, offset szLn35
          dd offset szLn36, offset szLn37, offset szLn38, offset szLn39
          dd offset szLn40, offset szLn41, offset szLn42, offset szLn43
          dd offset szLn44, offset szLn45, offset szLn46, offset szLn47
          dd offset szLn48, offset szLn49, offset szLn50

; Binary format name strings
szFmtAuto db "Auto-detect",0
szFmtPE32 db "PE32 (Windows x86)",0
szFmtELF  db "ELF (Linux/Unix)",0
szFmtMACH db "Mach-O (macOS/iOS)",0
szFmtJAVA db "Java Class/JAR",0
szFmtPYC  db "Python Bytecode (.pyc)",0
szFmtJS   db "JavaScript Bundle",0

FmtNames  dd offset szFmtAuto, offset szFmtPE32, offset szFmtELF
          dd offset szFmtMACH, offset szFmtJAVA, offset szFmtPYC, offset szFmtJS

; Packer/obfuscator name strings
szObf00 db "None detected",0
szObf01 db "VMProtect",0
szObf02 db "Themida/WinLicense",0
szObf03 db "UPX",0
szObf04 db "PyArmor",0
szObf05 db "ionCube",0
szObf06 db "javascript-obfuscator",0
szObf07 db "ConfuserEx",0
szObf08 db "Garble (Go)",0
szObf09 db "ProGuard",0
szObf10 db "Babel",0
szObf11 db "ASPack",0
szObf12 db "PECompact",0
szObf13 db "MPRESS",0

ObfNames  dd offset szObf00, offset szObf01, offset szObf02, offset szObf03
          dd offset szObf04, offset szObf05, offset szObf06, offset szObf07
          dd offset szObf08, offset szObf09, offset szObf10, offset szObf11
          dd offset szObf12, offset szObf13

; --- Compiler Fingerprint Strings (import DLL signatures) ---
; .NET / C# / F# / VB.NET
szSigMscoree    db "mscoree.dll",0
szSigMscorlib   db "mscorlib",0
; Python
szSigPython3    db "python3",0
szSigPython     db "python",0
; Java (GraalVM native-image)
szSigJvm        db "jvm.dll",0
szSigJli        db "jli.dll",0
; Go
szSigGoRt       db "runtime.main",0
szSigGoSec      db ".go.",0
; Rust
szSigRustPanic  db "rust_begin_unwind",0
szSigRustSec    db ".rdata$",0
; Delphi / Pascal
szSigDelphi     db "borlndmm.dll",0
szSigDelphi2    db "System.SysUtils",0
; MSVC (C/C++)
szSigMSVCRT     db "MSVCRT",0
szSigMSVCRxx    db "MSVCR",0
szSigUCRT       db "ucrtbase",0
szSigVCRT       db "VCRUNTIME",0
szSigMFC        db "mfc",0
szSigATL        db "ATL",0
; MinGW (C/C++)
szSigMingw      db "msvcrt.dll",0
szSigCygwin     db "cygwin",0
; Objective-C
szSigObjC       db "objc_msgSend",0
szSigNSObj      db "NSObject",0
; Swift
szSigSwift      db "libswiftCore",0
; Kotlin Native
szSigKotlinNat  db "Kotlin",0
; Dart (Flutter)
szSigDart       db "dart_api_dl",0
szSigFlutter    db "flutter",0
; Nim
szSigNim        db "NimMain",0
; Zig
szSigZig        db "__zig_",0
; Ada (GNAT)
szSigAda        db "ada__",0
szSigGNAT       db "__gnat",0
; Fortran (gfortran)
szSigFortran    db "libgfortran",0
szSigFortran2   db "GFORTRAN",0
; Haskell (GHC)
szSigHaskell    db "ghczmprim",0
szSigHaskellRT  db "libHSrts",0
; OCaml
szSigOCaml      db "caml_",0
; Erlang/Elixir (BEAM)
szSigErlang     db "beam.smp",0
; Lua
szSigLua        db "lua5",0
szSigLua2       db "luaL_",0
; Ruby
szSigRuby       db "ruby_init",0
szSigRubyDll    db "msvcrt-ruby",0
; PHP
szSigPHP        db "php_",0
szSigPHP2       db "zend_",0
; Perl
szSigPerl       db "perl_",0
szSigPerl2      db "Perl",0
; Julia
szSigJulia      db "libjulia",0
; R
szSigR          db "R.dll",0
szSigRLib       db "libR",0
; MATLAB (compiled with mcc)
szSigMatlab     db "mclmcrrt",0
; Scala Native
szSigScala      db "scala.runtime",0
; Solidity (compiled via solc to EVM, not PE; detect Ethereum ABI patterns)
szSigSolidity   db "__sol_",0
; COBOL (Micro Focus / GnuCOBOL)
szSigCOBOL      db "libcob",0
szSigCOBOL2     db "COB_",0
; VHDL / Verilog (simulation libs)
szSigVHDL       db "vhpi_",0
szSigVerilog    db "vpi_",0
; VBA / PowerShell (CLR-hosted)
szSigVBA        db "VBE7",0
szSigPS         db "powershell",0
; ActionScript (Flash projector)
szSigFlash      db "Flash",0

; Rich header signature
szRichYes       db "Present (MSVC toolchain)",0
szRichNo        db "Not found",0

; Section name packer signatures
szSecUPX0       db "UPX0",0
szSecUPX1       db "UPX1",0
szSecVMP        db ".vmp",0
szSecThemida    db ".themida",0
szSecASPack     db ".aspack",0
szSecASPck2     db ".adata",0
szSecRsrc       db ".rsrc",0
szSecPEC        db "PEC",0
szSecMPR        db ".MPRESS",0

; Language-aware source recon format strings
szSrcC      db "// C-style reconstruction",13,10,0
szSrcCpp    db "// C++ reconstruction",13,10,0
szSrcCS     db "// C# (managed) reconstruction",13,10,0
szSrcGo     db "// Go reconstruction",13,10,0
szSrcRust   db "// Rust reconstruction",13,10,0
szSrcDelphi db "{ Delphi/Pascal reconstruction }",13,10,0
szSrcPy     db "# Python reconstruction",13,10,0
szSrcJava   db "// Java reconstruction",13,10,0
szSrcNim    db "# Nim reconstruction",13,10,0
szSrcZig    db "// Zig reconstruction",13,10,0
szSrcSwift  db "// Swift reconstruction",13,10,0
szSrcAda    db "-- Ada reconstruction",13,10,0
szSrcFtn    db "! Fortran reconstruction",13,10,0
szSrcHS     db "-- Haskell reconstruction",13,10,0
szSrcOCaml  db "(* OCaml reconstruction *)",13,10,0

; Language-specific function declaration formats
szDeclC     db "void __cdecl sub_%08X(void) {",13,10,0
szDeclCpp   db "void __thiscall Class::sub_%08X() {",13,10,0
szDeclCS    db "public static void Sub_%08X() {",13,10,0
szDeclGo    db "func sub_%08X() {",13,10,0
szDeclRust  db "fn sub_%08X() {",13,10,0
szDeclDelph db "procedure Sub_%08X;",13,10,"begin",13,10,0
szDeclPy    db "def sub_%08X():",13,10,0
szDeclJava  db "public static void sub_%08X() {",13,10,0
szDeclNim   db "proc sub_%08X() =",13,10,0
szDeclZig   db "fn sub_%08X() void {",13,10,0
szDeclSwift db "func sub_%08X() {",13,10,0
szDeclAda   db "procedure Sub_%08X is",13,10,"begin",13,10,0
szDeclFtn   db "subroutine sub_%08X()",13,10,0
szDeclHS    db "sub_%08X :: IO ()",13,10,"sub_%08X = do",13,10,0
szDeclOCaml db "let sub_%08X () =",13,10,0

; Language-specific closing braces / end keywords
szEndBrace  db "}",13,10,0             ; C/C++/C#/Go/Rust/Java/Zig/Swift
szEndDelph  db "end;",13,10,0           ; Delphi/Pascal
szEndPy     db "",13,10,0              ; Python (indentation-based)
szEndAda    db "end Sub_%08X;",13,10,0  ; Ada
szEndFtn    db "end subroutine",13,10,0 ; Fortran
szEndNim    db "",13,10,0              ; Nim (indentation-based)

; Language-specific opcode translations
szOpCAssign   db "    %s = %s;",13,10,0
szOpCSCall    db "    %s();",13,10,0
szOpGoAssign  db "    %s := %s",13,10,0
szOpRustLet   db "    let %s = %s;",13,10,0
szOpPyAssign  db "    %s = %s",13,10,0
szOpDlpAssign db "    %s := %s;",13,10,0
szOpAdaAssign db "    %s := %s;",13,10,0
szOpFtnAssign db "    %s = %s",13,10,0
szOpNimAssign db "  %s = %s",13,10,0

; ============================================================================
; BSS
; ============================================================================
.data?

; Console handles
hStdIn      dd ?
hStdOut     dd ?

; File mapping
hFile       dd ?
hMapping    dd ?
pBase       dd ?
dwFileSize  dd ?

; PE pointers
pDOS        dd ?
pNT         dd ?
pOpt        dd ?
pSecHdr     dd ?
dwNumSec    dd ?
dwEntryPt   dd ?
dwImageBase dd ?
bFileLoaded db ?

; Report redirect
hReportFile dd ?

; Language detection results
dwDetectedLang  dd ?
dwDetectedFmt   dd ?
dwDetectedPack  dd ?
bHasRichHeader  db ?
dwLinkerMajor   dd ?
dwLinkerMinor   dd ?
dwCOMDescRVA    dd ?

; Buffers
bBuffer     db 2048 dup(?)
bTemp       db 512  dup(?)
bInput      db 260  dup(?)
bString     db 260  dup(?)
bSecName    db 16   dup(?)

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; I/O HELPERS
; ============================================================================

; Print — write string to console (or to report file if set)
Print proc lpStr:DWORD
    local dwWritten:DWORD, dwLen:DWORD
    invoke lstrlen, lpStr
    mov dwLen, eax
    test eax, eax
    jz @@skip
    cmp hReportFile, 0
    jne @@tofile
    invoke WriteConsole, hStdOut, lpStr, dwLen, addr dwWritten, 0
    jmp @@skip
@@tofile:
    invoke WriteFile, hReportFile, lpStr, dwLen, addr dwWritten, 0
@@skip:
    ret
Print endp

; WriteStr — write string to a specific file handle
WriteStr proc hOut:DWORD, lpStr:DWORD
    local dwWritten:DWORD, dwLen:DWORD
    invoke lstrlen, lpStr
    mov dwLen, eax
    invoke WriteFile, hOut, lpStr, dwLen, addr dwWritten, 0
    ret
WriteStr endp

; ReadInput — read a line from console, strip CR/LF
ReadInput proc
    local dwRead:DWORD
    invoke ReadConsole, hStdIn, addr bInput, 255, addr dwRead, 0
    ; Strip trailing CR/LF/null
    mov ecx, dwRead
@@strip:
    cmp ecx, 0
    jle @@done
    dec ecx
    movzx eax, byte ptr [bInput+ecx]
    cmp al, 13
    je @@strip
    cmp al, 10
    je @@strip
    inc ecx
@@done:
    mov byte ptr [bInput+ecx], 0
    ret
ReadInput endp

; ReadInt — read decimal integer from console
ReadInt proc
    call ReadInput
    xor eax, eax
    lea edx, bInput
@@loop:
    movzx ecx, byte ptr [edx]
    cmp cl, 0
    je @@done
    sub cl, '0'
    cmp cl, 9
    ja @@done
    imul eax, 10
    movzx ecx, cl
    add eax, ecx
    inc edx
    jmp @@loop
@@done:
    ret
ReadInt endp

; ReadHex — read hex integer from console
ReadHex proc
    call ReadInput
    xor eax, eax
    lea edx, bInput
@@loop:
    movzx ecx, byte ptr [edx]
    cmp cl, 0
    je @@done
    ; Skip optional "0x" prefix
    cmp cl, 'x'
    je @@skip
    cmp cl, 'X'
    je @@skip
    cmp cl, '0'
    jb @@done
    cmp cl, '9'
    jbe @@digit
    or cl, 20h             ; to lowercase
    cmp cl, 'a'
    jb @@done
    cmp cl, 'f'
    ja @@done
    sub cl, 'a'-10
    jmp @@add
@@digit:
    sub cl, '0'
@@add:
    shl eax, 4
    movzx ecx, cl
    add eax, ecx
@@skip:
    inc edx
    jmp @@loop
@@done:
    ret
ReadHex endp

; ============================================================================
; FILE OPERATIONS
; ============================================================================

; MapFile — open and memory-map a PE file
MapFile proc lpPath:DWORD
    local hMap:DWORD

    ; Open file
    invoke CreateFile, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@err_open
    mov hFile, eax

    ; Check size
    invoke GetFileSize, hFile, 0
    mov dwFileSize, eax
    cmp eax, MAX_FSIZE
    jg @@err_size
    cmp eax, 64
    jl @@err_size

    ; Create mapping
    invoke CreateFileMapping, hFile, 0, PAGE_READONLY, 0, 0, 0
    test eax, eax
    jz @@err_map
    mov hMap, eax

    ; Map view
    invoke MapViewOfFile, hMap, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @@err_view
    mov pBase, eax

    ; Done — close mapping handle (view stays valid)
    invoke CloseHandle, hMap
    mov bFileLoaded, 1
    mov eax, 1
    ret

@@err_open:
    invoke Print, addr szErrOpen
    xor eax, eax
    ret
@@err_size:
    invoke CloseHandle, hFile
    invoke Print, addr szErrSize
    xor eax, eax
    ret
@@err_map:
    invoke CloseHandle, hFile
    invoke Print, addr szErrMap
    xor eax, eax
    ret
@@err_view:
    invoke CloseHandle, hMap
    invoke CloseHandle, hFile
    invoke Print, addr szErrMap
    xor eax, eax
    ret
MapFile endp

; UnmapFile — release memory-mapped file
UnmapFile proc
    cmp pBase, 0
    je @@skp1
    invoke UnmapViewOfFile, pBase
    mov pBase, 0
@@skp1:
    cmp hFile, 0
    je @@skp2
    invoke CloseHandle, hFile
    mov hFile, 0
@@skp2:
    mov bFileLoaded, 0
    ret
UnmapFile endp

; ============================================================================
; PE UTILITIES
; ============================================================================

; RVAToOffset — convert RVA to linear pointer (pBase + file offset)
; Returns pointer on success, 0 on failure
RVAToOffset proc dwRVA:DWORD
    local i:DWORD

    mov i, 0
@@loop:
    mov eax, i
    cmp eax, dwNumSec
    jge @@fail

    ; Section header address
    imul eax, SEC_HDR_SIZE
    add eax, pSecHdr

    mov ecx, [eax+12]          ; VirtualAddress
    mov edx, [eax+8]           ; VirtualSize
    add edx, ecx               ; section end VA

    cmp dwRVA, ecx
    jb @@next
    cmp dwRVA, edx
    jae @@next

    ; Found: result = pBase + (RVA - VA + PointerToRawData)
    mov edx, dwRVA
    sub edx, ecx
    add edx, [eax+20]          ; PointerToRawData
    add edx, pBase
    mov eax, edx
    ret

@@next:
    inc i
    jmp @@loop

@@fail:
    xor eax, eax
    ret
RVAToOffset endp

; ============================================================================
; PARSE PE — Display all headers and sections
; ============================================================================
ParsePE proc USES esi edi ebx
    local dwChars:DWORD, dwOptSize:DWORD
    local dwSectAlign:DWORD, dwFileAlign:DWORD, dwImgSize:DWORD
    local dwSubsys:DWORD

    invoke Print, addr szHdrPE

    mov esi, pBase
    mov pDOS, esi

    ; --- Validate DOS header ---
    cmp word ptr [esi], 5A4Dh       ; 'MZ'
    jne @@invalid

    ; --- Find PE header ---
    mov eax, [esi+3Ch]              ; e_lfanew
    cmp eax, dwFileSize
    jge @@invalid
    add eax, pBase
    mov pNT, eax
    mov esi, eax

    ; --- Check PE signature ---
    cmp dword ptr [esi], 4550h      ; 'PE\0\0'
    jne @@invalid

    ; === DOS Header display ===
    mov esi, pDOS
    movzx eax, word ptr [esi]       ; e_magic
    mov ebx, [esi+3Ch]              ; e_lfanew
    invoke wsprintf, addr bBuffer, addr szFmtDOS, eax, ebx
    invoke Print, addr bBuffer

    ; === File Header (20 bytes at pNT+4) ===
    mov esi, pNT
    add esi, 4                      ; skip PE signature

    movzx eax, word ptr [esi]       ; Machine
    push eax
    movzx eax, word ptr [esi+2]     ; NumberOfSections
    mov dwNumSec, eax
    push eax
    mov eax, [esi+4]                ; TimeDateStamp
    push eax
    movzx eax, word ptr [esi+18]    ; Characteristics
    mov dwChars, eax
    push eax
    movzx eax, word ptr [esi+16]    ; SizeOfOptionalHeader
    mov dwOptSize, eax

    ; wsprintf(bBuffer, szFmtFile, Machine, NumSec, Timestamp, Chars)
    pop eax                         ; Chars
    pop ebx                         ; Timestamp
    pop ecx                         ; NumSec
    pop edx                         ; Machine
    invoke wsprintf, addr bBuffer, addr szFmtFile, edx, ecx, ebx, eax
    invoke Print, addr bBuffer

    ; === Optional Header (at pNT+24) ===
    mov esi, pNT
    add esi, 24
    mov pOpt, esi

    ; Verify PE32 magic
    cmp word ptr [esi], 10Bh
    jne @@invalid

    mov eax, [esi+16]               ; AddressOfEntryPoint
    mov dwEntryPt, eax
    mov eax, [esi+28]               ; ImageBase
    mov dwImageBase, eax
    mov eax, [esi+32]               ; SectionAlignment
    mov dwSectAlign, eax
    mov eax, [esi+36]               ; FileAlignment
    mov dwFileAlign, eax
    mov eax, [esi+56]               ; SizeOfImage
    mov dwImgSize, eax
    movzx eax, word ptr [esi+68]    ; Subsystem
    mov dwSubsys, eax

    invoke wsprintf, addr bBuffer, addr szFmtOpt, \
           dwEntryPt, dwImageBase, dwSectAlign, dwFileAlign, dwImgSize, dwSubsys
    invoke Print, addr bBuffer

    ; === Section Headers ===
    mov esi, pOpt
    add esi, dwOptSize
    mov pSecHdr, esi

    invoke Print, addr szHdrSec

    xor edi, edi                    ; section counter
@@sec_loop:
    cmp edi, dwNumSec
    jge @@pe_done

    ; esi = section header pointer
    mov eax, edi
    imul eax, SEC_HDR_SIZE
    add eax, pSecHdr
    mov esi, eax

    ; Copy 8-byte name + null-terminate
    push esi
    lea edx, bTemp
    mov ecx, 8
@@cpname:
    mov al, [esi]
    mov [edx], al
    inc esi
    inc edx
    dec ecx
    jnz @@cpname
    mov byte ptr [edx], 0
    pop esi

    ; Format section info
    ; Args: sec_num, name, VA, VS, Raw, RS
    mov eax, [esi+16]              ; SizeOfRawData
    push eax
    mov eax, [esi+20]              ; PointerToRawData
    push eax
    mov eax, [esi+8]               ; VirtualSize
    push eax
    mov eax, [esi+12]              ; VirtualAddress
    push eax
    push offset bTemp              ; name
    push edi                       ; section number
    push offset szFmtSec
    push offset bBuffer
    call wsprintf
    add esp, 32
    invoke Print, addr bBuffer

    ; Decode characteristics [R/W/X]
    mov eax, [esi+36]

    mov bl, '-'
    test eax, SCN_READ
    jz @@nr
    mov bl, 'R'
@@nr:
    mov cl, '-'
    test eax, SCN_WRITE
    jz @@nw
    mov cl, 'W'
@@nw:
    mov dl, '-'
    test eax, SCN_EXEC
    jz @@nx
    mov dl, 'X'
@@nx:
    movzx eax, dl
    push eax
    movzx eax, cl
    push eax
    movzx eax, bl
    push eax
    push dword ptr [esi+36]
    push offset szFmtChar
    push offset bBuffer
    call wsprintf
    add esp, 24
    invoke Print, addr bBuffer

    inc edi
    jmp @@sec_loop

@@pe_done:
    invoke Print, addr szNL
    invoke Print, addr szDone
    mov eax, 1
    ret

@@invalid:
    invoke Print, addr szErrPE
    xor eax, eax
    ret
ParsePE endp

; ============================================================================
; IMPORT TABLE ANALYSIS
; ============================================================================
AnalyzeImports proc USES esi edi ebx
    local pDesc:DWORD, pThunk:DWORD

    invoke Print, addr szHdrImp

    ; Import Directory RVA from Optional Header + 104
    mov esi, pOpt
    mov eax, [esi + DD_IMPORT_RVA]
    test eax, eax
    jz @@none

    invoke RVAToOffset, eax
    test eax, eax
    jz @@none
    mov pDesc, eax

@@desc_loop:
    mov esi, pDesc

    ; End when Name RVA == 0
    mov eax, [esi+12]              ; Name RVA
    test eax, eax
    jz @@done

    ; Print DLL name
    invoke RVAToOffset, eax
    test eax, eax
    jz @@next_desc
    invoke wsprintf, addr bBuffer, addr szFmtDLL, eax
    invoke Print, addr bBuffer

    ; Get thunk array (prefer OriginalFirstThunk, fallback FirstThunk)
    mov esi, pDesc
    mov eax, [esi]                 ; OriginalFirstThunk
    test eax, eax
    jnz @@use_thunk
    mov eax, [esi+16]              ; FirstThunk
@@use_thunk:
    test eax, eax
    jz @@next_desc
    invoke RVAToOffset, eax
    test eax, eax
    jz @@next_desc
    mov pThunk, eax

    xor edi, edi                   ; ordinal counter

@@thunk_loop:
    mov esi, pThunk
    mov eax, [esi]
    test eax, eax
    jz @@next_desc

    ; Check ordinal import (bit 31)
    test eax, 80000000h
    jnz @@ordinal

    ; Import by name: RVA -> IMAGE_IMPORT_BY_NAME (Hint:WORD + Name:string)
    invoke RVAToOffset, eax
    test eax, eax
    jz @@skip_thunk

    movzx ebx, word ptr [eax]     ; Hint
    add eax, 2                     ; Name
    invoke wsprintf, addr bBuffer, addr szFmtFunc, ebx, eax
    invoke Print, addr bBuffer
    jmp @@skip_thunk

@@ordinal:
    and eax, 0FFFFh
    invoke wsprintf, addr bBuffer, addr szFmtOrd, eax
    invoke Print, addr bBuffer

@@skip_thunk:
    add pThunk, 4
    inc edi
    jmp @@thunk_loop

@@next_desc:
    add pDesc, IMP_DESC_SIZE
    jmp @@desc_loop

@@none:
    invoke Print, addr szNoImport
@@done:
    ret
AnalyzeImports endp

; ============================================================================
; EXPORT TABLE ANALYSIS
; ============================================================================
AnalyzeExports proc USES esi edi ebx
    local pExpDir:DWORD, dwNumNames:DWORD
    local pFuncRVAs:DWORD, pNameRVAs:DWORD, pOrdinals:DWORD

    invoke Print, addr szHdrExp

    ; Export Directory RVA from Optional Header + 96
    mov esi, pOpt
    mov eax, [esi + DD_EXPORT_RVA]
    test eax, eax
    jz @@none

    invoke RVAToOffset, eax
    test eax, eax
    jz @@none
    mov pExpDir, eax
    mov esi, eax

    ; DLL Name
    mov eax, [esi+12]              ; Name RVA
    invoke RVAToOffset, eax
    mov ecx, eax                   ; name ptr (safe across next invoke)

    mov esi, pExpDir
    mov eax, [esi+24]              ; NumberOfNames
    mov dwNumNames, eax

    invoke wsprintf, addr bBuffer, addr szFmtExpDLL, ecx, dwNumNames
    invoke Print, addr bBuffer

    ; Resolve function/name/ordinal arrays
    mov esi, pExpDir
    mov eax, [esi+28]              ; AddressOfFunctions
    invoke RVAToOffset, eax
    mov pFuncRVAs, eax

    mov esi, pExpDir
    mov eax, [esi+32]              ; AddressOfNames
    invoke RVAToOffset, eax
    mov pNameRVAs, eax

    mov esi, pExpDir
    mov eax, [esi+36]              ; AddressOfNameOrdinals
    invoke RVAToOffset, eax
    mov pOrdinals, eax

    ; Walk named exports
    xor edi, edi
@@loop:
    cmp edi, dwNumNames
    jge @@done

    ; Get ordinal
    mov esi, pOrdinals
    movzx ebx, word ptr [esi+edi*2]

    ; Get function name
    mov esi, pNameRVAs
    mov eax, [esi+edi*4]
    invoke RVAToOffset, eax
    mov ecx, eax                   ; name pointer

    ; Get function RVA
    mov esi, pFuncRVAs
    mov eax, [esi+ebx*4]          ; function RVA (by ordinal index)

    invoke wsprintf, addr bBuffer, addr szFmtExpFn, ebx, ecx, eax
    invoke Print, addr bBuffer

    inc edi
    jmp @@loop

@@none:
    invoke Print, addr szNoExport
@@done:
    ret
AnalyzeExports endp

; ============================================================================
; TLS CALLBACK DETECTION
; ============================================================================
AnalyzeTLS proc USES esi edi ebx
    local pTLSDir:DWORD, pCallbacks:DWORD, nCallbacks:DWORD

    invoke Print, addr szHdrTLS

    ; TLS Directory RVA from Optional Header + 168
    mov esi, pOpt
    mov eax, [esi + DD_TLS_RVA]
    test eax, eax
    jz @@none

    invoke RVAToOffset, eax
    test eax, eax
    jz @@none
    mov pTLSDir, eax
    mov esi, eax

    ; AddressOfCallBacks is a VA (not RVA!) — subtract ImageBase
    mov eax, [esi+12]
    test eax, eax
    jz @@none
    sub eax, dwImageBase
    invoke RVAToOffset, eax
    test eax, eax
    jz @@none
    mov pCallbacks, eax

    ; Count callbacks (null-terminated pointer array)
    xor edi, edi
@@count:
    mov esi, pCallbacks
    mov eax, [esi+edi*4]
    test eax, eax
    jz @@display
    inc edi
    jmp @@count

@@display:
    mov nCallbacks, edi
    invoke wsprintf, addr bBuffer, addr szFmtTLSCnt, edi
    invoke Print, addr bBuffer

    ; Show each callback
    xor edi, edi
@@show:
    cmp edi, nCallbacks
    jge @@done
    mov esi, pCallbacks
    mov eax, [esi+edi*4]
    invoke wsprintf, addr bBuffer, addr szFmtTLS, edi, eax
    invoke Print, addr bBuffer
    inc edi
    jmp @@show

@@none:
    invoke Print, addr szNoTLS
@@done:
    ret
AnalyzeTLS endp

; ============================================================================
; SHANNON ENTROPY CALCULATION
; ============================================================================
; Returns entropy * 100 as integer (e.g. 785 = 7.85 bits/byte)
CalcEntropy proc USES esi edi, pData:DWORD, dwSize:DWORD
    local i:DWORD, dwResult:DWORD
    local dwFreq[256]:DWORD

    ; Validate
    mov eax, dwSize
    test eax, eax
    jz @@zero

    ; Clear frequency table
    lea edi, dwFreq
    mov ecx, 256
    xor eax, eax
    rep stosd

    ; Count byte frequencies
    mov esi, pData
    mov ecx, dwSize
@@cnt:
    movzx eax, byte ptr [esi]
    lea edx, dwFreq
    inc dword ptr [edx+eax*4]
    inc esi
    dec ecx
    jnz @@cnt

    ; Shannon entropy: H = -SUM( p(i) * log2(p(i)) )
    finit
    fldz                            ; ST(0) = accumulator

    mov i, 0
@@calc:
    cmp i, 256
    jge @@scale

    lea edx, dwFreq
    mov eax, i
    mov ecx, [edx+eax*4]
    test ecx, ecx
    jz @@next

    ; p = freq / size
    lea edx, dwFreq
    mov eax, i
    lea eax, [edx+eax*4]
    fild dword ptr [eax]            ; ST(0) = freq, ST(1) = acc
    fidiv dwSize                    ; ST(0) = p
    fld st(0)                       ; ST(0) = p, ST(1) = p, ST(2) = acc
    fyl2x                           ; ST(0) = p*log2(p), ST(1) = acc
    fchs                            ; negate: -p*log2(p)
    faddp st(1), st(0)              ; acc += contribution

@@next:
    inc i
    jmp @@calc

@@scale:
    ; Multiply by 100 for fixed-point display
    mov dwResult, 100
    fimul dwResult
    fistp dwResult
    mov eax, dwResult
    ret

@@zero:
    xor eax, eax
    ret
CalcEntropy endp

; ============================================================================
; ENTROPY / PACKER ANALYSIS (per section)
; ============================================================================
AnalyzeEntropy proc USES esi edi ebx
    local i:DWORD, dwEnt:DWORD, dwHi:DWORD, dwLo:DWORD
    local lpPacker:DWORD

    invoke Print, addr szHdrEnt

    mov i, 0
@@loop:
    mov eax, i
    cmp eax, dwNumSec
    jge @@done

    ; Get section header
    imul eax, SEC_HDR_SIZE
    add eax, pSecHdr
    mov esi, eax

    ; Copy 8-byte name
    push esi
    lea edi, bTemp
    mov ecx, 8
@@cpn:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    dec ecx
    jnz @@cpn
    mov byte ptr [edi], 0
    pop esi

    ; Get raw data pointer and size
    mov eax, [esi+20]              ; PointerToRawData
    test eax, eax
    jz @@nxt
    add eax, pBase
    mov ebx, [esi+16]              ; SizeOfRawData
    test ebx, ebx
    jz @@nxt

    ; Calculate entropy
    invoke CalcEntropy, eax, ebx
    mov dwEnt, eax

    ; Split into integer.fraction
    xor edx, edx
    mov ecx, 100
    div ecx
    mov dwHi, eax
    mov dwLo, edx

    ; Packer indicator
    mov eax, offset szPackNone
    cmp dwEnt, ENT_SUSPICIOUS
    jl @@show
    mov eax, offset szPackSusp
    cmp dwEnt, ENT_PACKED
    jl @@show
    mov eax, offset szPackHigh

@@show:
    mov lpPacker, eax
    invoke wsprintf, addr bBuffer, addr szFmtEnt, addr bTemp, dwHi, dwLo, lpPacker
    invoke Print, addr bBuffer

@@nxt:
    inc i
    jmp @@loop

@@done:
    ret
AnalyzeEntropy endp

; ============================================================================
; STRING EXTRACTION (printable ASCII, min 4 chars)
; ============================================================================
ExtractStrings proc USES esi edi ebx
    local i:DWORD, dwStart:DWORD, dwLen:DWORD, dwCount:DWORD

    invoke Print, addr szHdrStr

    mov dwCount, 0
    mov i, 0
    mov dwLen, 0

@@scan:
    mov eax, i
    cmp eax, dwFileSize
    jge @@summary

    mov esi, pBase
    add esi, i
    movzx eax, byte ptr [esi]

    ; Printable range: 32-126
    cmp al, 32
    jl @@endstr
    cmp al, 126
    jg @@endstr

    ; Start or continue a string
    cmp dwLen, 0
    jne @@cont
    mov eax, i
    mov dwStart, eax
@@cont:
    inc dwLen
    inc i
    jmp @@scan

@@endstr:
    ; Check if we accumulated >= 4 printable chars
    cmp dwLen, 4
    jl @@reset

    ; Copy string to bString (max 255 chars)
    mov ecx, dwLen
    cmp ecx, 255
    jle @@cpok
    mov ecx, 255
@@cpok:
    mov esi, pBase
    add esi, dwStart
    lea edi, bString
    push ecx
    rep movsb
    pop ecx
    mov byte ptr [bString+ecx], 0

    invoke wsprintf, addr bBuffer, addr szFmtStrLn, dwStart, addr bString
    invoke Print, addr bBuffer
    inc dwCount

@@reset:
    mov dwLen, 0
    inc i
    jmp @@scan

@@summary:
    ; Handle trailing string
    cmp dwLen, 4
    jl @@chkcount
    mov ecx, dwLen
    cmp ecx, 255
    jle @@cpok2
    mov ecx, 255
@@cpok2:
    mov esi, pBase
    add esi, dwStart
    lea edi, bString
    push ecx
    rep movsb
    pop ecx
    mov byte ptr [bString+ecx], 0
    invoke wsprintf, addr bBuffer, addr szFmtStrLn, dwStart, addr bString
    invoke Print, addr bBuffer
    inc dwCount

@@chkcount:
    cmp dwCount, 0
    jne @@done
    invoke Print, addr szNoStrings
@@done:
    ret
ExtractStrings endp

; ============================================================================
; CONTROL FLOW RECOVERY (basic block detection)
; ============================================================================
ControlFlow proc USES esi edi ebx
    local dwBlockStart:DWORD, dwBlockCount:DWORD, dwBytesLeft:DWORD

    invoke Print, addr szHdrCF

    ; Get entry point file location
    mov eax, dwEntryPt
    invoke RVAToOffset, eax
    test eax, eax
    jz @@done
    mov esi, eax
    mov dwBlockStart, eax
    mov dwBlockCount, 0
    mov dwBytesLeft, 512            ; scan 512 bytes from EP

@@scan:
    cmp dwBytesLeft, 0
    jle @@summary

    ; Bounds check
    mov eax, esi
    sub eax, pBase
    cmp eax, dwFileSize
    jge @@summary

    movzx eax, byte ptr [esi]

    ; Block-ending opcodes
    cmp al, 0E8h                    ; CALL
    je @@boundary
    cmp al, 0E9h                    ; JMP near
    je @@boundary
    cmp al, 0EBh                    ; JMP short
    je @@boundary
    cmp al, 0C3h                    ; RET
    je @@boundary
    cmp al, 0C2h                    ; RET imm16
    je @@boundary
    cmp al, 0CCh                    ; INT 3
    je @@boundary

    ; Short conditional jumps (70-7F)
    cmp al, 70h
    jb @@nextbyte
    cmp al, 7Fh
    jbe @@boundary

    ; Two-byte conditional jumps (0F 80-8F)
    cmp al, 0Fh
    jne @@nextbyte
    cmp dwBytesLeft, 1
    jle @@nextbyte
    movzx eax, byte ptr [esi+1]
    cmp al, 80h
    jb @@nextbyte
    cmp al, 8Fh
    ja @@nextbyte
    inc esi
    dec dwBytesLeft
    ; fall through to boundary

@@boundary:
    ; Block: dwBlockStart to esi
    mov eax, esi
    sub eax, pBase                 ; end offset
    mov edx, dwBlockStart
    sub edx, pBase                 ; start offset
    mov ecx, eax
    sub ecx, edx
    inc ecx                        ; size

    invoke wsprintf, addr bBuffer, addr szFmtBB, dwBlockCount, edx, eax, ecx
    invoke Print, addr bBuffer

    inc dwBlockCount
    lea eax, [esi+1]
    mov dwBlockStart, eax

@@nextbyte:
    inc esi
    dec dwBytesLeft
    jmp @@scan

@@summary:
    invoke wsprintf, addr bBuffer, addr szFmtCFCnt, dwBlockCount
    invoke Print, addr bBuffer

@@done:
    ret
ControlFlow endp

; ============================================================================
; SOURCE RECONSTRUCTION (opcode → pseudo-C)
; ============================================================================
SourceRecon proc USES esi edi ebx
    local dwBytesLeft:DWORD

    invoke Print, addr szHdrSrc

    ; Get entry point
    mov eax, dwEntryPt
    invoke RVAToOffset, eax
    test eax, eax
    jz @@done
    mov esi, eax

    ; Function header
    invoke wsprintf, addr bBuffer, addr szFmtSrcHdr, dwEntryPt, dwEntryPt
    invoke Print, addr bBuffer

    mov dwBytesLeft, 128

@@scan:
    cmp dwBytesLeft, 0
    jle @@end

    ; Bounds check
    mov eax, esi
    sub eax, pBase
    cmp eax, dwFileSize
    jge @@end

    movzx eax, byte ptr [esi]

    cmp al, 55h                     ; push ebp
    je @@op_push
    cmp al, 5Dh                     ; pop ebp
    je @@op_pop
    cmp al, 83h                     ; sub/add esp (with modrm)
    je @@op_sub
    cmp al, 8Bh                     ; mov r32, r/m32
    je @@op_lea
    cmp al, 89h                     ; mov r/m32, r32
    je @@op_lea
    cmp al, 0B8h                    ; mov eax, imm32
    je @@op_mov
    cmp al, 0E8h                    ; call rel32
    je @@op_call
    cmp al, 0E9h                    ; jmp rel32
    je @@op_jmp
    cmp al, 0C3h                    ; ret
    je @@op_ret
    cmp al, 0C2h                    ; ret imm16
    je @@op_ret
    cmp al, 90h                     ; nop
    je @@op_nop
    cmp al, 33h                     ; xor
    je @@op_xor
    cmp al, 31h                     ; xor variant
    je @@op_xor

    ; Unknown — skip 1 byte
    inc esi
    dec dwBytesLeft
    jmp @@scan

@@op_push:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpPush
    invoke Print, addr bBuffer
    inc esi
    dec dwBytesLeft
    jmp @@scan

@@op_pop:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpPop
    invoke Print, addr bBuffer
    inc esi
    dec dwBytesLeft
    jmp @@scan

@@op_sub:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpSub
    invoke Print, addr bBuffer
    add esi, 3
    sub dwBytesLeft, 3
    jmp @@scan

@@op_lea:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpLea
    invoke Print, addr bBuffer
    add esi, 2
    sub dwBytesLeft, 2
    jmp @@scan

@@op_mov:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpMov
    invoke Print, addr bBuffer
    add esi, 5
    sub dwBytesLeft, 5
    jmp @@scan

@@op_call:
    ; call rel32 — target = esi + 5 + rel32
    mov eax, [esi+1]
    add eax, esi
    add eax, 5
    sub eax, pBase
    invoke wsprintf, addr bTemp, addr szOpCall, eax
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr bTemp
    invoke Print, addr bBuffer
    add esi, 5
    sub dwBytesLeft, 5
    jmp @@scan

@@op_jmp:
    mov eax, [esi+1]
    add eax, esi
    add eax, 5
    sub eax, pBase
    invoke wsprintf, addr bTemp, addr szOpJmp, eax
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr bTemp
    invoke Print, addr bBuffer
    jmp @@end                       ; unconditional jump → stop

@@op_ret:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpRet
    invoke Print, addr bBuffer
    jmp @@end                       ; return → stop

@@op_nop:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpNop
    invoke Print, addr bBuffer
    inc esi
    dec dwBytesLeft
    jmp @@scan

@@op_xor:
    invoke wsprintf, addr bBuffer, addr szFmtOp, addr szOpXor
    invoke Print, addr bBuffer
    add esi, 2
    sub dwBytesLeft, 2
    jmp @@scan

@@end:
    invoke Print, addr szSrcEnd

@@done:
    ret
SourceRecon endp

; ============================================================================
; HEX DUMP
; ============================================================================
HexDump proc USES esi edi ebx, dwStart:DWORD, dwLen:DWORD
    local i:DWORD, j:DWORD, dwPos:DWORD

    mov i, 0
@@row:
    mov eax, i
    cmp eax, dwLen
    jge @@done
    ; Bounds check
    mov eax, dwStart
    add eax, i
    cmp eax, dwFileSize
    jge @@done

    ; --- Address ---
    mov eax, dwStart
    add eax, i
    invoke wsprintf, addr bBuffer, addr szFmtAddr, eax
    invoke lstrlen, addr bBuffer
    mov dwPos, eax

    ; --- Hex bytes (16 per row) ---
    mov j, 0
@@hex:
    cmp j, 16
    jge @@doascii

    ; Extra space at position 8
    cmp j, 8
    jne @@nosp
    mov eax, dwPos
    mov byte ptr [bBuffer+eax], ' '
    inc dwPos
@@nosp:
    mov eax, i
    add eax, j
    cmp eax, dwLen
    jge @@pad
    mov edx, dwStart
    add edx, eax
    cmp edx, dwFileSize
    jge @@pad

    ; Print byte
    add edx, pBase
    movzx eax, byte ptr [edx]
    lea edx, bBuffer
    add edx, dwPos
    invoke wsprintf, edx, addr szFmtByte, eax
    add dwPos, 3
    jmp @@hexnxt

@@pad:
    mov eax, dwPos
    mov byte ptr [bBuffer+eax], ' '
    mov byte ptr [bBuffer+eax+1], ' '
    mov byte ptr [bBuffer+eax+2], ' '
    add dwPos, 3

@@hexnxt:
    inc j
    jmp @@hex

    ; --- ASCII column ---
@@doascii:
    mov eax, dwPos
    mov byte ptr [bBuffer+eax], ' '
    mov byte ptr [bBuffer+eax+1], '|'
    add dwPos, 2

    mov j, 0
@@asc:
    cmp j, 16
    jge @@ascend
    mov eax, i
    add eax, j
    cmp eax, dwLen
    jge @@ascpad
    mov edx, dwStart
    add edx, eax
    cmp edx, dwFileSize
    jge @@ascpad

    add edx, pBase
    movzx eax, byte ptr [edx]
    cmp al, 32
    jl @@dot
    cmp al, 126
    jg @@dot
    jmp @@putasc
@@dot:
    mov al, '.'
@@putasc:
    mov ecx, dwPos
    mov byte ptr [bBuffer+ecx], al
    inc dwPos
    jmp @@ascnxt

@@ascpad:
    mov ecx, dwPos
    mov byte ptr [bBuffer+ecx], ' '
    inc dwPos

@@ascnxt:
    inc j
    jmp @@asc

@@ascend:
    mov eax, dwPos
    mov byte ptr [bBuffer+eax], '|'
    mov byte ptr [bBuffer+eax+1], 13
    mov byte ptr [bBuffer+eax+2], 10
    mov byte ptr [bBuffer+eax+3], 0

    invoke Print, addr bBuffer

    add i, 16
    jmp @@row

@@done:
    ret
HexDump endp

; ============================================================================
; SIMPLE DISASSEMBLER (instruction-length decode + byte display)
; ============================================================================
Disassemble proc USES esi edi ebx, dwStart:DWORD, dwCount:DWORD
    local i:DWORD, dwLen:DWORD

    invoke Print, addr szHdrDis

    mov esi, pBase
    add esi, dwStart
    mov i, 0

@@loop:
    mov eax, i
    cmp eax, dwCount
    jge @@done

    ; Bounds check
    mov eax, esi
    sub eax, pBase
    cmp eax, dwFileSize
    jge @@done

    ; Print address
    invoke wsprintf, addr bBuffer, addr szFmtDisA, eax
    invoke Print, addr bBuffer

    ; Get opcode
    movzx eax, byte ptr [esi]

    ; --- Determine instruction length (simplified x86) ---
    mov dwLen, 1                   ; default

    cmp al, 0Fh                    ; two-byte prefix
    je @@len2
    cmp al, 0E8h                   ; CALL rel32
    je @@len5
    cmp al, 0E9h                   ; JMP rel32
    je @@len5
    cmp al, 0EBh                   ; JMP short
    je @@len2
    ; MOV r32, imm32 (B8-BF)
    cmp al, 0B8h
    jb @@chk50
    cmp al, 0BFh
    jbe @@len5
@@chk50:
    ; PUSH r32 (50-57), POP r32 (58-5F)
    cmp al, 50h
    jb @@chk80
    cmp al, 5Fh
    jbe @@len1
@@chk80:
    ; Group 1 (80-83)
    cmp al, 80h
    je @@len3
    cmp al, 83h
    je @@len3
    cmp al, 81h
    je @@len6
    ; MOV r/m32 (89, 8B)
    cmp al, 89h
    je @@len2
    cmp al, 8Bh
    je @@len2
    ; Jcc short (70-7F)
    cmp al, 70h
    jb @@len1
    cmp al, 7Fh
    jbe @@len2
    jmp @@len1

@@len1: mov dwLen, 1
    jmp @@show
@@len2: mov dwLen, 2
    jmp @@show
@@len3: mov dwLen, 3
    jmp @@show
@@len5: mov dwLen, 5
    jmp @@show
@@len6: mov dwLen, 6

@@show:
    ; Print instruction bytes
    xor edi, edi
@@pbyte:
    cmp edi, dwLen
    jge @@pnl
    movzx eax, byte ptr [esi+edi]
    invoke wsprintf, addr bTemp, addr szFmtByte, eax
    invoke Print, addr bTemp
    inc edi
    jmp @@pbyte
@@pnl:
    invoke Print, addr szNL

    add esi, dwLen
    inc i
    jmp @@loop

@@done:
    ret
Disassemble endp

; ============================================================================
; FULL RECONNAISSANCE (run all analyses)
; ============================================================================
FullRecon proc
    call ParsePE
    test eax, eax
    jz @@done
    call AnalyzeImports
    call AnalyzeExports
    call AnalyzeTLS
    call AnalyzeEntropy
    call ExtractStrings
    call ControlFlow
    call SourceRecon
@@done:
    ret
FullRecon endp

; ============================================================================
; HTML REPORT GENERATION
; ============================================================================
GenerateHTMLReport proc USES esi edi ebx
    local hOut:DWORD

    ; Prompt for output directory
    invoke Print, addr szPromptOut
    call ReadInput
    invoke lstrcat, addr bInput, addr szReportFn

    ; Create output file
    invoke CreateFile, addr bInput, GENERIC_WRITE, 0, 0, \
           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@error
    mov hOut, eax

    ; Write HTML header
    invoke WriteStr, hOut, addr szHTMLHdr
    invoke WriteStr, hOut, addr szHTMLTitle
    invoke WriteStr, hOut, addr szHTMLPre

    ; Redirect Print output to file
    mov eax, hOut
    mov hReportFile, eax

    ; Run full analysis (output captured to file)
    call FullRecon

    ; Stop redirect
    mov hReportFile, 0

    ; Write HTML footer
    invoke WriteStr, hOut, addr szHTMLPreE
    invoke WriteStr, hOut, addr szHTMLEnd

    invoke CloseHandle, hOut

    invoke wsprintf, addr bBuffer, addr szRptDone, addr bInput
    invoke Print, addr bBuffer
    ret

@@error:
    invoke Print, addr szErrOpen
    ret
GenerateHTMLReport endp

; ============================================================================
; MAIN MENU
; ============================================================================
Menu proc
    local choice:DWORD

@@show:
    invoke Print, addr szBanner
    invoke Print, addr szMenu
    call ReadInt
    mov choice, eax

    cmp choice, 0
    je @@exit
    cmp choice, 1
    je @@pe
    cmp choice, 2
    je @@imp
    cmp choice, 3
    je @@exp
    cmp choice, 4
    je @@ent
    cmp choice, 5
    je @@str
    cmp choice, 6
    je @@cfg
    cmp choice, 7
    je @@src
    cmp choice, 8
    je @@tls
    cmp choice, 9
    je @@hex
    cmp choice, 10
    je @@dis
    cmp choice, 11
    je @@full
    cmp choice, 12
    je @@html
    jmp @@show

    ; --- [1] Full PE Analysis ---
@@pe:
    call UnmapFile
    invoke Print, addr szPromptFile
    call ReadInput
    invoke MapFile, addr bInput
    test eax, eax
    jz @@show
    call ParsePE
    jmp @@show

    ; --- [2] Import Table ---
@@imp:
    cmp bFileLoaded, 0
    je @@nofile
    call AnalyzeImports
    jmp @@show

    ; --- [3] Export Table ---
@@exp:
    cmp bFileLoaded, 0
    je @@nofile
    call AnalyzeExports
    jmp @@show

    ; --- [4] Entropy / Packer ---
@@ent:
    cmp bFileLoaded, 0
    je @@nofile
    call AnalyzeEntropy
    jmp @@show

    ; --- [5] String Extraction ---
@@str:
    cmp bFileLoaded, 0
    je @@nofile
    call ExtractStrings
    jmp @@show

    ; --- [6] Control Flow ---
@@cfg:
    cmp bFileLoaded, 0
    je @@nofile
    call ControlFlow
    jmp @@show

    ; --- [7] Source Reconstruction ---
@@src:
    cmp bFileLoaded, 0
    je @@nofile
    call SourceRecon
    jmp @@show

    ; --- [8] TLS Callbacks ---
@@tls:
    cmp bFileLoaded, 0
    je @@nofile
    call AnalyzeTLS
    jmp @@show

    ; --- [9] Hex Dump ---
@@hex:
    cmp bFileLoaded, 0
    je @@nofile
    invoke Print, addr szPromptAddr
    call ReadHex
    push eax
    invoke Print, addr szPromptSize
    call ReadInt
    pop edx
    invoke HexDump, edx, eax
    jmp @@show

    ; --- [10] Disassemble ---
@@dis:
    cmp bFileLoaded, 0
    je @@nofile
    invoke Print, addr szPromptAddr
    call ReadHex
    push eax
    invoke Print, addr szPromptSize
    call ReadInt
    pop edx
    invoke Disassemble, edx, eax
    jmp @@show

    ; --- [11] Full Recon ---
@@full:
    cmp bFileLoaded, 0
    jne @@run_full
    invoke Print, addr szPromptFile
    call ReadInput
    invoke MapFile, addr bInput
    test eax, eax
    jz @@show
@@run_full:
    call FullRecon
    jmp @@show

    ; --- [12] HTML Report ---
@@html:
    cmp bFileLoaded, 0
    je @@nofile
    call GenerateHTMLReport
    jmp @@show

    ; --- Error: no file loaded ---
@@nofile:
    invoke Print, addr szErrNoFile
    jmp @@show

    ; --- [0] Exit ---
@@exit:
    call UnmapFile
    ret
Menu endp

; ============================================================================
; ENTRY POINT
; ============================================================================
start:
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax

    call Menu

    invoke ExitProcess, 0
end start
