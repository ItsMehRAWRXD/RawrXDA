# GitHub Repository Creation Script
# This script provides commands to create all 15 GitHub repositories

Write-Host "=== GitHub Repository Creation Commands ===" -ForegroundColor Green
Write-Host ""
Write-Host "1. Go to GitHub.com and log in" -ForegroundColor Yellow
Write-Host "2. Create these repositories manually:" -ForegroundColor Yellow
Write-Host ""

$repos = @(
    "rawrz-clean",
    "rawrz-app", 
    "rawrz-http-encryptor",
    "ai-tools",
    "compiler-toolchain",
    "game-engine",
    "k8s-configs",
    "monitoring-configs",
    "nginx-configs",
    "scripts-collection",
    "documentation",
    "apk-builder",
    "dev-tools",
    "source-platform",
    "misc-files"
)

for ($i = 0; $i -lt $repos.Count; $i++) {
    $repoNumber = $i + 1
    $repoName = $repos[$i]
    Write-Host "$repoNumber. $repoName" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "3. After creating repositories, run these push commands:" -ForegroundColor Yellow
Write-Host ""

# Generate push commands for each project
$projectDirs = Get-ChildItem -Directory | Sort-Object Name

foreach ($dir in $projectDirs) {
    $repoName = $dir.Name -replace '^\d+-', '' -replace '-', ''
    Write-Host "# Push $($dir.Name)" -ForegroundColor Magenta
    Write-Host "cd $($dir.Name)"
    Write-Host "git remote add origin https://github.com/ItsMehRAWRXD/$repoName.git"
    Write-Host "git branch -M main"
    Write-Host "git push -u origin main"
    Write-Host ""
}

Write-Host "=== END OF SCRIPT ===" -ForegroundColor Green
