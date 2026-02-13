<#
.SYNOPSIS
    Install Local Agentic Copilot VS Code Extension
.DESCRIPTION
    Builds and installs the Local Agentic Copilot extension to VS Code
#>

param(
    [string]$VSCodeExtensionsPath = "$env:USERPROFILE\.vscode\extensions",
    [switch]$BuildOnly = $false
)

$ErrorActionPreference = "Stop"

$extensionPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\local-agentic-copilot"

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  📦 Local Agentic Copilot Extension Installer    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Check if extension directory exists
if (-not (Test-Path $extensionPath)) {
    Write-Host "`n❌ Extension directory not found: $extensionPath" -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ Extension directory found" -ForegroundColor Green
Write-Host "   Path: $extensionPath" -ForegroundColor Gray

# Check if Node.js is installed
Write-Host "`n⏳ Checking for Node.js..." -ForegroundColor Yellow
$nodeCheck = node --version 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Node.js is not installed or not in PATH" -ForegroundColor Red
    Write-Host "   Please install Node.js from https://nodejs.org/" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ Node.js detected: $nodeCheck" -ForegroundColor Green

# Install dependencies
Write-Host "`n⏳ Installing dependencies..." -ForegroundColor Yellow
Push-Location $extensionPath
try {
    npm install
    if ($LASTEXITCODE -ne 0) {
        throw "npm install failed"
    }
}
catch {
    Write-Host "❌ Failed to install dependencies" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}

Write-Host "✅ Dependencies installed" -ForegroundColor Green

# Compile TypeScript
Write-Host "`n⏳ Compiling TypeScript..." -ForegroundColor Yellow
Push-Location $extensionPath
try {
    npm run compile
    if ($LASTEXITCODE -ne 0) {
        throw "npm run compile failed"
    }
}
catch {
    Write-Host "❌ Failed to compile extension" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}

Write-Host "✅ TypeScript compiled successfully" -ForegroundColor Green

if ($BuildOnly) {
    Write-Host "`n✅ Build completed! Extension is ready." -ForegroundColor Green
    Write-Host "   Location: $extensionPath" -ForegroundColor Gray
    exit 0
}

# Copy to VS Code extensions
Write-Host "`n⏳ Installing to VS Code..." -ForegroundColor Yellow

if (-not (Test-Path $VSCodeExtensionsPath)) {
    Write-Host "`n⚠️  VS Code extensions directory not found" -ForegroundColor Yellow
    Write-Host "   Creating: $VSCodeExtensionsPath" -ForegroundColor Gray
    New-Item -ItemType Directory -Path $VSCodeExtensionsPath -Force | Out-Null
}

$targetPath = Join-Path $VSCodeExtensionsPath "local-agentic-copilot"

# Backup existing installation
if (Test-Path $targetPath) {
    $backupPath = "$targetPath.backup-$(Get-Date -Format 'yyyy-MM-dd-HHmmss')"
    Write-Host "   Backing up existing installation to: $backupPath" -ForegroundColor Yellow
    Move-Item -Path $targetPath -Destination $backupPath -Force
}

# Copy extension
try {
    Copy-Item -Path $extensionPath -Destination $targetPath -Recurse -Force
    Write-Host "✅ Extension installed successfully" -ForegroundColor Green
    Write-Host "   Location: $targetPath" -ForegroundColor Gray
}
catch {
    Write-Host "❌ Failed to copy extension" -ForegroundColor Red
    exit 1
}

# Final instructions
Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  ✅ INSTALLATION COMPLETE                          ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Green

Write-Host "`nNext steps:" -ForegroundColor Cyan
Write-Host "  1. Reload VS Code (Ctrl+Shift+P → Developer: Reload Window)" -ForegroundColor White
Write-Host "  2. Enable Agentic Mode (Ctrl+Shift+A)" -ForegroundColor White
Write-Host "  3. Start using:" -ForegroundColor White
Write-Host "     • Ctrl+Shift+J - Generate Code" -ForegroundColor Gray
Write-Host "     • Ctrl+Shift+E - Explain Code" -ForegroundColor Gray
Write-Host "     • Ctrl+Shift+F - Fix Code" -ForegroundColor Gray

Write-Host "`nKeyboard Shortcuts:" -ForegroundColor Cyan
Write-Host "  Ctrl+Shift+A - Toggle Agentic/Standard Mode" -ForegroundColor Green
Write-Host "  Ctrl+Shift+J - Generate Code" -ForegroundColor Green

Write-Host "`nConfiguration:" -ForegroundColor Cyan
Write-Host "  Edit VS Code settings.json to customize:" -ForegroundColor White
Write-Host "  • agenticCopilot.ollamaEndpoint" -ForegroundColor Gray
Write-Host "  • agenticCopilot.agenticModel" -ForegroundColor Gray
Write-Host "  • agenticCopilot.standardModel" -ForegroundColor Gray
Write-Host "  • agenticCopilot.enableInlineCompletion" -ForegroundColor Gray

Write-Host "`n🚀 Make sure Ollama is running (ollama serve)" -ForegroundColor Yellow
Write-Host "`n"
