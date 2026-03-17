# Fix GitHub Copilot Chat "chatParticipantPrivate" API proposal warning
# The extension declares a proposal that no longer exists in your VS Code version.
# Removing it from the extension manifest silences the warning; the extension
# typically still works (the API was likely finalized under another name).

$ErrorActionPreference = 'Stop'

# Possible extension locations (user install vs. system)
$extBasePaths = @(
    "$env:USERPROFILE\.vscode\extensions",
    "${env:ProgramFiles}\Microsoft VS Code\resources\app\extensions",
    "${env:LOCALAPPDATA}\Programs\Microsoft VS Code\resources\app\extensions"
)

$extensionId = "github.copilot-chat"
# Match base name so "chatParticipantPrivate" and "chatParticipantPrivate@11" are both removed
$proposalToRemove = "chatParticipantPrivate"

$found = $false
foreach ($base in $extBasePaths) {
    if (-not (Test-Path $base)) { continue }
    $dirs = Get-ChildItem -Directory -Path $base -Filter "${extensionId}*" -ErrorAction SilentlyContinue
    foreach ($dir in $dirs) {
        $pkgPath = Join-Path $dir.FullName "package.json"
        if (-not (Test-Path $pkgPath)) { continue }
        $found = $true
        $pkg = Get-Content $pkgPath -Raw | ConvertFrom-Json
        $changed = $false

        # package.json can use "enabledApiProposals" (array of strings)
        if ($pkg.PSObject.Properties['enabledApiProposals'] -and $pkg.enabledApiProposals -is [array]) {
            $before = $pkg.enabledApiProposals.Count
            # Remove exact match or versioned match (e.g. chatParticipantPrivate@11)
            $pkg.enabledApiProposals = @($pkg.enabledApiProposals | Where-Object {
                $_ -ne $proposalToRemove -and $_ -notlike "${proposalToRemove}*"
            })
            if ($pkg.enabledApiProposals.Count -lt $before) {
                $changed = $true
            }
        }

        if ($changed) {
            $json = $pkg | ConvertTo-Json -Depth 100
            [System.IO.File]::WriteAllText($pkgPath, $json, [System.Text.UTF8Encoding]::new($false))
            Write-Host "Updated: $pkgPath" -ForegroundColor Green
            Write-Host "  Removed proposal: $proposalToRemove"
        }
        else {
            Write-Host "Checked: $pkgPath" -ForegroundColor Yellow
            Write-Host "  No '$proposalToRemove' in enabledApiProposals (or not present)."
        }
    }
}

if (-not $found) {
    Write-Host "Extension '${extensionId}*' not found under:" -ForegroundColor Red
    foreach ($b in $extBasePaths) { Write-Host "  $b" }
    Write-Host ""
    Write-Host "Open VS Code -> Ctrl+Shift+P -> 'Extensions: Open Extensions Folder'" -ForegroundColor Cyan
    Write-Host "Then manually edit the github.copilot-chat-*\package.json and remove 'chatParticipantPrivate' from enabledApiProposals."
    exit 1
}

Write-Host ""
Write-Host "Restart VS Code for the change to take effect." -ForegroundColor Cyan
