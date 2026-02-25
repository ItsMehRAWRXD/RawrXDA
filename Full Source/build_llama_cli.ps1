# Build the minimal RawrXD llama.dll-backed CLI (no HTTP).
# Output: .\build\llama_cli\RawrXD_LlamaCLI.exe

$ErrorActionPreference = "Stop"

$root    = Split-Path -Parent $MyInvocation.MyCommand.Path
$asmDir  = Join-Path $root "src\\asm"
$outDir  = Join-Path $root "build\\llama_cli"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ml64 = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe"
$link = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\link.exe"

$winSdkUm   = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64"

foreach ($p in @($ml64, $link)) {
  if (-not (Test-Path $p)) { throw "Tool not found: $p" }
}
foreach ($p in @(
  (Join-Path $winSdkUm "kernel32.lib")
)) {
  if (-not (Test-Path $p)) { throw "Library not found: $p" }
}

Push-Location $asmDir
try {
  Write-Host "[ASM] RawrXD_LlamaBridge.asm"
  & $ml64 /nologo /c /Fo (Join-Path $outDir "RawrXD_LlamaBridge.obj") /I $asmDir "RawrXD_LlamaBridge.asm"

  Write-Host "[ASM] RawrXD_LlamaCLI.asm"
  & $ml64 /nologo /c /Fo (Join-Path $outDir "RawrXD_LlamaCLI.obj") /I $asmDir "RawrXD_LlamaCLI.asm"

  Write-Host "[LINK] RawrXD_LlamaCLI.exe"
  $outExe = Join-Path $outDir "RawrXD_LlamaCLI.exe"
  & $link /nologo /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup `
    ("/LIBPATH:$winSdkUm") `
    ("/OUT:$outExe") `
    (Join-Path $outDir "RawrXD_LlamaCLI.obj") (Join-Path $outDir "RawrXD_LlamaBridge.obj") `
    (Join-Path $winSdkUm "kernel32.lib")

  Write-Host "[OK] Built: $outExe"
  Write-Host "[NOTE] Place x64 llama.dll (+ ggml.dll if required) next to the EXE."
  Write-Host "[NOTE] Default model path: models\\model.gguf (relative to EXE working dir)."
} finally {
  Pop-Location
}
