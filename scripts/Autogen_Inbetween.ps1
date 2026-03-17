# Autogen_Inbetween.ps1 - Generates inbetween using patterns + context. Wired to D:\rawrxd.
# Usage: .\scripts\Autogen_Inbetween.ps1 [-DryRun] [-Model phi-3-mini]
param([switch]$DryRun, [string]$Model = "phi-3-mini", [string]$Root = "D:\rawrxd")
$ErrorActionPreference = "SilentlyContinue"
$api = "http://localhost:11434/api/generate"
Get-ChildItem "$Root\src\*.asm","$Root\Ship\*.asm","$Root\*.asm" -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $f = $_; $c = Get-Content $f.FullName -Raw -EA SilentlyContinue
    if (-not $c) { return }
    $ctx = [regex]::Matches($c, "(?:public|extrn)\s+(\w+)|(?:Vulkan|Ollama|GGUF|AVX|DMA|stub|TODO|FIXME|proc|endp)") | ForEach-Object { $_.Value } | Select-Object -Unique
    $blanks = [regex]::Matches($c, "(?m)(;.*?(?:TODO|FIXME|stub)|^\s*;.*placeholder|extrn\s+(\w+)\s*:)")
    if ($blanks.Count -eq 0 -and $c -notmatch "TODO|FIXME|stub|placeholder") { return }
    $stubs = $blanks | ForEach-Object { if ($_.Groups[2].Value) { $_.Groups[2].Value } } | Where-Object { $_ } | Select-Object -Unique
    foreach ($s in $stubs) {
        if ((Get-ChildItem "$Root\src\*.asm","$Root\Ship\*.asm" -Recurse -EA SilentlyContinue | ForEach-Object { Get-Content $_.FullName } | Select-String "^\s*$s\s+proc")) { continue }
        $prompt = "MASM64 x64: implement $s. Context: $($ctx -join ','). Output ONLY assembly: proc/endp, no markdown."
        try {
            $r = Invoke-RestMethod -Uri $api -Method Post -Body (@{model=$Model;prompt=$prompt;stream=$false;options=@{temperature=0.1}} | ConvertTo-Json) -ContentType "application/json" -TimeoutSec 45 -EA Stop
            $code = ($r.response -replace "```.*?```" -replace "^\s*Here is.*" -replace "^\s*This code.*" -replace "^\s*```assembly" -replace "```\s*$").Trim()
            if ($code.Length -lt 30) { continue }
            $out = "$Root\src\gen_$s.asm"
            "include rawrxd.inc`n$code`nend" | Out-File $out -Encoding ASCII -Force
            if (-not $DryRun) {
                $obj = "$Root\build_prod\gen_$s.obj"
                ml64.exe $out /c /Fo$obj /nologo 2>&1 | Out-Null
                if ($LASTEXITCODE -eq 0) { Write-Host "GEN:$s" -ForegroundColor Green } else { Write-Host "ERR:$s" -ForegroundColor Red }
            } else { Write-Host "DRY:$s" -ForegroundColor Cyan }
        } catch { Write-Host "SKIP:$s" -ForegroundColor Yellow }
    }
}
Write-Host "Autogen done." -ForegroundColor Magenta
