@echo off
call "C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64
cd build
rmdir /s /q CMakeFiles
del /q /s CMakeCache.txt
cmake -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="/D_DEBUG /D_ITERATOR_DEBUG_LEVEL=2 /RTC1 /GS /guard:cf" ..
cmake --build . --config Debug -j 8
