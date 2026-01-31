# Setup-VSCode-Path.ps1
# Helps locate and configure VS Code after moving to E drive

Write-Host "🔍 VS Code Location Helper" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Check common locations
$locations = @(
    @{ Path = "E:\Everything\~dev"; Description = "E drive dev folder" },
    @{ Path = "D:\~dev"; Description = "D drive dev folder (original)" },
    @{ Path = "$env:LOCALAPPDATA\Programs\Microsoft VS Code"; Description = "Standard AppData location" },
    @{ Path = "$env:ProgramFiles\Microsoft VS Code"; Description = "Program Files" },
    @{ Path = "$env:ProgramFiles(x86)\Microsoft VS Code"; Description = "Program Files (x86)" },
    @{ Path = "E:\Program Files\Microsoft VS Code"; Description = "E drive Program Files" },
    @{ Path = "E:\Microsoft VS Code"; Description = "E drive root" },
    @{ Path = "E:\Everything\Microsoft VS Code"; Description = "E drive Everything folder" },
    @{ Path = "E:\Everything\VSCode"; Description = "E drive VSCode folder" }
)

$foundLocations = @()

Write-Host "`n📂 Checking common locations...`n" -ForegroundColor Yellow

foreach ($location in $locations) {
    if (Test-Path $location.Path) {
        Write-Host "✅ Found: $($location.Description)" -ForegroundColor Green
        Write-Host "   Path: $($location.Path)" -ForegroundColor Gray
        
        # Check for code.exe
        $codeExe = Get-ChildItem -Path $location.Path -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 2 | Select-Object -First 1
        if ($codeExe) {
            Write-Host "   ✅ code.exe found: $($codeExe.FullName)" -ForegroundColor Green
            $foundLocations += @{
                Path = $codeExe.FullName
                Directory = $codeExe.DirectoryName
                InstallPath = $location.Path
                Description = $location.Description
            }
        } else {
            Write-Host "   ⚠️  Directory exists but code.exe not found" -ForegroundColor Yellow
        }
    }
}

if ($foundLocations.Count -eq 0) {
    Write-Host "`n❌ VS Code installation not found in common locations" -ForegroundColor Red
    Write-Host "`n💡 Options:" -ForegroundColor Yellow
    Write-Host "   1. VS Code may still be on D drive" -ForegroundColor Gray
    Write-Host "   2. The move may not have completed" -ForegroundColor Gray
    Write-Host "   3. VS Code might be in a custom location" -ForegroundColor Gray
    Write-Host "   4. You may need to reinstall VS Code" -ForegroundColor Gray
    
    Write-Host "`n🔍 Let's search more thoroughly..." -ForegroundColor Cyan
    Write-Host "   This may take a few minutes...`n" -ForegroundColor Gray
    
    # Deep search on E drive
    Write-Host "   Searching E drive for code.exe..." -ForegroundColor Yellow
    $deepSearch = Get-ChildItem -Path E:\ -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 6 | Select-Object -First 5
    
    if ($deepSearch) {
        Write-Host "   ✅ Found potential installations:" -ForegroundColor Green
        foreach ($result in $deepSearch) {
            Write-Host "      - $($result.FullName)" -ForegroundColor White
        }
    } else {
        Write-Host "   ❌ No code.exe found on E drive" -ForegroundColor Red
    }
    
    # Check D drive
    Write-Host "`n   Searching D drive for code.exe..." -ForegroundColor Yellow
    if (Test-Path "D:\") {
        $dDriveSearch = Get-ChildItem -Path D:\ -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 4 | Select-Object -First 3
        
        if ($dDriveSearch) {
            Write-Host "   ✅ Found on D drive:" -ForegroundColor Green
            foreach ($result in $dDriveSearch) {
                Write-Host "      - $($result.FullName)" -ForegroundColor White
            }
        } else {
            Write-Host "   ❌ Not found on D drive either" -ForegroundColor Red
        }
    }
} else {
    Write-Host "`n✅ Found $($foundLocations.Count) VS Code installation(s)!" -ForegroundColor Green
    
    $primary = $foundLocations[0]
    Write-Host "`n📋 Primary Installation:" -ForegroundColor Cyan
    Write-Host "   Location: $($primary.InstallPath)" -ForegroundColor White
    Write-Host "   Executable: $($primary.Path)" -ForegroundColor White
    
    # Check for code.cmd
    $binDir = Join-Path $primary.InstallPath "bin"
    $codeCmd = Join-Path $binDir "code.cmd"
    
    if (Test-Path $codeCmd) {
        Write-Host "   ✅ Command-line launcher found: $codeCmd" -ForegroundColor Green
    }
    
    Write-Host "`n💡 To use this installation:" -ForegroundColor Yellow
    Write-Host "   1. Add to PATH: $binDir" -ForegroundColor Gray
    Write-Host "   2. Create shortcut to: $($primary.Path)" -ForegroundColor Gray
    Write-Host "   3. Or run: & '$($primary.Path)'" -ForegroundColor Gray
    
    # Offer to add to PATH
    Write-Host "`n❓ Would you like to add VS Code to your PATH? (y/N)" -ForegroundColor Cyan
    $response = Read-Host
    if ($response -eq "y" -or $response -eq "Y") {
        $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($currentPath -notlike "*$binDir*") {
            [Environment]::SetEnvironmentVariable("Path", "$currentPath;$binDir", "User")
            Write-Host "✅ Added $binDir to PATH" -ForegroundColor Green
            Write-Host "   You may need to restart your terminal for changes to take effect" -ForegroundColor Yellow
        } else {
            Write-Host "ℹ️  VS Code is already in your PATH" -ForegroundColor Yellow
        }
    }
}

Write-Host "`n" + "=" * 60 -ForegroundColor Cyan

