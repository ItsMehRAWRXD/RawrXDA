
$env:PATH += ";C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64;C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
$env:INCLUDE="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"
$env:LIB="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\onecore\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
& cmd.exe /c "cd D:\rawrxd && rmdir /s /q build_ninja3 & mkdir build_ninja3 & cd build_ninja3 && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=""/D_DEBUG /D_ITERATOR_DEBUG_LEVEL=2 /RTC1 /GS /guard:cf"" .. && ninja"

