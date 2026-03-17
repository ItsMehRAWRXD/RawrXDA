param(
  [string]$RspPath = "D:\\rawrxd\\bin\\RawrXD-Win32IDE.exe-link.rsp",
  [string]$OutExe = "",
  [switch]$Map
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$linker = "D:\\rawrxd\\tools\\inhouse\\bin\\rawrxd_linker.exe"
if (-not (Test-Path $linker)) { throw "Missing $linker (publish tools/inhouse/linker first)" }
if (-not (Test-Path $RspPath)) { throw "Missing rsp: $RspPath" }

if ($OutExe) {
  # Keep it simple: patch /OUT: token by adding an override after rsp.
  $args = @("@$RspPath", "--out", $OutExe, "--map")
} else {
  $args = @("@$RspPath")
  if ($Map) { $args += "--map" }
}

Write-Host "[InHouseLink/RSP] $RspPath" -Fore Cyan
& $linker @args
exit $LASTEXITCODE

