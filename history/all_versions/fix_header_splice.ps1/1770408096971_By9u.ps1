$headerFile = "d:\rawrxd\src\win32app\Win32IDE.h"
$middleFile = "d:\rawrxd\clean_middle.txt"

$lines = Get-Content $headerFile
$middleLines = Get-Content $middleFile

Write-Host "Original file: $($lines.Count) lines"
Write-Host "Middle replacement: $($middleLines.Count) lines"

# Keep lines 1-1014 (indices 0-1013) 
$cleanStart = $lines[0..1013]

# Keep lines 1306-end (indices 1305+) - the clean PowerShell panel section
$cleanEnd = $lines[1305..($lines.Count - 1)]

Write-Host "Clean start: $($cleanStart.Count) lines"
Write-Host "Clean end: $($cleanEnd.Count) lines"

# Backup
Copy-Item $headerFile "$headerFile.bak" -Force

# Splice together
$newContent = $cleanStart + $middleLines + $cleanEnd
$newContent | Set-Content $headerFile -Encoding UTF8

Write-Host "New file: $((Get-Content $headerFile).Count) lines"
Write-Host "Done!"
