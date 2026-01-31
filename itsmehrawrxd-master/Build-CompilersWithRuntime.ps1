param(
  [string]$OutDir = "$PSScriptRoot/bin/compilers",
  [string]$RuntimeOut = "$PSScriptRoot/bin/runtime",
  [string]$Name,
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Resolve-MSVC {
  $msvcRoot = 'C:\VS2022Enterprise\VC\Tools\MSVC'
  if (-not (Test-Path $msvcRoot)) { throw "MSVC root not found at $msvcRoot" }
  $msvcVer = Get-ChildItem -Path $msvcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $msvcVer) { throw "No MSVC versions under $msvcRoot" }
  $bin = Join-Path $msvcVer.FullName 'bin\Hostx64\x64'
  $lib = Join-Path $msvcVer.FullName 'lib\x64'
  $tools = [pscustomobject]@{ nasm = 'E:\nasm\nasm-2.16.01\nasm.exe'; link = Join-Path $bin 'link.exe'; lib = $lib }
  if (-not (Test-Path $tools.nasm) -or -not (Test-Path $tools.link)) { throw "Missing tools in $bin" }
  return $tools
}

function Resolve-Kits {
  $root = 'C:\Program Files (x86)\Windows Kits\10\Lib'
  if (-not (Test-Path $root)) { throw "Windows Kits lib root not found" }
  $ver = Get-ChildItem -Path $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $ver) { throw "No Windows Kits versions under $root" }
  $ucrt = Join-Path $ver.FullName 'ucrt\x64'
  $um = Join-Path $ver.FullName 'um\x64'
  if (-not (Test-Path $ucrt) -or -not (Test-Path $um)) { throw "Missing ucrt/um lib paths" }
  return [pscustomobject]@{ ucrt = $ucrt; um = $um }
}

function Ensure-Dir($dir) { if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null } }

function Build-Runtime {
  $tools = Resolve-MSVC
  $kits = Resolve-Kits
  Ensure-Dir $RuntimeOut
  
  $src = Join-Path $PSScriptRoot "universal_compiler_runtime_clean.asm"
  if (-not (Test-Path $src)) { throw "Runtime source not found: $src" }
  
  $obj = Join-Path $RuntimeOut "runtime.obj"
  $exe = Join-Path $RuntimeOut "runtime.exe"
  
  Write-Host "[RUNTIME] Compiling: $src" -ForegroundColor Cyan
  
  # Compile runtime
  $ncmd = '"{0}" -f win64 "{1}" -o "{2}"' -f $tools.nasm, $src, $obj
  cmd /c $ncmd
  if ($LASTEXITCODE -ne 0) { throw "Runtime NASM compile failed" }
  
  # Link runtime
  $linkCmd = '"{0}" /nologo /subsystem:console /entry:compiler_init "{1}" /LIBPATH:"{2}" /LIBPATH:"{3}" /LIBPATH:"{4}" kernel32.lib /out:"{5}"' -f $tools.link, $obj, $tools.lib, $kits.ucrt, $kits.um, $exe
  cmd /c $linkCmd
  if ($LASTEXITCODE -ne 0) { Write-Host "  (link warning, obj file created)" -ForegroundColor Yellow }
  
  Write-Host "[RUNTIME] Built: $obj" -ForegroundColor Green
  return $obj
}

function Get-CompilerFiles {
  $pattern = if ($Name) { "${Name}_compiler_from_scratch*.asm" } else { "*_compiler_from_scratch*.asm" }
  $files = Get-ChildItem -Path $PSScriptRoot -Filter $pattern -File -Recurse
  
  # Prefer _fixed versions, fallback to originals
  $fixed = $files | Where-Object { $_.Name -like "*_fixed.asm" }
  $orig = $files | Where-Object { $_.Name -notlike "*_fixed.asm" }
  
  # Return fixed versions first, then originals
  return @($fixed) + @($orig)
}

function Compile-Compiler($asmFile, $runtimeObj){
  $tools = Resolve-MSVC
  $kits = Resolve-Kits
  Ensure-Dir $OutDir
  
  $basename = [IO.Path]::GetFileNameWithoutExtension($asmFile.Name)
  $obj = Join-Path $OutDir "${basename}.obj"
  $exe = Join-Path $OutDir "${basename}.exe"
  
  Write-Host "[COMPILER] $basename" -ForegroundColor Yellow
  
  # Compile
  $ncmd = '"{0}" -f win64 "{1}" -o "{2}"' -f $tools.nasm, $asmFile.FullName, $obj
  cmd /c $ncmd
  if ($LASTEXITCODE -ne 0) { 
    Write-Host "  NASM compile failed (may need fixing)" -ForegroundColor Red
    return
  }
  
  # Link with runtime
  $linkCmd = '"{0}" /nologo /subsystem:console "{1}" "{2}" /LIBPATH:"{3}" /LIBPATH:"{4}" /LIBPATH:"{5}" kernel32.lib user32.lib /out:"{6}"' -f $tools.link, $obj, $runtimeObj, $tools.lib, $kits.ucrt, $kits.um, $exe
  cmd /c $linkCmd
  if ($LASTEXITCODE -eq 0) {
    Write-Host "  Built: $exe" -ForegroundColor Green
  } else {
    Write-Host "  Link warning (object created)" -ForegroundColor Yellow
  }
}

function Make-Manifest($files, $runtimeObj){
  $langsFound = @()
  foreach($f in $files){
    if ($f.Name -match '^(.*)_compiler_from_scratch\.asm$') {
      $langsFound += $matches[1]
    }
  }
  
  $manifest = [pscustomobject]@{
    timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    runtime_obj = $runtimeObj
    found = $langsFound | Sort-Object -Unique
    count = ($langsFound | Sort-Object -Unique).Count
  }
  
  $out = Join-Path $OutDir 'manifest.json'
  Ensure-Dir $OutDir
  $manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $out -Encoding utf8
  Write-Host "[MANIFEST] $out" -ForegroundColor Green
}

# Main build
$runtimeObj = Build-Runtime
$files = Get-CompilerFiles
Make-Manifest $files $runtimeObj

if ($DryRun) {
  Write-Host "[DRY] Would compile: $($files.Count) compilers" -ForegroundColor Cyan
  foreach($f in $files){
    Write-Host "  - $($f.Name)" -ForegroundColor Gray
  }
} else {
  foreach($f in $files){
    try {
      Compile-Compiler $f $runtimeObj
    } catch {
      Write-Host "[ERR] $_" -ForegroundColor Red
    }
  }
}

Write-Host "[DONE]" -ForegroundColor Green
