# Setup-GitHubCopilot.ps1
# Sets up GitHub Copilot extension in the workspace

Write-Host "🚀 GitHub Copilot Extension Setup" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Check if .vscode directory exists
if (-not (Test-Path ".vscode")) {
    New-Item -ItemType Directory -Path ".vscode" | Out-Null
    Write-Host "✅ Created .vscode directory" -ForegroundColor Green
}

# Update extensions.json to include GitHub Copilot
$extensionsPath = ".vscode\extensions.json"
$extensions = @{
    recommendations = @()
    unwantedRecommendations = @()
}

if (Test-Path $extensionsPath) {
    $extensions = Get-Content $extensionsPath -Raw | ConvertFrom-Json
}

# Add GitHub Copilot if not already present
$copilotId = "GitHub.copilot"
if ($extensions.recommendations -notcontains $copilotId) {
    $extensions.recommendations += $copilotId
    Write-Host "✅ Added GitHub Copilot to recommended extensions" -ForegroundColor Green
} else {
    Write-Host "ℹ️  GitHub Copilot already in recommendations" -ForegroundColor Yellow
}

# Add GitHub Copilot Nightly if user wants bleeding edge
$copilotNightlyId = "GitHub.copilot-nightly"
if ($extensions.recommendations -notcontains $copilotNightlyId) {
    Write-Host "`n💡 Tip: GitHub Copilot Nightly provides latest features" -ForegroundColor Cyan
    $addNightly = Read-Host "Add GitHub Copilot Nightly? (y/N)"
    if ($addNightly -eq "y" -or $addNightly -eq "Y") {
        $extensions.recommendations += $copilotNightlyId
        Write-Host "✅ Added GitHub Copilot Nightly" -ForegroundColor Green
    }
}

# Save extensions.json
$extensions | ConvertTo-Json -Depth 10 | Set-Content $extensionsPath -Encoding UTF8
Write-Host "✅ Updated extensions.json" -ForegroundColor Green

# Update workspace settings.json
$settingsPath = ".vscode\settings.json"
$settings = @{}

if (Test-Path $settingsPath) {
    $settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
}

# Add GitHub Copilot settings
$settings.'github.copilot.enable' = @{
    "*" = $true
}
$settings.'github.copilot.editor.enableAutoCompletions' = $true
$settings.'github.copilot.chat.enabled' = $true
$settings.'github.copilot.editor.enableCodeActions' = $true

# Save settings.json
$settings | ConvertTo-Json -Depth 20 | Set-Content $settingsPath -Encoding UTF8
Write-Host "✅ Updated workspace settings.json with GitHub Copilot configuration" -ForegroundColor Green

Write-Host "`n✅ GitHub Copilot setup complete!" -ForegroundColor Green
Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
Write-Host "  1. VS Code/Cursor will prompt to install GitHub Copilot extension" -ForegroundColor Yellow
Write-Host "  2. Sign in to GitHub when prompted" -ForegroundColor Yellow
Write-Host "  3. Activate your GitHub Copilot subscription if needed" -ForegroundColor Yellow
Write-Host "  4. Start using Copilot with Ctrl+Shift+P -> 'GitHub Copilot: Enable'" -ForegroundColor Yellow

