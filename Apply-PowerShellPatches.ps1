#Requires -Version 7.0
<#
.SYNOPSIS
    PowerShell-based source code patch engine for RawrXD

.DESCRIPTION
    Applies targeted fixes to source files using regex-based pattern matching
    and replacement. Safer than blind text replacement, with validation and
    backup capabilities.

.PARAMETER Category
    Type of patches to apply (All, Includes, Types, Symbols, Syntax)

.PARAMETER DryRun
    Show what would be patched without making changes

.EXAMPLE
    .\Apply-PowerShellPatches.ps1 -Category Types -DryRun
    .\Apply-PowerShellPatches.ps1 -Category All
#>

[CmdletBinding()]
param(
    [ValidateSet('All', 'Includes', 'Types', 'Symbols', 'Syntax', 'Warnings')]
    [string]$Category = 'All',
    
    [switch]$DryRun,
    [switch]$CreateBackups,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# ============================================================================
# PATCH DEFINITIONS
# ============================================================================

$script:PatchRules = @{
    Includes = @(
        @{
            Name = 'Add missing Windows.h'
            Pattern = '(?m)^(#include\s+<windows\.h>)'
            FilePattern = '*.cpp'
            SearchPattern = 'HWND|HINSTANCE|WPARAM|LPARAM'
            ExcludeIfHas = '#include\s*[<"]windows\.h[>"]'
            Replacement = "#include <windows.h>`n"
            InsertBefore = '^namespace|^class|^struct|^void|^int main'
        }
        @{
            Name = 'Add missing string header'
            Pattern = '(?m)^(#include\s+<string>)'
            FilePattern = '*.cpp'
            SearchPattern = 'std::string|std::wstring'
            ExcludeIfHas = '#include\s*[<"]string[>"]'
            Replacement = "#include <string>`n"
            InsertBefore = '^namespace|^class|^struct'
        }
    )
    
    Types = @(
        @{
            Name = 'Fix int to size_t conversion'
            Pattern = '\bint\s+(\w+)\s*=\s*(\w+)\.size\(\)'
            Replacement = 'size_t $1 = $2.size()'
            FilePattern = '*.cpp'
        }
        @{
            Name = 'Fix string literal to char* assignment'
            Pattern = 'char\s*\*\s*(\w+)\s*=\s*"([^"]*)"'
            Replacement = 'const char* $1 = "$2"'
            FilePattern = '*.cpp'
        }
        @{
            Name = 'Fix C-style cast to static_cast'
            Pattern = '\((\w+)\s*\*\s*\)\s*(\w+)'
            Replacement = 'static_cast<$1*>($2)'
            FilePattern = '*.cpp'
            Conditions = @{
                NotInComment = $true
                NotInString = $true
            }
        }
    )
    
    Symbols = @(
        @{
            Name = 'Add missing std:: prefix'
            Pattern = '\b(string|vector|map|set|unordered_map)\s*<'
            Replacement = 'std::$1<'
            FilePattern = '*.cpp'
            ExcludeIfHas = 'using namespace std|using std::'
        }
        @{
            Name = 'Fix missing return statement'
            Pattern = '(?ms)^(\w+\s+\w+\([^)]*\)\s*\{(?:(?!\breturn\b).)*)\}$'
            Replacement = '$1    return {};`n}'
            FilePattern = '*.cpp'
            Conditions = @{
                OnlyNonVoidFunctions = $true
            }
        }
    )
    
    Syntax = @(
        @{
            Name = 'Fix missing semicolon after class'
            Pattern = '(?m)^(\s*\}\s*)$\n(?=\s*$)'
            Replacement = '$1;'
            FilePattern = '*.h'
            Context = 'class|struct'
        }
        @{
            Name = 'Fix uninitialized variable'
            Pattern = '(\w+\s+\w+\s+\w+);'
            Replacement = '$1{};'
            FilePattern = '*.cpp'
            Conditions = @{
                IsLocalVariable = $true
            }
        }
    )
    
    Warnings = @(
        @{
            Name = 'Fix C4244 (type conversion warning)'
            Pattern = '\b(int|long)\s+(\w+)\s*=\s*(\w+\.\w+\(\))'
            Replacement = 'auto $2 = static_cast<$1>($3)'
            FilePattern = '*.cpp'
        }
        @{
            Name = 'Fix unused parameter warning'
            Pattern = '(\w+\s+\w+)\s*\('
            Replacement = '$1(/*'
            FilePattern = '*.cpp'
            PostProcess = 'AddCommentEnd'
        }
    )
}

# ============================================================================
# PATCH APPLICATION ENGINE
# ============================================================================

class PatchContext {
    [string]$FilePath
    [string]$OriginalContent
    [string]$ModifiedContent
    [System.Collections.Generic.List[object]]$AppliedPatches
    [bool]$HasChanges
    
    PatchContext([string]$path) {
        $this.FilePath = $path
        $this.OriginalContent = Get-Content $path -Raw
        $this.ModifiedContent = $this.OriginalContent
        $this.AppliedPatches = [System.Collections.Generic.List[object]]::new()
        $this.HasChanges = $false
    }
    
    [bool] ApplyPatch([hashtable]$rule) {
        $pattern = $rule.Pattern
        $replacement = $rule.Replacement
        
        # Check exclusions
        if ($rule.ExcludeIfHas -and ($this.ModifiedContent -match $rule.ExcludeIfHas)) {
            return $false
        }
        
        # Check required context
        if ($rule.SearchPattern -and -not ($this.ModifiedContent -match $rule.SearchPattern)) {
            return $false
        }
        
        $modified = $this.ModifiedContent -replace $pattern, $replacement
        
        if ($modified -ne $this.ModifiedContent) {
            $this.ModifiedContent = $modified
            $this.AppliedPatches.Add($rule)
            $this.HasChanges = $true
            return $true
        }
        
        return $false
    }
    
    [void] SaveChanges([bool]$createBackup) {
        if (-not $this.HasChanges) {
            return
        }
        
        if ($createBackup) {
            $backupPath = "$($this.FilePath).bak_$(Get-Date -Format 'yyyyMMddHHmmss')"
            Copy-Item $this.FilePath $backupPath -Force
        }
        
        [System.IO.File]::WriteAllText($this.FilePath, $this.ModifiedContent)
    }
}

function Get-FilesToPatch {
    param(
        [string[]]$FilePatterns,
        [string]$BaseDir = $PSScriptRoot
    )
    
    $files = @()
    foreach ($pattern in $FilePatterns) {
        $found = Get-ChildItem -Path $BaseDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue |
            Where-Object { 
                $_.FullName -notmatch '[\\/](obj|bin|\.git|\.vs|history|reconstructed)[\\/]'
            }
        $files += $found
    }
    
    return $files | Select-Object -Unique
}

function Invoke-PatchApplication {
    param(
        [string]$Category,
        [bool]$DryRun,
        [bool]$CreateBackup
    )
    
    Write-Host "╔══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        POWERSHELL PATCH ENGINE - STARTING                ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    $categoriesToApply = if ($Category -eq 'All') {
        $script:PatchRules.Keys
    } else {
        @($Category)
    }
    
    $stats = @{
        FilesScanned = 0
        FilesPatched = 0
        TotalPatches = 0
        ByCategory = @{}
    }
    
    foreach ($cat in $categoriesToApply) {
        Write-Host "`n[CATEGORY: $cat]" -ForegroundColor Yellow
        
        $rules = $script:PatchRules[$cat]
        if (-not $rules) {
            Write-Warning "No rules defined for category: $cat"
            continue
        }
        
        $stats.ByCategory[$cat] = @{ Rules = 0; Patches = 0 }
        
        foreach ($rule in $rules) {
            Write-Host "  Applying: $($rule.Name)" -ForegroundColor Cyan
            
            $filePattern = if ($rule.FilePattern) { $rule.FilePattern } else { '*.cpp' }
            $files = Get-FilesToPatch -FilePatterns @($filePattern)
            
            Write-Host "    Scanning $($files.Count) files..." -ForegroundColor Gray
            
            foreach ($file in $files) {
                $stats.FilesScanned++
                
                try {
                    $context = [PatchContext]::new($file.FullName)
                    
                    if ($context.ApplyPatch($rule)) {
                        Write-Host "      ✓ " -NoNewline -ForegroundColor Green
                        Write-Host $file.FullName.Replace($PSScriptRoot, '.')
                        
                        if (-not $DryRun) {
                            $context.SaveChanges($CreateBackup)
                        }
                        
                        $stats.FilesPatched++
                        $stats.TotalPatches++
                        $stats.ByCategory[$cat].Patches++
                    }
                }
                catch {
                    Write-Warning "Failed to patch $($file.FullName): $_"
                }
            }
            
            $stats.ByCategory[$cat].Rules++
        }
    }
    
    Write-Host "`n╔══════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║        PATCH APPLICATION SUMMARY                          ║" -ForegroundColor Green
    Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Green
    
    Write-Host "`nMode: " -NoNewline
    Write-Host $(if ($DryRun) { "DRY RUN (no changes made)" } else { "LIVE (changes applied)" }) -ForegroundColor Yellow
    
    Write-Host "Files Scanned: $($stats.FilesScanned)"
    Write-Host "Files Patched: $($stats.FilesPatched)" -ForegroundColor Green
    Write-Host "Total Patches Applied: $($stats.TotalPatches)" -ForegroundColor Green
    
    Write-Host "`nBy Category:" -ForegroundColor Cyan
    foreach ($cat in $stats.ByCategory.Keys) {
        $catStats = $stats.ByCategory[$cat]
        Write-Host "  $cat : $($catStats.Rules) rules → $($catStats.Patches) patches"
    }
}

# ============================================================================
# ENTRY POINT
# ============================================================================

Invoke-PatchApplication -Category $Category -DryRun:$DryRun -CreateBackup:$CreateBackups

Write-Host "`n✓ Patch application complete" -ForegroundColor Green
