# Delete specific files
$filesToDelete = @(
    "D:\rawrxd\src\agent\sentry_integration.cpp",
    "D:\rawrxd\src\agent\sentry_integration.hpp",
    "D:\rawrxd\src\registry_core\include\logger.hpp",
    "D:\rawrxd\src\ui\telemetry_optin_dialog.cpp",
    "D:\rawrxd\src\ui\telemetry_optin_dialog.h",
    "D:\rawrxd\src\win32app\IDELogger.h",
    "D:\rawrxd\src\win32app\Win32IDE_Logger.cpp"
)

# Pattern matching for deleting compliance logger files
$qtappDir = "D:\rawrxd\src\qtapp"
if (Test-Path $qtappDir) {
    Get-ChildItem -Path $qtappDir -Filter "compliance_logger_*.hpp" | Remove-Item -Force
    Get-ChildItem -Path $qtappDir -Filter "compliance_logger_*.cpp" | Remove-Item -Force
}

foreach ($file in $filesToDelete) {
    if (Test-Path $file) {
        Remove-Item -Path $file -Force
        Write-Host "Deleted: $file"
    }
}

# Now strip code from remaining files
$srcDir = "D:\rawrxd\src"
$filesToScan = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp

$patternsToRemove = @(
    '(?m)^#include\s+["<].*?logger.*?[">]\s*$',
    '(?m)^#include\s+["<].*?telemetry.*?[">]\s*$',
    '(?m)^#include\s+["<].*?sentry.*?[">]\s*$',
    '(?m)^#include\s+["<]IDELogger\.h[">]\s*$',
    # Remove usage of IDELogger::...
    'IDELogger::getInstance\(\)\.log\(.*?\);',
    'IDELogger::getInstance\(\)\.initialize\(.*?\);',
    # Remove blocks like if (telemetry_) { ... }
    '(?s)if\s*\(\s*telemetry_\s*\)\s*\{.*?\}',
    # Remove member variables like Telemetry* telemetry_;
    '\s*Telemetry\*\s*telemetry_(\s*=\s*nullptr)?\s*;',
    '\s*std::unique_ptr<Telemetry>\s*telemetry_(\s*=\s*nullptr)?\s*;'
)

foreach ($file in $filesToScan) {
    # Skip if file was just deleted
    if (-not (Test-Path $file.FullName)) { continue }

    $content = Get-Content -Path $file.FullName -Raw
    $originalContent = $content
    
    foreach ($pattern in $patternsToRemove) {
        $content = $content -replace $pattern, ''
    }
    
    # Specific fix for Win32IDEBridge.cpp and similar using telemetry_->
    $content = $content -replace 'telemetry_->.*?;', ''
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
        Write-Host "Cleaned: $($file.FullName)"
    }
}
