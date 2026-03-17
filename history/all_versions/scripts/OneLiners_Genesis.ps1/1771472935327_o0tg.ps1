# RawrXD Genesis One-Liners — Copy/paste to wire immediately.
# Root: D:\rawrxd   |   All paths hardcoded   |   Pure MASM64 + Win32
# =============================================================================

# 1. AUTOGEN INBETWEEN (scan stubs → Ollama → fill blanks → assemble)
#    .\scripts\Autogen_Inbetween.ps1 [-DryRun] [-Model phi-3-mini]

# 2. BUILD ALL ASM → OBJ
# gci D:\rawrxd\src\*.asm, D:\rawrxd\Ship\*.asm -Recurse -EA 0 | %{ ml64 $_.FullName /c /Fo"D:\rawrxd\build_prod\$($_.BaseName).obj" /nologo 2>&1 }; Write-Host "Build done" -fg Green

# 3. BUILD GENESIS ENGINE (standalone)
# ml64 D:\rawrxd\src\genesis.asm /c /FoD:\rawrxd\build_prod\genesis.obj /nologo /W3 /I D:\rawrxd\include; link D:\rawrxd\build_prod\genesis.obj /OUT:D:\rawrxd\build_prod\genesis.exe /SUBSYSTEM:CONSOLE /ENTRY:GenesisMain /MACHINE:X64 kernel32.lib winhttp.lib user32.lib /NOLOGO; if(Test-Path D:\rawrxd\build_prod\genesis.exe){Write-Host "Genesis built" -fg Green}else{Write-Host "Genesis FAILED" -fg Red}

# 4. LINK CHECK (verify all OBJs link)
# $objs=gci D:\rawrxd\build_prod\*.obj -EA 0|%{$_.FullName};if($objs){link $objs /OUT:"D:\rawrxd\build_prod\RawrXD-Final.exe" /SUBSYSTEM:WINDOWS /MACHINE:X64 /NOLOGO 2>&1;Write-Host "Link: $(Test-Path D:\rawrxd\build_prod\RawrXD-Final.exe)" -fg $(if(Test-Path D:\rawrxd\build_prod\RawrXD-Final.exe){"Green"}else{"Red"})}

# 5. CMAKE BUILD (full project)
# cmake -S D:\rawrxd -B D:\rawrxd\build_prod -G Ninja -DRAWR_ARCH=x64; cmake --build D:\rawrxd\build_prod --config Release --target RawrXD-Agent

# 6. IDE LAUNCH
# Stop-Process RawrXD* -Force -EA 0; Start-Sleep -Milliseconds 500; $env:OLLAMA_HOST="http://localhost:11434"; Start-Process "D:\rawrxd\build_prod\RawrXD-AgenticIDE.exe" -ArgumentList "--agent-mode" -WorkingDirectory D:\rawrxd; Write-Host "IDE started" -fg Green

# 7. GENESIS RUN (start autonomous code gen)
# Start-Process "D:\rawrxd\build_prod\genesis.exe" -WorkingDirectory D:\rawrxd -NoNewWindow; Write-Host "Genesis engine running — press Q to stop" -fg Magenta
