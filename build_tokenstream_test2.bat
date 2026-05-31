@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /EHsc /std:c++17 /O2 /W4 d:\rawrxd\AI_TokenStream_Test.cpp /Fe:d:\rawrxd\AI_TokenStream_Test.exe /link winhttp.lib
