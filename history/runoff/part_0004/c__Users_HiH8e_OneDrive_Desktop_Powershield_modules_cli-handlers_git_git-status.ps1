# ============================================
# CLI HANDLER: git-status
# ============================================
# Category: git
# Command: git-status
# Purpose: Show formatted git status
# ============================================

function Invoke-CliGitStatus {
    <#
    .SYNOPSIS
        Show formatted git status
    .DESCRIPTION
        Displays git repository status with enhanced formatting and error handling
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command git-status
    .OUTPUTS
        [bool] $true if successful, $false otherwise
    #>
    
    Write-Host "`n=== Git Repository Status ===" -ForegroundColor Cyan
    
    try {
        # Check if git is available
        $gitCheck = Get-Command git -ErrorAction SilentlyContinue
        if (-not $gitCheck) {
            Write-Host "✗ Git is not installed or not in PATH" -ForegroundColor Red
            return $false
        }
        
        # Check if we're in a git repository
        $gitDir = git rev-parse --git-dir 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ Not a git repository" -ForegroundColor Red
            return $false
        }
        
        # Get branch name
        $branch = git branch --show-current 2>&1
        Write-Host "`nBranch: " -NoNewline -ForegroundColor Yellow
        Write-Host $branch -ForegroundColor Green
        
        # Get status
        $status = git status --porcelain 2>&1
        
        if ([string]::IsNullOrWhiteSpace($status)) {
            Write-Host "`n✓ Working directory clean" -ForegroundColor Green
        } else {
            $changes = $status -split "`n"
            $staged = @()
            $unstaged = @()
            $untracked = @()
            
            foreach ($change in $changes) {
                if ($change.Length -lt 3) { continue }
                
                $indexStatus = $change[0]
                $workStatus = $change[1]
                $file = $change.Substring(3).Trim()
                
                switch ($indexStatus) {
                    'M' { $staged += "Modified: $file" }
                    'A' { $staged += "Added: $file" }
                    'D' { $staged += "Deleted: $file" }
                    'R' { $staged += "Renamed: $file" }
                    '?' { $untracked += $file }
                }
                
                switch ($workStatus) {
                    'M' { $unstaged += "Modified: $file" }
                    'D' { $unstaged += "Deleted: $file" }
                }
            }
            
            if ($staged.Count -gt 0) {
                Write-Host "`n📦 Staged Changes ($($staged.Count)):" -ForegroundColor Green
                foreach ($item in $staged) {
                    Write-Host "  ✓ $item" -ForegroundColor White
                }
            }
            
            if ($unstaged.Count -gt 0) {
                Write-Host "`n📝 Unstaged Changes ($($unstaged.Count)):" -ForegroundColor Yellow
                foreach ($item in $unstaged) {
                    Write-Host "  • $item" -ForegroundColor White
                }
            }
            
            if ($untracked.Count -gt 0) {
                Write-Host "`n❓ Untracked Files ($($untracked.Count)):" -ForegroundColor Red
                foreach ($item in $untracked) {
                    Write-Host "  ? $item" -ForegroundColor Gray
                }
            }
        }
        
        # Get recent commits
        Write-Host "`n📜 Recent Commits:" -ForegroundColor Yellow
        $commits = git log --oneline -5 2>&1
        if ($LASTEXITCODE -eq 0) {
            $commits -split "`n" | ForEach-Object {
                Write-Host "  • $_" -ForegroundColor Gray
            }
        }
        
        Write-Host ""
        return $true
    }
    catch {
        Write-Host "✗ Error getting git status" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliGitStatus
}
