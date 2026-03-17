# Script to remove Qt dependencies from CMakeLists.txt files

$cmakeFiles = Get-ChildItem -Path "D:\rawrxd" -Recurse -Filter "CMakeLists.txt"

foreach ($file in $cmakeFiles) {
    Write-Host "Processing $($file.FullName)..."
    
    $content = Get-Content -Path $file.FullName
    $newContent = @()
    
    foreach ($line in $content) {
        # Skip lines with find_package(Qt...)
        if ($line -match "find_package\s*\(\s*Qt") {
            continue
        }
        
        # Skip lines with Qt modules in target_link_libraries (simplified check)
        # This might be too aggressive if multiple libs are on one line, but for now we assume they are listed individually or formatted nicely
        if ($line -match "Qt[56]::\w+") {
            continue
        }
        
        # Skip lines with cmake_automoc/autouic/autorcc
        if ($line -match "(CMAKE_AUTOMOC|CMAKE_AUTOUIC|CMAKE_AUTORCC|set\s*\(\s*CMAKE_AUTOMOC\s+ON\s*\))") {
            continue
        }

        # Clean up empty linking blocks or leftover artifacts could be hard, but let's start with removing the explicit deps.
        
        $newContent += $line
    }
    
    $newContent | Set-Content -Path $file.FullName
}

Write-Host "CMakeLists.txt cleanup complete."