# PowerShell script to remove Qt dependencies from CMakeLists.txt files
# This script removes find_package(Qt6), qt_add_executable, and other Qt-specific commands

# Main CMakeLists file
$mainCMake = "D:\rawrxd\CMakeLists.txt"
$content = Get-Content $mainCMake -Raw

# Count original
$beforeCount = ($content | Measure-Object -Line).Lines
Write-Host "=== Removing Qt from CMakeLists.txt ===" -ForegroundColor Cyan
Write-Host "Original file: $beforeCount lines"

# Remove Qt-specific find_package
$content = $content -replace "find_package\(Qt6.*?\)", ""
$content = $content -replace "^\s*find_package\(Qt6[^\)]*\)[`n]*", ""

# Remove qt_add_executable and related functions
$content = $content -replace "qt_add_executable\(", "add_executable("
$content = $content -replace "qt_add_library\(", "add_library("
$content = $content -replace "qt_generate_moc\([^\)]+\)", ""
$content = $content -replace "qt_add_translation\([^\)]+\)", ""
$content = $content -replace "qt_add_resources\([^\)]+\)", ""

# Remove TARGET_LINK_LIBRARIES entries that link Qt libraries
$lines = $content -split "`n"
$newLines = @()
foreach ($line in $lines) {
    # Skip lines that are just Qt library links
    if ($line -match "^\s*target_link_libraries.*Qt[0-9]" -or 
        $line -match "^\s*target_link_libraries.*Qt::" -or
        $line -match "Qt6::" -or
        $line -match "\$\{Qt6_LIBRARIES\}") {
        # Remove Qt library references but keep the line if there are other libraries
        $line = $line -replace "\s*Qt6::[A-Za-z0-9]+\s*", ""
        $line = $line -replace "\s*\$\{Qt6_LIBRARIES\}\s*", ""
        $line = $line -replace "\s*Qt[0-9]Core\s*", ""
        $line = $line -replace "\s*Qt[0-9]Gui\s*", ""
        $line = $line -replace "\s*Qt[0-9]Widgets\s*", ""
        # Only add non-empty lines
        if ($line.Trim() -ne "" -and $line.Trim() -ne "target_link_libraries()") {
            $newLines += $line
        }
    } else {
        $newLines += $line
    }
}
$content = $newLines -join "`n"

# Remove include_directories that reference Qt
$content = $content -replace "include_directories\(\s*\$\{Qt6_INCLUDE_DIRS\}\s*\)", ""
$content = $content -replace "include_directories\(\s*\$\{Qt6_INCLUDE_DIRS\}", "include_directories("

# Remove empty find_package blocks
$content = $content -replace "find_package\(\s*\)", ""

# Remove multiple consecutive blank lines
$content = $content -replace "`n\s*`n\s*`n", "`n`n"

# Write back
Set-Content $mainCMake -Value $content -Force

$afterCount = ($content | Measure-Object -Line).Lines
Write-Host "Modified file: $afterCount lines (removed $($beforeCount - $afterCount) lines)" -ForegroundColor Green
Write-Host "✓ Qt dependencies removed from CMakeLists.txt" -ForegroundColor Green
