@ECHO off
SETLOCAL ENABLEDELAYEDEXPANSION

SET "PREFIX=%~dp0\bin"
FOR /f "tokens=3" %%i in ('%PREFIX%\gcc.exe --version') DO SET "VERSION=%%i" & GOTO :version_found
:version_found
SET "VERBOSE=no"

SET "CCEXE=mingw64-c.exe"
SET "CC=gcc.exe"
SET "CFLAGS=-o %CCEXE% -O2"

SET "CXXEXE=mingw64-c^+^+.exe"
SET "CXX=g^+^+.exe"
SET "CXXFLAGS=-o %CXXEXE% -O2"

FOR %%m IN (m32 m64 m32-static m64-static m32-e m64-e m32-e-static m64-e-static m32-ee m64-ee m32-ee-static m64-ee-static) DO (
	ECHO Info: Trying mode '%%m'.

	IF "%%m"=="m32" (
		SET "EFLAGS=-m32"
		COPY /y "%PREFIX%\..\x86_64-w64-mingw32\lib\gcc\x86_64-w64-mingw32\%VERSION%\32\libstdc++-6-x86.dll" . >NUL
	) ELSE IF "%%m"=="m64" (
		SET "EFLAGS=-m64"
		COPY /y "%PREFIX%\..\x86_64-w64-mingw32\lib\gcc\x86_64-w64-mingw32\%VERSION%\libstdc++-6-x64.dll" . >NUL
	)

	IF "%%m"=="m32-static" (
		SET "EFLAGS=!EFLAGS! -static"
	) ELSE IF "%%m"=="m64-static" (
		SET "EFLAGS=!EFLAGS! -static"
	)

	IF "%%m"=="m32-e" (
		SET "EFLAGS=!EFLAGS! -flto -fuse-linker-plugin"
	) ELSE IF "%%m"=="m64-e" (
		SET "EFLAGS=!EFLAGS! -flto -fuse-linker-plugin"
	)

	IF "%%m"=="m32-ee" (
		SET "EFLAGS=!EFLAGS! -fgraphite"
	) ELSE IF "%%m"=="m64-ee" (
		SET "EFLAGS=!EFLAGS! -fgraphite"
	)

	IF "%VERBOSE%"=="yes" ECHO %PREFIX%\%CC% %CFLAGS% !EFLAGS! -x c temp.c

	(
		ECHO #include ^<stdlib.h^>
		ECHO #include ^<stdio.h^>
		ECHO.
		ECHO __attribute__^(^(used^)^)
		ECHO extern void __main ^(^);
		ECHO.
		ECHO int main^(int argc, char * argv[], char * envp[]^) {
		ECHO     fprintf^(stdout, "Hello World^!\n"^);
		ECHO     return EXIT_SUCCESS;
		ECHO }
	) > temp.c

	%PREFIX%\%CC% %CFLAGS% !EFLAGS! -x c temp.c
	IF NOT %errorlevel% == 0 (
		DEL /f temp.c
		ECHO Error: Failed to build C application.
		REM exit /b 1
	)
	DEL /f temp.c

	CALL .\%CCEXE%
	IF NOT %errorlevel% == 0 (
		ECHO Error: Failed to run built C application.
		REM exit /b 1
	)

	IF "%VERBOSE%"=="yes" ECHO %PREFIX%\%CXX% %CXXFLAGS% !EFLAGS! -x c^+^+ temp.cpp

	(
		ECHO #include ^<cstdlib^>
		ECHO #include ^<iostream^>
		ECHO.
		ECHO __attribute__^(^(used^)^)
		ECHO extern void __main ^(^);
		ECHO.
		ECHO __attribute__^(^(used^)^)
		ECHO int main^(int argc, char * argv[], char * envp[]^) {
		ECHO     std::cout ^<^< "Hello World^!" ^<^< std::endl;
		ECHO     return EXIT_SUCCESS;
		ECHO }
	) > temp.cpp

	%PREFIX%\%CXX% %CXXFLAGS% !EFLAGS! -x c++ temp.cpp
	IF NOT %errorlevel% == 0 (
		DEL /f temp.cpp
		ECHO Error: Failed to build C++ application.
		REM exit /b 1
	)
	DEL /f temp.cpp

	CALL .\%CXXEXE%
	IF NOT %errorlevel% == 0 (
		ECHO Error: Failed to run built C++ application.
		REM exit /b 1
	)
)
