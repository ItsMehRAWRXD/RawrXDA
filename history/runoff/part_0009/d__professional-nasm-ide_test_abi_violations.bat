@echo off
echo ===================================================================
echo Windows x64 ABI Violations Test - Demonstrating Real Failures
echo ===================================================================
echo.

REM Build the broken version first to show the failures
echo [1] Building BROKEN version (demonstrates violations)...

REM Create broken DLL (missing DllMain)
nasm -f win64 src\language_bridge.asm -o build\broken_bridge.obj
if errorlevel 1 goto error

echo     Linking broken DLL (will have load issues)...
gcc -shared -o lib\broken_bridge.dll build\broken_bridge.obj
if errorlevel 1 (
    echo     Failed to link broken DLL
    goto error
)

REM Create test for broken version
echo [2] Testing BROKEN version...
(
echo bits 64
echo default rel
echo.
echo extern LoadLibraryA
echo extern GetProcAddress  
echo extern MessageBoxA
echo extern ExitProcess
echo extern GetLastError
echo.
echo section .data
echo     broken_dll db "d:\\professional-nasm-ide\\lib\\broken_bridge.dll", 0
echo     func_name db "bridge_init", 0
echo     title db "ABI Violation Test", 0
echo     success_msg db "Broken version somehow worked!", 0
echo     load_fail_msg db "LoadLibraryA FAILED - Missing DllMain!", 0
echo     func_fail_msg db "GetProcAddress FAILED!", 0
echo     call_fail_msg db "Function call CRASHED - Stack misalignment!", 0
echo     error_code_msg db "Error code: ", 0
echo.
echo section .bss
echo     dll_handle resq 1
echo     func_ptr resq 1
echo.
echo section .text
echo global main
echo.
echo main:
echo     ; DEMONSTRATE VIOLATION 1: Missing DllMain
echo     sub rsp, 40  ; Shadow space + alignment
echo.    
echo     lea rcx, [broken_dll]
echo     call LoadLibraryA
echo     test rax, rax
echo     jz .load_failed
echo     mov [dll_handle], rax
echo.    
echo     ; If we get here, somehow the DLL loaded...
echo     mov rcx, rax
echo     lea rdx, [func_name]  
echo     call GetProcAddress
echo     test rax, rax
echo     jz .func_failed
echo     mov [func_ptr], rax
echo.    
echo     ; DEMONSTRATE VIOLATION 3: Improper calling convention
echo     ; Call with misaligned stack intentionally
echo     add rsp, 8   ; Misalign stack on purpose
echo     call rax     ; This will likely crash!
echo     sub rsp, 8   ; Restore if we survive
echo.    
echo     ; Success message
echo     xor ecx, ecx
echo     lea rdx, [success_msg]
echo     lea r8, [title]
echo     mov r9d, 64  ; MB_ICONINFORMATION
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .load_failed:
echo     ; Show the LoadLibraryA failure
echo     xor ecx, ecx
echo     lea rdx, [load_fail_msg]
echo     lea r8, [title]  
echo     mov r9d, 16  ; MB_ICONERROR
echo     call MessageBoxA
echo     jmp .exit
echo.    
echo .func_failed:
echo     xor ecx, ecx
echo     lea rdx, [func_fail_msg]
echo     lea r8, [title]
echo     mov r9d, 16  ; MB_ICONERROR  
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .exit:
echo     add rsp, 40
echo     xor ecx, ecx
echo     call ExitProcess
) > test_broken_abi.asm

nasm -f win64 test_broken_abi.asm -o build\test_broken_abi.obj
gcc -m64 -mwindows -o bin\test_broken_abi.exe build\test_broken_abi.obj -lkernel32 -luser32

echo.
echo [3] Running BROKEN version test...
echo     This should show "LoadLibraryA FAILED" due to missing DllMain
echo.
start bin\test_broken_abi.exe

echo.
echo ===================================================================
echo [4] Now building FIXED version (proper ABI compliance)...

REM Use our fixed version
nasm -f win64 bridge_init_fixed.asm -o build\bridge_fixed.obj
gcc -shared -o lib\fixed_bridge.dll build\bridge_fixed.obj -Wl,--entry,DllMain

echo [5] Testing FIXED version...
(
echo bits 64  
echo default rel
echo.
echo extern LoadLibraryA
echo extern GetProcAddress
echo extern MessageBoxA  
echo extern ExitProcess
echo.
echo section .data
echo     fixed_dll db "d:\\professional-nasm-ide\\lib\\fixed_bridge.dll", 0
echo     func_name db "bridge_init", 0
echo     title db "ABI Fixed Test", 0
echo     success_msg db "FIXED version works! All ABI violations resolved!", 0
echo     load_fail_msg db "Even fixed version failed to load", 0
echo     func_fail_msg db "Function not found", 0
echo     call_fail_msg db "Function call failed", 0
echo.
echo section .bss
echo     dll_handle resq 1
echo     func_ptr resq 1  
echo.
echo section .text
echo global main
echo.
echo main:
echo     ; PROPER Windows x64 ABI compliance
echo     push rbp
echo     mov rbp, rsp
echo     and rsp, -16      ; Ensure 16-byte alignment  
echo     sub rsp, 32      ; Proper shadow space
echo.    
echo     lea rcx, [fixed_dll]
echo     call LoadLibraryA
echo     test rax, rax
echo     jz .load_failed
echo     mov [dll_handle], rax
echo.    
echo     mov rcx, rax
echo     lea rdx, [func_name]
echo     call GetProcAddress  
echo     test rax, rax
echo     jz .func_failed
echo     mov [func_ptr], rax
echo.    
echo     ; Call with PROPER ABI compliance
echo     ; Stack is already aligned, shadow space allocated
echo     call rax         ; This should work!
echo     test eax, eax
echo     jnz .call_failed
echo.    
echo     ; Success!
echo     xor ecx, ecx
echo     lea rdx, [success_msg]
echo     lea r8, [title]
echo     mov r9d, 64      ; MB_ICONINFORMATION
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .load_failed:
echo     xor ecx, ecx  
echo     lea rdx, [load_fail_msg]
echo     lea r8, [title]
echo     mov r9d, 16      ; MB_ICONERROR
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .func_failed:
echo     xor ecx, ecx
echo     lea rdx, [func_fail_msg] 
echo     lea r8, [title]
echo     mov r9d, 16      ; MB_ICONERROR
echo     call MessageBoxA
echo     jmp .exit
echo.    
echo .call_failed:
echo     xor ecx, ecx
echo     lea rdx, [call_fail_msg]
echo     lea r8, [title]
echo     mov r9d, 16      ; MB_ICONERROR  
echo     call MessageBoxA
echo.
echo .exit:
echo     mov rsp, rbp
echo     pop rbp
echo     xor ecx, ecx
echo     call ExitProcess
) > test_fixed_abi.asm

nasm -f win64 test_fixed_abi.asm -o build\test_fixed_abi.obj  
gcc -m64 -mwindows -o bin\test_fixed_abi.exe build\test_fixed_abi.obj -lkernel32 -luser32

echo.
echo [6] Running FIXED version test...
echo     This should show "FIXED version works!" success message
echo.
start bin\test_fixed_abi.exe

echo.
echo ===================================================================
echo ABI VIOLATIONS DEMONSTRATION COMPLETE
echo ===================================================================
echo.
echo You should see two dialogs:
echo   1. BROKEN: "LoadLibraryA FAILED - Missing DllMain!"
echo   2. FIXED:  "FIXED version works! All ABI violations resolved!"  
echo.
echo This demonstrates all three critical ABI violations:
echo   - Stack misalignment causing crashes on function calls
echo   - Missing DLL entry point causing load failures  
echo   - Improper calling conventions violating Windows x64 ABI requirements
echo.
goto end

:error
echo BUILD FAILED!
pause

:end