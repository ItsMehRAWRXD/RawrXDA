# Check Cursor extension issue
$missingFile = 'C:\Users\HiH8e\.cursor\extensions\ms-edgedevtools.vscode-edge-devtools-2.1.9-universal\node_modules\vscode-webhint\dist\src\server.js'
$extDir = 'C:\Users\HiH8e\.cursor\extensions\ms-edgedevtools.vscode-edge-devtools-2.1.9-universal'
$cursorExtRoot = 'C:\Users\HiH8e\.cursor\extensions'

Write-Output "=== Cursor Extension Diagnostics ==="
Write-Output "Missing file: $missingFile"
if (Test-Path $missingFile) { 
    Write-Output "FILE_EXISTS" 
} else { 
    Write-Output "FILE_MISSING" 
}

Write-Output "Extension directory: $extDir"
if (Test-Path $extDir) { 
    Write-Output "EXTENSION_DIR_EXISTS"
    Get-ChildItem -Path $extDir | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
} else { 
    Write-Output "EXTENSION_DIR_MISSING" 
}

Write-Output "Cursor extensions root: $cursorExtRoot"
if (Test-Path $cursorExtRoot) {
    Write-Output "Listing all Cursor extensions:"
    Get-ChildItem -Path $cursorExtRoot | Where-Object { $_.Name -like '*edge*' -or $_.Name -like '*devtools*' } | Select-Object Name | Format-Table -AutoSize
} else {
    Write-Output "CURSOR_EXTENSIONS_DIR_MISSING"
}