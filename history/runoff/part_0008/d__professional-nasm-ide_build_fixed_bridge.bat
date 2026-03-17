@echo off
echo Building fixed bridge_init DLL...

REM Create lib directory if it doesn't exist
if not exist lib mkdir lib

REM Assemble the fixed bridge_init
nasm -f win64 bridge_init_fixed.asm -o build\bridge_init_fixed.obj
if errorlevel 1 (
    echo Failed to assemble bridge_init_fixed.asm
    goto error
)

REM Link as DLL with proper entry point
gcc -shared -o lib\nasm_ide_core_fixed.dll build\bridge_init_fixed.obj -Wl,--out-implib,lib\nasm_ide_core_fixed.lib -e DllMain
if errorlevel 1 (
    echo Failed to link DLL, trying with LD...
    ld -shared -o lib\nasm_ide_core_fixed.dll build\bridge_init_fixed.obj --entry DllMain
    if errorlevel 1 (
        echo Failed to link DLL with both GCC and LD
        goto error
    )
)

echo Fixed DLL built successfully!

REM Create test program
echo Creating test for fixed version...

REM Create modified test that uses the fixed DLL
(
echo ; Test for fixed bridge_init
echo bits 64
echo default rel
echo.
echo extern LoadLibraryA
echo extern GetProcAddress
echo extern MessageBoxA
echo extern ExitProcess
echo.
echo section .data
echo     dllPath db "d:\\professional-nasm-ide\\lib\\nasm_ide_core_fixed.dll", 0
echo     funcName db "bridge_init", 0
echo     success_title db "Success", 0
echo     error_title db "Error", 0
echo     success_msg db "Fixed bridge_init called successfully!", 0
echo     load_error_msg db "Failed to load fixed DLL", 0
echo     func_error_msg db "Failed to get bridge_init function", 0
echo     call_error_msg db "Fixed bridge_init returned error", 0
echo.
echo section .bss
echo     hDll resq 1
echo     pFunc resq 1
echo.
echo section .text
echo global main
echo.
echo main:
echo     ; Proper Windows x64 ABI - ensure stack alignment
echo     push rbp
echo     mov rbp, rsp
echo     and rsp, -16        ; Align to 16-byte boundary
echo     sub rsp, 32        ; Shadow space
echo.    
echo     ; Load fixed DLL
echo     lea rcx, [dllPath]
echo     call LoadLibraryA
echo     test rax, rax
echo     jz .load_error
echo     mov [hDll], rax
echo.    
echo     ; Get bridge_init function
echo     mov rcx, rax
echo     lea rdx, [funcName]
echo     call GetProcAddress
echo     test rax, rax
echo     jz .func_error
echo     mov [pFunc], rax
echo.    
echo     ; Call bridge_init with proper ABI
echo     call rax
echo     test eax, eax
echo     jnz .call_error
echo.    
echo     ; Success
echo     xor ecx, ecx
echo     lea rdx, [success_msg]
echo     lea r8, [success_title]
echo     mov r9d, 0x40
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .load_error:
echo     xor ecx, ecx
echo     lea rdx, [load_error_msg]
echo     lea r8, [error_title]
echo     mov r9d, 0x10
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .func_error:
echo     xor ecx, ecx
echo     lea rdx, [func_error_msg]
echo     lea r8, [error_title]
echo     mov r9d, 0x10
echo     call MessageBoxA
echo     jmp .exit
echo.
echo .call_error:
echo     xor ecx, ecx
echo     lea rdx, [call_error_msg]
echo     lea r8, [error_title]
echo     mov r9d, 0x10
echo     call MessageBoxA
echo.
echo .exit:
echo     mov rsp, rbp
echo     pop rbp
echo     xor ecx, ecx
echo     call ExitProcess
) > test_fixed_bridge.asm

REM Assemble test
nasm -f win64 test_fixed_bridge.asm -o build\test_fixed_bridge.obj
if errorlevel 1 (
    echo Failed to assemble test
    goto error
)

REM Link test executable
gcc -m64 -mwindows -o bin\test_fixed_bridge.exe build\test_fixed_bridge.obj -lkernel32 -luser32
if errorlevel 1 (
    echo Failed to link test executable
    goto error
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Testing fixed bridge_init...
bin\test_fixed_bridge.exe

echo.
echo If you saw a "Success" message, the fix worked!
goto end

:error
echo Build failed!
pause

:end