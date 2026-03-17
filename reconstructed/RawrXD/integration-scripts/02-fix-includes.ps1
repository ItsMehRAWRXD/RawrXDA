# Fix #include statements in newly integrated files
# Ensures all headers point to correct locations in the new structure

param(
    [string]$SrcPath = "D:\rawrxd\src",
    [switch]$DryRun = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Continue"
$fixCount = 0

Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║ RawrXD Include Path Auto-Fixer                        ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Common path remappings
$pathMappings = @{
    # Old source structure → New structure
    'src/qtapp/streaming_inference.hpp' = 'streaming_inference.hpp'
    'src/qtapp/inference_engine.hpp' = 'inference_engine.hpp'
    'src/qtapp/gguf_loader.hpp' = 'gguf_loader.hpp'
    'src/qtapp/model_memory_hotpatch.hpp' = 'model_memory_hotpatch.hpp'
    'src/qtapp/ai_digestion_engine.h' = 'ai_digestion_engine.h'
    'src/qtapp/metrics_collector.hpp' = 'metrics_collector.hpp'
    'src/agentic/agentic_engine.h' = 'agentic_engine.h'
    'src/agentic/agentic_executor.h' = 'agentic_executor.h'
}

$files = Get-ChildItem -Path $SrcPath -Include "*.cpp", "*.h", "*.hpp" -Recurse

Write-Host "`nScanning $($files.Count) files for include path issues..." -ForegroundColor Cyan

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    $originalContent = $content
    $localFixes = 0
    
    foreach ($oldPath in $pathMappings.Keys) {
        $newPath = $pathMappings[$oldPath]
        
        # Fix various include patterns
        $patterns = @(
            "#include `"$oldPath`"",
            "#include <$oldPath>",
            "#include `"../$oldPath`"",
            "#include `"../../$oldPath`""
        )
        
        foreach ($pattern in $patterns) {
            if ($content -match [regex]::Escape($pattern)) {
                $content = $content -replace [regex]::Escape($pattern), "#include `"$newPath`""
                $localFixes++
            }
        }
    }
    
    # Also add include directories if missing
    if ($file.Name -match '\.cpp$' -and $content -match 'streaming_inference|inference_engine|gguf_loader') {
        if ($content -notmatch '#include ".*\.hpp"') {
            # File uses these components but no explicit includes - this is OK, might use forward declarations
            if ($Verbose) {
                Write-Host "  ⓘ $($file.Name) - using forward declarations" -ForegroundColor Gray
            }
        }
    }
    
    if ($localFixes -gt 0) {
        if (-not $DryRun) {
            Set-Content -Path $file.FullName -Value $content -NoNewline
        }
        Write-Host "✓ Fixed: $($file.Name) ($localFixes replacements)" -ForegroundColor Green
        $fixCount += $localFixes
    }
}

Write-Host "`n╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║ Include Path Fixing Complete                          ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host "Total fixes applied: $fixCount" -ForegroundColor Cyan
Write-Host "DryRun: $DryRun" -ForegroundColor Gray
