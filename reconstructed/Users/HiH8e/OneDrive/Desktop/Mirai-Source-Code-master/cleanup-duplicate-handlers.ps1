# Clean up duplicate DOMContentLoaded handlers
# Remove all duplicate handlers except the one at the end

$content = Get-Content "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -Raw

Write-Host "🧹 Cleaning up duplicate DOMContentLoaded handlers..." -ForegroundColor Yellow

# Find all instances of the consolidated handler
$handlerPattern = '(?s)// Consolidated DOMContentLoaded Handler.*?console\.log\(''✅ All IDE components initialized successfully''\);\s*\}\);'

# Keep only the last instance (the properly wrapped one)
$matches = [regex]::Matches($content, $handlerPattern)
if ($matches.Count -gt 1) {
    Write-Host "   Found $($matches.Count) duplicate handlers" -ForegroundColor Cyan

    # Remove all but the last one
    for ($i = 0; $i -lt $matches.Count - 1; $i++) {
        $match = $matches[$i]
        $content = $content.Remove($match.Index, $match.Length)
        Write-Host "   Removed duplicate handler at position $($match.Index)" -ForegroundColor White
    }
}

# Also clean up any remaining "moved to end of file" comments
$content = $content -replace '// Consolidated DOMContentLoaded handler moved to end of file.*?`;?', ''

# Write back the cleaned content
$content | Set-Content "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -Encoding UTF8

Write-Host "✅ Duplicate handlers cleaned up!" -ForegroundColor Green
Write-Host "📁 File: C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -ForegroundColor Cyan