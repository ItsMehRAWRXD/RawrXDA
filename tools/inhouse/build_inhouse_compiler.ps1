param(
  [string]$Root = "D:\RawrXD"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$project = Join-Path $Root "tools\\inhouse\\RawrXD.Compiler\\RawrXD.Compiler.csproj"
if (!(Test-Path $project)) { throw "Missing project: $project" }

$outDir = Join-Path $Root "tools\\inhouse\\bin"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

dotnet publish $project -c Release -r win-x64 -o $outDir /p:SelfContained=true /p:PublishSingleFile=true
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed (exit $LASTEXITCODE)" }

$exe = Join-Path $outDir "rawrxd_compiler.exe"
if (!(Test-Path $exe)) { throw "Expected output missing: $exe" }

Copy-Item -LiteralPath $exe -Destination (Join-Path $Root "tools\\inhouse\\rawrxd_compiler.exe") -Force

Write-Host "OK: Built Windows-native in-house compiler: $(Join-Path $Root 'tools\\inhouse\\rawrxd_compiler.exe')"

