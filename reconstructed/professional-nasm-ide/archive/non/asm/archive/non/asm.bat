@echo off
REM Archive non-ASM assets into archive_non_asm directory
echo Creating archive directories...
if not exist "d:\professional-nasm-ide\archive_non_asm\frontend" mkdir "d:\professional-nasm-ide\archive_non_asm\frontend"
if not exist "d:\professional-nasm-ide\archive_non_asm\swarm" mkdir "d:\professional-nasm-ide\archive_non_asm\swarm"
if not exist "d:\professional-nasm-ide\archive_non_asm\swarm-agent" mkdir "d:\professional-nasm-ide\archive_non_asm\swarm-agent"
if not exist "d:\professional-nasm-ide\archive_non_asm\docs" mkdir "d:\professional-nasm-ide\archive_non_asm\docs"
if not exist "d:\professional-nasm-ide\archive_non_asm\examples" mkdir "d:\professional-nasm-ide\archive_non_asm\examples"
if not exist "d:\professional-nasm-ide\archive_non_asm\tools" mkdir "d:\professional-nasm-ide\archive_non_asm\tools"

echo Moving directories (if present)...
move "d:\professional-nasm-ide\frontend" "d:\professional-nasm-ide\archive_non_asm\frontend" 2>nul
move "d:\professional-nasm-ide\swarm" "d:\professional-nasm-ide\archive_non_asm\swarm" 2>nul
move "d:\professional-nasm-ide\swarm-agent" "d:\professional-nasm-ide\archive_non_asm\swarm-agent" 2>nul
move "d:\professional-nasm-ide\docs" "d:\professional-nasm-ide\archive_non_asm\docs" 2>nul
move "d:\professional-nasm-ide\examples" "d:\professional-nasm-ide\archive_non_asm\examples" 2>nul
move "d:\professional-nasm-ide\tools" "d:\professional-nasm-ide\archive_non_asm\tools" 2>nul

echo Moving root scripts and non-ASM files...
move "d:\professional-nasm-ide\run_swarm.py" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\swarm_controller.py" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\build-ollama.bat" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\OLLAMA-WRAPPER-README.md" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\OPEN-IDE.bat" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\start_ide.bat" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\swarm_config.json" "d:\professional-nasm-ide\archive_non_asm\" 2>nul

echo Moving non-ASM source files from src/...
move "d:\professional-nasm-ide\src\ollama_bridge.py" "d:\professional-nasm-ide\archive_non_asm\" 2>nul
move "d:\professional-nasm-ide\src\ollama_wrapper_win.c" "d:\professional-nasm-ide\archive_non_asm\" 2>nul

echo Archive complete. Review d:\professional-nasm-ide\archive_non_asm for moved items.
echo Note: You can restore files by moving them back from the archive directory.
pause
