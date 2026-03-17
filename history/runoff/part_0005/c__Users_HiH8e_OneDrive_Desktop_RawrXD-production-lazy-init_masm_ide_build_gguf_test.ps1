$ErrorActionPreference = 'Stop'

$masmRoot = 'C:\masm32'
$ml      = Join-Path $masmRoot 'bin\ml.exe'
$link    = Join-Path $masmRoot 'bin\link.exe'
$libPath = Join-Path $masmRoot 'lib'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$include = Join-Path $scriptDir 'include'
$src     = Join-Path $scriptDir 'src'
$out     = Join-Path $scriptDir 'build'

if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

# Create a 120MB dummy file to exercise streaming & memory mapping
$dummy = Join-Path $out 'dummy-gguf.bin'
if (-not (Test-Path $dummy)) {
    Write-Host "Creating 120MB dummy file..." -ForegroundColor Cyan
    $fs = [System.IO.File]::Create($dummy)
    $fs.SetLength(120MB)
    $fs.Close()
}

Write-Host "Assembling gguf_stream.asm + perf_metrics.asm + test harness..." -ForegroundColor Cyan

$objs = @()
foreach ($f in @('gguf_stream','perf_metrics','gguf_stream_test')) {
    $result = & $ml /c /coff /Cp /nologo /I"$include" /I"$masmRoot\include" /Fo "$out\$f.obj" "$src\$f.asm" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ $f.asm compiled successfully" -ForegroundColor Green
        $objs += "$out\$f.obj"
    } else {
        Write-Host "  ✗ $f.asm failed" -ForegroundColor Red
        $result | Select-String "error" | ForEach-Object { Write-Host "    $_" -ForegroundColor Yellow }
        exit 1
    }
}

Write-Host "Linking gguf_stream_test.exe..." -ForegroundColor Cyan
& $link /SUBSYSTEM:CONSOLE /OUT:"$out\gguf_stream_test.exe" /LIBPATH:"$libPath" $objs kernel32.lib user32.lib 2>&1 | Select-String "error|warning|Creating"

if ($LASTEXITCODE -eq 0 -and (Test-Path "$out\gguf_stream_test.exe")) {
    Write-Host "\n✓ Test built: $out\gguf_stream_test.exe" -ForegroundColor Green
} else {
    Write-Host "\n✗ Link failed" -ForegroundColor Red
    exit 1
}
