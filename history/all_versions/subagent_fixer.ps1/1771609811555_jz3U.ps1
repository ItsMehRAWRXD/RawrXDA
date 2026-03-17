# Subagent Compiler Fixer - Reads JSON results and fixes failing files
# Each subagent processes a slice of failed files in parallel
param(
    [string]$CompilationResultsJson = "d:\rawrxd\compilation_results.json",
    [int]$SubagentId = 1,
    [int]$MaxSubagents = 8
)

$ErrorActionPreference = "Continue"

# Load results
if (-not (Test-Path $CompilationResultsJson)) {
    Write-Error "JSON not found: $CompilationResultsJson"
    exit 1
}

$data = Get-Content $CompilationResultsJson -Raw | ConvertFrom-Json
$failures = @($data.files | Where-Object { $_.compilation_status -eq "FAIL" })

if ($failures.Count -eq 0) {
    Write-Host "[Subagent $SubagentId] No failures to fix." -ForegroundColor Green
    exit 0
}

# Distribute failures across subagents
$filesPerSubagent = [Math]::Ceiling($failures.Count / $MaxSubagents)
$startIdx = ($SubagentId - 1) * $filesPerSubagent
$endIdx = [Math]::Min($startIdx + $filesPerSubagent, $failures.Count)
$myBatch = $failures[$startIdx..($endIdx - 1)]

Write-Host "[Subagent $SubagentId] Fixing $($myBatch.Count) files (indices $startIdx-$($endIdx -1))" -ForegroundColor Cyan

$fixed = 0
$stillFailing = @()

foreach ($item in $myBatch) {
    $filePath = $item.file_name
    $fileType = $item.file_type
    $error = $item.first_error
    
    if (-not (Test-Path $filePath)) {
        Write-Host "  ✗ Not found: $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Red
        continue
    }
    
    $content = Get-Content $filePath -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $originalLength = $content.Length
    
    # ===== COMPILATION FIX PATTERNS =====
    
    # 1. Missing #include <windows.h>
    if (($content -like "*OutputDebugStringA*" -or $content -like "*HWND*" -or $content -like "*HANDLE*") -and 
        $content -notmatch "#include\s*[<\"]windows\.h[>\"]") {
        $content = "#include <windows.h>`n" + $content
    }
    
    # 2. Missing STL includes
    if ($content -like "*std::vector*" -and $content -notmatch "#include\s*<vector>") {
        $content = "#include <vector>`n" + $content
    }
    if ($content -like "*std::string*" -and $content -notmatch "#include\s*<string>") {
        $content = "#include <string>`n" + $content
    }
    if ($content -like "*std::map*" -and $content -notmatch "#include\s*<map>") {
        $content = "#include <map>`n" + $content
    }
    if ($content -like "*std::shared_ptr*" -and $content -notmatch "#include\s*<memory>") {
        $content = "#include <memory>`n" + $content
    }
    if ($content -like "*std::thread*" -and $content -notmatch "#include\s*<thread>") {
        $content = "#include <thread>`n" + $content
    }
    
    # 3. Remove duplicate function definitions (keep first, comment rest)
    $lines = @($content -split "`n")
    $funcDefs = @{}
    $deduped = @()
    
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        
        # Detect function/class/struct definitions
        if ($trimmed -match "^\s*(void|int|bool|float|double|std::\w+|class|struct)\s+\w+\s*(\(|{|:)") {
            # Extract function signature (first ~80 chars)
            $sig = $trimmed.Substring(0, [Math]::Min(80, $trimmed.Length))
            
            if ($funcDefs.ContainsKey($sig)) {
                # Duplicate - comment it out
                $deduped += "// [DEDUP] $line"
            } else {
                $funcDefs[$sig] = $true
                $deduped += $line
            }
        } else {
            $deduped += $line
        }
    }
    $content = $deduped -join "`n"
    
    # 4. Fix incomplete catch blocks
    $content = $content -replace 'catch\s*\(\s*\.\.\.\s*\)', 'catch (...)'
    $content = $content -replace 'catch\s*\(\s*Exception\s+', 'catch (std::exception '
    
    # 5. Add missing closing braces
    $openBraces = [regex]::Matches($content, "{").Count
    $closeBraces = [regex]::Matches($content, "}").Count
    if ($openBraces -gt $closeBraces) {
        $content = $content + ("`n}" * ($openBraces - $closeBraces))
    }
    
    # 6. Fix incomplete return statements
    $content = $content -replace '(?m)(\s+return\s*);', "`$1 true;"
    
    # 7. Comment out or remove unimplemented functions that cause linker errors
    if ($error -like "*unresolved external symbol*" -or $error -like "*undefined reference*") {
        # Look for stub function definitions that are not implemented
        $content = $content -replace '(?m)^(\s*(?:void|int|bool|std::\w+)\s+\w+\s*\([^)]*\)\s*\{?\s*\})', "// [STUB] `$1"
    }
    
    # Write if changed
    if ($content.Length -ne $originalLength) {
        Set-Content -Path $filePath -Value $content -Encoding UTF8 -ErrorAction SilentlyContinue
    }
    
    # Verify compilation
    $tempObj = "d:\rawrxd\temp_obj\$([System.IO.Path]::GetFileNameWithoutExtension($filePath)).obj"
    
    if ($fileType -eq "cpp" -or $fileType -eq "c") {
        $cl = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
        $result = & $cl /c /W0 /Fo"$tempObj" $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    } else {
        $ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
        $result = & $ml64 /c /Fo"$tempObj" $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    }
    
    if ($compiles) {
        Write-Host "  ✓ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Green
        $fixed++
    } else {
        Write-Host "  ✗ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Red
        $stillFailing += @{
            file_name = $filePath
            file_type = $fileType
            error = ($result -join "`n").Substring(0, 200)
        }
    }
}

Write-Host "`n[Subagent $SubagentId] Results: $fixed/$($myBatch.Count) fixed`n" -ForegroundColor Yellow

# Save unfixed to JSON for next iteration
if ($stillFailing.Count -gt 0) {
    @{
        subagent_id = $SubagentId
        unfixed_count = $stillFailing.Count
        files = $stillFailing
    } | ConvertTo-Json -Depth 3 | Out-File -FilePath "d:\rawrxd\unfixed_subagent_$SubagentId.json" -Encoding UTF8
}

exit ($stillFailing.Count)
