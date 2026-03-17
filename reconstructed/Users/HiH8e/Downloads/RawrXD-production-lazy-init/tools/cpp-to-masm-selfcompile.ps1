param(
    [string[]]$Files,
    [string]$SourceDir = "$PSScriptRoot/..",
    [string]$OutDir = "$PSScriptRoot/../bin/self-masmize",
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
function Log([string]$m){ if(-not (Test-Path $OutDir)){ New-Item -ItemType Directory -Path $OutDir | Out-Null }; $p = Join-Path $OutDir 'self-masmize.log'; $ts=(Get-Date).ToString('s'); $l="[$ts] $m"; Write-Host $l; Add-Content -Path $p -Value $l -Encoding UTF8 }
function Find-CL { $c=Get-Command cl -ErrorAction SilentlyContinue; if($c){return $c.Source}; return $null }
function Ensure-Dir([string]$p){ if(-not (Test-Path $p)){ New-Item -ItemType Directory -Path $p | Out-Null } }

# Pick a reasonable default set if none provided (keeps first run fast)
if(-not $Files -or $Files.Count -eq 0){
    $Files = @(
        Join-Path $SourceDir 'src/qtapp/codec.cpp'),
        (Join-Path $SourceDir 'src/qtapp/gguf_loader.cpp'),
        (Join-Path $SourceDir 'src/qtapp/vocabulary_loader.cpp')
}

$cl = Find-CL
if(-not $cl){ Log 'cl.exe not found in PATH. Ensure VS Build Tools environment.'; exit 1 }

Ensure-Dir $OutDir
$objDir = Join-Path $OutDir 'obj'; Ensure-Dir $objDir
$lstDir = Join-Path $OutDir 'lst'; Ensure-Dir $lstDir
$asmDir = Join-Path $OutDir 'asm'; Ensure-Dir $asmDir

# Basic include set (extend as needed)
$inc = @(
    (Join-Path $SourceDir 'include'),
    (Join-Path $SourceDir 'src'),
    (Join-Path $SourceDir 'src/qtapp'),
    (Join-Path $SourceDir 'src/llm_adapter'),
    (Join-Path $SourceDir '3rdparty/ggml/include'),
    (Join-Path $SourceDir '3rdparty/ggml/src')
) | Where-Object { Test-Path $_ }

function Compile-ToListing([string]$cpp){
    $base = [IO.Path]::GetFileNameWithoutExtension($cpp)
    $cod = Join-Path $lstDir ($base + '.cod')
    $obj = Join-Path $objDir ($base + '.obj')
    $fa  = '/FAcs'  # mixed source+asm
    $FaOut = '/Fa' + (Split-Path -LiteralPath $cod -Resolve:$false)
    $FoOut = '/Fo' + (Split-Path -LiteralPath $obj -Resolve:$false)
    $incs = $inc | ForEach-Object { '/I"' + $_ + '"' }
    $args = @('/nologo','/c','/std:c++20','/EHsc','/MD') + $fa + $FaOut + $FoOut + $incs + @($cpp)
    if($WhatIf){ Log "[WhatIf] cl $($args -join ' ')"; return $cod }
    Log "Compiling to listing: $cpp"
    & $cl @args | Out-Null
    if(-not (Test-Path $cod)){ Log "Listing not produced: $cod" }
    return $cod
}

function Generate-StubFromHeader([string]$root,[string]$outAsm){
    $gen = Join-Path $SourceDir 'bin/Release/masm_source_stub_gen.exe'
    if(Test-Path $gen){
        if($WhatIf){ Log "[WhatIf] $gen `"$root`" `"$outAsm`""; return }
        Log "Generating MASM stubs: $root → $outAsm"
        & $gen $root $outAsm | Out-Null
        if(Test-Path $outAsm){ return }
        Log "masm_source_stub_gen did not produce output; using PS fallback stubs."
    } else {
        Log "masm_source_stub_gen.exe missing at $gen; using PS fallback stubs."
    }
    # Fallback: emit simple stubs from header prototypes
    $headers = Get-ChildItem -Path $root -Recurse -Include *.h,*.hpp -File -ErrorAction SilentlyContinue
    $names = New-Object System.Collections.Generic.HashSet[string]
    foreach($h in $headers){
        $txt = Get-Content -Path $h.FullName -ErrorAction SilentlyContinue
        foreach($line in $txt){
            if($line -match '\)\s*;\s*$' -and $line -match '\('){
                if($line -match '([A-Za-z_][A-Za-z0-9_]*)\s*\('){
                    [void]$names.Add($Matches[1])
                }
            }
        }
    }
    $out = @(); $out += '.code'
    foreach($n in $names){
        $out += "PUBLIC $n"; $out += "$n PROC"; $out += '  ret'; $out += "$n ENDP"; $out += ''
    }
    $out += 'END'
    $dir = Split-Path -Path $outAsm -Parent
    if(-not (Test-Path $dir)){ New-Item -ItemType Directory -Path $dir | Out-Null }
    $out -join "`n" | Set-Content -Path $outAsm -Encoding ASCII
}

try{
    Log 'Self-masmize starting…'
    $plan = @()
    foreach($f in $Files){
        if(-not (Test-Path $f)){ Log "Skip missing: $f"; continue }
        $cod = Compile-ToListing -cpp $f
        $plan += @{ cpp=$f; cod=$cod }
    }
    $planPath = Join-Path $OutDir 'compile_plan.json'
    $plan | ConvertTo-Json -Depth 5 | Set-Content -Path $planPath -Encoding UTF8
    Log "Wrote plan: $planPath"

    # Header-driven stubs as a starting point (robust, compilable)
    $hdrAsm = Join-Path $asmDir 'includes_stubs.asm'
    $qtAsm  = Join-Path $asmDir 'qtapp_stubs.asm'
    Generate-StubFromHeader -root (Join-Path $SourceDir 'include') -outAsm $hdrAsm
    Generate-StubFromHeader -root (Join-Path $SourceDir 'src/qtapp') -outAsm $qtAsm

    Log 'Self-masmize complete.'
}
catch{
    Log ("ERROR: " + $_.Exception.Message)
    exit 1
}