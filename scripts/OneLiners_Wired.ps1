# RawrXD One-Liners - Copy/paste to wire immediately. Path: D:\rawrxd
# =============================================================================

# 1. AUTOGEN INBETWEEN (patterns + context → fill blanks)
# $(gci D:\rawrxd\src\*.asm,D:\rawrxd\Ship\*.asm -Recurse|%{$f=$_;$c=gc $f.FullName -Raw -EA 0;if(-not $c){return};$stubs=([regex]'(?m)(?:extrn|call)\s+(\w+)').Matches($c)|%{$_.Groups[1].Value}|?{$_}|Select -Unique|?{-not(gci D:\rawrxd\src\*.asm,D:\rawrxd\Ship\*.asm -Recurse|%{gc $_.FullName}|sls "^\s*$_\s+proc")};$stubs|%{$s=$_;try{$r=(irm http://localhost:11434/api/generate -Method Post -Body (@{model="phi-3-mini";prompt="MASM64: implement $s. Output only proc/endp.";stream=$false}|ConvertTo-Json) -ContentType "application/json" -TimeoutSec 30).response;if($r.Length -gt 30){("include rawrxd.inc`n$($r.Trim())`nend"|Out-File "D:\rawrxd\src\gen_$s.asm" -Force);ml64 "D:\rawrxd\src\gen_$s.asm" /c /Fo"D:\rawrxd\build_prod\gen_$s.obj" 2>$null;if($?){Write-Host "GEN:$s" -fg Green}}}catch{}}) }; Write-Host "Done" -fg Magenta

# 2. BUILD (compile all ASM)
# gci D:\rawrxd\src\*.asm,D:\rawrxd\Ship\*.asm -Recurse|%{ml64 $_.FullName /c /Fo"D:\rawrxd\build_prod\$($_.BaseName).obj" /nologo 2>&1};Write-Host "Build done" -fg Green

# 3. LINK CHECK (verify OBJs exist and link)
# $objs=gci D:\rawrxd\build_prod\*.obj -EA 0|%{$_.FullName};if($objs){link $objs /OUT:"D:\rawrxd\build_prod\RawrXD-Final.exe" /SUBSYSTEM:WINDOWS /MACHINE:X64 /NOLOGO 2>&1;Write-Host "Link: $(Test-Path D:\rawrxd\build_prod\RawrXD-Final.exe)" -fg $(if(Test-Path D:\rawrxd\build_prod\RawrXD-Final.exe){"Green"}else{"Red"})}

# 4. CMake BUILD (full project)
# cmake -S D:\rawrxd -B D:\rawrxd\build_prod -G Ninja -DRAWR_ARCH=x64; cmake --build D:\rawrxd\build_prod --config Release --target RawrXD-Agent

# 5. IDE LAUNCH
# Stop-Process RawrXD* -Force -EA 0; Start-Sleep 500; $env:OLLAMA_HOST="http://localhost:11434"; Start-Process "D:\rawrxd\build_prod\RawrXD-AgenticIDE.exe" -ArgumentList "--agent-mode" -WorkingDirectory D:\rawrxd; Write-Host "IDE started" -fg Green
