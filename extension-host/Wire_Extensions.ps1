# RawrXD Extension Host Wiring - Sanitize publishers, stub native DLLs, optional .asm hijack
param([switch]$Install, [switch]$Native)

$extDir = "$env:USERPROFILE\.vscode\extensions"
$cursorExt = "$env:USERPROFILE\.cursor\extensions"
$rawrDir = "D:\rawrxd\extensions"

Write-Host "WIRING EXTENSIONS TO RAWRXD NATIVE HOST" -ForegroundColor Magenta

# 1. Sanitize undefined_publisher -> BigDaddyG
foreach ($base in @($extDir, $cursorExt)) {
    if (-not (Test-Path $base)) { continue }
    Get-ChildItem $base -Filter "undefined_publisher*" -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $proper = $_.Name -replace 'undefined_publisher', 'BigDaddyG'
        $dest = Join-Path $base $proper
        if (-not (Test-Path $dest)) {
            Rename-Item $_.FullName $dest -Force
            Write-Host "  PURGED: $($_.Name) -> $proper" -ForegroundColor DarkYellow
        }
    }
}

# 2. Create unified extension directory structure
@('agentic', 'copilot', 'lsp', 'asm-ide', 'chat', 'ollama') | ForEach-Object {
    New-Item -ItemType Directory -Force -Path "$rawrDir\$_" | Out-Null
}

# 3. Generate native extension .def stubs (DLL exports for RawrXD host)
$stubs = @{
    "agentic" = "RawrXD_Agentic_Extension.dll"
    "copilot" = "BigDaddyG_Copilot.dll"
    "lsp"     = "RawrXD_LSP_Native.dll"
}
foreach ($stub in $stubs.GetEnumerator()) {
    $def = @"
EXPORTS
    ExtensionInit
    ExtensionActivate
    ExtensionExecuteCommand
    ExtensionHandleChat
"@
    $defPath = "$rawrDir\$($stub.Key)\$($stub.Value -replace '\.dll$','.def')"
    $def | Out-File $defPath -Encoding ASCII
    Write-Host "  GENERATED: $($stub.Value) stub" -ForegroundColor DarkGreen
}

# 4. Optional: .asm file association -> RawrXD
if ($Native) {
    $exePath = "D:\rawrxd\RawrXD-AgenticIDE.exe"
    if (Test-Path $exePath) {
        Set-ItemProperty -Path "HKCU:\Software\Classes\.asm" -Name "(Default)" -Value "RawrXD.asm" -Force -ErrorAction SilentlyContinue
        $cmdPath = "HKCU:\Software\Classes\RawrXD.asm\shell\open\command"
        New-Item -Path $cmdPath -Force -ErrorAction SilentlyContinue | Out-Null
        Set-ItemProperty -Path $cmdPath -Name "(Default)" -Value "`"$exePath`" `"%1`"" -Force -ErrorAction SilentlyContinue
        Write-Host "  HIJACKED: .asm file association -> RawrXD" -ForegroundColor Red
    } else {
        Write-Host "  SKIP .asm hijack: $exePath not found" -ForegroundColor Yellow
    }
}

Write-Host "`nEXTENSIONS WIRED TO RAWRXD NATIVE HOST" -ForegroundColor Green
Write-Host "Run: FixBigDaddyGTypo.ps1 then UninstallExtensionBloat.ps1 as needed." -ForegroundColor Cyan
