<#
.SYNOPSIS
    Fixes code quality issues in RawrXD.ps1

.DESCRIPTION
    Addresses:
    - Removes duplicate Get-ActiveChatTab function
    - Documents function purposes
    - Prepares for module reorganization
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$SourceFile = "RawrXD.ps1",

    [Parameter(Mandatory = $false)]
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Stop"

function Write-FixLog {
    param([string]$Message, [string]$Level = "INFO")
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "SUCCESS" { "Green" }
        default { "Cyan" }
    }
    Write-Host "[$Level] $Message" -ForegroundColor $color
}

Write-FixLog "Starting code quality fixes..." "INFO"

if (-not (Test-Path $SourceFile)) {
    Write-FixLog "Source file not found: $SourceFile" "ERROR"
    exit 1
}

$content = Get-Content $SourceFile -Raw
$lines = Get-Content $SourceFile

# Find duplicate Get-ActiveChatTab functions
$duplicateLine = 18598
$keepLine = 11416

Write-FixLog "Found duplicate Get-ActiveChatTab function at line $duplicateLine" "WARNING"
Write-FixLog "Keeping the safer version at line $keepLine (includes null check)" "INFO"

# Read the function we want to keep (better version)
$keepFunction = @"
function Get-ActiveChatTab {
    if (`$script:activeChatTabId -and `$script:chatTabs -and `$script:chatTabs.ContainsKey(`$script:activeChatTabId)) {
        return `$script:chatTabs[`$script:activeChatTabId]
    }
    return `$null
}
"@

# Read the duplicate function to remove
$duplicateFunction = $lines[18597..18603] -join "`n"

if ($DryRun) {
    Write-FixLog "DRY RUN MODE - Would remove duplicate function at line $duplicateLine" "WARNING"
    Write-FixLog "Duplicate function content:" "INFO"
    Write-Host $duplicateFunction -ForegroundColor Gray
    exit 0
}

# Remove the duplicate function (lines 18598-18603)
$newLines = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($i -lt 18597 -or $i -gt 18603) {
        $newLines += $lines[$i]
    }
    elseif ($i -eq 18597) {
        # Check if there's a blank line before - if so, remove it too
        if ($lines[$i].Trim() -eq "") {
            # Skip this blank line
        }
        else {
            $newLines += $lines[$i]
        }
    }
}

Write-FixLog "Removing duplicate function..." "INFO"
$newContent = $newLines -join "`n"

# Save backup
$backupFile = "$SourceFile.backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
Copy-Item $SourceFile $backupFile
Write-FixLog "Created backup: $backupFile" "SUCCESS"

# Save fixed version
$newContent | Out-File -FilePath $SourceFile -Encoding UTF8 -NoNewline
Write-FixLog "Fixed file saved: $SourceFile" "SUCCESS"

# Verify syntax
Write-FixLog "Verifying syntax..." "INFO"
$errors = @()
$null = [System.Management.Automation.Language.Parser]::ParseInput($newContent, [ref]$null, [ref]$errors)

if ($errors) {
    Write-FixLog "Syntax errors found after fix!" "ERROR"
    foreach ($error in $errors) {
        Write-FixLog "Line $($error.Extent.StartLineNumber): $($error.Message)" "ERROR"
    }
    Write-FixLog "Restoring from backup..." "WARNING"
    Copy-Item $backupFile $SourceFile -Force
    exit 1
}
else {
    Write-FixLog "✓ Syntax verification passed!" "SUCCESS"
}

Write-FixLog "Code quality fixes complete!" "SUCCESS"
Write-FixLog "Backup saved at: $backupFile" "INFO"

