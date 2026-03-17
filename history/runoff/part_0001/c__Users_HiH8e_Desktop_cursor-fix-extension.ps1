# Check and fix Cursor Edge Tools extension issue
$extDir = 'C:\Users\HiH8e\.cursor\extensions\ms-edgedevtools.vscode-edge-devtools-2.1.9-universal'
$nodeModulesDir = Join-Path $extDir 'node_modules'
$webhintDir = Join-Path $nodeModulesDir 'vscode-webhint'

Write-Output "=== Extension Contents Check ==="
if (Test-Path $nodeModulesDir) {
    Write-Output "node_modules contents:"
    Get-ChildItem -Path $nodeModulesDir | Select-Object Name | Format-Table -AutoSize
} else {
    Write-Output "node_modules directory missing"
}

Write-Output "Checking for vscode-webhint package:"
if (Test-Path $webhintDir) {
    Write-Output "vscode-webhint EXISTS"
    $serverFile = Join-Path $webhintDir 'dist\src\server.js'
    if (Test-Path $serverFile) {
        Write-Output "server.js EXISTS"
    } else {
        Write-Output "server.js MISSING"
        Write-Output "vscode-webhint structure:"
        if (Test-Path $webhintDir) { Get-ChildItem -Path $webhintDir -Recurse | Select-Object FullName | Format-Table -AutoSize }
    }
} else {
    Write-Output "vscode-webhint package MISSING - this is the problem"
}

Write-Output ""
Write-Output "=== Recommended Fix ==="
Write-Output "The vscode-webhint package is missing from node_modules."
Write-Output "Options:"
Write-Output "1. Remove the broken extension: Remove-Item -LiteralPath '$extDir' -Recurse -Force"
Write-Output "2. Reinstall the extension through Cursor marketplace"
Write-Output ""