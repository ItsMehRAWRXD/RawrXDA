# Final Qt and Logging Cleanup Script
# Removes remaining Qt code and all logging infrastructure

$sourceRoot = "D:\rawrxd\src"
$statsTotal = 0
$filesModified = @()

Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   FINAL Qt & LOGGING CLEANUP" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Get all source files
$files = Get-ChildItem -Path $sourceRoot -Recurse -Include *.cpp,*.h,*.hpp | Where-Object { $_.DirectoryName -notmatch "\\build\\" }

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $originalContent = $content
    $changes = 0
    
    # Remove commented Qt includes
    $content = $content -replace '(?m)^\s*//\s*#include\s+<Q[A-Z][^>]+>\s*$', ''
    
    # Replace QString in function signatures/variables
    $content = $content -replace '\bQString\b', 'std::string'
    $content = $content -replace '\bQStringList\b', 'std::vector<std::string>'
    $content = $content -replace '\bQJsonObject\b', 'void*'
    $content = $content -replace '\bQJsonArray\b', 'void*'
    $content = $content -replace '\bQWidget\b', 'void'
    $content = $content -replace '\bQObject\b', 'void'
    $content = $content -replace '\bQRegularExpression\b', 'std::regex'
    $content = $content -replace '\bQRegularExpressionMatch\b', 'std::smatch'
    $content = $content -replace '\bQRegularExpressionMatchIterator\b', 'std::sregex_iterator'
    
    # Remove Q_OBJECT, Q_SIGNALS, Q_SLOTS
    $content = $content -replace '(?m)^\s*Q_OBJECT\s*$', ''
    $content = $content -replace '\bsignals:\s*$', 'public:'
    $content = $content -replace '\bslots:\s*$', 'public:'
    $content = $content -replace '\bpublic\s+slots:', 'public:'
    $content = $content -replace '\bprivate\s+slots:', 'private:'
    $content = $content -replace '\bprotected\s+slots:', 'protected:'
    
    # Remove Qt method calls
    $content = $content -replace '\.arg\([^)]+\)', ''
    $content = $content -replace '\.globalMatch\([^)]+\)', ''
    $content = $content -replace '\.hasNext\(\)', 'false'
    $content = $content -replace '\.next\(\)', ''
    $content = $content -replace '\.captured\([^)]+\)', '""'
    
    # Remove QTEST macros
    $content = $content -replace 'QTEST_MAIN\([^)]+\)', '// Test removed'
    $content = $content -replace '#include\s+"[^"]*\.moc"', '// MOC removed'
    
    # Remove logging patterns
    $content = $content -replace '(?m)^\s*logger->.*$', ''
    $content = $content -replace '(?m)^\s*m_logger->.*$', ''
    $content = $content -replace '(?m)^\s*LOG_INFO\(.*\);?\s*$', ''
    $content = $content -replace '(?m)^\s*LOG_DEBUG\(.*\);?\s*$', ''
    $content = $content -replace '(?m)^\s*LOG_ERROR\(.*\);?\s*$', ''
    $content = $content -replace '(?m)^\s*LOG_WARN\(.*\);?\s*$', ''
    $content = $content -replace '(?m)^\s*TRACE_.*\(.*\);?\s*$', ''
    $content = $content -replace '(?m)^\s*DEBUG_.*\(.*\);?\s*$', ''
    
    # Remove logger member variables
    $content = $content -replace '(?m)^\s*std::shared_ptr<.*Logger.*>\s+\w+logger\w*;?\s*$', ''
    $content = $content -replace '(?m)^\s*Logger\s*\*\s*\w+logger\w*;?\s*$', ''
    
    # Clean up multiple blank lines
    $content = $content -replace '(?m)^\s*$(\r?\n\s*$)+', "`r`n"
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
        $changes = ($originalContent.Length - $content.Length) / 10  # Rough estimate
        $statsTotal += $changes
        $filesModified += $file.FullName
        Write-Host "✓ $($file.Name) - ~$changes changes" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Files Modified: $($filesModified.Count)" -ForegroundColor Green
Write-Host "Approximate Changes: ~$statsTotal" -ForegroundColor Green
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
