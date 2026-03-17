$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp | Where-Object { $_.FullName -notmatch "_noqt" }

$mappings = @{
    'Q_OS_WIN' = '_WIN32'
    'Q_OS_MAC' = '__APPLE__'
    'Q_OS_MACOS' = '__APPLE__'
    'Q_OS_LINUX' = '__linux__'
    'Q_OS_UNIX' = '__unix__'
    'Q_WS_WIN' = '_WIN32'
    'Q_WS_MAC' = '__APPLE__'
}

$totalModified = 0
$totalReplacements = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    $originalContent = $content
    $fileReplacements = 0

    foreach ($key in $mappings.Keys) {
        $val = $mappings[$key]
        $pattern = "\b" + [regex]::Escape($key) + "\b"
        $matches = [regex]::Matches($content, $pattern)
        if ($matches.Count -gt 0) {
            $content = $content -replace $pattern, $val
            $fileReplacements += $matches.Count
        }
    }
    
    # Handle QT_VERSION blocks - comment them out or assume they are false
    $qtVersionPattern = '(?m)^#if\s+QT_VERSION.*?$.*?^#endif.*?$'
    # Actually, simpler to just replace QT_VERSION with 0
    if ($content -match "QT_VERSION") {
        $content = $content -replace "QT_VERSION", "0"
        $fileReplacements++
    }

    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
        Write-Host "Modified: $($file.FullName) ($fileReplacements replacements)"
        $totalModified++
        $totalReplacements += $fileReplacements
    }
}

Write-Host "`nTotal files modified: $totalModified"
Write-Host "Total replacements: $totalReplacements"
