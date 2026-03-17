$ML64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
$LIB_PATH = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
$CDB = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"

Set-Location "D:\RawrXD-ExecAI"

# Assemble
& $ML64 /c /Zi gguf_analyzer_working.asm
if ($LASTEXITCODE -ne 0) { Write-Error "Assembly failed"; exit }

# Link
& $LINK /nologo /subsystem:console /entry:main /debug gguf_analyzer_working.obj /LIBPATH:"$LIB_PATH" kernel32.lib /out:gguf_analyzer_working.exe
if ($LASTEXITCODE -ne 0) { Write-Error "Linking failed"; exit }

Write-Host "Build successful. Running in debugger..."

# Run in CDB
# -g ignores initial breakpoint
# -G ignores final breakpoint (process exit)
# -c "g; q" runs the program and then quits
& $CDB -g -G -c "g; q" .\gguf_analyzer_working.exe
