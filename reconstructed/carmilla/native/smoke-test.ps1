param(
  [string]$Exe = "$(Join-Path $PSScriptRoot 'carmilla_x64.exe')",
  [string]$StubGen = "$(Join-Path $PSScriptRoot 'carmilla_stub_gen_x64.exe')",
  [string]$Pass = 'test-pass'
)

$ErrorActionPreference = 'Stop'

if (!(Test-Path $Exe)) { throw "Executable not found: $Exe" }
if (!(Test-Path $StubGen)) { throw "Stub generator not found: $StubGen" }

$root = Join-Path $PSScriptRoot 'temp'
New-Item -ItemType Directory -Force -Path $root | Out-Null

$in  = Join-Path $root 'sample.bin'
$car = Join-Path $root 'sample.car'
$out = Join-Path $root 'sample.out'
$stub = Join-Path $root 'stub.bin'
$packed = Join-Path $root 'packed.exe'

# Write a deterministic 256-byte file
[IO.File]::WriteAllBytes($in, [byte[]](0..255))

Write-Host "[*] Testing encrypt/decrypt" -ForegroundColor Cyan
& $Exe encrypt $in $car $Pass | Write-Host

& $Exe decrypt $car $out $Pass | Write-Host

$h1 = (Get-FileHash $in -Algorithm SHA256).Hash
$h2 = (Get-FileHash $out -Algorithm SHA256).Hash

if ($h1 -ne $h2) {
  throw "Hash mismatch! Expected $h1, got $h2"
}

Write-Host "[+] Encrypt/decrypt OK" -ForegroundColor Green

Write-Host "[*] Generating stub" -ForegroundColor Cyan
& $StubGen save $root 12345 | Write-Host

if (!(Test-Path $stub)) { throw "Stub file not generated" }

Write-Host "[*] Testing pack mode" -ForegroundColor Cyan
& $Exe pack $in $packed $stub $Pass | Write-Host

if (!(Test-Path $packed)) { throw "Packed file not created" }

Write-Host "[+] Pack mode OK" -ForegroundColor Green

Write-Host "[+] All smoke tests passed" -ForegroundColor Green
