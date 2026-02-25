; build_configurations.asm
; Professional Build Configuration System for NASM IDE
; Complete build system with multiple toolchains and optimization levels

format PE64 GUI 5.0

include 'win64a.inc'

; Build configuration constants
BUILD_DEBUG     equ 0
BUILD_RELEASE   equ 1
BUILD_SMALL     equ 2
BUILD_FAST      equ 3

; Architecture constants
ARCH_X64        equ 0
ARCH_X86        equ 1
ARCH_ARM64      equ 2

; Toolchain constants
TOOLCHAIN_NASM  equ 1
TOOLCHAIN_YASM  equ 2
TOOLCHAIN_MS    equ 3
TOOLCHAIN_GCC   equ 4

; Optimization levels
OPTIMIZATION_NONE       equ 0
OPTIMIZATION_SIZE       equ 1
OPTIMIZATION_SPEED      equ 2
OPTIMIZATION_AGGRESSIVE equ 3

section '.data' data readable writeable

; Toolchain detection strings
szNasmExe          db "nasm.exe", 0
szYasmExe          db "yasm.exe", 0
szGccExe           db "gcc.exe", 0
szClExe            db "cl.exe", 0
szLinkExe          db "link.exe", 0
szMasmExe          db "ml64.exe", 0
szAsExe            db "as.exe", 0
szLdExe            db "ld.exe", 0

; Version query strings
szNasmVersionCmd   db "nasm --version", 0
szYasmVersionCmd   db "yasm --version", 0
szGccVersionCmd    db "gcc --version", 0

; Build configuration names
szConfigDebug      db "Debug", 0
szConfigRelease    db "Release", 0
szConfigSmall      db "Small", 0
szConfigFast       db "Fast", 0

; Architecture names
szArchX64          db "x64", 0
szArchX86          db "x86", 0
szArchARM64        db "ARM64", 0

; Status messages
szDetectingTools   db "Detecting available toolchains...", 0
szToolFound        db "Found: %s", 0
szToolNotFound     db "Not found: %s", 0
szNoToolsFound     db "No compatible toolchains found!", 0

; Build command templates for different toolchains

; NASM toolchain commands
NasmCommands:
    ; Debug build
    db "nasm -g -f win64 %s -o %s.obj", 0
    ; Release build
    db "nasm -O2 -f win64 %s -o %s.obj && gcc -O2 -s %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0
    ; Small build (size optimized)
    db "nasm -Os -f win64 %s -o %s.obj && gcc -Os -s %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0
    ; Fast build (speed optimized)
    db "nasm -O3 -f win64 %s -o %s.obj && gcc -O3 %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0

; YASM toolchain commands
YasmCommands:
    ; Debug build
    db "yasm -g -f win64 %s -o %s.obj", 0
    ; Release build
    db "yasm -O2 -f win64 %s -o %s.obj && gcc -O2 -s %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0
    ; Small build
    db "yasm -Os -f win64 %s -o %s.obj && gcc -Os -s %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0
    ; Fast build
    db "yasm -O3 -f win64 %s -o %s.obj && gcc -O3 %s.obj -o %s.exe -lkernel32 -luser32 -lgdi32", 0

; Microsoft toolchain commands
MsCommands:
    ; Debug build
    db "ml64 /c /Zi %s /Fo%s.obj", 0
    ; Release build
    db "ml64 /c %s /Fo%s.obj && link /subsystem:console %s.obj /out:%s.exe", 0
    ; Small build
    db "ml64 /c /Os %s /Fo%s.obj && link /subsystem:console %s.obj /out:%s.exe", 0
    ; Fast build
    db "ml64 /c /O2 %s /Fo%s.obj && link /subsystem:console %s.obj /out:%s.exe", 0

; Output file templates
szOutputExe        db "%s.exe", 0
szOutputDll        db "%s.dll", 0
szOutputObj        db "%s.obj", 0
szOutputTempObj    db "temp.obj", 0

section '.bss' data readable writeable

; Build system state
currentToolchain   resd 1
currentConfig      resd 1
currentArch        resd 1
availableTools     resd 1

; Tool paths (detected at runtime)
nasmPath           resb 260
yasmPath           resb 260
gccPath            resb 260
clPath             resb 260
linkPath           resb 260

; Tool versions
nasmVersion        resb 256
yasmVersion        resb 256
gccVersion         resb 256

; Build command buffer
buildCommand       resb 2048
currentOutputFile  resb 260

section '.text' code readable executable

; Initialize build system
InitBuildSystem:
    push rbp
    mov rbp, rsp

    ; Detect available toolchains
    call DetectAvailableToolchains

    ; Set default configuration
    mov dword [currentConfig], BUILD_DEBUG
    mov dword [currentArch], ARCH_X64

    ; Select best available toolchain
    call SelectBestToolchain

    leave
    ret

; Detect all available toolchains
DetectAvailableToolchains:
    push rbp
    mov rbp, rsp
    sub rsp, 512

    ; Clear available tools
    xor eax, eax
    mov [availableTools], eax

    ; Update status
    lea rcx, [szDetectingTools]
    call UpdateStatusBar

    ; Check for NASM
    lea rcx, [szNasmExe]
    call CheckToolInPath
    test eax, eax
    jz .check_yasm

    ; NASM found
    mov dword [availableTools], TOOLCHAIN_NASM
    lea rcx, [szToolFound]
    lea rdx, [szNasmExe]
    call PrintStatusMessage

    ; Get NASM path and version
    lea rcx, [szNasmExe]
    lea rdx, [nasmPath]
    call GetToolPath
    lea rcx, [szNasmVersionCmd]
    lea rdx, [nasmVersion]
    call GetToolVersion

.check_yasm:
    ; Check for YASM
    lea rcx, [szYasmExe]
    call CheckToolInPath
    test eax, eax
    jz .check_gcc

    ; YASM found
    or dword [availableTools], TOOLCHAIN_YASM
    lea rcx, [szToolFound]
    lea rdx, [szYasmExe]
    call PrintStatusMessage

    ; Get YASM path and version
    lea rcx, [szYasmExe]
    lea rdx, [yasmPath]
    call GetToolPath
    lea rcx, [szYasmVersionCmd]
    lea rdx, [yasmVersion]
    call GetToolVersion

.check_gcc:
    ; Check for GCC
    lea rcx, [szGccExe]
    call CheckToolInPath
    test eax, eax
    jz .check_ms

    ; GCC found
    or dword [availableTools], TOOLCHAIN_GCC
    lea rcx, [szToolFound]
    lea rdx, [szGccExe]
    call PrintStatusMessage

    ; Get GCC path and version
    lea rcx, [szGccExe]
    lea rdx, [gccPath]
    call GetToolPath
    lea rcx, [szGccVersionCmd]
    lea rdx, [gccVersion]
    call GetToolVersion

.check_ms:
    ; Check for Microsoft toolchain
    lea rcx, [szClExe]
    call CheckToolInPath
    test eax, eax
    jz .check_link

    ; CL found
    or dword [availableTools], TOOLCHAIN_MS
    lea rcx, [szToolFound]
    lea rdx, [szClExe]
    call PrintStatusMessage

    ; Get CL path
    lea rcx, [szClExe]
    lea rdx, [clPath]
    call GetToolPath

.check_link:
    ; Check for LINK
    lea rcx, [szLinkExe]
    call CheckToolInPath
    test eax, eax
    jz .detection_complete

    ; LINK found
    lea rcx, [szToolFound]
    lea rdx, [szLinkExe]
    call PrintStatusMessage

    ; Get LINK path
    lea rcx, [szLinkExe]
    lea rdx, [linkPath]
    call GetToolPath

.detection_complete:
    ; Check if any tools were found
    mov eax, [availableTools]
    test eax, eax
    jnz .tools_found

    ; No tools found
    lea rcx, [szNoToolsFound]
    call ShowErrorMessage

.tools_found:
    leave
    ret

; Check if tool exists in PATH
CheckToolInPath:
    push rbp
    mov rbp, rsp

    ; Use where.exe to check if tool exists
    sub rsp, 512

    lea rcx, [rsp+100]    ; Command buffer
    lea rdx, [szWhereCmd]
    lea r8, [rsi]         ; Tool name
    call sprintf

    ; Execute where command
    lea rcx, [rsp+100]
    lea rdx, [rsp+200]    ; Output buffer
    call ExecuteCommand

    ; Check if output contains the tool path
    lea rcx, [rsp+200]
    call strlen
    test eax, eax
    jz .not_found

    mov eax, 1
    jmp .done

.not_found:
    xor eax, eax

.done:
    leave
    ret

; Select the best available toolchain
SelectBestToolchain:
    push rbp
    mov rbp, rsp

    ; Priority: NASM > YASM > GCC > Microsoft
    mov eax, [availableTools]

    test eax, TOOLCHAIN_NASM
    jnz .select_nasm

    test eax, TOOLCHAIN_YASM
    jnz .select_yasm

    test eax, TOOLCHAIN_GCC
    jnz .select_gcc

    test eax, TOOLCHAIN_MS
    jnz .select_ms

    ; No toolchain available
    mov dword [currentToolchain], 0
    jmp .done

.select_nasm:
    mov dword [currentToolchain], TOOLCHAIN_NASM
    lea rcx, [szNasmExe]
    call PrintStatusMessage
    jmp .done

.select_yasm:
    mov dword [currentToolchain], TOOLCHAIN_YASM
    lea rcx, [szYasmExe]
    call PrintStatusMessage
    jmp .done

.select_gcc:
    mov dword [currentToolchain], TOOLCHAIN_GCC
    lea rcx, [szGccExe]
    call PrintStatusMessage
    jmp .done

.select_ms:
    mov dword [currentToolchain], TOOLCHAIN_MS
    lea rcx, [szClExe]
    call PrintStatusMessage

.done:
    leave
    ret

; Build command based on current configuration
BuildWithCurrentConfig:
    push rbp
    mov rbp, rsp
    sub rsp, 512

    ; Get current configuration
    mov eax, [currentConfig]
    mov ebx, [currentToolchain]

    ; Build command based on toolchain and config
    call GetBuildCommand

    ; Execute the build
    lea rcx, [buildCommand]
    call ExecuteBuildProcess

    leave
    ret

; Get build command for current toolchain and configuration
GetBuildCommand:
    push rbp
    mov rbp, rsp

    mov eax, [currentToolchain]
    mov ebx, [currentConfig]

    cmp eax, TOOLCHAIN_NASM
    je .nasm_commands

    cmp eax, TOOLCHAIN_YASM
    je .yasm_commands

    cmp eax, TOOLCHAIN_MS
    je .ms_commands

    ; Unknown toolchain
    lea rcx, [buildCommand]
    lea rdx, [szUnknownToolchain]
    call strcpy
    jmp .done

.nasm_commands:
    lea rsi, [NasmCommands]
    jmp .get_command

.yasm_commands:
    lea rsi, [YasmCommands]
    jmp .get_command

.ms_commands:
    lea rsi, [MsCommands]

.get_command:
    ; Calculate offset: ebx * 256 (each command is max 256 bytes)
    mov eax, ebx
    mov ecx, 256
    mul ecx
    add rsi, rax

    ; Copy command to build buffer
    lea rdi, [buildCommand]
    call strcpy

    ; Replace placeholders
    call ReplacePlaceholders

.done:
    leave
    ret

; Replace placeholders in build command
ReplacePlaceholders:
    push rbp
    mov rbp, rsp

    lea rdi, [buildCommand]

    ; Replace %s (input file) with current file
.replace_input:
    lea rcx, [rdi]
    lea rdx, [szInputPlaceholder]
    call strstr
    test rax, rax
    jz .replace_output

    ; Found %s, replace with current file
    lea rsi, [szCurrentInputFile]
    call ReplaceSubstring

.replace_output:
    lea rcx, [buildCommand]
    lea rdx, [szOutputPlaceholder]
    call strstr
    test rax, rax
    jz .done

    ; Replace %o with output file
    lea rsi, [currentOutputFile]
    call ReplaceSubstring

.done:
    leave
    ret

; Execute command and capture output
ExecuteCommand:
    push rbp
    mov rbp, rsp
    sub rsp, 1024

    ; Create pipes for output capture
    lea rcx, [rsp+100]    ; hReadPipe
    lea rdx, [rsp+108]    ; hWritePipe
    xor r8, r8
    xor r9, r9
    mov qword [rsp+32], 0
    call [CreatePipe]

    ; Setup process
    lea rdi, [rsp+200]    ; STARTUPINFO
    mov rcx, 104
    xor al, al
    rep stosb
    mov dword [rsp+200], 104
    mov dword [rsp+200+44], 0x101  ; STARTF_USESTDHANDLES
    mov rax, [rsp+108]
    mov [rsp+200+64], rax          ; hStdOutput
    mov [rsp+200+72], rax          ; hStdError

    ; Create process
    xor rcx, rcx
    mov rdx, rsi                   ; Command line
    xor r8, r8
    xor r9, r9
    mov qword [rsp+32], 0x0800     ; CREATE_NO_WINDOW
    xor rax, rax
    mov [rsp+40], rax
    mov [rsp+48], rax
    lea rax, [rsp+200]
    mov [rsp+56], rax
    lea rax, [rsp+400]
    mov [rsp+64], rax

    call [CreateProcessA]
    test rax, rax
    jz .error

    ; Close write pipe
    mov rcx, [rsp+108]
    call [CloseHandle]

    ; Wait for completion
    mov rcx, [rsp+400]
    mov rdx, 10000
    call [WaitForSingleObject]

    ; Read output
    mov rcx, [rsp+100]
    mov rdx, rdi                   ; Output buffer
    mov r8, 1023
    lea r9, [rsp+500]
    mov qword [rsp+32], 0
    call [ReadFile]

    ; Close handles
    mov rcx, [rsp+100]
    call [CloseHandle]
    mov rcx, [rsp+400]
    call [CloseHandle]
    mov rcx, [rsp+408]
    call [CloseHandle]

    ; Null terminate
    mov rax, [rsp+500]
    mov byte [rdi + rax], 0

    mov eax, 1
    jmp .done

.error:
    xor eax, eax

.done:
    leave
    ret

; Helper functions
ReplaceSubstring:
    ; Replace occurrence of substring in string
    ; rcx = destination, rsi = replacement
    push rbp
    mov rbp, rsp

    ; This is a simplified version - in practice would need proper string replacement
    call strcpy

    leave
    ret

; String search function
strstr:
    push rbp
    mov rbp, rsp

    ; Simple substring search
    mov al, [rdx]
    test al, al
    jz .not_found

.search_loop:
    mov bl, [rcx]
    test bl, bl
    jz .not_found

    cmp bl, al
    je .check_match

    inc rcx
    jmp .search_loop

.check_match:
    ; Check if full substring matches
    push rcx
    push rdx

.compare:
    mov al, [rdx]
    test al, al
    jz .match_found

    mov bl, [rcx]
    cmp al, bl
    jne .no_match

    inc rcx
    inc rdx
    jmp .compare

.match_found:
    pop rdx
    pop rax
    jmp .done

.no_match:
    pop rdx
    pop rcx
    inc rcx
    jmp .search_loop

.not_found:
    xor rax, rax

.done:
    leave
    ret

; String length
strlen:
    push rbp
    mov rbp, rsp

    xor rax, rax
.count:
    mov cl, [rdi + rax]
    test cl, cl
    jz .done
    inc rax
    jmp .count

.done:
    leave
    ret

section '.data' data readable writeable

; Command templates
szWhereCmd         db "where %s >nul 2>&1", 0
szInputPlaceholder db "%s", 0
szOutputPlaceholder db "%o", 0

; Error messages
szUnknownToolchain db "Unknown toolchain", 0

; Current file (would be set by IDE)
szCurrentInputFile db "source.asm", 0

; Status message format
szWhereFormat      db "where %s", 0

section '.bss' data readable writeable

; Temporary buffers for command execution
tempCmdBuffer      resb 512
outputBuffer       resb 1024

section '.idata' import data readable writeable

library kernel32, 'KERNEL32.DLL',\
        user32, 'USER32.DLL'

import kernel32,\
       CreateProcessA, 'CreateProcessA',\
       WaitForSingleObject, 'WaitForSingleObject',\
       CreatePipe, 'CreatePipe',\
       ReadFile, 'ReadFile',\
       CloseHandle, 'CloseHandle',\
       ExitProcess, 'ExitProcess'

import user32,\
       MessageBoxA, 'MessageBoxA'

; External functions (would be implemented in main IDE)
extern UpdateStatusBar, PrintStatusMessage, ShowErrorMessage, ExecuteBuildProcess
extern sprintf, strcpy
