Write-Host "Temp debug starting"
Set-Location 'd:\lazy init ide'
$Error.Clear()
try {
    & 'd:\lazy init ide\scripts\SourceWiringDigest.ps1' -RootPath 'd:\lazy init ide' -ModuleDir 'd:\lazy init ide\scripts'
} catch {
    Write-Host "Outer catch: $_"
}
"ErrorCount=$($Error.Count)"
if ($Error.Count -gt 0) {
    $Error[0] | Format-List * -Force
    $Error[0].InvocationInfo | Format-List * -Force
    $Error[0].ScriptStackTrace
}
Write-Host "Temp debug finished"
