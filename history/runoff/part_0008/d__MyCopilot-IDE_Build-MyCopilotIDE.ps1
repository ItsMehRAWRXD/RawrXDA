# Build-MyCopilotIDE.ps1
# Build script to package MyCopilot IDE as Electron EXE

param(
    [switch]$Clean,
    [switch]$Install,
    [switch]$Build,
    [switch]$Run
)

$projectRoot = $PSScriptRoot
$packageJson = Join-Path $projectRoot "package.json"

function Install-Dependencies {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    if (Test-Path $packageJson) {
        npm install
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Dependencies installed successfully" -ForegroundColor Green
        } else {
            Write-Error "Failed to install dependencies"
            exit 1
        }
    } else {
        Write-Error "package.json not found"
        exit 1
    }
}

function Build-ElectronApp {
    Write-Host "Building Electron app..." -ForegroundColor Yellow
    npx electron-builder --publish=never
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Electron app built successfully" -ForegroundColor Green
        Write-Host "Check the 'dist' folder for your EXE file" -ForegroundColor Cyan
    } else {
        Write-Error "Failed to build Electron app"
        exit 1
    }
}

function Clean-Build {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path "dist") {
        Remove-Item -Recurse -Force "dist"
        Write-Host "✓ Build artifacts cleaned" -ForegroundColor Green
    }
    if (Test-Path "node_modules") {
        Remove-Item -Recurse -Force "node_modules"
        Write-Host "✓ Node modules cleaned" -ForegroundColor Green
    }
}

function Run-Development {
    Write-Host "Starting development server..." -ForegroundColor Yellow
    npm start
}

# Main execution logic
if ($Clean) {
    Clean-Build
} elseif ($Install) {
    Install-Dependencies
} elseif ($Build) {
    Install-Dependencies
    Build-ElectronApp
} elseif ($Run) {
    Run-Development
} else {
    # Default: full build
    Write-Host "=== Building MyCopilot IDE ===" -ForegroundColor Cyan
    Clean-Build
    Install-Dependencies
    Build-ElectronApp
    Write-Host "=== Build Complete ===" -ForegroundColor Green
    Write-Host "Your EXE is ready in the 'dist' folder!" -ForegroundColor Cyan
}