# Compare key src files between D and E (size only)
$files = @(
    "win32app\Win32IDE.cpp",
    "win32app\Win32IDE_AgenticBridge.cpp",
    "win32app\Win32IDE_Sidebar.cpp",
    "win32app\Win32IDE_PowerShell.cpp",
    "win32app\Win32IDE_VSCodeUI.cpp",
    "gguf_loader.cpp",
    "agentic_error_handler.cpp"
)
foreach ($f in $files) {
    $d = "d:\rawrxd\src\$f"
    $e = "e:\RawrXD\src\$f"
    $dExists = Test-Path $d
    $eExists = Test-Path $e
    if ($dExists -and $eExists) {
        $dSize = (Get-Item $d).Length
        $eSize = (Get-Item $e).Length
        $match = if ($dSize -eq $eSize) { "MATCH" } else { "DIFF($dSize vs $eSize)" }
        Write-Host "$f : $match"
    } elseif ($dExists) { Write-Host "$f : D only" } else { Write-Host "$f : E only" }
}
