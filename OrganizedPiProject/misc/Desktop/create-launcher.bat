@echo off
echo Creating Windows launcher...

echo @echo off > SimpleCursorIDE.bat
echo java -jar SimpleCursorIDE.jar >> SimpleCursorIDE.bat

REM Create PowerShell script for true EXE
echo $jar = Get-Content -Raw "SimpleCursorIDE.jar" -Encoding Byte > build-exe.ps1
echo $launcher = @' >> build-exe.ps1
echo using System; >> build-exe.ps1
echo using System.Diagnostics; >> build-exe.ps1
echo class Program { >> build-exe.ps1
echo     static void Main() { >> build-exe.ps1
echo         Process.Start("java", "-jar SimpleCursorIDE.jar"); >> build-exe.ps1
echo     } >> build-exe.ps1
echo } >> build-exe.ps1
echo '@ >> build-exe.ps1
echo Add-Type -TypeDefinition $launcher -OutputAssembly "SimpleCursorIDE.exe" >> build-exe.ps1

powershell -ExecutionPolicy Bypass -File build-exe.ps1

echo ✅ EXE launcher created: SimpleCursorIDE.exe