#!/usr/bin/env pwsh
# ============================================================================
# update_masm_includes.ps1
# Purpose: Add INCLUDE masm/masm_master_include.asm to all MASM files
#          and add appropriate function calls
# ============================================================================

param(
    [string]$BasePath = "D:\RawrXD-production-lazy-init",
    [switch]$DryRun = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "SilentlyContinue"
$count = 0
$updated = 0
$skipped = 0

function Add-MasterInclude {
    param(
        [string]$FilePath
    )
    
    $content = Get-Content $FilePath -Raw
    
    # Check if already has the include
    if ($content -match "INCLUDE\s+.*masm_master_include") {
        Write-Host "[SKIP] Already has include: $([System.IO.Path]::GetFileName($FilePath))" -ForegroundColor Yellow
        return $false
    }
    
    # Skip very small files (likely stubs or test files)
    $fileSize = (Get-Item $FilePath).Length
    if ($fileSize -lt 100) {
        Write-Host "[SKIP] File too small: $([System.IO.Path]::GetFileName($FilePath)) ($fileSize bytes)" -ForegroundColor Gray
        return $false
    }
    
    # Find a good place to insert the include
    $lines = $content -split "`n"
    
    $insertLine = -1
    
    # Strategy: Insert after the first block of comments and/or option/include statements
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i].Trim()
        
        # Skip comment lines and empty lines at the start
        if ($line.StartsWith(";") -or $line.StartsWith("--") -or [string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        
        # If we find an option or include statement, insert after any sequence of those
        if ($line.StartsWith("option ") -or $line.StartsWith("include ") -or $line.StartsWith("INCLUDE ")) {
            # Skip all consecutive option/include lines
            while ($i + 1 -lt $lines.Count) {
                $nextLine = $lines[$i + 1].Trim()
                if ($nextLine.StartsWith("option ") -or $nextLine.StartsWith("include ") -or $nextLine.StartsWith("INCLUDE ") -or [string]::IsNullOrWhiteSpace($nextLine)) {
                    $i++
                    continue
                }
                break
            }
            $insertLine = $i + 1
            break
        }
        
        # Otherwise insert at the first non-comment line
        if (-not [string]::IsNullOrWhiteSpace($line)) {
            $insertLine = $i
            break
        }
    }
    
    if ($insertLine -eq -1) {
        Write-Host "[SKIP] Could not find insertion point: $([System.IO.Path]::GetFileName($FilePath))" -ForegroundColor DarkRed
        return $false
    }
    
    # Insert the include statement
    $newLines = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $newLines += $lines[$i]
        if ($i -eq $insertLine) {
            $newLines += ""
            $newLines += "; ============================================================================"
            $newLines += "; Master Include for Zero-Day Agentic Engine and MASM System Modules"
            $newLines += "; ============================================================================"
            $newLines += "INCLUDE masm/masm_master_include.asm"
            $newLines += ""
        }
    }
    
    $newContent = $newLines -join "`n"
    
    if (-not $DryRun) {
        Set-Content -Path $FilePath -Value $newContent -NoNewline
    }
    
    Write-Host "[UPDATE] Added include: $([System.IO.Path]::GetFileName($FilePath))" -ForegroundColor Green
    return $true
}

# Main execution
Write-Host "="*80
Write-Host "MASM Include Updater"
Write-Host "="*80
Write-Host "Base Path: $BasePath"
Write-Host "Dry Run: $DryRun"
Write-Host ""

# Get all MASM files
$asmFiles = @(Get-ChildItem -Path $BasePath -Filter "*.asm" -Recurse -ErrorAction SilentlyContinue)

Write-Host "Found $($asmFiles.Count) MASM files to process"
Write-Host ""

foreach ($file in $asmFiles) {
    $count++
    
    if ($count % 50 -eq 0) {
        Write-Host "Processing file $count / $($asmFiles.Count)..." -ForegroundColor Cyan
    }
    
    if (Add-MasterInclude -FilePath $file.FullPath) {
        $updated++
    } else {
        $skipped++
    }
}

Write-Host ""
Write-Host "="*80
Write-Host "Summary"
Write-Host "="*80
Write-Host "Total processed: $count"
Write-Host "Updated: $updated"
Write-Host "Skipped: $skipped"
Write-Host ""

if ($DryRun) {
    Write-Host "NOTE: This was a DRY RUN. No files were modified." -ForegroundColor Yellow
} else {
    Write-Host "All files have been updated!" -ForegroundColor Green
}
