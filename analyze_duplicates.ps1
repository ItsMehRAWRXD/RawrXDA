# Get all duplicate function names from linker errors
$duplicates = Select-String -Path 'build_replacement_check\rawrengine_link.log' -Pattern 'error LNK2005.*\((.handle\w+)@@' | ForEach-Object {
    if ($_.Line -match '\?(handle\w+)@@') {
        $matches[1]
    }
} | Sort-Object -Unique

Write-Host "Found $($duplicates.Count) duplicate handlers:"
$duplicates | ForEach-Object { Write-Host "  - $_" }
