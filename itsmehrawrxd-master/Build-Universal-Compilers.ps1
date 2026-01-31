param(
  [string]$OutDir = "$PSScriptRoot/bin/compilers",
  [string]$Name,
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Get-AsmFiles {
  Get-ChildItem -Path $PSScriptRoot -Filter '*_compiler*.asm' -File -Recurse | Where-Object { $_.Name -notlike '*_fixed.asm' -and $_.Name -like '*_compiler*' }
}

function DetectAssemblerType($path){
  $head = Get-Content -Path $path -TotalCount 100 -ErrorAction SilentlyContinue
  if ($head -match '\bEXTERN\b' -or $head -match '\bPROC\b') { return 'masm' }
  if ($head -match '\bglobal\b' -or $head -match '\bsection\b') { return 'nasm' }
  return 'nasm'
}

function DetectEntry($path,$tool){
  $head = Get-Content -Path $path -TotalCount 200 -ErrorAction SilentlyContinue
  if ($tool -eq 'masm') {
    if ($head -match '\bWinMain\b') { return 'WinMain' }
    if ($head -match '^\s*main\b') { return 'main' }
    return 'main'
  } else {
    if ($head -match '^\s*global\s+start') { return 'start' }
    if ($head -match '^\s*global\s+_start') { return '_start' }
    return 'start'
  }
}

function Ensure-Out($dir){ if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null } }

function Compile-One($asmFile){
  $tool = DetectAssemblerType $asmFile.FullName
  $entry = DetectEntry $asmFile.FullName $tool
  Ensure-Out $OutDir
  
  # First compile the universal runtime
  $runtimePath = Join-Path $PSScriptRoot 'universal_compiler_runtime.asm'
  if (-not (Test-Path $runtimePath)) { throw "Universal runtime not found at $runtimePath" }
  
  $compiler = 'E:\masm\Unified-PowerShell-Compiler.ps1'
  if (-not (Test-Path $compiler)) { throw "Unified-PowerShell-Compiler.ps1 not found at $compiler" }
  
  # Compile runtime
  $runtimeCmd = 'pwsh -NoProfile -File "{0}" -Source "{1}" -Tool {2} -SubSystem console -OutDir "{3}" -Entry compiler_init' -f $compiler, $runtimePath, $tool, $OutDir
  if (-not $DryRun) {
    Write-Host "[RUNTIME] Compiling universal runtime" -ForegroundColor Magenta
    Invoke-Expression $runtimeCmd
    if ($LASTEXITCODE -ne 0) { throw "Runtime compilation failed" }
  }
  
  # Compile compiler with runtime
  $subsys = 'console'
  $runtimeObj = Join-Path $OutDir 'universal_compiler_runtime.obj'
  $cmd = 'pwsh -NoProfile -File "{0}" -Source "{1}" -Tool {2} -SubSystem {3} -OutDir "{4}" -Entry {5} -Runtime "{6}"' -f $compiler, $asmFile.FullName, $tool, $subsys, $OutDir, $entry, $runtimeObj
  if ($DryRun) {
    Write-Host "[DRY] $cmd" -ForegroundColor Yellow
  } else {
    Write-Host "[BUILD] $($asmFile.Name) → $tool entry=$entry" -ForegroundColor Cyan
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $($asmFile.Name)" }
  }
}

function Make-Manifest($files){
  $langsFound = @()
  foreach($f in $files){
    if ($f.Name -match '^(.*)_compiler.*\.asm$') {
      $langsFound += $matches[1]
    }
  }
  $supported63 = @(
    'assembly','c','cpp','csharp','java','javascript','python','rust','go','ruby','php','swift','kotlin','scala','haskell','perl','lua','elixir','erlang','nim','ocaml','fsharp','dart','typescript','sql','r','matlab','zig','ada','cobol','fortran','pascal','delphi','vb_net','clojure','crystal','julia','odin','scala','motoko','move','solidity','vyper','webassembly','llvm_ir','eon','ml','prolog','scheme','lisp','bash','powershell','dockerfile','make','json','xml','yaml','ini','toml','markdown'
  )
  $manifest = [pscustomobject]@{
    found = $langsFound | Sort-Object -Unique
    missing = ($supported63 | Where-Object { $_ -notin $langsFound })
    totalRequested = 63
    foundCount = ($langsFound | Sort-Object -Unique).Count
    missingCount = ($supported63 | Where-Object { $_ -notin $langsFound }).Count
  }
  $out = Join-Path $OutDir 'languages_supported_manifest.json'
  Ensure-Out $OutDir
  $manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $out -Encoding utf8
  Write-Host "[MANIFEST] $out" -ForegroundColor Green
}

$files = Get-AsmFiles
if (-not $files) { Write-Host "No compiler ASM files found." -ForegroundColor Red; exit 1 }

Write-Host ("Found {0} compiler ASM sources" -f $files.Count) -ForegroundColor Green
Make-Manifest $files

foreach($f in $files){
  try { Compile-One $f } catch { Write-Host "[ERR] $_" -ForegroundColor Red }
}