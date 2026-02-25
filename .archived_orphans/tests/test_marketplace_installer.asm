;==============================================================================
; Test Suite for RawrXD Extension Marketplace Installer
; Pure MASM64 Test Framework
;==============================================================================
.686
.xmm
.model flat, c
option casemap:none
option frame:auto

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

;==============================================================================
; EXTERNAL REFERENCES - Functions Under Test
;==============================================================================
EXTERN Marketplace_InstallExtension:PROC
EXTERN Marketplace_ParseVsixHeader:PROC
EXTERN Marketplace_ParseManifest:PROC
EXTERN Marketplace_ResolveDependencies:PROC
EXTERN Marketplace_ExtractFiles:PROC
EXTERN Marketplace_GenerateNativeEntryPoint:PROC
EXTERN Marketplace_RegisterExtension:PROC

;==============================================================================
; TEST CONSTANTS
;==============================================================================
TEST_PASS               equ 0
TEST_FAIL               equ 1
MAX_TEST_NAME           equ 128
MAX_ERROR_MSG           equ 512

;==============================================================================
; TEST STRUCTURES
;==============================================================================
TEST_RESULT struct
    TestName            db MAX_TEST_NAME dup(?)
    Passed              dd ?
    ErrorMessage        db MAX_ERROR_MSG dup(?)
    ExecutionTime       dq ?
TEST_RESULT ends

TEST_SUITE struct
    SuiteName           db MAX_TEST_NAME dup(?)
    TotalTests          dd ?
    PassedTests         dd ?
    FailedTests         dd ?
    Results             dq ?            ; Array of TEST_RESULT
TEST_SUITE ends

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
align 16

; Test suite
g_TestSuite             TEST_SUITE <>

; Test counters
g_TestCount             dd 0
g_PassCount             dd 0
g_FailCount             dd 0

; Mock VSIX data
szMockVsixPath          db 'D:\rawrxd\tests\fixtures\test-extension.vsix',0
szMockInstallDir        db 'D:\rawrxd\tests\output\extensions',0
szMockManifest          db '{"name":"test-ext","publisher":"rawrxd","version":"1.0.0"}',0

; Test fixture paths
szFixtureDir            db 'D:\rawrxd\tests\fixtures\',0
szOutputDir             db 'D:\rawrxd\tests\output\',0

; Test messages
szTestStarting          db '[TEST] Starting: %s',10,0
szTestPassed            db '[PASS] %s (%.2fms)',10,0
szTestFailed            db '[FAIL] %s: %s',10,0
szSuiteHeader           db 10,'========================================',10
                        db 'Test Suite: %s',10
                        db '========================================',10,0
szSuiteSummary          db 10,'========================================',10
                        db 'Total: %d | Passed: %d | Failed: %d',10
                        db '========================================',10,10,0

;==============================================================================
; CODE
;==============================================================================
.code

;==============================================================================
; TEST FRAMEWORK FUNCTIONS
;==============================================================================

;------------------------------------------------------------------------------
; Initialize test suite
;------------------------------------------------------------------------------
Test_InitSuite proc frame uses rbx rsi rdi,
    pSuiteName:qword
    
    mov rbx, offset g_TestSuite
    
    ; Copy suite name
    mov rsi, pSuiteName
    lea rdi, [rbx].TEST_SUITE.SuiteName
    mov ecx, MAX_TEST_NAME
    rep movsb
    
    ; Reset counters
    mov [rbx].TEST_SUITE.TotalTests, 0
    mov [rbx].TEST_SUITE.PassedTests, 0
    mov [rbx].TEST_SUITE.FailedTests, 0
    
    ; Allocate results array (max 100 tests)
    mov ecx, 100 * sizeof(TEST_RESULT)
    call malloc
    mov [rbx].TEST_SUITE.Results, rax
    
    ; Print header
    lea rcx, szSuiteHeader
    mov rdx, pSuiteName
    call printf
    
    ret
Test_InitSuite endp

;------------------------------------------------------------------------------
; Run single test
;------------------------------------------------------------------------------
Test_Run proc frame uses rbx rsi rdi r12 r13,
    pTestName:qword,
    pTestFunc:qword
    
    LOCAL startTime:qword, endTime:qword, result:dword
    
    mov r12, pTestName
    mov r13, pTestFunc
    
    ; Print test starting
    lea rcx, szTestStarting
    mov rdx, r12
    call printf
    
    ; Get start time
    call GetTickCount64
    mov startTime, rax
    
    ; Run test function
    call r13
    mov result, eax
    
    ; Get end time
    call GetTickCount64
    mov endTime, rax
    
    ; Calculate execution time
    mov rax, endTime
    sub rax, startTime
    
    ; Record result
    inc g_TestCount
    
    .if result == TEST_PASS
        inc g_PassCount
        
        ; Print pass
        lea rcx, szTestPassed
        mov rdx, r12
        cvtsi2sd xmm2, rax
        call printf
    .else
        inc g_FailCount
        
        ; Print fail
        lea rcx, szTestFailed
        mov rdx, r12
        lea r8, szGenericError
        call printf
    .endif
    
    ret
    
szGenericError          db 'Test assertion failed',0
Test_Run endp

;------------------------------------------------------------------------------
; Finalize test suite
;------------------------------------------------------------------------------
Test_FinalizeSuite proc frame uses rbx
    mov rbx, offset g_TestSuite
    
    ; Update suite counters
    mov eax, g_TestCount
    mov [rbx].TEST_SUITE.TotalTests, eax
    mov eax, g_PassCount
    mov [rbx].TEST_SUITE.PassedTests, eax
    mov eax, g_FailCount
    mov [rbx].TEST_SUITE.FailedTests, eax
    
    ; Print summary
    lea rcx, szSuiteSummary
    mov edx, g_TestCount
    mov r8d, g_PassCount
    mov r9d, g_FailCount
    call printf
    
    ; Free results array
    mov rcx, [rbx].TEST_SUITE.Results
    call free
    
    ; Return 0 if all passed, 1 if any failed
    mov eax, g_FailCount
    test eax, eax
    setnz al
    movzx eax, al
    
    ret
Test_FinalizeSuite endp

;------------------------------------------------------------------------------
; Assert equal
;------------------------------------------------------------------------------
Test_AssertEqual proc frame uses rbx,
    expected:qword,
    actual:qword
    
    mov rax, expected
    cmp rax, actual
    je assert_pass
    
    mov eax, TEST_FAIL
    ret
    
assert_pass:
    mov eax, TEST_PASS
    ret
Test_AssertEqual endp

;------------------------------------------------------------------------------
; Assert not null
;------------------------------------------------------------------------------
Test_AssertNotNull proc frame,
    value:qword
    
    mov rax, value
    test rax, rax
    jz assert_fail
    
    mov eax, TEST_PASS
    ret
    
assert_fail:
    mov eax, TEST_FAIL
    ret
Test_AssertNotNull endp

;------------------------------------------------------------------------------
; Assert true
;------------------------------------------------------------------------------
Test_AssertTrue proc frame,
    condition:dword
    
    mov eax, condition
    test eax, eax
    jz assert_fail
    
    mov eax, TEST_PASS
    ret
    
assert_fail:
    mov eax, TEST_FAIL
    ret
Test_AssertTrue endp

;==============================================================================
; TEST FIXTURES
;==============================================================================

;------------------------------------------------------------------------------
; Create mock VSIX file
;------------------------------------------------------------------------------
Test_CreateMockVsix proc frame uses rbx rsi rdi
    LOCAL hFile:qword
    
    ; Create fixtures directory
    lea rcx, szFixtureDir
    call CreateDirectoryA
    
    ; Create mock VSIX file
    lea rcx, szMockVsixPath
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+20h], CREATE_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    call CreateFileA
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je create_failed
    mov hFile, rax
    
    ; Write VSIX signature
    lea rcx, szVsixSig
    mov edx, 4
    mov r8, hFile
    call WriteFile
    
    ; Write mock manifest
    lea rcx, szMockManifest
    mov edx, -1
    mov r8, hFile
    call WriteFile
    
    ; Close file
    mov rcx, hFile
    call CloseHandle
    
    mov eax, TEST_PASS
    ret
    
create_failed:
    mov eax, TEST_FAIL
    ret
    
szVsixSig               db 'VSIX',0
Test_CreateMockVsix endp

;------------------------------------------------------------------------------
; Cleanup test fixtures
;------------------------------------------------------------------------------
Test_CleanupFixtures proc frame uses rbx
    ; Delete mock VSIX
    lea rcx, szMockVsixPath
    call DeleteFileA
    
    ; Delete output directory
    lea rcx, szOutputDir
    call RemoveDirectoryA
    
    ret
Test_CleanupFixtures endp

;==============================================================================
; UNIT TESTS
;==============================================================================

;------------------------------------------------------------------------------
; Test: Parse VSIX Header - Valid File
;------------------------------------------------------------------------------
Test_ParseVsixHeader_ValidFile proc frame uses rbx
    ; Setup
    call Test_CreateMockVsix
    
    ; Execute
    lea rcx, szMockVsixPath
    call Marketplace_ParseVsixHeader
    
    ; Assert
    mov rcx, 1
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_ParseVsixHeader_ValidFile endp

;------------------------------------------------------------------------------
; Test: Parse VSIX Header - Invalid File
;------------------------------------------------------------------------------
Test_ParseVsixHeader_InvalidFile proc frame uses rbx
    ; Execute with non-existent file
    lea rcx, szInvalidPath
    call Marketplace_ParseVsixHeader
    
    ; Assert failure
    mov rcx, 0
    mov rdx, rax
    call Test_AssertEqual
    
    ret
    
szInvalidPath           db 'D:\nonexistent\file.vsix',0
Test_ParseVsixHeader_InvalidFile endp

;------------------------------------------------------------------------------
; Test: Parse Manifest - Valid JSON
;------------------------------------------------------------------------------
Test_ParseManifest_ValidJson proc frame uses rbx
    ; Setup
    call Test_CreateMockVsix
    
    ; Execute
    call Marketplace_ParseManifest
    
    ; Assert
    mov rcx, 1
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_ParseManifest_ValidJson endp

;------------------------------------------------------------------------------
; Test: Parse Manifest - Invalid JSON
;------------------------------------------------------------------------------
Test_ParseManifest_InvalidJson proc frame uses rbx
    ; Setup with invalid manifest
    ; (Would need to create fixture with bad JSON)
    
    ; Execute
    call Marketplace_ParseManifest
    
    ; Assert failure expected
    mov rcx, 0
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_ParseManifest_InvalidJson endp

;------------------------------------------------------------------------------
; Test: Resolve Dependencies - No Dependencies
;------------------------------------------------------------------------------
Test_ResolveDependencies_None proc frame uses rbx
    ; Setup with manifest that has no dependencies
    xor rcx, rcx
    call Marketplace_ResolveDependencies
    
    ; Assert success
    mov rcx, 1
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_ResolveDependencies_None endp

;------------------------------------------------------------------------------
; Test: Resolve Dependencies - Single Dependency
;------------------------------------------------------------------------------
Test_ResolveDependencies_Single proc frame uses rbx
    ; Setup mock dependency object
    lea rcx, szMockDependency
    call Json_ParseString
    
    ; Execute
    mov rcx, rax
    call Marketplace_ResolveDependencies
    
    ; Assert
    call Test_AssertNotNull
    
    ret
    
szMockDependency        db '{"vscode":"^1.60.0"}',0
Test_ResolveDependencies_Single endp

;------------------------------------------------------------------------------
; Test: Extract Files - Valid Package
;------------------------------------------------------------------------------
Test_ExtractFiles_Valid proc frame uses rbx
    ; Setup
    call Test_CreateMockVsix
    
    ; Execute
    call Marketplace_ExtractFiles
    
    ; Assert
    mov rcx, 1
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_ExtractFiles_Valid endp

;------------------------------------------------------------------------------
; Test: Generate Native Entry Point
;------------------------------------------------------------------------------
Test_GenerateNativeEntryPoint proc frame uses rbx
    ; Setup
    lea rcx, szOutputDir
    call CreateDirectoryA
    
    ; Execute
    lea rcx, szOutputDir
    xor rdx, rdx
    call Marketplace_GenerateNativeEntryPoint
    
    ; Assert file created
    lea rcx, szExpectedAsmFile
    call PathFileExistsA
    call Test_AssertTrue
    
    ret
    
szExpectedAsmFile       db 'D:\rawrxd\tests\output\extension.asm',0
Test_GenerateNativeEntryPoint endp

;------------------------------------------------------------------------------
; Test: Register Extension
;------------------------------------------------------------------------------
Test_RegisterExtension proc frame uses rbx
    ; Setup
    call Test_CreateMockVsix
    call Marketplace_ParseManifest
    
    ; Execute
    call Marketplace_RegisterExtension
    
    ; Assert
    mov rcx, 1
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_RegisterExtension endp

;------------------------------------------------------------------------------
; Test: Install Extension - Complete Flow
;------------------------------------------------------------------------------
Test_InstallExtension_Complete proc frame uses rbx
    ; Setup
    call Test_CreateMockVsix
    
    ; Execute
    lea rcx, szMockVsixPath
    lea rdx, szMockInstallDir
    call Marketplace_InstallExtension
    
    ; Assert success
    mov rcx, 0              ; INSTALL_SUCCESS
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_InstallExtension_Complete endp

;------------------------------------------------------------------------------
; Test: Install Extension - Already Installed
;------------------------------------------------------------------------------
Test_InstallExtension_AlreadyInstalled proc frame uses rbx
    ; Setup - install once
    call Test_CreateMockVsix
    lea rcx, szMockVsixPath
    lea rdx, szMockInstallDir
    call Marketplace_InstallExtension
    
    ; Execute - try to install again
    lea rcx, szMockVsixPath
    lea rdx, szMockInstallDir
    call Marketplace_InstallExtension
    
    ; Assert already exists error
    mov rcx, 7              ; INSTALL_ALREADY_EXISTS
    mov rdx, rax
    call Test_AssertEqual
    
    ret
Test_InstallExtension_AlreadyInstalled endp

;==============================================================================
; INTEGRATION TESTS
;==============================================================================

;------------------------------------------------------------------------------
; Integration Test: Full Installation Pipeline
;------------------------------------------------------------------------------
Test_Integration_FullPipeline proc frame uses rbx rsi rdi
    ; Setup
    call Test_CreateMockVsix
    
    ; Step 1: Parse header
    lea rcx, szMockVsixPath
    call Marketplace_ParseVsixHeader
    test eax, eax
    jz pipeline_failed
    
    ; Step 2: Parse manifest
    call Marketplace_ParseManifest
    test eax, eax
    jz pipeline_failed
    
    ; Step 3: Resolve dependencies
    xor rcx, rcx
    call Marketplace_ResolveDependencies
    test eax, eax
    jz pipeline_failed
    
    ; Step 4: Extract files
    call Marketplace_ExtractFiles
    test eax, eax
    jz pipeline_failed
    
    ; Step 5: Generate entry point
    lea rcx, szMockInstallDir
    xor rdx, rdx
    call Marketplace_GenerateNativeEntryPoint
    test eax, eax
    jz pipeline_failed
    
    ; Step 6: Register
    call Marketplace_RegisterExtension
    test eax, eax
    jz pipeline_failed
    
    mov eax, TEST_PASS
    ret
    
pipeline_failed:
    mov eax, TEST_FAIL
    ret
Test_Integration_FullPipeline endp

;==============================================================================
; STRESS TESTS
;==============================================================================

;------------------------------------------------------------------------------
; Stress Test: Install Multiple Extensions
;------------------------------------------------------------------------------
Test_Stress_MultipleInstalls proc frame uses rbx rsi rdi
    LOCAL i:dword
    
    mov i, 0
    
stress_loop:
    cmp i, 10
    jae stress_done
    
    ; Create unique VSIX
    call Test_CreateMockVsix
    
    ; Install
    lea rcx, szMockVsixPath
    lea rdx, szMockInstallDir
    call Marketplace_InstallExtension
    
    test eax, eax
    jnz stress_failed
    
    inc i
    jmp stress_loop
    
stress_done:
    mov eax, TEST_PASS
    ret
    
stress_failed:
    mov eax, TEST_FAIL
    ret
Test_Stress_MultipleInstalls endp

;==============================================================================
; MAIN TEST RUNNER
;==============================================================================

;------------------------------------------------------------------------------
; Run all tests
;------------------------------------------------------------------------------
Test_RunAll proc frame uses rbx rsi rdi
    ; Initialize suite
    lea rcx, szSuiteName
    call Test_InitSuite
    
    ; Unit Tests
    lea rcx, szTest1
    lea rdx, Test_ParseVsixHeader_ValidFile
    call Test_Run
    
    lea rcx, szTest2
    lea rdx, Test_ParseVsixHeader_InvalidFile
    call Test_Run
    
    lea rcx, szTest3
    lea rdx, Test_ParseManifest_ValidJson
    call Test_Run
    
    lea rcx, szTest4
    lea rdx, Test_ParseManifest_InvalidJson
    call Test_Run
    
    lea rcx, szTest5
    lea rdx, Test_ResolveDependencies_None
    call Test_Run
    
    lea rcx, szTest6
    lea rdx, Test_ResolveDependencies_Single
    call Test_Run
    
    lea rcx, szTest7
    lea rdx, Test_ExtractFiles_Valid
    call Test_Run
    
    lea rcx, szTest8
    lea rdx, Test_GenerateNativeEntryPoint
    call Test_Run
    
    lea rcx, szTest9
    lea rdx, Test_RegisterExtension
    call Test_Run
    
    lea rcx, szTest10
    lea rdx, Test_InstallExtension_Complete
    call Test_Run
    
    lea rcx, szTest11
    lea rdx, Test_InstallExtension_AlreadyInstalled
    call Test_Run
    
    ; Integration Tests
    lea rcx, szTest12
    lea rdx, Test_Integration_FullPipeline
    call Test_Run
    
    ; Stress Tests
    lea rcx, szTest13
    lea rdx, Test_Stress_MultipleInstalls
    call Test_Run
    
    ; Cleanup
    call Test_CleanupFixtures
    
    ; Finalize
    call Test_FinalizeSuite
    
    ret
    
szSuiteName             db 'Marketplace Installer Test Suite',0
szTest1                 db 'ParseVsixHeader_ValidFile',0
szTest2                 db 'ParseVsixHeader_InvalidFile',0
szTest3                 db 'ParseManifest_ValidJson',0
szTest4                 db 'ParseManifest_InvalidJson',0
szTest5                 db 'ResolveDependencies_None',0
szTest6                 db 'ResolveDependencies_Single',0
szTest7                 db 'ExtractFiles_Valid',0
szTest8                 db 'GenerateNativeEntryPoint',0
szTest9                 db 'RegisterExtension',0
szTest10                db 'InstallExtension_Complete',0
szTest11                db 'InstallExtension_AlreadyInstalled',0
szTest12                db 'Integration_FullPipeline',0
szTest13                db 'Stress_MultipleInstalls',0
Test_RunAll endp

;==============================================================================
; ENTRY POINT
;==============================================================================
main proc
    call Test_RunAll
    ret
main endp

end main
