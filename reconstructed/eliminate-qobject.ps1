# Script to brutally remove QObject references

$files = Get-ChildItem -Path "D:\rawrxd\src" -Recurse -Include *.cpp,*.h,*.hpp

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName
    $newContent = @()
    $modified = $false
    
    foreach ($line in $content) {
        $newLine = $line
        
        # Remove inheritance
        if ($newLine -match ":\s*(public|protected|private)\s+QObject") {
            $newLine = $newLine -replace ":\s*(public|protected|private)\s+QObject\s*,?", ":"
            # If it ended up as just "class Foo :", fix it
             if ($newLine -match ":\s*$") {
                $newLine = $newLine -replace ":\s*$", ""
             }
             $modified = $true
        }
        
        # Remove multiple inheritance part
        if ($newLine -match ",\s*(public|protected|private)\s+QObject") {
            $newLine = $newLine -replace ",\s*(public|protected|private)\s+QObject", ""
            $modified = $true
        }
        
        # Remove QObject pointers in function args
        if ($newLine -match "QObject\s*\*\s*\w+") {
            $newLine = $newLine -replace "QObject\s*\*\s*\w+\s*=\s*(nullptr|NULL|0)", "" # Remove default arg first
            $newLine = $newLine -replace "QObject\s*\*\s*\w+", "" # Remove arg
            # Fix dangling commas
             $newLine = $newLine -replace ",\s*\)", ")"
             $newLine = $newLine -replace "\(\s*,", "("
             $newLine = $newLine -replace ",\s*,", ","
            $modified = $true
        }

        # Remove straight up substitutions if it's a member variable
        if ($newLine -match "QObject\s*\*\s*\w+;") {
            $newLine = "// " + $newLine
            $modified = $true
        }

        $newContent += $newLine
    }
    
    if ($modified) {
        $newContent | Set-Content -Path $file.FullName
        Write-Host "De-QObjected $file"
    }
}
