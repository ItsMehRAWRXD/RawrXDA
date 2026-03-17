# Remove ALL emojis and fix character encoding issues
# Hardens pipelines by removing malformed UTF-8 characters

Write-Host "Removing emojis and fixing character encoding..." -ForegroundColor Yellow
Write-Host ""

$emojiPattern = '[^\x00-\x7F]'
$files = Get-ChildItem -Path . -Include *.py,*.md,*.bat,*.ps1 -Recurse

$totalFixed = 0

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -Encoding UTF8
    $originalContent = $content
    
    # Remove common emojis and their variations
    $content = $content -replace '🚀', '[ROCKET]'
    $content = $content -replace '✓', '[OK]'
    $content = $content -replace '✅', '[OK]'
    $content = $content -replace '❌', '[X]'
    $content = $content -replace '⚠️', '[WARNING]'
    $content = $content -replace '⚠', '[WARNING]'
    $content = $content -replace '📋', '[CLIPBOARD]'
    $content = $content -replace '🎉', '[SUCCESS]'
    $content = $content -replace '🎯', '[TARGET]'
    $content = $content -replace '⚡', '[LIGHTNING]'
    $content = $content -replace '📦', '[PACKAGE]'
    $content = $content -replace '🐛', '[BUG]'
    $content = $content -replace '🔧', '[WRENCH]'
    $content = $content -replace '🎮', '[GAME]'
    $content = $content -replace '🌐', '[GLOBE]'
    $content = $content -replace '📊', '[CHART]'
    $content = $content -replace '📁', '[FOLDER]'
    $content = $content -replace '✨', '[SPARKLES]'
    $content = $content -replace 'ð\x9F[\x80-\xBF][\x80-\xBF]', '[EMOJI]'
    
    # Remove any remaining non-ASCII characters
    $content = $content -replace $emojiPattern, ''
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding ASCII -NoNewline
        Write-Host "[FIXED] $($file.Name)" -ForegroundColor Green
        $totalFixed++
    }
}

Write-Host ""
Write-Host "Complete! Fixed $totalFixed files" -ForegroundColor Cyan
Write-Host "All emojis removed and encoding normalized to ASCII" -ForegroundColor Green
