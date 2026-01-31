$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp

$mappings = @{
    'qRegisterMetaType' = '// qRegisterMetaType'
    'qRegisterMetaType<' = '// qRegisterMetaType<'
    'qRegisterConverterFunction' = '// qRegisterConverterFunction'
    'Q_REGISTER_METATYPE' = '// Q_REGISTER_METATYPE'
}

$totalModified = 0
$totalReplacements = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    $originalContent = $content
    $fileReplacements = 0

    foreach ($key in $mappings.Keys) {
        $val = $mappings[$key]
        $pattern = [regex]::Escape($key)
        $matches = [regex]::Matches($content, $pattern)
        if ($matches.Count -gt 0) {
            $content = $content -replace $pattern, $val
            $fileReplacements += $matches.Count
        }
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
