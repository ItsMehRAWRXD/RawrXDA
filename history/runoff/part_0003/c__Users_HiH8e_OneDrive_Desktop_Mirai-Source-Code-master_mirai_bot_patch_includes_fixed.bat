@echo off
REM Batch script to patch includes for Windows compatibility in all .c files

for %%F in (*.c) do (
    powershell -Command ^
    "(Get-Content -Raw '%%F') -replace '([#]include <unistd.h>|[#]include <sys/socket.h>|[#]include <arpa/inet.h>|[#]include <fcntl.h>|[#]include <sys/select.h>|[#]include <signal.h>|[#]include <errno.h>|[#]include <linux/ip.h>|[#]include <sys/types.h>)', '\n#ifdef _WIN32\n#include <winsock2.h>\n#include <windows.h>\n#pragma comment(lib, \"ws2_32.lib\")\n#else\n$&\n#endif' | Set-Content '%%F.tmp'; Move-Item -Force '%%F.tmp' '%%F'"
)

echo All .c files patched for Windows includes.
pause
