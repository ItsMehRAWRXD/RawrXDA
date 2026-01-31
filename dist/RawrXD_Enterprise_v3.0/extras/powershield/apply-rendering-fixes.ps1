# ============================================
# RAWR XD IDE - TEXT RENDERING FIX
# Fixes invisible text by correcting colors
# ============================================

$RawrXDPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"

# FIXES TO APPLY:
# 1. Replace dark text with light text
# 2. Ensure contrasting colors
# 3. Fix all text boxes and chat areas
# 4. Add logging to capture initialization

$fixes = @(
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(30,\s*30,\s*30\)"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Change editor text from dark to light"
    },
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(20,\s*20,\s*20\)"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Change chat text from dark to light"
    },
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::White"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Ensure bright white text"
    },
    @{
        Pattern = "BackColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(255,\s*255,\s*255\)"
        Replacement = "BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)"
        Description = "Ensure dark background"
    }
)

foreach ($fix in $fixes) {
    Write-Host "Applying: $($fix.Description)" -ForegroundColor Green
    $content = Get-Content $RawrXDPath -Raw
    $content = $content -replace $fix.Pattern, $fix.Replacement
    $content | Out-File $RawrXDPath -Force
    Write-Host "✓ Applied" -ForegroundColor Green
}

Write-Host "`n✅ Fixes applied. Restart RawrXD.ps1" -ForegroundColor Green
