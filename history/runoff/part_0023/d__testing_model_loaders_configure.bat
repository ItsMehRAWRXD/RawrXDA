@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\testing_model_loaders
if exist build rmdir /s /q build
cmake -G Ninja -DUSE_GPU=OFF -DBUILD_TESTS=ON -DBUILD_QTAPP=ON -B build
echo CMAKE_EXIT_CODE=%ERRORLEVEL%
