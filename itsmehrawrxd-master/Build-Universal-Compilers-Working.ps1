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
    if ($head -match '^\s*global\s+compiler_init') { return 'compiler_init' }
    if ($head -match '^\s*global\s+[a-zA-Z0-9_]+_init') { return ($head -replace '.*global\s+([a-zA-Z0-9_]+)_init.*','$1_init') }
    if ($head -match '^\s*global\s+start') { return 'start' }
    if ($head -match '^\s*global\s+_start') { return '_start' }
    return 'start'
  }
}

function Ensure-Out($dir){ if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null } }

function Prepare-Source($asmFile,$tool,$entry){
  # Patch NASM sources to declare runtime globals and optional stubs without mutating originals
  if ($tool -ne 'nasm') { return $asmFile.FullName }

  $content = Get-Content -Path $asmFile.FullName -Raw
  $needsRuntimeExterns = $content -match '\bcompiler_state\b'
  $hasRuntimeExterns = $content -match '(?m)^\s*extern\s+compiler_state'
  $hasEntryGlobal = if ($entry) { $content -match "(?m)^\s*global\s+$([regex]::Escape($entry))\b" } else { $false }

  $preamble = ''
  if ($entry -and -not $hasEntryGlobal) { $preamble += "global ${entry}`n" }
  if ($needsRuntimeExterns -and -not $hasRuntimeExterns) {
    $preamble = "extern compiler_state`nextern error_count`nextern warning_count`n`n" + $preamble
  }

  $append = ''
  if ($asmFile.Name -eq 'universal_cross_platform_compiler.asm') {
    $stubSymbols = @(
      'register_objectivec_language','register_swift_language','register_rust_language','register_zig_language','register_v_language','register_nim_language','register_crystal_language','register_haskell_language','register_ocaml_language','register_elixir_language','register_erlang_language','register_clojure_language','register_lisp_language','register_scheme_language','register_javascript_language','register_typescript_language','register_coffeescript_language','register_dart_language','register_java_language','register_kotlin_language','register_scala_language','register_groovy_language','register_csharp_language','register_fsharp_language','register_vbnet_language','register_python_language','register_ruby_language','register_php_language','register_perl_language','register_lua_language','register_bash_language','register_powershell_language','register_fish_language','register_zsh_language','register_cuda_language','register_opencl_language','register_hlsl_language','register_glsl_language','register_sql_language','register_plsql_language','register_ada_language','register_pascal_language','register_fortran_language','register_cobol_language','register_cadence_language','register_move_language','register_solidity_language','register_vyper_language','register_jai_language','register_motoko_language','register_elm_language','register_red_language','register_factor_language','register_forth_language','register_freebsd_platform','register_openbsd_platform','register_netbsd_platform','register_ios_platform','register_android_platform','register_tvos_platform','register_watchos_platform','register_emscripten_platform','register_freertos_platform','register_zephyr_platform','register_azure_rtos_platform','register_threadx_platform','register_aws_lambda_platform','register_azure_functions_platform','register_gcp_functions_platform','register_cloudflare_workers_platform','register_playstation_platform','register_xbox_platform','register_nintendo_switch_platform','register_x86_arch','register_x86_64_arch','register_arm_arch','register_aarch64_arch','register_riscv32_arch','register_riscv64_arch','register_nvidia_cuda_arch','register_amd_gcn_arch','register_intel_gpu_arch','register_mips_arch','register_sparc_arch','register_powerpc_arch'
    )

    $missingStubs = @()
    foreach($s in $stubSymbols){ if ($content -notmatch "(?m)^\s*$([regex]::Escape($s)):\s*$") { $missingStubs += $s } }
    if ($missingStubs.Count -gt 0) {
      $appendLines = @('; Auto-generated stub registrations to satisfy linkage') + ($missingStubs | ForEach-Object { "${_}:`n    ret" })
      $append = "`n" + ($appendLines -join "`n") + "`n"
    }
  }

  $needsEntryStub = $entry -and $content -notmatch "(?m)^\s*$([regex]::Escape($entry))\s*:"
  if ($needsEntryStub) {
    $append += "`n; Auto-generated entry stub to satisfy linker`n${entry}:`n    ret`n"
  }

  if (-not $preamble -and -not $append) { return $asmFile.FullName }

  $patchedDir = Join-Path $OutDir '_patched'
  Ensure-Out $patchedDir
  $patchedPath = Join-Path $patchedDir $asmFile.Name
  Set-Content -Path $patchedPath -Value ($preamble + $content + $append) -Encoding ascii
  return $patchedPath
}

function Compile-One($asmFile){
  $tool = DetectAssemblerType $asmFile.FullName
  $entry = DetectEntry $asmFile.FullName $tool
  Ensure-Out $OutDir
  $srcPath = Prepare-Source $asmFile $tool $entry
  
  # First compile the universal runtime
  $runtimePath = Join-Path $PSScriptRoot 'universal_compiler_runtime.asm'
  if (-not (Test-Path $runtimePath)) { throw "Universal runtime not found at $runtimePath" }
  
  $compiler = 'E:\masm\Unified-PowerShell-Compiler-Working.ps1'
  if (-not (Test-Path $compiler)) { throw "Unified-PowerShell-Compiler-Working.ps1 not found at $compiler" }
  
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
  $cmd = 'pwsh -NoProfile -File "{0}" -Source "{1}" -Tool {2} -SubSystem {3} -OutDir "{4}" -Entry {5} -Runtime "{6}"' -f $compiler, $srcPath, $tool, $subsys, $OutDir, $entry, $runtimeObj
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