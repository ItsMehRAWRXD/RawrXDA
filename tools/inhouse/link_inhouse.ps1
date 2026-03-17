param(
  [string]$OutExe = "D:\\rawrxd\\build_prod\\RawrXD.inhouse.exe",
  [ValidateSet("WINDOWS","CONSOLE")] [string]$Subsystem = "WINDOWS",
  [string]$Entry = "WinMain",
  [string[]]$Inputs = @(),
  [string]$ObjDir = "",
  [string]$LibDir = "",
  [switch]$Map
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Resolve-Linker {
  $candidates = @(
    "D:\\rawrxd\\tools\\inhouse\\bin\\rawrxd_linker.exe",
    "D:\\rawrxd\\tools\\inhouse\\linker\\bin\\Release\\net8.0\\win-x64\\publish\\rawrxd_linker.exe",
    "D:\\rawrxd\\tools\\inhouse\\linker\\bin\\Release\\net8.0\\rawrxd_linker.exe"
  )
  foreach ($c in $candidates) { if (Test-Path $c) { return (Resolve-Path $c).Path } }
  throw "rawrxd_linker.exe not found. Build it: dotnet publish D:\\rawrxd\\tools\\inhouse\\linker -c Release -r win-x64 -o D:\\rawrxd\\tools\\inhouse\\bin"
}

$linker = Resolve-Linker

if (-not $Inputs -or $Inputs.Count -eq 0) {
  if ($ObjDir -and (Test-Path $ObjDir)) {
    $Inputs += (Get-ChildItem -LiteralPath $ObjDir -File -Filter *.obj | Select-Object -ExpandProperty FullName)
  }
  if ($LibDir -and (Test-Path $LibDir)) {
    $Inputs += (Get-ChildItem -LiteralPath $LibDir -File -Filter *.lib | Select-Object -ExpandProperty FullName)
  }
}

if (-not $Inputs -or $Inputs.Count -eq 0) { throw "No inputs. Provide -Inputs or -ObjDir/-LibDir." }

$args = @(
  "--out", $OutExe,
  "--entry", $Entry,
  "--subsystem", $Subsystem,
  "--in"
) + $Inputs

if ($Map) { $args += "--map" }

Write-Host "[InHouseLink] $OutExe" -Fore Cyan
& $linker @args
exit $LASTEXITCODE

