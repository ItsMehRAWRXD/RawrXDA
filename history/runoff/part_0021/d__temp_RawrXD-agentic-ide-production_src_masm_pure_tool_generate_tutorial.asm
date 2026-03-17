; ============================================================================
; TOOL 17: Generate Tutorial - Pure x64 MASM
; Autonomous tutorial generation
; ============================================================================

option casemap:none

; ============================================================================
; WIN32 API DECLARATIONS
; ============================================================================
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC
EXTERN lstrlenA:PROC

; Constants
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
GENERIC_WRITE       equ 40000000h
CREATE_ALWAYS       equ 2
INVALID_HANDLE_VALUE equ -1

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC Tool_GenerateTutorial

; ============================================================================
; GLOBAL STATE
; ============================================================================
.data
    g_qwToolCalls      dq 0
    g_dwLastError      dd 0

; File names
tutorialFileName     db 'TUTORIAL.md',0

; Tutorial templates
szTutorialHeader     db '# Tutorial: Getting Started',10,10,'## Overview',10,10,'This tutorial was generated automatically by the RawrXD autonomous agent.',10,10,0
szTutorialStep1      db '## Step 1: Setup',10,10,'1. Install dependencies',10,'2. Configure environment',10,'3. Run initial setup',10,10,0
szTutorialStep2      db '## Step 2: Basic Usage',10,10,'1. Import the module',10,'2. Create an instance',10,'3. Call basic methods',10,10,0
szTutorialStep3      db '## Step 3: Advanced Features',10,10,'1. Configure advanced settings',10,'2. Use advanced APIs',10,'3. Handle edge cases',10,10,0
szTutorialStep4      db '## Step 4: Testing',10,10,'1. Write unit tests',10,'2. Run test suite',10,'3. Verify functionality',10,10,0
szTutorialStep5      db '## Step 5: Deployment',10,10,'1. Build for production',10,'2. Deploy to server',10,'3. Monitor performance',10,10,0
szTutorialFooter     db '## Conclusion',10,10,'This tutorial covers the complete workflow. For more details, consult the API documentation.',10,0

; ============================================================================
; TOOL IMPLEMENTATION
; ============================================================================
.code

Tool_GenerateTutorial PROC
    ; RCX = JSON params string
    inc g_qwToolCalls
    
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save params
    
    ; Validate parameters
    test rbx, rbx
    jz @invalidParams
    
    ; Create tutorial file
    lea rcx, tutorialFileName
    mov rdx, GENERIC_WRITE
    mov r8, 0
    mov r9, 0
    mov qword ptr [rsp+32], CREATE_ALWAYS
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je @fileError
    
    mov rsi, rax                    ; File handle
    
    ; Write tutorial header
    lea rcx, szTutorialHeader
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write step 1
    lea rcx, szTutorialStep1
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write step 2
    lea rcx, szTutorialStep2
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write step 3
    lea rcx, szTutorialStep3
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write step 4
    lea rcx, szTutorialStep4
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write step 5
    lea rcx, szTutorialStep5
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write footer
    lea rcx, szTutorialFooter
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Close file
    mov rcx, rsi
    call CloseHandle
    
    mov rax, TRUE                   ; Success
    jmp @exit
    
@invalidParams:
    xor rax, rax
    jmp @exit
    
@fileError:
    call GetLastError
    mov g_dwLastError, eax
    xor rax, rax
    
@exit:
    add rsp, 32
    pop rsi
    pop rbx
    ret
Tool_GenerateTutorial ENDP

END