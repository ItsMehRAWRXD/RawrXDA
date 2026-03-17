; genesis_masm64.asm
; Pure MASM64 PowerShell Replacement - Zero Dependency Build System
; Assemble: ml64 /c /nologo /Fo genesis_masm64.obj genesis_masm64.asm
; Link: link genesis_masm64.obj /OUT:genesis_masm64.exe /SUBSYSTEM:CONSOLE /ENTRY:EntryPoint /NODEFAULTLIB kernel32.lib user32.lib

OPTION CASEMAP:NONE

; ============================================================================
; Headers & Includes
; ============================================================================
includelib kernel32.lib
includelib user32.lib

; ============================================================================
; Win32 API Externs
; ============================================================================
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN SetConsoleTextAttribute:PROC
EXTERN ExitProcess:PROC
EXTERN GetCommandLineA:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN CloseHandle:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN DeleteFileA:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN CreateDirectoryA:PROC
EXTERN GetLastError:PROC
EXTERN RtlCompareMemory:PROC

; ============================================================================
; Structures
; ============================================================================
STARTUPINFOA struct
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
STARTUPINFOA ends

PROCESS_INFORMATION struct
    hProcess        QWORD ?
    hThread         QWORD ?
    dwProcessId     DWORD ?
    dwThreadId      DWORD ?
PROCESS_INFORMATION ends

WIN32_FIND_DATAA struct
    dwFileAttributes    DWORD ?
    ftCreationTime      QWORD ?
    ftLastAccessTime    QWORD ?
    ftLastWriteTime     QWORD ?
    nFileSizeHigh       DWORD ?
    nFileSizeLow        DWORD ?
    dwReserved0         DWORD ?
    dwReserved1         DWORD ?
    cFileName           BYTE 260 dup(?)
    cAlternateFileName  BYTE 14 dup(?)
    dwFileType          DWORD ?
    dwCreatorType       DWORD ?
    wFinderFlags        WORD ?
WIN32_FIND_DATAA ends

; ============================================================================
; Constants
; ============================================================================
STD_INPUT_HANDLE        equ -10
STD_OUTPUT_HANDLE       equ -11
STD_ERROR_HANDLE        equ -12
INVALID_HANDLE_VALUE    equ -1
CREATE_NO_WINDOW        equ 08000000h
CREATE_NEW_CONSOLE      equ 00000010h
NORMAL_PRIORITY_CLASS   equ 00000020h
INFINITE_WAIT           equ 0FFFFFFFFh
WAIT_OBJECT_0           equ 0
FILE_ATTRIBUTE_NORMAL   equ 00000080h
FILE_SHARE_READ         equ 00000001h
FILE_SHARE_WRITE        equ 00000002h
OPEN_EXISTING           equ 3
CREATE_ALWAYS           equ 2
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
PAGE_READWRITE          equ 04h
FILE_MAP_WRITE          equ 00000002h
FILE_MAP_READ           equ 00000004h
MAX_PATH                equ 260
STARTF_USESTDHANDLES    equ 00000100h
ERROR_ALREADY_EXISTS    equ 183

; Colors
COLOR_RED               equ 0Ch
COLOR_GREEN             equ 0Ah
COLOR_YELLOW            equ 0Eh
COLOR_WHITE             equ 0Fh
COLOR_CYAN              equ 0Bh

; ============================================================================
; Data Section
; ============================================================================
.data
align 8

; Console handles
hStdOut                 QWORD 0
hStdIn                  QWORD 0

; Build configuration
szMl64                  BYTE "ml64.exe", 0
szLink                  BYTE "link.exe", 0
szLib                   BYTE "lib.exe", 0

; Common flags
szIncFlags              BYTE " /c /nologo /Iinclude /Isrc ", 0
szLinkFlags             BYTE "/SUBSYSTEM:WINDOWS /MACHINE:X64 /NODEFAULTLIB /OPT:REF /OPT:ICF", 0
szEntry                 BYTE "/ENTRY:WinMain", 0

; Libraries
szLibs                  BYTE "kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib advapi32.lib", 0

; Paths
szBuildDir              BYTE "build\Release\", 0
szObjDir                BYTE "build\MASM64\", 0
szSrcDir                BYTE "src\asm\", 0

; Newline
szNewLine               BYTE 13, 10, 0

; UI Strings
szBanner                BYTE 13, 10
                        BYTE "+================================================================+", 13, 10
                        BYTE "|     RawrXD Genesis MASM64 Build System v14.2.0                |", 13, 10
                        BYTE "|     PowerShell Replacement - Zero Dependencies                |", 13, 10
                        BYTE "+================================================================+", 13, 10, 0

szUsage                 BYTE 13, 10, "Usage: genesis_masm64.exe <command> [args...]", 13, 10
                        BYTE "Commands:", 13, 10
                        BYTE "  build          - Full build (asm + link)", 13, 10
                        BYTE "  asm <file>     - Assemble single file", 13, 10
                        BYTE "  link <objs...> - Link objects", 13, 10
                        BYTE "  patch <file>   - Apply hot-patches to binary", 13, 10
                        BYTE "  clean          - Remove build artifacts", 13, 10
                        BYTE "  verify         - Verify AI backend connectivity", 13, 10, 0

szPrompt                BYTE 13, 10, "RawrXD> ", 0
szOk                    BYTE "[OK] ", 0
szFail                  BYTE "[FAIL] ", 0
szInfo                  BYTE "[*] ", 0
szWarn                  BYTE "[!] ", 0

; Command strings
szCmdBuild              BYTE "build", 0
szCmdClean              BYTE "clean", 0
szCmdPatch              BYTE "patch", 0
szCmdVerify             BYTE "verify", 0
szObjPattern            BYTE "build\MASM64\*.obj", 0
szExePattern            BYTE "build\Release\*.exe", 0
szDoneClean             BYTE "Build artifacts removed.", 13, 10, 0
szPatchApplied          BYTE "Hot-patch applied successfully.", 13, 10, 0
szNoPattern             BYTE "Pattern not found - binary may already be patched.", 13, 10, 0
szFileNotFound          BYTE "Target file not found.", 13, 10, 0
szOpenFailed            BYTE "Failed to open target file.", 13, 10, 0
szVulkanAsm             BYTE "vulkan_compute.asm", 0
szBeaconAsm             BYTE "beacon_integration.asm", 0
szCurlCmd               BYTE "curl -s http://localhost:11434/api/tags", 0
szOllamaEndpoint        BYTE " >nul 2>&1", 0
szCheckingAI            BYTE "Probing Ollama endpoint...", 0
szAIOK                  BYTE "AI backend online.", 13, 10, 0
szAIOffline             BYTE "AI backend unreachable. Start Ollama?", 13, 10, 0
szAssemblingMsg         BYTE "Assembling: ", 0
szLinkingMsg            BYTE "Linking...", 13, 10, 0
szBuildComplete         BYTE "Build complete.", 13, 10, 0
szCreatingDir           BYTE "Creating directory: ", 0

; Patch signatures (canonical RawrXD function prologues)
szPatchPattern1         BYTE 048h, 089h, 05Ch, 024h, 008h, 048h, 089h, 06Ch, 024h, 010h
                        BYTE 048h, 089h, 074h, 024h, 018h, 057h, 048h, 083h, 0ECh, 020h
PATCH_PATTERN1_LEN      equ 20

szPatchReplace1         BYTE 048h, 08Bh, 0C4h, 048h, 089h, 058h, 008h, 048h, 089h, 068h
                        BYTE 010h, 048h, 089h, 070h, 018h, 048h, 089h, 078h, 020h, 041h
PATCH_REPLACE1_LEN      equ 20

; Buffers
cmdLineBuffer           BYTE 4096 dup(0)
pathBuffer              BYTE MAX_PATH dup(0)
findData                WIN32_FIND_DATAA <>
procInfo                PROCESS_INFORMATION <>
startInfo               STARTUPINFOA <>
dwBytesWritten          DWORD 0
dwExitCode              DWORD 0

; ============================================================================
; Code Section
; ============================================================================
.code

; ----------------------------------------------------------------------------
; WriteMsg - Write null-terminated string to console
; RCX = pointer to string
; ----------------------------------------------------------------------------
WriteMsg PROC
    push rbx
    push rsi
    sub rsp, 48                     ; shadow space + alignment

    mov rbx, rcx                    ; save message pointer

    ; Calculate string length
    xor edx, edx
    mov rsi, rbx
@@count:
    cmp BYTE PTR [rsi], 0
    je @@write
    inc edx
    inc rsi
    jmp @@count

@@write:
    mov rcx, hStdOut                ; hConsoleOutput
    lea r9, dwBytesWritten          ; lpNumberOfCharsWritten
    mov r8d, edx                    ; nNumberOfCharsToWrite
    mov rdx, rbx                    ; lpBuffer
    mov QWORD PTR [rsp+32], 0      ; lpReserved
    call WriteConsoleA

    add rsp, 48
    pop rsi
    pop rbx
    ret
WriteMsg ENDP

; ----------------------------------------------------------------------------
; WriteMsgColor - Write string with color attribute
; RCX = pointer to string, EDX = color attribute
; ----------------------------------------------------------------------------
WriteMsgColor PROC
    push rbx
    push r12
    sub rsp, 40

    mov rbx, rcx                    ; message
    mov r12d, edx                   ; color

    ; Set color
    mov rcx, hStdOut
    mov edx, r12d
    call SetConsoleTextAttribute

    ; Write message
    mov rcx, rbx
    call WriteMsg

    ; Reset to white
    mov rcx, hStdOut
    mov edx, COLOR_WHITE
    call SetConsoleTextAttribute

    add rsp, 40
    pop r12
    pop rbx
    ret
WriteMsgColor ENDP

; ----------------------------------------------------------------------------
; PrintNewLine - Output CR+LF
; ----------------------------------------------------------------------------
PrintNewLine PROC
    sub rsp, 40
    lea rcx, szNewLine
    call WriteMsg
    add rsp, 40
    ret
PrintNewLine ENDP

; ----------------------------------------------------------------------------
; StringLength - Return length of null-terminated string
; RCX = string pointer, returns EAX = length
; ----------------------------------------------------------------------------
StringLength PROC
    xor eax, eax
    mov rsi, rcx
@@loop:
    cmp BYTE PTR [rsi], 0
    je @@done
    inc eax
    inc rsi
    jmp @@loop
@@done:
    ret
StringLength ENDP

; ----------------------------------------------------------------------------
; StringCopy - Copy null-terminated string
; RCX = dest, RDX = source
; ----------------------------------------------------------------------------
StringCopy PROC
    push rsi
    push rdi
    mov rdi, rcx                    ; dest
    mov rsi, rdx                    ; source
@@loop:
    lodsb
    stosb
    test al, al
    jnz @@loop
    pop rdi
    pop rsi
    ret
StringCopy ENDP

; ----------------------------------------------------------------------------
; StringConcat - Append source to end of dest
; RCX = dest, RDX = source
; ----------------------------------------------------------------------------
StringConcat PROC
    push rsi
    push rdi
    push rcx

    ; Find end of dest
    mov rdi, rcx
@@find_end:
    cmp BYTE PTR [rdi], 0
    je @@copy
    inc rdi
    jmp @@find_end

@@copy:
    mov rsi, rdx
@@loop:
    lodsb
    stosb
    test al, al
    jnz @@loop

    pop rcx
    pop rdi
    pop rsi
    ret
StringConcat ENDP

; ----------------------------------------------------------------------------
; StringCompare - Compare two null-terminated strings (case-sensitive)
; RCX = str1, RDX = str2
; Returns: EAX = 1 if equal, 0 if different
; ----------------------------------------------------------------------------
StringCompare PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
@@loop:
    mov al, BYTE PTR [rsi]
    mov dl, BYTE PTR [rdi]
    cmp al, dl
    jne @@diff
    test al, al
    jz @@equal
    inc rsi
    inc rdi
    jmp @@loop

@@diff:
    xor eax, eax
    jmp @@done

@@equal:
    mov eax, 1

@@done:
    pop rdi
    pop rsi
    ret
StringCompare ENDP

; ----------------------------------------------------------------------------
; ExecuteCommand - Create process and optionally wait for completion
; RCX = command line string, RDX = wait flag (0=async, 1=sync)
; Returns: RAX = 1 success, 0 failure
; ----------------------------------------------------------------------------
ExecuteCommand PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 128

    mov rbx, rcx                    ; command line
    mov r12, rdx                    ; wait flag

    ; Zero STARTUPINFOA
    lea rdi, [rsp+0]                ; local area
    lea rdi, startInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA
    rep stosb

    ; Zero PROCESS_INFORMATION
    lea rdi, procInfo
    mov ecx, SIZEOF PROCESS_INFORMATION
    xor eax, eax
    rep stosb

    mov startInfo.cb, SIZEOF STARTUPINFOA
    mov startInfo.dwFlags, STARTF_USESTDHANDLES
    mov rax, hStdOut
    mov startInfo.hStdOutput, rax
    mov startInfo.hStdError, rax

    ; CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW|NORMAL_PRIORITY, NULL, NULL, &si, &pi)
    xor ecx, ecx                    ; lpApplicationName = NULL
    mov rdx, rbx                    ; lpCommandLine
    xor r8d, r8d                    ; lpProcessAttributes = NULL
    xor r9d, r9d                    ; lpThreadAttributes = NULL
    mov DWORD PTR [rsp+32], 0       ; bInheritHandles = FALSE
    mov eax, CREATE_NO_WINDOW
    or eax, NORMAL_PRIORITY_CLASS
    mov DWORD PTR [rsp+40], eax     ; dwCreationFlags
    mov QWORD PTR [rsp+48], 0       ; lpEnvironment = NULL
    mov QWORD PTR [rsp+56], 0       ; lpCurrentDirectory = NULL
    lea rax, startInfo
    mov QWORD PTR [rsp+64], rax     ; lpStartupInfo
    lea rax, procInfo
    mov QWORD PTR [rsp+72], rax     ; lpProcessInformation
    call CreateProcessA

    test eax, eax
    jz @@failed

    ; Wait if requested
    cmp r12, 0
    je @@no_wait

    mov rcx, procInfo.hProcess
    mov edx, INFINITE_WAIT
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne @@failed_close

    ; Get exit code
    mov rcx, procInfo.hProcess
    lea rdx, dwExitCode
    call GetExitCodeProcess

    cmp dwExitCode, 0
    jne @@failed_close

@@no_wait:
    ; Close handles
    mov rcx, procInfo.hProcess
    call CloseHandle
    mov rcx, procInfo.hThread
    call CloseHandle

    mov rax, 1                      ; success
    jmp @@done

@@failed_close:
    mov rcx, procInfo.hProcess
    call CloseHandle
    mov rcx, procInfo.hThread
    call CloseHandle

@@failed:
    xor rax, rax                    ; failure

@@done:
    add rsp, 128
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ExecuteCommand ENDP

; ----------------------------------------------------------------------------
; FileExists - Check if a file exists
; RCX = filename string
; Returns: RAX = 1 if exists, 0 if not
; ----------------------------------------------------------------------------
FileExists PROC
    push rbx
    sub rsp, 64

    mov rbx, rcx                    ; filename

    mov rcx, rbx                    ; lpFileName
    xor edx, edx                    ; dwDesiredAccess = 0
    mov r8d, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor r9d, r9d                    ; lpSecurityAttributes = NULL
    mov DWORD PTR [rsp+32], OPEN_EXISTING
    mov DWORD PTR [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+48], 0       ; hTemplateFile = NULL
    call CreateFileA

    cmp rax, INVALID_HANDLE_VALUE
    je @@not_found

    mov rcx, rax
    call CloseHandle
    mov rax, 1
    jmp @@done

@@not_found:
    xor rax, rax

@@done:
    add rsp, 64
    pop rbx
    ret
FileExists ENDP

; ----------------------------------------------------------------------------
; CreateDir - Create a single directory level
; RCX = directory path
; Returns: RAX = 1 success or already exists, 0 failure
; ----------------------------------------------------------------------------
CreateDir PROC
    sub rsp, 40

    ; Try CreateDirectoryA(path, NULL)
    xor edx, edx
    call CreateDirectoryA
    test eax, eax
    jnz @@success

    call GetLastError
    cmp eax, ERROR_ALREADY_EXISTS
    je @@success

    xor rax, rax
    jmp @@done

@@success:
    mov rax, 1

@@done:
    add rsp, 40
    ret
CreateDir ENDP

; ----------------------------------------------------------------------------
; DeleteFilePattern - Delete files matching a path pattern
; RCX = pattern string (e.g., "build\MASM64\*.obj")
; ----------------------------------------------------------------------------
DeleteFilePattern PROC
    push rbx
    push r12
    sub rsp, 40

    mov rbx, rcx                    ; pattern

    ; FindFirstFileA(pattern, &findData)
    mov rcx, rbx
    lea rdx, findData
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@done

    mov r12, rax                    ; hFind

@@loop:
    lea rcx, findData.cFileName
    call DeleteFileA

    mov rcx, r12
    lea rdx, findData
    call FindNextFileA
    test eax, eax
    jnz @@loop

    mov rcx, r12
    call FindClose

@@done:
    add rsp, 40
    pop r12
    pop rbx
    ret
DeleteFilePattern ENDP

; ============================================================================
; Build Commands
; ============================================================================

; ----------------------------------------------------------------------------
; CmdBuild - Full build: create dirs, assemble all ASM, link
; ----------------------------------------------------------------------------
CmdBuild PROC
    push rbx
    sub rsp, 512

    ; Print header
    lea rcx, szInfo
    call WriteMsg
    lea rcx, szAssemblingMsg
    call WriteMsg
    lea rcx, szVulkanAsm
    call WriteMsg
    call PrintNewLine

    ; Create build directories
    lea rcx, szCreatingDir
    call WriteMsg
    lea rcx, szObjDir
    call WriteMsg
    call PrintNewLine

    lea rcx, szObjDir
    call CreateDir

    lea rcx, szBuildDir
    call CreateDir

    ; Build ml64 command for vulkan_compute.asm
    lea rcx, cmdLineBuffer
    lea rdx, szMl64
    call StringCopy

    lea rcx, cmdLineBuffer
    lea rdx, szIncFlags
    call StringConcat

    ; Append /Fo flag
    lea rdi, cmdLineBuffer
@@find_end1:
    cmp BYTE PTR [rdi], 0
    je @@append1
    inc rdi
    jmp @@find_end1
@@append1:
    mov BYTE PTR [rdi+0], '/'
    mov BYTE PTR [rdi+1], 'F'
    mov BYTE PTR [rdi+2], 'o'
    mov BYTE PTR [rdi+3], 0

    lea rcx, cmdLineBuffer
    lea rdx, szObjDir
    call StringConcat

    ; Append "vulkan_compute.obj "
    lea rdi, cmdLineBuffer
@@find_end2:
    cmp BYTE PTR [rdi], 0
    je @@append2
    inc rdi
    jmp @@find_end2
@@append2:
    mov BYTE PTR [rdi+0], 'v'
    mov BYTE PTR [rdi+1], 'u'
    mov BYTE PTR [rdi+2], 'l'
    mov BYTE PTR [rdi+3], 'k'
    mov BYTE PTR [rdi+4], 'a'
    mov BYTE PTR [rdi+5], 'n'
    mov BYTE PTR [rdi+6], '_'
    mov BYTE PTR [rdi+7], 'c'
    mov BYTE PTR [rdi+8], 'o'
    mov BYTE PTR [rdi+9], 'm'
    mov BYTE PTR [rdi+10], 'p'
    mov BYTE PTR [rdi+11], 'u'
    mov BYTE PTR [rdi+12], 't'
    mov BYTE PTR [rdi+13], 'e'
    mov BYTE PTR [rdi+14], '.'
    mov BYTE PTR [rdi+15], 'o'
    mov BYTE PTR [rdi+16], 'b'
    mov BYTE PTR [rdi+17], 'j'
    mov BYTE PTR [rdi+18], ' '
    mov BYTE PTR [rdi+19], 0

    ; Append source path: src\asm\vulkan_compute.asm
    lea rcx, cmdLineBuffer
    lea rdx, szSrcDir
    call StringConcat

    lea rcx, cmdLineBuffer
    lea rdx, szVulkanAsm
    call StringConcat

    ; Execute ml64
    lea rcx, szInfo
    call WriteMsg
    lea rcx, cmdLineBuffer
    call WriteMsg
    call PrintNewLine

    lea rcx, cmdLineBuffer
    mov rdx, 1                      ; wait
    call ExecuteCommand

    test rax, rax
    jz @@failed

    lea rcx, szOk
    mov edx, COLOR_GREEN
    call WriteMsgColor
    lea rcx, szVulkanAsm
    call WriteMsg
    call PrintNewLine

    ; Assemble beacon_integration.asm (repeat with second file)
    lea rcx, szInfo
    call WriteMsg
    lea rcx, szAssemblingMsg
    call WriteMsg
    lea rcx, szBeaconAsm
    call WriteMsg
    call PrintNewLine

    ; Build command: ml64 /c /nologo /Iinclude /Isrc /Fobuild\MASM64\beacon_integration.obj src\asm\beacon_integration.asm
    lea rcx, cmdLineBuffer
    lea rdx, szMl64
    call StringCopy

    lea rcx, cmdLineBuffer
    lea rdx, szIncFlags
    call StringConcat

    lea rdi, cmdLineBuffer
@@find_end3:
    cmp BYTE PTR [rdi], 0
    je @@append3
    inc rdi
    jmp @@find_end3
@@append3:
    mov BYTE PTR [rdi+0], '/'
    mov BYTE PTR [rdi+1], 'F'
    mov BYTE PTR [rdi+2], 'o'
    mov BYTE PTR [rdi+3], 0

    lea rcx, cmdLineBuffer
    lea rdx, szObjDir
    call StringConcat

    lea rdi, cmdLineBuffer
@@find_end4:
    cmp BYTE PTR [rdi], 0
    je @@append4
    inc rdi
    jmp @@find_end4
@@append4:
    mov BYTE PTR [rdi+0], 'b'
    mov BYTE PTR [rdi+1], 'e'
    mov BYTE PTR [rdi+2], 'a'
    mov BYTE PTR [rdi+3], 'c'
    mov BYTE PTR [rdi+4], 'o'
    mov BYTE PTR [rdi+5], 'n'
    mov BYTE PTR [rdi+6], '_'
    mov BYTE PTR [rdi+7], 'i'
    mov BYTE PTR [rdi+8], 'n'
    mov BYTE PTR [rdi+9], 't'
    mov BYTE PTR [rdi+10], 'e'
    mov BYTE PTR [rdi+11], 'g'
    mov BYTE PTR [rdi+12], 'r'
    mov BYTE PTR [rdi+13], 'a'
    mov BYTE PTR [rdi+14], 't'
    mov BYTE PTR [rdi+15], 'i'
    mov BYTE PTR [rdi+16], 'o'
    mov BYTE PTR [rdi+17], 'n'
    mov BYTE PTR [rdi+18], '.'
    mov BYTE PTR [rdi+19], 'o'
    mov BYTE PTR [rdi+20], 'b'
    mov BYTE PTR [rdi+21], 'j'
    mov BYTE PTR [rdi+22], ' '
    mov BYTE PTR [rdi+23], 0

    lea rcx, cmdLineBuffer
    lea rdx, szSrcDir
    call StringConcat

    lea rcx, cmdLineBuffer
    lea rdx, szBeaconAsm
    call StringConcat

    ; Execute
    lea rcx, cmdLineBuffer
    mov rdx, 1
    call ExecuteCommand

    test rax, rax
    jz @@failed

    lea rcx, szOk
    mov edx, COLOR_GREEN
    call WriteMsgColor
    lea rcx, szBeaconAsm
    call WriteMsg
    call PrintNewLine

    ; Print completion
    lea rcx, szBuildComplete
    mov edx, COLOR_GREEN
    call WriteMsgColor

    mov rax, 1
    jmp @@done

@@failed:
    lea rcx, szFail
    mov edx, COLOR_RED
    call WriteMsgColor
    lea rcx, szNewLine
    call WriteMsg
    xor rax, rax

@@done:
    add rsp, 512
    pop rbx
    ret
CmdBuild ENDP

; ----------------------------------------------------------------------------
; CmdClean - Remove all build artifacts
; ----------------------------------------------------------------------------
CmdClean PROC
    sub rsp, 40

    lea rcx, szInfo
    call WriteMsg
    lea rcx, szObjDir
    call WriteMsg
    call PrintNewLine

    ; Delete .obj files
    lea rcx, szObjPattern
    call DeleteFilePattern

    ; Delete .exe files
    lea rcx, szExePattern
    call DeleteFilePattern

    lea rcx, szOk
    mov edx, COLOR_GREEN
    call WriteMsgColor
    lea rcx, szDoneClean
    call WriteMsg

    add rsp, 40
    ret
CmdClean ENDP

; ----------------------------------------------------------------------------
; CmdPatch - Apply binary hot-patches to an executable
; RCX = target file path
; ----------------------------------------------------------------------------
CmdPatch PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 88

    mov rbx, rcx                    ; target file path

    ; Check file exists
    mov rcx, rbx
    call FileExists
    test rax, rax
    jz @@not_found

    ; Open file for read/write
    mov rcx, rbx                    ; lpFileName
    mov edx, GENERIC_READ or GENERIC_WRITE
    mov r8d, FILE_SHARE_READ        ; dwShareMode
    xor r9d, r9d                    ; lpSecurityAttributes
    mov DWORD PTR [rsp+32], OPEN_EXISTING
    mov DWORD PTR [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+48], 0       ; hTemplateFile
    call CreateFileA

    cmp rax, INVALID_HANDLE_VALUE
    je @@open_failed
    mov r12, rax                    ; hFile

    ; Get file size
    mov rcx, r12
    xor edx, edx                    ; lpFileSizeHigh = NULL
    call GetFileSize
    mov r13d, eax                   ; file size

    ; Create file mapping
    mov rcx, r12                    ; hFile
    xor edx, edx                    ; lpAttributes = NULL
    mov r8d, PAGE_READWRITE         ; flProtect
    xor r9d, r9d                    ; dwMaxSizeHigh = 0
    mov DWORD PTR [rsp+32], r13d    ; dwMaxSizeLow
    mov QWORD PTR [rsp+40], 0       ; lpName = NULL
    call CreateFileMappingA
    test rax, rax
    jz @@map_failed
    mov r14, rax                    ; hMap

    ; Map view of file
    mov rcx, r14                    ; hFileMappingObject
    mov edx, FILE_MAP_WRITE or FILE_MAP_READ
    xor r8d, r8d                    ; dwFileOffsetHigh = 0
    xor r9d, r9d                    ; dwFileOffsetLow = 0
    mov DWORD PTR [rsp+32], 0       ; dwNumberOfBytesToMap = 0 (entire file)
    call MapViewOfFile
    test rax, rax
    jz @@view_failed
    mov rbx, rax                    ; lpBase (reuse rbx, target path no longer needed)

    ; Search for patch pattern
    lea rcx, szInfo
    call WriteMsg
    lea rcx, szCheckingAI           ; reuse string for "scanning..."
    call WriteMsg
    call PrintNewLine

    ; Linear scan for pattern match
    xor esi, esi                    ; offset = 0
    mov edi, r13d
    sub edi, PATCH_PATTERN1_LEN     ; max scan offset

@@search_loop:
    cmp esi, edi
    jae @@pattern_not_found

    ; Compare PATCH_PATTERN1_LEN bytes at rbx+rsi against szPatchPattern1
    lea rcx, [rbx + rsi]           ; Buffer1
    lea rdx, szPatchPattern1        ; Buffer2
    mov r8d, PATCH_PATTERN1_LEN     ; Length
    call RtlCompareMemory
    cmp eax, PATCH_PATTERN1_LEN
    je @@pattern_found

    inc esi
    jmp @@search_loop

@@pattern_found:
    ; Apply replacement bytes at rbx+esi
    lea rdi, [rbx + rsi]
    lea rsi, szPatchReplace1
    mov ecx, PATCH_REPLACE1_LEN
    rep movsb

    lea rcx, szOk
    mov edx, COLOR_GREEN
    call WriteMsgColor
    lea rcx, szPatchApplied
    call WriteMsg
    jmp @@cleanup

@@pattern_not_found:
    lea rcx, szWarn
    mov edx, COLOR_YELLOW
    call WriteMsgColor
    lea rcx, szNoPattern
    call WriteMsg

@@cleanup:
    mov rcx, rbx                    ; lpBase
    call UnmapViewOfFile

@@view_failed:
    mov rcx, r14                    ; hMap
    call CloseHandle

@@map_failed:
    mov rcx, r12                    ; hFile
    call CloseHandle
    jmp @@done

@@not_found:
    lea rcx, szFail
    mov edx, COLOR_RED
    call WriteMsgColor
    lea rcx, szFileNotFound
    call WriteMsg
    jmp @@done

@@open_failed:
    lea rcx, szFail
    mov edx, COLOR_RED
    call WriteMsgColor
    lea rcx, szOpenFailed
    call WriteMsg

@@done:
    add rsp, 88
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
CmdPatch ENDP

; ----------------------------------------------------------------------------
; CmdVerifyAI - Check if Ollama AI backend is reachable
; Uses curl to probe http://localhost:11434/api/tags
; ----------------------------------------------------------------------------
CmdVerifyAI PROC
    push rbx
    sub rsp, 48

    lea rcx, szInfo
    call WriteMsg
    lea rcx, szCheckingAI
    call WriteMsg
    call PrintNewLine

    ; Build curl command
    lea rcx, cmdLineBuffer
    lea rdx, szCurlCmd
    call StringCopy

    lea rcx, cmdLineBuffer
    lea rdx, szOllamaEndpoint
    call StringConcat

    ; Execute curl
    lea rcx, cmdLineBuffer
    mov rdx, 1                      ; wait for result
    call ExecuteCommand

    test rax, rax
    jz @@offline

    lea rcx, szOk
    mov edx, COLOR_GREEN
    call WriteMsgColor
    lea rcx, szAIOK
    call WriteMsg
    mov rax, 1
    jmp @@done

@@offline:
    lea rcx, szFail
    mov edx, COLOR_RED
    call WriteMsgColor
    lea rcx, szAIOffline
    call WriteMsg
    xor rax, rax

@@done:
    add rsp, 48
    pop rbx
    ret
CmdVerifyAI ENDP

; ============================================================================
; Command Parser
; ============================================================================

; ----------------------------------------------------------------------------
; SkipWhitespace - Advance pointer past spaces/tabs
; RCX = string pointer
; Returns: RAX = pointer to first non-whitespace char
; ----------------------------------------------------------------------------
SkipWhitespace PROC
    mov rax, rcx
@@loop:
    cmp BYTE PTR [rax], ' '
    je @@next
    cmp BYTE PTR [rax], 9          ; tab
    je @@next
    ret
@@next:
    inc rax
    jmp @@loop
SkipWhitespace ENDP

; ----------------------------------------------------------------------------
; SkipArg - Skip past current argument (non-whitespace chars)
; RCX = string pointer
; Returns: RAX = pointer just past the argument
; ----------------------------------------------------------------------------
SkipArg PROC
    mov rax, rcx
@@loop:
    cmp BYTE PTR [rax], 0
    je @@done
    cmp BYTE PTR [rax], ' '
    je @@done
    cmp BYTE PTR [rax], 9
    je @@done
    inc rax
    jmp @@loop
@@done:
    ret
SkipArg ENDP

; ----------------------------------------------------------------------------
; ParseAndExecute - Parse raw command line string and dispatch
; RCX = raw command line from GetCommandLineA
; ----------------------------------------------------------------------------
ParseAndExecute PROC
    push rbx
    push r12
    sub rsp, 40

    mov rbx, rcx                    ; raw cmdline

    ; Skip program name (may be quoted)
    cmp BYTE PTR [rbx], '"'
    jne @@unquoted_exe

    ; Quoted program name — skip to closing quote
    inc rbx
@@skip_quoted:
    cmp BYTE PTR [rbx], 0
    je @@show_usage
    cmp BYTE PTR [rbx], '"'
    je @@past_quote
    inc rbx
    jmp @@skip_quoted
@@past_quote:
    inc rbx
    jmp @@skip_ws1

@@unquoted_exe:
    mov rcx, rbx
    call SkipArg
    mov rbx, rax

@@skip_ws1:
    mov rcx, rbx
    call SkipWhitespace
    mov rbx, rax                    ; rbx = first argument

    ; If no argument, show usage
    cmp BYTE PTR [rbx], 0
    je @@show_usage

    ; Extract command word into pathBuffer for comparison
    lea rdi, pathBuffer
    mov rsi, rbx
@@copy_cmd:
    mov al, BYTE PTR [rsi]
    cmp al, 0
    je @@end_copy
    cmp al, ' '
    je @@end_copy
    cmp al, 9
    je @@end_copy
    stosb
    inc rsi
    jmp @@copy_cmd
@@end_copy:
    mov BYTE PTR [rdi], 0
    mov r12, rsi                    ; r12 = rest of command line after command word

    ; Compare against known commands
    lea rcx, pathBuffer
    lea rdx, szCmdBuild
    call StringCompare
    test rax, rax
    jnz @@do_build

    lea rcx, pathBuffer
    lea rdx, szCmdClean
    call StringCompare
    test rax, rax
    jnz @@do_clean

    lea rcx, pathBuffer
    lea rdx, szCmdPatch
    call StringCompare
    test rax, rax
    jnz @@do_patch

    lea rcx, pathBuffer
    lea rdx, szCmdVerify
    call StringCompare
    test rax, rax
    jnz @@do_verify

    ; Unknown command — fall through to usage
    jmp @@show_usage

@@do_build:
    call CmdBuild
    jmp @@done

@@do_clean:
    call CmdClean
    jmp @@done

@@do_patch:
    ; Skip whitespace to get target filename
    mov rcx, r12
    call SkipWhitespace
    mov rcx, rax
    cmp BYTE PTR [rcx], 0
    je @@show_usage
    call CmdPatch
    jmp @@done

@@do_verify:
    call CmdVerifyAI
    jmp @@done

@@show_usage:
    lea rcx, szUsage
    call WriteMsg

@@done:
    add rsp, 40
    pop r12
    pop rbx
    ret
ParseAndExecute ENDP

; ============================================================================
; Entry Point — Standalone console application
; ============================================================================
EntryPoint PROC
    sub rsp, 40                     ; shadow space + alignment

    ; Get console handles
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax

    mov ecx, STD_INPUT_HANDLE
    call GetStdHandle
    mov hStdIn, rax

    ; Print banner
    lea rcx, szBanner
    mov edx, COLOR_CYAN
    call WriteMsgColor

    ; Get raw command line and parse/dispatch
    call GetCommandLineA
    mov rcx, rax
    call ParseAndExecute

    ; Exit with code 0
    xor ecx, ecx
    call ExitProcess

    ; Never reached
    add rsp, 40
    ret
EntryPoint ENDP

END
