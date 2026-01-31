$MSVC_Bin = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
if (Test-Path $MSVC_Bin) { $env:PATH = "$MSVC_Bin;$env:PATH" }
$Src = "$PSScriptRoot\..\src\thermal\masm"
Set-Location "$PSScriptRoot\..\build"

# 1-Liner Equivalent
& ml64.exe /c /nologo /Fo"pocket_lab.obj" "$Src\pocket_lab.asm"
if ($LASTEXITCODE -eq 0) {
    & link.exe /NOLOGO /SUBSYSTEM:CONSOLE /OUT:"pocket_lab.exe" "pocket_lab.obj" kernel32.lib ntdll.lib
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build Success." -ForegroundColor Green
        .\pocket_lab.exe
    }
}
