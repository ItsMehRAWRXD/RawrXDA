$ErrorActionPreference = "Stop"

$ml = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$ln = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
$db = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe"

$writerAsm = "D:\RawrXD\RawrXD_PE_Writer.asm"
$writerObj = "D:\RawrXD\RawrXD_PE_Writer.obj"
$harnessAsm = "D:\RawrXD\pe_writer_import_e2e.asm"
$harnessObj = "D:\RawrXD\pe_writer_import_e2e.obj"
$harnessExe = "D:\RawrXD\pe_writer_import_e2e.exe"
$outExe = "D:\RawrXD\test_output_imports.exe"

& $ml /c /Fo$writerObj $writerAsm
if ($LASTEXITCODE -ne 0) { throw "Writer assembly failed." }

& $ml /c /Fo$harnessObj $harnessAsm
if ($LASTEXITCODE -ne 0) { throw "Harness assembly failed." }

& $ln /SUBSYSTEM:CONSOLE /MACHINE:X64 /ENTRY:main /OUT:$harnessExe $harnessObj $writerObj kernel32.lib `
    /LIBPATH:"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64" `
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
if ($LASTEXITCODE -ne 0) { throw "Harness link failed." }

& $harnessExe
if ($LASTEXITCODE -ne 0) { throw "Runtime generation failed with exit code $LASTEXITCODE." }
if (-not (Test-Path $outExe)) { throw "Output executable not created." }

$size = (Get-Item $outExe).Length
if ($size -lt 0xA00) { throw "Output too small ($size bytes), expected at least 0xA00." }

$imports = (& $db /imports $outExe) -join "`n"
foreach ($needle in @(
    "kernel32.dll", "ExitProcess", "GetStdHandle", "WriteFile",
    "user32.dll", "MessageBoxA",
    "ntdll.dll", "RtlInitUnicodeString"
)) {
    if ($imports -notmatch [Regex]::Escape($needle)) {
        throw "Missing import marker: $needle"
    }
}

$headers = (& $db /headers $outExe) -join "`n"
foreach ($needle in @(".text name", ".rdata name", ".idata name")) {
    if ($headers -notmatch [Regex]::Escape($needle)) {
        throw "Missing section header marker: $needle"
    }
}
if ($headers -match "(?im)^\s*0\s+\[\s*0\s*\]\s+RVA \[size\] of Import Directory") {
    throw "Import directory is zeroed in optional header."
}

Write-Host "PE writer regression check passed."
