# Nuclear option: Complete handler cleanup and re-add proper one

$content = Get-Content "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -Raw

Write-Host "💥 Nuclear Handler Cleanup..." -ForegroundColor Red

# Step 1: Remove ALL instances of consolidated handlers
Write-Host "1. Removing all consolidated handlers..." -ForegroundColor Yellow
$content = [regex]::Replace($content, '(?s)// Consolidated DOMContentLoaded Handler.*?console\.log\(''✅ All IDE components initialized successfully''\);\s*\}\);', '')

# Step 2: Remove all "moved to end of file" comments
Write-Host "2. Removing handler movement comments..." -ForegroundColor Yellow
$content = $content -replace '// Consolidated DOMContentLoaded handler moved to end of file.*?`;?', ''

# Step 3: Add the proper handler at the end
Write-Host "3. Adding proper consolidated handler..." -ForegroundColor Green
$properHandler = @"

    <script>
// Consolidated DOMContentLoaded Handler
ensureDOMReady(function() {
    console.log('🔧 DOM fully loaded, initializing IDE components...');

    // Initialize all components safely
    safeDOMOperation(() => initializeFunctions());
    safeDOMOperation(() => attachResizeTests());
    safeDOMOperation(() => addEditorToggleButton());
    safeDOMOperation(() => attachHyperIDEButton());
    safeDOMOperation(() => initializeAgenticFeatures());
    safeDOMOperation(() => setupEventListeners());

    console.log('✅ All IDE components initialized successfully');
});
    </script>
"@

# Replace the closing body tag with proper handler + body close
$content = $content -replace '</body>', ($properHandler + '</body>')

# Step 4: Final cleanup of any remaining issues
Write-Host "4. Final cleanup..." -ForegroundColor Cyan
$content = $content -replace '\$1\.', 'document.'

$content | Set-Content "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -Encoding UTF8

Write-Host "✅ Nuclear cleanup complete!" -ForegroundColor Green
Write-Host "🎯 Only one properly wrapped handler should remain at the end" -ForegroundColor White