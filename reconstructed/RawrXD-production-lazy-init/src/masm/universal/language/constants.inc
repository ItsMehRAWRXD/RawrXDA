; ===============================================================================
; Universal Language Constants - Complete Language Support
; Maps all 48+ compiler sources to unified language IDs
; ===============================================================================

; Language Type Constants (Extended from 16 to 48+)
LANG_UNKNOWN        EQU 0
LANG_ASM            EQU 1
LANG_C              EQU 2
LANG_CPP            EQU 3
LANG_RUST           EQU 4
LANG_JAVASCRIPT     EQU 5
LANG_PYTHON         EQU 6
LANG_GO             EQU 7
LANG_JAVA           EQU 8
LANG_CSHARP         EQU 9
LANG_PHP            EQU 10
LANG_RUBY           EQU 11
LANG_SWIFT          EQU 12
LANG_KOTLIN         EQU 13
LANG_DART           EQU 14
LANG_TYPESCRIPT     EQU 15
LANG_LUA            EQU 16
LANG_ADA            EQU 17
LANG_CADENCE        EQU 18
LANG_CARBON         EQU 19
LANG_CLOJURE        EQU 20
LANG_COBOL          EQU 21
LANG_CRYSTAL        EQU 22
LANG_DELPHI         EQU 23
LANG_ELIXIR         EQU 24
LANG_ERLANG         EQU 25
LANG_FSHARP         EQU 26
LANG_FORTRAN        EQU 27
LANG_HASKELL        EQU 28
LANG_JAI            EQU 29
LANG_JULIA          EQU 30
LANG_LLVM_IR        EQU 31
LANG_MATLAB         EQU 32
LANG_MOTOKO         EQU 33
LANG_MOVE           EQU 34
LANG_NIM            EQU 35
LANG_OCAML          EQU 36
LANG_ODIN           EQU 37
LANG_PASCAL         EQU 38
LANG_PERL           EQU 39
LANG_R              EQU 40
LANG_SCALA          EQU 41
LANG_SOLIDITY       EQU 42
LANG_VBNET          EQU 43
LANG_V              EQU 44
LANG_VYPER          EQU 45
LANG_WEBASSEMBLY    EQU 46
LANG_ZIG            EQU 47

; Maximum supported languages
MAX_LANGUAGES       EQU 48

; File Extension Mapping Structure
LanguageExtension STRUCT
    extension       QWORD ?     ; Pointer to extension string
    languageId      DWORD ?     ; Language ID constant
    padding         DWORD ?     ; Alignment padding
LanguageExtension ENDS

; Compiler Function Mapping Structure
CompilerMapping STRUCT
    languageId      DWORD ?     ; Language ID constant
    compilerProc    QWORD ?     ; Pointer to compiler procedure
    sourceFile      QWORD ?     ; Pointer to source filename
CompilerMapping ENDS

; Language Information Structure
LanguageInfo STRUCT
    languageId      DWORD ?     ; Language ID constant
    name            QWORD ?     ; Pointer to language name
    extension       QWORD ?     ; Pointer to primary extension
    description     QWORD ?     ; Pointer to description
    compilerProc    QWORD ?     ; Pointer to compiler procedure
    cliSupported    DWORD ?     ; CLI support flag
    qtSupported     DWORD ?     ; Qt GUI support flag
    scaffolder      QWORD ?     ; Pointer to scaffolder procedure
LanguageInfo ENDS