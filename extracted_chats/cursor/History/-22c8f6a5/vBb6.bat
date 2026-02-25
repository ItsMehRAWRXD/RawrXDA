@echo off
REM ========================================
REM Enhanced ASM IDE Build Script
REM ========================================
REM Builds the full-featured ASM IDE with all enhancements
REM ========================================

echo Building Enhanced ASM IDE
echo =====================
echo Features:
echo   - File editing and management
echo   - NASM compilation integration
echo   - Step-by-step debugging
echo   - Syntax highlighting
echo   - Multi-file project support
echo.

REM Configuration
set ASM_FILE=asm_ide_enhanced.asm
set OUTPUT_FILE=asm_ide_enhanced.exe
set OBJECT_FILE=asm_ide_enhanced.o

REM Find NASM
set NASM_PATH=
if exist "C:\Program Files\NASM\nasm.exe" set NASM_PATH="C:\Program Files\NASM\nasm.exe"
if exist "C:\nasm\nasm.exe" set NASM_PATH="C:\nasm\nasm.exe"
if exist "C:\msys64\usr\bin\nasm.exe" set NASM_PATH="C:\msys64\usr\bin\nasm.exe"

if "%NASM_PATH%"=="" (
    where nasm >nul 2>nul
    if %errorlevel% equ 0 (
        set NASM_PATH=nasm
    ) else (
        echo NASM not found. Please install NASM.
        echo Download from: https://www.nasm.us/
        pause
        exit /b 1
    )
)

echo Using NASM: %NASM_PATH%

REM Step 1: Assemble the enhanced IDE
echo.
echo Step 1: Assembling enhanced ASM IDE...
%NASM_PATH% -f elf64 -o %OBJECT_FILE% %ASM_FILE%
if %errorlevel% neq 0 (
    echo Assembly failed
    pause
    exit /b 1
)
echo Assembly successful

REM Step 2: Link the executable
echo.
echo Step 2: Linking executable...
gcc -o %OUTPUT_FILE% %OBJECT_FILE% -static
if %errorlevel% neq 0 (
    echo GCC linking failed, trying ld...
    ld -o %OUTPUT_FILE% %OBJECT_FILE%
    if %errorlevel% neq 0 (
        echo Both linking methods failed
        pause
        exit /b 1
    )
)
echo Linking successful

REM Step 3: Create project directory structure
echo.
echo Step 3: Creating project structure...
if not exist "projects" mkdir projects
if not exist "projects\examples" mkdir projects\examples
if not exist "projects\templates" mkdir projects\templates
if not exist "projects\debug" mkdir projects\debug

REM Step 4: Create example files
echo.
echo Step 4: Creating example files...

REM Create example assembly file
(
echo ; Example Assembly Program
echo ; Created by Enhanced ASM IDE
echo.
echo section .data
echo     msg db 'Hello, Enhanced ASM IDE!', 10, 0
echo     msg_len equ $ - msg
echo.
echo section .text
echo     global _start
echo.
echo _start:
echo     ; Display message
echo     mov rax, 1          ; sys_write
echo     mov rdi, 1          ; stdout
echo     mov rsi, msg
echo     mov rdx, msg_len
echo     syscall
echo.
echo     ; Exit
echo     mov rax, 60         ; sys_exit
echo     mov rdi, 0
echo     syscall
) > projects\examples\hello.asm

REM Create template file
(
echo ; Assembly Template
echo ; Use this as a starting point for new projects
echo.
echo section .data
echo     ; Data declarations go here
echo.
echo section .bss
echo     ; Uninitialized data goes here
echo.
echo section .text
echo     global _start
echo.
echo _start:
echo     ; Your code goes here
echo.
echo     ; Exit program
echo     mov rax, 60
echo     mov rdi, 0
echo     syscall
) > projects\templates\template.asm

REM Step 5: Create configuration files
echo.
echo Step 5: Creating configuration files...

REM Create IDE configuration
(
echo [ASM_IDE_CONFIG]
echo version=2.0
echo default_project=projects
echo nasm_path=%NASM_PATH%
echo syntax_highlighting=enabled
echo auto_save=enabled
echo debug_mode=enabled
echo project_management=enabled
) > asm_ide.conf

REM Step 6: Clean up
del %OBJECT_FILE%

echo.
echo Enhanced ASM IDE Build Complete!
echo ================================
echo.
echo Executable: %OUTPUT_FILE%
echo Configuration: asm_ide.conf
echo Project Directory: projects\
echo.
echo Features Available:
echo   - File Operations: new, open, save, saveas, list
echo   - Compilation: compile with NASM integration
echo   - Debugging: step, break, watch commands
echo   - Syntax Highlighting: Assembly language support
echo   - Project Management: Multi-file projects
echo.
echo Example Usage:
echo   %OUTPUT_FILE%
echo   ASM> new hello.asm
echo   ASM> open projects\examples\hello.asm
echo   ASM> compile
echo   ASM> run
echo   ASM> debug
echo   ASM> step
echo.
echo Project Structure:
echo   projects\
echo   ├── examples\        # Example assembly files
echo   ├── templates\       # Project templates
echo   └── debug\          # Debug output files
echo.

pause
