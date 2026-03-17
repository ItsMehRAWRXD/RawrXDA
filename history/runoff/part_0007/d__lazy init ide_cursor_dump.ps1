# CURSOR EXTRACTION SUITE
# Using OMEGA-POLYGLOT v4.0 PRO and Toolkit

$ErrorActionPreference = "Continue"
$cursorBase = "C:\Users\$env:USERNAME\AppData\Local\Programs\Cursor"
$outputBase = "D:\Cursor_Complete_Dump_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

Write-Host "=== CURSOR REVERSE ENGINEERING SUITE ===" -ForegroundColor Cyan
Write-Host "Output: $outputBase" -ForegroundColor Yellow
Write-Host ""

# Create output directory
New-Item -ItemType Directory -Path $outputBase -Force | Out-Null

# 1. EXTRACT ASAR ARCHIVES
Write-Host "[1/6] Extracting ASAR archives..." -ForegroundColor Green
$asarFiles = Get-ChildItem "$cursorBase\resources" -Recurse -Filter "*.asar"
foreach ($asar in $asarFiles) {
    $extractPath = Join-Path $outputBase "asar_$($asar.BaseName)"
    Write-Host "  Extracting: $($asar.Name) -> $extractPath"
    
    try {
        if (Get-Command npx -ErrorAction SilentlyContinue) {
            npx asar extract $asar.FullName $extractPath 2>&1 | Out-Null
            Write-Host "    ✓ Extracted $($asar.Name)" -ForegroundColor DarkGreen
        } else {
            Write-Host "    ⚠ npx not found, skipping ASAR extraction" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "    ✗ Failed: $_" -ForegroundColor Red
    }
}

# 2. ANALYZE MAIN EXECUTABLES
Write-Host "`n[2/6] Analyzing executables with OMEGA-POLYGLOT..." -ForegroundColor Green
$exePath = Join-Path $outputBase "executable_analysis"
New-Item -ItemType Directory -Path $exePath -Force | Out-Null

$mainExe = "$cursorBase\Cursor.exe"
if (Test-Path $mainExe) {
    Write-Host "  Analyzing: Cursor.exe"
    
    # Use dumpbin if available
    if (Test-Path "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue) {
        $dumpbin = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($dumpbin) {
            Write-Host "    Running dumpbin /headers..."
            & $dumpbin.FullName /headers $mainExe > "$exePath\Cursor_exe_headers.txt" 2>&1
            
            Write-Host "    Running dumpbin /imports..."
            & $dumpbin.FullName /imports $mainExe > "$exePath\Cursor_exe_imports.txt" 2>&1
            
            Write-Host "    Running dumpbin /exports..."
            & $dumpbin.FullName /exports $mainExe > "$exePath\Cursor_exe_exports.txt" 2>&1
        }
    }
}

# 3. EXTRACT JAVASCRIPT/TYPESCRIPT SOURCE
Write-Host "`n[3/6] Extracting JavaScript/TypeScript source..." -ForegroundColor Green
$jsPath = Join-Path $outputBase "source_code"
New-Item -ItemType Directory -Path $jsPath -Force | Out-Null

Get-ChildItem $outputBase -Recurse -Include "*.js","*.ts","*.json" -ErrorAction SilentlyContinue | 
    ForEach-Object {
        $relativePath = $_.FullName.Replace($outputBase, "")
        Write-Host "  Found: $relativePath" -ForegroundColor DarkGray
    }

# 4. FIND COPILOT/AI RELATED FILES
Write-Host "`n[4/6] Finding AI/Copilot components..." -ForegroundColor Green
$aiPath = Join-Path $outputBase "ai_components"
New-Item -ItemType Directory -Path $aiPath -Force | Out-Null

Get-ChildItem $outputBase -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match "(copilot|ai|completion|chat|claude|openai|anthropic)" } |
    ForEach-Object {
        Write-Host "  AI Component: $($_.Name)" -ForegroundColor Magenta
        Copy-Item $_.FullName -Destination $aiPath -Force -ErrorAction SilentlyContinue
    }

# 5. EXTRACT STRINGS FROM EXECUTABLES
Write-Host "`n[5/6] Extracting strings from binaries..." -ForegroundColor Green
$stringsPath = Join-Path $outputBase "extracted_strings"
New-Item -ItemType Directory -Path $stringsPath -Force | Out-Null

Get-ChildItem "$cursorBase" -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 5 |
    ForEach-Object {
        $outFile = Join-Path $stringsPath "$($_.BaseName)_strings.txt"
        Write-Host "  Extracting strings from: $($_.Name)"
        
        # Simple string extraction (printable ASCII)
        $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
        $strings = New-Object System.Collections.ArrayList
        $current = ""
        
        foreach ($byte in $bytes) {
            if ($byte -ge 32 -and $byte -le 126) {
                $current += [char]$byte
            } elseif ($current.Length -ge 4) {
                [void]$strings.Add($current)
                $current = ""
            } else {
                $current = ""
            }
        }
        
        $strings | Out-File $outFile -Encoding UTF8
        Write-Host "    ✓ Extracted $($strings.Count) strings" -ForegroundColor DarkGreen
    }

# 6. ANALYZE PACKAGE.JSON FILES
Write-Host "`n[6/6] Analyzing package.json files..." -ForegroundColor Green
$packagePath = Join-Path $outputBase "package_analysis"
New-Item -ItemType Directory -Path $packagePath -Force | Out-Null

Get-ChildItem $outputBase -Recurse -Filter "package.json" -ErrorAction SilentlyContinue |
    ForEach-Object {
        Write-Host "  Found package: $($_.Directory.Name)"
        Copy-Item $_.FullName -Destination "$packagePath\$($_.Directory.Name)_package.json" -Force
        
        try {
            $pkg = Get-Content $_.FullName | ConvertFrom-Json
            Write-Host "    Name: $($pkg.name)" -ForegroundColor Cyan
            Write-Host "    Version: $($pkg.version)" -ForegroundColor Cyan
            if ($pkg.dependencies) {
                Write-Host "    Dependencies: $($pkg.dependencies.PSObject.Properties.Name.Count)" -ForegroundColor DarkGray
            }
        } catch {}
    }

# Generate summary
Write-Host "`n=== EXTRACTION COMPLETE ===" -ForegroundColor Green
Write-Host "Output location: $outputBase" -ForegroundColor Yellow

$summary = @"
CURSOR DUMP SUMMARY
===================
Date: $(Get-Date)
Location: $outputBase

Directory Structure:
- asar_* : Extracted ASAR archives
- executable_analysis : PE file analysis (headers, imports, exports)
- source_code : JavaScript/TypeScript files
- ai_components : AI/Copilot related files
- extracted_strings : String dumps from binaries
- package_analysis : package.json files

Files Extracted:
- ASAR archives: $($asarFiles.Count)
- Total files: $(Get-ChildItem $outputBase -Recurse -File | Measure-Object | Select-Object -ExpandProperty Count)
- Total size: $([math]::Round((Get-ChildItem $outputBase -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB, 2)) MB

Next Steps:
1. Review ai_components directory for Copilot implementation
2. Analyze source_code for completion logic
3. Check executable_analysis for API keys or endpoints
4. Search extracted_strings for configuration data
"@

$summary | Out-File "$outputBase\README.txt" -Encoding UTF8
Write-Host "`n$summary"

Write-Host "`nDone!" -ForegroundColor Green
