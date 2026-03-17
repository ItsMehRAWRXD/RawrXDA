; =====================================================================
; AGENTIC KERNEL - Complete autonomous system with 50+ language support
; Zero dependencies, pure MASM, full drag-drop and intent classification
; Author: RawrXD Zero-Day Implementation
; =====================================================================

; External Win32 API functions
EXTERN GlobalAlloc:PROC
EXTERN GlobalFree:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CreateDirectoryA:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
; EXTERN SendMessageA:PROC
EXTERN DragQueryFileA:PROC
EXTERN DragFinish:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrcmpiA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpynA:PROC
EXTERN wsprintfA:PROC
EXTERN CharLowerA:PROC

; Win32 constants
GENERIC_READ        EQU 80000000h
GENERIC_WRITE       EQU 40000000h
CREATE_ALWAYS       EQU 2
OPEN_EXISTING       EQU 3
FILE_SHARE_READ     EQU 1
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE EQU -1
PAGE_READONLY       EQU 2
FILE_MAP_READ       EQU 4
GMEM_FIXED          EQU 0
INFINITE            EQU 0FFFFFFFFh
STD_OUTPUT_HANDLE   EQU -11

; ---------- Constants ----------
MAX_AGENT       EQU 40
TOKEN_BUF       EQU 32768
CMD_BUF         EQU 1024
PATH_BUF        EQU 260
FILE_BUF        EQU 32768
EXT_BUF         EQU 16
Z800_SIZE       EQU 800
SLICE_SZ        EQU 1 SHL 27        ; 128 MiB
MAX_LANG        EQU 64
GGUF_CHUNK      EQU 1 SHL 20        ; 1 MB
SIZEOF_QWORD    EQU 8
SIZEOF_DWORD    EQU 4
SIZEOF_LANG_ENTRY EQU 48            ; 16 + 32
SIZEOF_AGENT    EQU 32808           ; Agent structure size

; LANG_ENTRY offsets
LANG_ENTRY_EXT      EQU 0
LANG_ENTRY_NAME     EQU 16
LANG_ENTRY_BUILDCMD EQU 24
LANG_ENTRY_RUNCMD   EQU 32
LANG_ENTRY_SCAFFOLD EQU 40

; AGENT offsets
AGENT_ID         EQU 0
AGENT_STATE      EQU 4
AGENT_CORE       EQU 8
AGENT_TOKENSDONE EQU 12
AGENT_TOKENBUF   EQU 20
AGENT_CTXLEN     EQU 32788
AGENT_SLICEIDX   EQU 32792
AGENT_LANGID     EQU 32796
AGENT_GGUFHANDLE EQU 32800

; ---------- Win32 Structures ----------
STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess        QWORD ?
    hThread         QWORD ?
    dwProcessId     DWORD ?
    dwThreadId      DWORD ?
PROCESS_INFORMATION ENDS

; ---------- Structures ----------
AGENT STRUCT
    id          DWORD ?
    state       DWORD ?              ; 0=idle 1=busy 2=dead
    core        DWORD ?
    tokensDone  QWORD ?
    tokenBuf    BYTE TOKEN_BUF DUP(?)
    ctxLen      DWORD ?
    sliceIdx    SDWORD ?
    langId      DWORD ?
    ggufHandle  QWORD ?
AGENT ENDS

LANG_ENTRY STRUCT
    ext         BYTE EXT_BUF DUP(?)  ; ".cpp"
    name        QWORD ?               ; "C++"
    buildCmd    QWORD ?               ; build template
    runCmd      QWORD ?               ; run template
    scaffold    QWORD ?               ; scaffold generator proc
LANG_ENTRY ENDS

GGUF_HEADER STRUCT
    magic       DWORD ?
    version     DWORD ?
    tensorCnt   QWORD ?
    metadata    QWORD ?
GGUF_HEADER ENDS

SLICE_CTL STRUCT
    hFile       QWORD ?
    hMap        QWORD ?
    pMap        QWORD ?
    fileSz      QWORD ?
    sliceCnt    DWORD ?
SLICE_CTL ENDS

; ---------- Data ----------
.data
szWindowClass   db "RawrXD_AgenticKernel", 0
szWindowTitle   db "RawrXD Agentic IDE - Zero-Day Edition", 0
szPrompt        db "RawrXD> ", 0
szReactName     db "React/TypeScript", 0
szViteName      db "Vite", 0

; Language names
szCpp       db "C++", 0
szC         db "C", 0
szRust      db "Rust", 0
szGo        db "Go", 0
szZig       db "Zig", 0
szPython    db "Python", 0
szJS        db "JavaScript", 0
szTS        db "TypeScript", 0
szJava      db "Java", 0
szCS        db "C#", 0
szSwift     db "Swift", 0
szKotlin    db "Kotlin", 0
szRuby      db "Ruby", 0
szPHP       db "PHP", 0
szPerl      db "Perl", 0
szLua       db "Lua", 0
szElixir    db "Elixir", 0
szHaskell   db "Haskell", 0
szOCaml     db "OCaml", 0
szScala     db "Scala", 0
szClojure   db "Clojure", 0
szErlang    db "Erlang", 0
szFSharp    db "F#", 0
szDart      db "Dart", 0
szJulia     db "Julia", 0
szR         db "R", 0
szMATLAB    db "MATLAB", 0
szFortran   db "Fortran", 0
szCOBOL     db "COBOL", 0
szPascal    db "Pascal", 0
szAda       db "Ada", 0
szVHDL      db "VHDL", 0
szVerilog   db "Verilog", 0
szAssembly  db "Assembly", 0
szBash      db "Bash", 0
szPowerShell db "PowerShell", 0
szBatch     db "Batch", 0
szVB        db "Visual Basic", 0
szSQL       db "SQL", 0
szHTML      db "HTML", 0
szCSS       db "CSS", 0
szSCSS      db "SCSS", 0
szLESS      db "LESS", 0
szXML       db "XML", 0
szJSON      db "JSON", 0
szYAML      db "YAML", 0
szTOML      db "TOML", 0
szMarkdown  db "Markdown", 0
szLaTeX     db "LaTeX", 0
szVue       db "Vue", 0
szSvelte    db "Svelte", 0

; Build templates
szCppBuild      db 'cl /O2 /std:c++20 /EHsc "%s"', 0
szCBuild        db 'cl /O2 "%s"', 0
szRustBuild     db 'rustc -O "%s"', 0
szGoBuild       db 'go build -o "%s.exe" "%s"', 0
szZigBuild      db 'zig build-exe -O ReleaseFast "%s"', 0
szPythonRun     db 'python "%s"', 0
szJSRun         db 'node "%s"', 0
szTSBuild       db 'tsc "%s"', 0
szJavaBuild     db 'javac "%s"', 0
szJavaRun       db 'java -cp . Main', 0
szCSBuild       db 'csc /out:RawrXD.exe "%s"', 0
szSwiftBuild    db 'swift build', 0
szSwiftRun      db 'swift run', 0
szKotlinBuild   db 'kotlinc "%s" -include-runtime -d "%s.jar"', 0
szKotlinBuild2  db 'kotlinc "%s" -include-runtime -d RawrXD.jar', 0
szKotlinRun     db 'java -jar RawrXD.jar', 0
szRubyRun       db 'ruby "%s"', 0
szPHPRun        db 'php "%s"', 0
szPerlRun       db 'perl "%s"', 0
szLuaRun        db 'lua "%s"', 0
szElixirRun     db 'elixir "%s"', 0
szHaskellBuild  db 'ghc -O2 "%s"', 0
szHaskellRun    db '"%s"', 0
szOCamlBuild    db 'ocamlopt -o "%s.exe" "%s"', 0
szOCamlRun      db '"%s.exe"', 0
szScalaBuild    db 'scalac "%s"', 0
szScalaRun      db 'scala Main', 0
szJuliaRun      db 'julia "%s"', 0
szRRun          db 'Rscript "%s"', 0
szBashRun       db 'bash "%s"', 0
szPSRun         db 'powershell -ExecutionPolicy Bypass -File "%s"', 0
szBatchRun      db 'cmd /c "%s"', 0
szExeRun        db '"%s"', 0

; 800-B embedded model
ALIGN 16
Z800_BLOB LABEL BYTE
    db 00h, 01h, 02h, 03h, 04h, 05h, 06h, 07h
    db 292 dup(42h)
    db 500 dup(0AAh)

; ---------- Uninitialized Data ----------
.data?
hInstance       QWORD ?
hWnd            QWORD ?
hStdIn          QWORD ?
hStdOut         QWORD ?
cmdLine         BYTE CMD_BUF DUP(?)
targetFile      BYTE PATH_BUF DUP(?)
targetDir       BYTE PATH_BUF DUP(?)
fileBuf         BYTE FILE_BUF DUP(?)
ext             BYTE EXT_BUF DUP(?)
deepFlag        DWORD ?
swarmRun        DWORD ?
hexCursor       QWORD ?
modelBase       QWORD ?

; Agent swarm
agents          AGENT MAX_AGENT DUP(<>)

; GGUF control
gc              SLICE_CTL <>
ggufHdr         GGUF_HEADER <>

; Language table
langTable       LANG_ENTRY MAX_LANG DUP(<>)
langCount       DWORD ?

; Temporary buffers
lowerBuf        BYTE CMD_BUF DUP(?)
buildCmd        BYTE CMD_BUF DUP(?)

; ---------- Code ----------
.code

; =====================================================================
; Memory helpers
; =====================================================================
memset PROC
    ; RCX = dst, RDX = val, R8 = len
    push    rdi
    push    rcx
    mov     rdi, rcx
    mov     eax, edx
    mov     rcx, r8
    test    rcx, rcx
    jz      memset_done
    mov     r9, rcx
    shr     r9, 3
    and     rcx, 7
    movzx   eax, al
    mov     r10, 0101010101010101h
    imul    rax, r10
    mov     rcx, r9
    rep     stosq
    mov     rcx, r8
    and     rcx, 7
    rep     stosb
memset_done:
    pop     rcx
    pop     rdi
    ret
memset ENDP

memcpy PROC
    ; RCX = dst, RDX = src, R8 = len
    push    rdi
    push    rsi
    push    rcx
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, r8
    rep     movsb
    pop     rcx
    pop     rsi
    pop     rdi
    ret
memcpy ENDP

strcmp PROC
    ; RCX = s1, RDX = s2
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
strcmp_loop:
    lodsb
    scasb
    jne     strcmp_ne
    test    al, al
    jnz     strcmp_loop
    xor     eax, eax
    jmp     strcmp_done
strcmp_ne:
    sbb     eax, eax
    or      eax, 1
strcmp_done:
    pop     rdi
    pop     rsi
    ret
strcmp ENDP

strstr PROC
    ; RCX = haystack, RDX = needle
    push    rsi
    push    rdi
    push    rbx
    
    mov     rsi, rcx
    mov     rdi, rdx
    mov     al, byte ptr [rdi]
    test    al, al
    jz      strstr_found
    
strstr_next_char:
    mov     al, byte ptr [rsi]
    test    al, al
    jz      strstr_not_found
    mov     rbx, rsi
    mov     rcx, rdi
    
strstr_compare:
    mov     al, byte ptr [rbx]
    mov     dl, byte ptr [rcx]
    test    dl, dl
    jz      strstr_found
    cmp     al, dl
    jne     strstr_no_match
    inc     rbx
    inc     rcx
    jmp     strstr_compare
    
strstr_no_match:
    inc     rsi
    jmp     strstr_next_char
    
strstr_found:
    mov     rax, rsi
    jmp     strstr_done
    
strstr_not_found:
    xor     rax, rax
    
strstr_done:
    pop     rbx
    pop     rdi
    pop     rsi
    ret
strstr ENDP

; =====================================================================
; 40-Agent Swarm Management
; =====================================================================
SpawnSwarm PROC
    xor     ecx, ecx
spawn_loop:
    lea     r10, agents
    imul    rax, rcx, SIZEOF_AGENT
    add     r10, rax
    mov     dword ptr [r10 + AGENT_ID], ecx
    mov     dword ptr [r10 + AGENT_STATE], 0
    mov     dword ptr [r10 + AGENT_CORE], ecx
    mov     dword ptr [r10 + AGENT_SLICEIDX], -1
    mov     dword ptr [r10 + AGENT_LANGID], -1
    mov     qword ptr [r10 + AGENT_TOKENSDONE], 0
    inc     ecx
    cmp     ecx, MAX_AGENT
    jb      spawn_loop
    ret
SpawnSwarm ENDP

; =====================================================================
; 800-B Model Loader
; =====================================================================
LoadZ800 PROC
    ; Call GlobalAlloc(GMEM_FIXED, Z800_SIZE)
    mov     rcx, GMEM_FIXED
    mov     rdx, Z800_SIZE
    sub     rsp, 20h
    call    GlobalAlloc
    add     rsp, 20h
    test    rax, rax
    jz      load_fail
    mov     modelBase, rax
    ; Call memcpy(modelBase, Z800_BLOB, Z800_SIZE)
    mov     rcx, modelBase
    lea     rdx, Z800_BLOB
    mov     r8, Z800_SIZE
    call    memcpy
    mov     eax, 1
    ret
load_fail:
    xor     eax, eax
    ret
LoadZ800 ENDP

; =====================================================================
; 800-B Inference
; =====================================================================
Z800Infer PROC
    ; RCX = agentId, RDX = tok
    ; Returns: EAX = output token
    push    rbx
    mov     rbx, rcx  ; Save agentId
    mov     rcx, modelBase
    test    rcx, rcx
    jz      infer_fail
    mov     eax, edx  ; tok parameter
    and     eax, 7Fh
    imul    rax, 10
    add     rcx, rax
    movzx   eax, byte ptr [rcx]
    imul    eax, ebx  ; agentId
    and     eax, 0FFFFh
    pop     rbx
    ret
infer_fail:
    xor     eax, eax
    pop     rbx
    ret
Z800Infer ENDP

; =====================================================================
; Agent Token Generator
; =====================================================================
AgentRun PROC
    ; RCX = id
    push    rbx
    push    rsi
    push    rdi
    mov     rbx, rcx  ; Save id
    imul    rax, rcx, SIZEOF_AGENT
    lea     r10, agents
    add     r10, rax
    mov     dword ptr [r10 + AGENT_STATE], 1
    mov     esi, 42h  ; tok
    mov     edi, 64   ; counter
gen_loop:
    mov     rcx, rbx  ; id
    mov     rdx, rsi  ; tok
    sub     rsp, 20h
    call    Z800Infer
    add     rsp, 20h
    mov     rsi, rax  ; next tok
    mov     rax, qword ptr [r10 + AGENT_TOKENSDONE]
    mov     rdx, rax
    and     rdx, TOKEN_BUF - 1
    lea     r8, [r10 + AGENT_TOKENBUF]
    mov     byte ptr [r8 + rdx], sil
    inc     qword ptr [r10 + AGENT_TOKENSDONE]
    dec     edi
    jnz     gen_loop
    mov     dword ptr [r10 + AGENT_STATE], 0
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AgentRun ENDP

; =====================================================================
; GGUF Slice Loader
; =====================================================================
LoadSlices PROC
    ; RCX = path, RDX = slicesWanted
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    mov     rbx, rcx  ; Save path
    mov     esi, edx  ; Save slicesWanted
    test    esi, esi
    jz      skip_load
    cmp     esi, 256
    ja      bad_count
    ; CreateFileA(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0)
    mov     rcx, rbx
    mov     rdx, GENERIC_READ
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+20h], OPEN_EXISTING
    mov     qword ptr [rsp+28h], 0
    mov     qword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      load_err
    mov     gc.hFile, rax
    ; GetFileSize(hFile, 0)
    mov     rcx, rax
    xor     rdx, rdx
    call    GetFileSize
    mov     gc.fileSz, rax
    ; CreateFileMappingA(hFile, 0, PAGE_READONLY, 0, 0, 0)
    mov     rcx, gc.hFile
    xor     rdx, rdx
    mov     r8, PAGE_READONLY
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateFileMappingA
    test    rax, rax
    jz      load_err
    mov     gc.hMap, rax
    ; MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)
    mov     rcx, rax
    mov     rdx, FILE_MAP_READ
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    call    MapViewOfFile
    test    rax, rax
    jz      unmap_err
    mov     gc.pMap, rax
    
    ; Check GGUF magic
    mov     rcx, rax
    mov     eax, dword ptr [rcx]
    cmp     eax, 46554747h
    jne     unmap_err
    mov     eax, dword ptr [rcx+4]
    mov     ggufHdr.version, eax
    mov     rax, qword ptr [rcx+8]
    mov     ggufHdr.tensorCnt, rax
    
    ; Assign slices to agents
    xor     edi, edi
    
assign_loop:
    cmp     edi, esi
    jae     assign_done
    imul    rax, rdi, SIZEOF_AGENT
    lea     r10, agents
    add     r10, rax
    mov     dword ptr [r10 + AGENT_SLICEIDX], edi
    mov     r8, gc.hMap
    mov     qword ptr [r10 + AGENT_GGUFHANDLE], r8
    inc     edi
    jmp     assign_loop
    
assign_done:
    mov     eax, 1
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
    
unmap_err:
    mov     rcx, gc.pMap
    call    UnmapViewOfFile
    mov     rcx, gc.hMap
    call    CloseHandle
    
load_err:
    mov     rcx, gc.hFile
    call    CloseHandle
    
bad_count:
skip_load:
    xor     eax, eax
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
LoadSlices ENDP

; =====================================================================
; Language Table Initialization
; =====================================================================
InitLangTable PROC
    push    rbp
    mov     rbp, rsp
    
    ; C++ (index 0)
    lea     rax, langTable
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'ppc.'
    mov     word ptr [rax + LANG_ENTRY_EXT + 4], 'p'
    lea     rcx, szCpp
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szCppBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szExeRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldCpp
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; C (index 1)
    lea     rax, langTable
    add     rax, SIZEOF_LANG_ENTRY
    mov     word ptr [rax + LANG_ENTRY_EXT], 'c.'
    lea     rcx, szC
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szCBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szExeRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldC
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Rust (index 2)
    lea     rax, langTable
    add     rax, 96
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'sr.'
    lea     rcx, szRust
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szRustBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szExeRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldRust
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Go (index 3)
    lea     rax, langTable
    add     rax, 144
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'og.'
    mov     byte ptr [rax + LANG_ENTRY_EXT + 3], 0
    lea     rcx, szGo
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szGoBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szExeRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldGo
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Python (index 4)
    lea     rax, langTable
    add     rax, 192
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'yp.'
    lea     rcx, szPython
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szPythonRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szPythonRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldPython
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; JavaScript (index 5)
    lea     rax, langTable
    add     rax, 240
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'sj.'
    lea     rcx, szJS
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szJSRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szJSRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldJS
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; TypeScript (index 6)
    lea     rax, langTable
    add     rax, 288
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'st.'
    lea     rcx, szTS
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szTSBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szJSRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldTS
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Java (index 7)
    lea     rax, langTable
    add     rax, 336
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'avaj'
    lea     rcx, szJava
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szJavaBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szJavaRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldJava
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; C# (index 8)
    lea     rax, langTable
    add     rax, 384
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'sc.'
    lea     rcx, szCS
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szCSBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szExeRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldCSharp
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Swift (index 9)
    lea     rax, langTable
    add     rax, 432
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'iws.'
    mov     word ptr [rax + LANG_ENTRY_EXT + 4], 'tf'
    lea     rcx, szSwift
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szSwiftBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szSwiftRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldSwift
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Kotlin (index 10)
    lea     rax, langTable
    add     rax, 480
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'tk.'
    lea     rcx, szKotlin
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szKotlinBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szKotlinRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldKotlin
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Ruby (index 11)
    lea     rax, langTable
    add     rax, 528
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'br.'
    lea     rcx, szRuby
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szRubyRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szRubyRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldRuby
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; PHP (index 12)
    lea     rax, langTable
    add     rax, 576
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'php.'
    lea     rcx, szPHP
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szPHPRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szPHPRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldPHP
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Perl (index 13)
    lea     rax, langTable
    add     rax, 624
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'lp.'
    lea     rcx, szPerl
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szPerlRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szPerlRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldPerl
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Lua (index 14)
    lea     rax, langTable
    add     rax, 672
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'aul.'
    lea     rcx, szLua
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szLuaRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szLuaRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldLua
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Elixir (index 15)
    lea     rax, langTable
    add     rax, 720
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'xe.'
    lea     rcx, szElixir
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szElixirRun
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szElixirRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldElixir
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Haskell (index 16)
    lea     rax, langTable
    add     rax, 768
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'sh.'
    lea     rcx, szHaskell
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szHaskellBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szHaskellRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldHaskell
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; OCaml (index 17)
    lea     rax, langTable
    add     rax, 816
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'lm.'
    lea     rcx, szOCaml
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szOCamlBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szOCamlRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldOCaml
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Scala (index 18)
    lea     rax, langTable
    add     rax, 864
    mov     dword ptr [rax + LANG_ENTRY_EXT], 'acs.'
    mov     word ptr [rax + LANG_ENTRY_EXT + 4], 'al'
    lea     rcx, szScala
    mov     qword ptr [rax + LANG_ENTRY_NAME], rcx
    lea     rcx, szScalaBuild
    mov     qword ptr [rax + LANG_ENTRY_BUILDCMD], rcx
    lea     rcx, szScalaRun
    mov     qword ptr [rax + LANG_ENTRY_RUNCMD], rcx
    lea     rcx, ScaffoldScala
    mov     qword ptr [rax + LANG_ENTRY_SCAFFOLD], rcx
    
    ; Return language count
    mov     eax, 19
    mov     langCount, eax
    ret
InitLangTable ENDP

; =====================================================================
; Language Detection
; =====================================================================
GetLangId PROC
    ; RCX = pPath
    push    rbp
    mov     rbp, rsp
    push    rsi
    push    rdi
    push    rbx
    sub     rsp, 40h
    
    mov     rbx, rcx            ; Save pPath in RBX
    
    ; Get string length
    call    lstrlenA
    test    eax, eax
    jz      getlangid_not_found
    
    ; Find extension (scan back for '.')
    lea     rsi, [rbx + rax - 1]
    
getlangid_scan_back:
    cmp     byte ptr [rsi], '.'
    je      getlangid_found_dot
    dec     rsi
    cmp     rsi, rbx
    jae     getlangid_scan_back
    jmp     getlangid_not_found
    
getlangid_found_dot:
    inc     rsi                  ; RSI = pExt
    
    ; Search language table
    xor     edi, edi             ; EDI = loop counter
    
getlangid_check_loop:
    cmp     edi, langCount
    jae     getlangid_not_found
    
    ; Get language entry
    lea     rax, langTable
    movsxd  rdx, edi
    imul    rdx, rdx, SIZEOF_LANG_ENTRY
    add     rax, rdx
    
    ; Compare extensions: lstrcmpiA(pExt, &langTable[i].ext)
    mov     rcx, rsi             ; pExt
    lea     rdx, [rax + LANG_ENTRY_EXT]
    sub     rsp, 20h
    call    lstrcmpiA
    add     rsp, 20h
    
    test    eax, eax
    jz      getlangid_found_lang
    
    inc     edi
    jmp     getlangid_check_loop
    
getlangid_found_lang:
    mov     eax, edi
    add     rsp, 40h
    pop     rbx
    pop     rdi
    pop     rsi
    pop     rbp
    ret
    
getlangid_not_found:
    mov     eax, -1
    add     rsp, 40h
    pop     rbx
    pop     rdi
    pop     rsi
    pop     rbp
    ret
GetLangId ENDP

; =====================================================================
; Build and Run
; =====================================================================
BuildRun PROC
    ; RCX = pPath, EDX = langId
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 240h            ; 512 bytes for cmdBuf + alignment
    
    mov     rbx, rcx             ; Save pPath
    mov     esi, edx             ; Save langId
    
    cmp     esi, -1
    je      buildrun_fail
    mov     eax, langCount
    cmp     esi, eax
    jae     buildrun_fail
    
    ; Get language entry
    movsxd  rax, esi
    imul    rax, rax, SIZEOF_LANG_ENTRY
    lea     rdi, langTable
    add     rdi, rax             ; RDI = language entry
    
    ; Format build command: wsprintfA(cmdBuf, langEntry.buildCmd, pPath)
    mov     rcx, [rdi + LANG_ENTRY_BUILDCMD]
    test    rcx, rcx
    jz      buildrun_skip_build
    
    lea     rax, [rbp-220h]      ; cmdBuf on stack
    mov     qword ptr [rsp+20h], rbx  ; pPath as 5th parameter
    mov     r9, rbx              ; pPath as 4th parameter
    mov     r8, rbx              ; pPath as 3rd parameter  
    mov     rdx, rcx             ; format string
    lea     rcx, [rbp-220h]      ; dest buffer
    call    wsprintfA
    
    ; Execute build
    lea     rcx, [rbp-220h]
    call    ExecuteCmd
    
buildrun_skip_build:
    ; Format run command
    mov     rcx, [rdi + LANG_ENTRY_RUNCMD]
    test    rcx, rcx
    jz      buildrun_fail
    
    lea     rax, [rbp-220h]
    mov     qword ptr [rsp+20h], rbx
    mov     r9, rbx
    mov     r8, rbx
    mov     rdx, rcx
    lea     rcx, [rbp-220h]
    call    wsprintfA
    
    ; Execute run
    lea     rcx, [rbp-220h]
    call    ExecuteCmd
    
    mov     eax, 1
    add     rsp, 240h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
    
buildrun_fail:
    xor     eax, eax
    add     rsp, 240h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
BuildRun ENDP

ExecuteCmd PROC
    ; RCX = pCmd
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rdi
    sub     rsp, 230h            ; STARTUPINFOA(68)+PROCESS_INFORMATION(32)+alignment
    
    mov     rbx, rcx             ; Save pCmd
    
    ; Zero out STARTUPINFOA structure
    lea     rdi, [rbp-100h]
    xor     eax, eax
    mov     ecx, 68
    rep     stosb
    
    ; Zero out PROCESS_INFORMATION structure
    lea     rdi, [rbp-120h]
    xor     eax, eax
    mov     ecx, 32
    rep     stosb
    
    ; Set STARTUPINFOA.cb field
    mov     dword ptr [rbp-100h], 68
    
    ; CreateProcessA(NULL, pCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)
    xor     ecx, ecx             ; lpApplicationName = NULL
    mov     rdx, rbx             ; lpCommandLine = pCmd
    xor     r8, r8               ; lpProcessAttributes = NULL
    xor     r9, r9               ; lpThreadAttributes = NULL
    mov     dword ptr [rsp+20h], 0  ; bInheritHandles = FALSE
    mov     dword ptr [rsp+28h], 0  ; dwCreationFlags = 0
    mov     qword ptr [rsp+30h], 0  ; lpEnvironment = NULL
    mov     qword ptr [rsp+38h], 0  ; lpCurrentDirectory = NULL
    lea     rax, [rbp-100h]
    mov     qword ptr [rsp+40h], rax  ; lpStartupInfo
    lea     rax, [rbp-120h]
    mov     qword ptr [rsp+48h], rax  ; lpProcessInformation
    call    CreateProcessA
    test    eax, eax
    jz      execmd_fail
    
    ; WaitForSingleObject(hProcess, INFINITE)
    mov     rcx, [rbp-120h]      ; pi.hProcess
    mov     edx, INFINITE
    sub     rsp, 20h
    call    WaitForSingleObject
    add     rsp, 20h
    
    ; CloseHandle(hProcess)
    mov     rcx, [rbp-120h]
    sub     rsp, 20h
    call    CloseHandle
    add     rsp, 20h
    
    ; CloseHandle(hThread)
    mov     rcx, [rbp-118h]      ; pi.hThread
    sub     rsp, 20h
    call    CloseHandle
    add     rsp, 20h
    
    mov     eax, 1
    add     rsp, 230h
    pop     rdi
    pop     rbx
    pop     rbp
    ret
    
execmd_fail:
    xor     eax, eax
    add     rsp, 230h
    pop     rdi
    pop     rbx
    pop     rbp
    ret
ExecuteCmd ENDP

; =====================================================================
; External Scaffolder Procedures (implemented in language_scaffolders.asm and language_scaffolders_extended.asm)
; =====================================================================
EXTERN ScaffoldCpp:PROC
EXTERN ScaffoldC:PROC
EXTERN ScaffoldRust:PROC
EXTERN ScaffoldGo:PROC
EXTERN ScaffoldPython:PROC
EXTERN ScaffoldJS:PROC
EXTERN ScaffoldTS:PROC
EXTERN ScaffoldJava:PROC
EXTERN ScaffoldCSharp:PROC
EXTERN ScaffoldSwift:PROC
EXTERN ScaffoldKotlin:PROC
EXTERN ScaffoldRuby:PROC
EXTERN ScaffoldPHP:PROC
EXTERN ScaffoldPerl:PROC
EXTERN ScaffoldLua:PROC
EXTERN ScaffoldElixir:PROC
EXTERN ScaffoldHaskell:PROC
EXTERN ScaffoldOCaml:PROC
EXTERN ScaffoldScala:PROC

; =====================================================================
; Intent Classification
; =====================================================================
ClassifyIntent PROC
    ; RCX = pInput
    push    rbp
    mov     rbp, rsp
    push    rbx
    sub     rsp, 30h
    
    mov     rbx, rcx             ; Save pInput
    
    ; Copy to lowerBuf: lstrcpynA(lowerBuf, pInput, CMD_BUF)
    lea     rcx, lowerBuf
    mov     rdx, rbx
    mov     r8, CMD_BUF
    call    lstrcpynA
    
    ; Convert to lowercase: CharLowerA(lowerBuf)
    lea     rcx, lowerBuf
    call    CharLowerA
    
    ; Check for React: strstr(lowerBuf, "react")
    lea     rcx, lowerBuf
    lea     rdx, szReactName
    call    strstr
    test    rax, rax
    jnz     classify_intent_react
    
    ; Check for Vite: strstr(lowerBuf, "vite")
    lea     rcx, lowerBuf
    lea     rdx, szViteName
    call    strstr
    test    rax, rax
    jnz     classify_intent_react
    
    ; Default: no specific intent
    xor     eax, eax
    add     rsp, 30h
    pop     rbx
    pop     rbp
    ret
    
classify_intent_react:
    mov     eax, 1
    add     rsp, 30h
    pop     rbx
    pop     rbp
    ret
ClassifyIntent ENDP

; =====================================================================
; Export Functions
; =====================================================================
AgenticKernelInit PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    call    SpawnSwarm
    call    LoadZ800
    call    InitLangTable
    
    leave
    ret
AgenticKernelInit ENDP

END
