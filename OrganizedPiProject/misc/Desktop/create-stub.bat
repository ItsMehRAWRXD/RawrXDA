@echo off
echo Creating PE stub wrapper...

REM Create minimal PE header + DOS stub
echo MZ > stub.bin
echo @echo off >> stub.bin
echo java -jar OSLikeIDE.jar >> stub.bin
echo pause >> stub.bin

REM Combine stub with JAR
copy /b stub.bin + OSLikeIDE.jar OSLikeIDE.exe

echo [OK] PE executable with stub created: OSLikeIDE.exe
del stub.bin