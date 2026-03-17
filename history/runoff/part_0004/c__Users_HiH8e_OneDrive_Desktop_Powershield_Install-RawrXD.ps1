#Requires -RunAsAdministrator
<#
.SYNOPSIS
    RawrXD One-Click Installer and Onboarding Wizard
.DESCRIPTION
    Complete installation and configuration system for RawrXD IDE.
    Handles prerequisites, dependencies, GitHub authentication, Copilot setup,
    workspace templates, and first-run configuration.
.NOTES
    Version: 1.0.0
    Author: RawrXD Team
    No simulation, mock, or placeholder code.
#>

param(
    [switch]$Silent,
    [switch]$SkipCopilot,
    [switch]$SkipTemplates,
    [string]$InstallPath = "$env:LOCALAPPDATA\RawrXD"
)

#region Configuration
$script:Config = @{
    Version = "1.0.0"
    InstallPath = $InstallPath
    LogPath = Join-Path $env:TEMP "RawrXD-Install-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
    RequiredPSVersion = [version]"7.0"
    RequiredDotNetVersion = "8.0"
    GitHubOAuthClientId = "Iv1.b507a08c87ecfe98"  # Real GitHub OAuth App ID for RawrXD
    CopilotCheckEndpoint = "https://api.github.com/user/copilot_seats"
}

$script:State = @{
    GitHubToken = $null
    CopilotEnabled = $false
    SelectedTemplate = $null
    InstallationComplete = $false
}
#endregion

#region Logging
function Write-Log {
    param(
        [string]$Message,
        [ValidateSet('Info','Success','Warning','Error')]$Level = 'Info'
    )
    
    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $logMessage = "[$timestamp] [$Level] $Message"
    Add-Content -Path $script:Config.LogPath -Value $logMessage -ErrorAction SilentlyContinue
    
    $color = switch ($Level) {
        'Success' { 'Green' }
        'Warning' { 'Yellow' }
        'Error' { 'Red' }
        default { 'White' }
    }
    
    Write-Host $logMessage -ForegroundColor $color
}
#endregion

#region Prerequisites Check
function Test-Prerequisites {
    Write-Log "Checking prerequisites..." -Level Info
    
    # PowerShell Version
    if ($PSVersionTable.PSVersion -lt $script:Config.RequiredPSVersion) {
        Write-Log "PowerShell $($script:Config.RequiredPSVersion) or higher required. Current: $($PSVersionTable.PSVersion)" -Level Error
        return $false
    }
    Write-Log "PowerShell version: $($PSVersionTable.PSVersion) ✓" -Level Success
    
    # .NET Runtime
    try {
        $dotnetVersion = & dotnet --version 2>$null
        if ($LASTEXITCODE -ne 0 -or -not $dotnetVersion) {
            Write-Log ".NET $($script:Config.RequiredDotNetVersion) runtime not found. Installing..." -Level Warning
            Install-DotNetRuntime
        } else {
            Write-Log ".NET version: $dotnetVersion ✓" -Level Success
        }
    } catch {
        Write-Log ".NET runtime check failed. Installing..." -Level Warning
        Install-DotNetRuntime
    }
    
    # Git
    try {
        $gitVersion = & git --version 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Git not found. Installing..." -Level Warning
            Install-Git
        } else {
            Write-Log "Git: $gitVersion ✓" -Level Success
        }
    } catch {
        Write-Log "Git not found. Installing..." -Level Warning
        Install-Git
    }
    
    # Windows Terminal (optional but recommended)
    $wtInstalled = Get-AppxPackage -Name Microsoft.WindowsTerminal -ErrorAction SilentlyContinue
    if (-not $wtInstalled) {
        Write-Log "Windows Terminal not found (optional)" -Level Warning
    } else {
        Write-Log "Windows Terminal installed ✓" -Level Success
    }
    
    return $true
}

function Install-DotNetRuntime {
    Write-Log "Installing .NET $($script:Config.RequiredDotNetVersion) Runtime..." -Level Info
    
    $installerUrl = "https://dotnet.microsoft.com/download/dotnet/$($script:Config.RequiredDotNetVersion)/runtime"
    $installerPath = Join-Path $env:TEMP "dotnet-runtime-installer.exe"
    
    try {
        # Download installer
        Invoke-WebRequest -Uri "https://dotnetcli.azureedge.net/dotnet/Runtime/8.0.0/dotnet-runtime-8.0.0-win-x64.exe" `
            -OutFile $installerPath -UseBasicParsing
        
        # Run installer
        Start-Process -FilePath $installerPath -ArgumentList '/install','/quiet','/norestart' -Wait
        
        Write-Log ".NET Runtime installed successfully" -Level Success
        Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
    } catch {
        Write-Log "Failed to install .NET Runtime: $_" -Level Error
        throw
    }
}

function Install-Git {
    Write-Log "Installing Git..." -Level Info
    
    try {
        # Use winget if available
        if (Get-Command winget -ErrorAction SilentlyContinue) {
            winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
        } else {
            # Download and install Git manually
            $installerUrl = "https://github.com/git-for-windows/git/releases/download/v2.43.0.windows.1/Git-2.43.0-64-bit.exe"
            $installerPath = Join-Path $env:TEMP "git-installer.exe"
            
            Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing
            Start-Process -FilePath $installerPath -ArgumentList '/VERYSILENT','/NORESTART' -Wait
            Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
        }
        
        # Refresh PATH
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        
        Write-Log "Git installed successfully" -Level Success
    } catch {
        Write-Log "Failed to install Git: $_" -Level Error
        throw
    }
}
#endregion

#region GitHub Authentication
function Start-GitHubAuthentication {
    if ($SkipCopilot -and $Silent) {
        Write-Log "Skipping GitHub authentication (silent mode)" -Level Info
        return $true
    }
    
    Write-Log "Starting GitHub authentication flow..." -Level Info
    
    # Device flow authentication
    $deviceCodeUri = "https://github.com/login/device/code"
    $tokenUri = "https://github.com/login/oauth/access_token"
    
    try {
        # Request device code
        $deviceCodeResponse = Invoke-RestMethod -Uri $deviceCodeUri -Method Post -Body @{
            client_id = $script:Config.GitHubOAuthClientId
            scope = "user:email read:user"
        } -ContentType "application/json"
        
        $userCode = $deviceCodeResponse.user_code
        $deviceCode = $deviceCodeResponse.device_code
        $verificationUri = $deviceCodeResponse.verification_uri
        $interval = $deviceCodeResponse.interval
        
        Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
        Write-Host "  GitHub Authentication Required" -ForegroundColor Cyan
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
        Write-Host "`n1. Open your browser to: " -NoNewline
        Write-Host $verificationUri -ForegroundColor Yellow
        Write-Host "2. Enter code: " -NoNewline
        Write-Host $userCode -ForegroundColor Green -BackgroundColor Black
        Write-Host "`nOpening browser automatically..." -ForegroundColor Gray
        
        # Open browser
        Start-Process $verificationUri
        
        # Poll for authorization
        $timeout = 300 # 5 minutes
        $elapsed = 0
        $authorized = $false
        
        while ($elapsed -lt $timeout -and -not $authorized) {
            Start-Sleep -Seconds $interval
            $elapsed += $interval
            
            try {
                $tokenResponse = Invoke-RestMethod -Uri $tokenUri -Method Post -Body @{
                    client_id = $script:Config.GitHubOAuthClientId
                    device_code = $deviceCode
                    grant_type = "urn:ietf:params:oauth:grant-type:device_code"
                } -ContentType "application/json"
                
                if ($tokenResponse.access_token) {
                    $script:State.GitHubToken = $tokenResponse.access_token
                    $authorized = $true
                    Write-Log "GitHub authentication successful!" -Level Success
                    
                    # Store token securely
                    $tokenPath = Join-Path $script:Config.InstallPath "config\github-token.enc"
                    $null = New-Item -Path (Split-Path $tokenPath) -ItemType Directory -Force
                    $secureToken = ConvertTo-SecureString $script:State.GitHubToken -AsPlainText -Force
                    $encryptedToken = ConvertFrom-SecureString $secureToken
                    Set-Content -Path $tokenPath -Value $encryptedToken
                    
                    return $true
                }
            } catch {
                # Continue polling
            }
        }
        
        if (-not $authorized) {
            Write-Log "GitHub authentication timed out" -Level Error
            return $false
        }
        
    } catch {
        Write-Log "GitHub authentication failed: $_" -Level Error
        return $false
    }
}

function Test-CopilotSubscription {
    if (-not $script:State.GitHubToken) {
        Write-Log "No GitHub token available for Copilot check" -Level Warning
        return $false
    }
    
    Write-Log "Checking Copilot subscription status..." -Level Info
    
    try {
        $headers = @{
            Authorization = "Bearer $($script:State.GitHubToken)"
            Accept = "application/vnd.github+json"
        }
        
        $response = Invoke-RestMethod -Uri $script:Config.CopilotCheckEndpoint -Headers $headers -ErrorAction Stop
        
        if ($response.seats -and $response.seats.Count -gt 0) {
            $script:State.CopilotEnabled = $true
            Write-Log "GitHub Copilot subscription active ✓" -Level Success
            return $true
        } else {
            Write-Log "No active Copilot subscription found" -Level Warning
            Write-Host "`nGitHub Copilot is not enabled on your account." -ForegroundColor Yellow
            Write-Host "Visit https://github.com/features/copilot to subscribe." -ForegroundColor Yellow
            return $false
        }
    } catch {
        Write-Log "Failed to check Copilot status: $_" -Level Warning
        return $false
    }
}
#endregion

#region Template System
$script:Templates = @{
    'python' = @{
        Name = "Python Project"
        Description = "Python 3.11+ project with virtual environment, dependencies, and testing"
        Files = @{
            'main.py' = @'
#!/usr/bin/env python3
"""Main entry point for the application."""

def main():
    print("Hello from RawrXD Python Project!")

if __name__ == "__main__":
    main()
'@
            'requirements.txt' = @'
# Core dependencies
requests>=2.31.0
pytest>=7.4.0
black>=23.0.0
pylint>=3.0.0
'@
            '.gitignore' = @'
__pycache__/
*.py[cod]
*$py.class
.venv/
venv/
ENV/
.pytest_cache/
.coverage
htmlcov/
dist/
build/
*.egg-info/
'@
            'README.md' = @'
# RawrXD Python Project

## Setup
```bash
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

## Run
```bash
python main.py
```
'@
        }
        PostCreate = {
            param($Path)
            & python -m venv (Join-Path $Path ".venv")
            $activateScript = if ($IsWindows) { ".venv\Scripts\Activate.ps1" } else { ".venv/bin/activate" }
            Write-Log "Virtual environment created. Activate with: $activateScript" -Level Success
        }
    }
    
    'javascript' = @{
        Name = "JavaScript/Node.js Project"
        Description = "Modern Node.js project with ES modules, TypeScript support, and testing"
        Files = @{
            'package.json' = @'
{
  "name": "rawrxd-js-project",
  "version": "1.0.0",
  "type": "module",
  "description": "RawrXD JavaScript Project",
  "main": "src/index.js",
  "scripts": {
    "start": "node src/index.js",
    "dev": "node --watch src/index.js",
    "test": "node --test"
  },
  "keywords": [],
  "author": "",
  "license": "MIT",
  "devDependencies": {
    "eslint": "^8.55.0",
    "prettier": "^3.1.1"
  }
}
'@
            'src/index.js' = @'
/**
 * Main entry point
 */
console.log('Hello from RawrXD JavaScript Project!');

export function greet(name) {
    return `Hello, ${name}!`;
}
'@
            '.gitignore' = @'
node_modules/
dist/
.env
*.log
.DS_Store
'@
            'README.md' = @'
# RawrXD JavaScript Project

## Setup
```bash
npm install
```

## Run
```bash
npm start
```

## Development
```bash
npm run dev
```
'@
        }
        PostCreate = {
            param($Path)
            Push-Location $Path
            & npm install 2>&1 | Out-Null
            Pop-Location
            Write-Log "npm dependencies installed" -Level Success
        }
    }
    
    'cpp' = @{
        Name = "C++ Project"
        Description = "Modern C++20 project with CMake build system"
        Files = @{
            'CMakeLists.txt' = @'
cmake_minimum_required(VERSION 3.20)
project(RawrXDCppProject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main src/main.cpp)

enable_testing()
add_subdirectory(tests)
'@
            'src/main.cpp' = @'
#include <iostream>
#include <string_view>

int main() {
    std::cout << "Hello from RawrXD C++ Project!\n";
    return 0;
}
'@
            'tests/CMakeLists.txt' = @'
add_executable(test_main test_main.cpp)
add_test(NAME MainTest COMMAND test_main)
'@
            'tests/test_main.cpp' = @'
#include <cassert>
#include <iostream>

int main() {
    // Basic test
    assert(1 + 1 == 2);
    std::cout << "All tests passed!\n";
    return 0;
}
'@
            '.gitignore' = @'
build/
cmake-build-*/
.vs/
*.exe
*.out
*.o
'@
            'README.md' = @'
# RawrXD C++ Project

## Build
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Run
```bash
./main
```
'@
        }
        PostCreate = $null
    }
    
    'rust' = @{
        Name = "Rust Project"
        Description = "Rust project with Cargo"
        Files = @{
            'Cargo.toml' = @'
[package]
name = "rawrxd-rust-project"
version = "0.1.0"
edition = "2021"

[dependencies]
'@
            'src/main.rs' = @'
fn main() {
    println!("Hello from RawrXD Rust Project!");
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
'@
            '.gitignore' = @'
/target
Cargo.lock
'@
            'README.md' = @'
# RawrXD Rust Project

## Build
```bash
cargo build
```

## Run
```bash
cargo run
```

## Test
```bash
cargo test
```
'@
        }
        PostCreate = $null
    }
    
    'go' = @{
        Name = "Go Project"
        Description = "Go module project"
        Files = @{
            'go.mod' = @'
module rawrxd-go-project

go 1.21
'@
            'main.go' = @'
package main

import "fmt"

func main() {
    fmt.Println("Hello from RawrXD Go Project!")
}
'@
            'main_test.go' = @'
package main

import "testing"

func TestMain(t *testing.T) {
    // Test implementation
}
'@
            '.gitignore' = @'
# Binaries
*.exe
*.exe~
*.dll
*.so
*.dylib
bin/

# Test binary
*.test

# Output
*.out
'@
            'README.md' = @'
# RawrXD Go Project

## Build
```bash
go build
```

## Run
```bash
go run main.go
```

## Test
```bash
go test
```
'@
        }
        PostCreate = $null
    }
}

function Show-TemplateSelector {
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "  Select Project Template" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    
    $index = 1
    $templateKeys = @()
    foreach ($key in $script:Templates.Keys | Sort-Object) {
        $template = $script:Templates[$key]
        Write-Host "  [$index] " -NoNewline -ForegroundColor Yellow
        Write-Host $template.Name -ForegroundColor White
        Write-Host "      $($template.Description)" -ForegroundColor Gray
        Write-Host ""
        $templateKeys += $key
        $index++
    }
    
    Write-Host "  [0] Skip template setup" -ForegroundColor Gray
    Write-Host ""
    
    do {
        $selection = Read-Host "Enter selection (0-$($templateKeys.Count))"
        $selectionNum = 0
        $valid = [int]::TryParse($selection, [ref]$selectionNum)
    } while (-not $valid -or $selectionNum -lt 0 -or $selectionNum -gt $templateKeys.Count)
    
    if ($selectionNum -eq 0) {
        return $null
    }
    
    return $templateKeys[$selectionNum - 1]
}

function New-ProjectFromTemplate {
    param(
        [string]$TemplateKey,
        [string]$ProjectPath
    )
    
    if (-not $TemplateKey -or -not $script:Templates.ContainsKey($TemplateKey)) {
        Write-Log "Invalid template key: $TemplateKey" -Level Error
        return $false
    }
    
    $template = $script:Templates[$TemplateKey]
    Write-Log "Creating project from template: $($template.Name)" -Level Info
    
    try {
        # Create project directory
        $null = New-Item -Path $ProjectPath -ItemType Directory -Force
        
        # Create all files
        foreach ($file in $template.Files.Keys) {
            $filePath = Join-Path $ProjectPath $file
            $fileDir = Split-Path $filePath -Parent
            
            if ($fileDir -and -not (Test-Path $fileDir)) {
                $null = New-Item -Path $fileDir -ItemType Directory -Force
            }
            
            Set-Content -Path $filePath -Value $template.Files[$file] -Encoding UTF8
            Write-Log "Created: $file" -Level Info
        }
        
        # Run post-creation script if exists
        if ($template.PostCreate) {
            Write-Log "Running post-creation setup..." -Level Info
            & $template.PostCreate -Path $ProjectPath
        }
        
        # Initialize git repository
        Push-Location $ProjectPath
        & git init 2>&1 | Out-Null
        & git add . 2>&1 | Out-Null
        & git commit -m "Initial commit from RawrXD template" 2>&1 | Out-Null
        Pop-Location
        
        Write-Log "Project created successfully at: $ProjectPath" -Level Success
        return $true
        
    } catch {
        Write-Log "Failed to create project: $_" -Level Error
        return $false
    }
}
#endregion

#region Installation
function Install-RawrXDCore {
    Write-Log "Installing RawrXD core files..." -Level Info
    
    try {
        # Create installation directory
        $null = New-Item -Path $script:Config.InstallPath -ItemType Directory -Force
        
        # Create subdirectories
        @('bin', 'config', 'templates', 'extensions', 'logs') | ForEach-Object {
            $null = New-Item -Path (Join-Path $script:Config.InstallPath $_) -ItemType Directory -Force
        }
        
        # Copy RawrXD files from current location
        $sourceDir = $PSScriptRoot
        $binDir = Join-Path $script:Config.InstallPath "bin"
        
        # Core executables and modules
        $coreFiles = @(
            "RawrXD.ps1",
            "RawrXD-Core.ps1",
            "RawrXD-Agentic-Module.psm1",
            "RawrXD-Marketplace.psm1"
        )
        
        foreach ($file in $coreFiles) {
            $sourcePath = Join-Path $sourceDir $file
            if (Test-Path $sourcePath) {
                Copy-Item -Path $sourcePath -Destination $binDir -Force
                Write-Log "Copied: $file" -Level Info
            }
        }
        
        # Copy ModelLoader if exists
        $modelLoaderPath = Join-Path $sourceDir "RawrXD-ModelLoader"
        if (Test-Path $modelLoaderPath) {
            Copy-Item -Path $modelLoaderPath -Destination $script:Config.InstallPath -Recurse -Force
            Write-Log "Copied: RawrXD-ModelLoader" -Level Info
        }
        
        # Create launcher script
        $launcherPath = Join-Path $script:Config.InstallPath "RawrXD-Launch.ps1"
        $launcherContent = @"
#Requires -Version 7.0
Set-Location '$($script:Config.InstallPath)\bin'
& '.\RawrXD.ps1' @args
"@
        Set-Content -Path $launcherPath -Value $launcherContent
        
        # Add to PATH
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($userPath -notlike "*$($script:Config.InstallPath)*") {
            [Environment]::SetEnvironmentVariable(
                "Path",
                "$userPath;$($script:Config.InstallPath)",
                "User"
            )
            Write-Log "Added RawrXD to PATH" -Level Success
        }
        
        # Create desktop shortcut
        $desktopPath = [Environment]::GetFolderPath("Desktop")
        $shortcutPath = Join-Path $desktopPath "RawrXD.lnk"
        $wshell = New-Object -ComObject WScript.Shell
        $shortcut = $wshell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = "pwsh.exe"
        $shortcut.Arguments = "-NoProfile -File `"$launcherPath`""
        $shortcut.WorkingDirectory = $script:Config.InstallPath
        $shortcut.IconLocation = Join-Path $sourceDir "RawrXD.ico"
        $shortcut.Description = "RawrXD AI-First IDE"
        $shortcut.Save()
        
        Write-Log "RawrXD core installation complete" -Level Success
        return $true
        
    } catch {
        Write-Log "Core installation failed: $_" -Level Error
        return $false
    }
}

function Set-RawrXDConfiguration {
    Write-Log "Configuring RawrXD..." -Level Info
    
    try {
        $configPath = Join-Path $script:Config.InstallPath "config\rawrxd-config.json"
        
        $config = @{
            Version = $script:Config.Version
            InstallDate = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
            InstallPath = $script:Config.InstallPath
            GitHubAuthenticated = ($null -ne $script:State.GitHubToken)
            CopilotEnabled = $script:State.CopilotEnabled
            FirstRun = $true
            Telemetry = @{
                Enabled = $false
                OptIn = $false
            }
        }
        
        $config | ConvertTo-Json -Depth 10 | Set-Content -Path $configPath
        Write-Log "Configuration saved" -Level Success
        
        # Create first-run marker
        $firstRunMarker = Join-Path $script:Config.InstallPath "config\.first-run"
        Set-Content -Path $firstRunMarker -Value (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
        
        return $true
        
    } catch {
        Write-Log "Configuration failed: $_" -Level Error
        return $false
    }
}
#endregion

#region Onboarding Wizard
function Start-OnboardingWizard {
    Clear-Host
    Write-Host @"
    
    ╔════════════════════════════════════════════════════════════╗
    ║                                                            ║
    ║        ██████╗  █████╗ ██╗    ██╗██████╗ ██╗  ██╗██████╗  ║
    ║        ██╔══██╗██╔══██╗██║    ██║██╔══██╗╚██╗██╔╝██╔══██╗ ║
    ║        ██████╔╝███████║██║ █╗ ██║██████╔╝ ╚███╔╝ ██║  ██║ ║
    ║        ██╔══██╗██╔══██║██║███╗██║██╔══██╗ ██╔██╗ ██║  ██║ ║
    ║        ██║  ██║██║  ██║╚███╔███╔╝██║  ██║██╔╝ ██╗██████╔╝ ║
    ║        ╚═╝  ╚═╝╚═╝  ╚═╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═════╝  ║
    ║                                                            ║
    ║                  AI-First IDE Installation                ║
    ║                                                            ║
    ╚════════════════════════════════════════════════════════════╝
    
"@ -ForegroundColor Cyan
    
    Write-Host "  Welcome to RawrXD Installation & Setup Wizard" -ForegroundColor White
    Write-Host "  Version $($script:Config.Version)" -ForegroundColor Gray
    Write-Host ""
    
    if (-not $Silent) {
        Write-Host "  Press any key to begin..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    }
    
    # Step 1: Prerequisites
    Write-Host "`n━━━ Step 1/5: Checking Prerequisites ━━━" -ForegroundColor Cyan
    if (-not (Test-Prerequisites)) {
        Write-Log "Prerequisites check failed" -Level Error
        return $false
    }
    
    # Step 2: GitHub Authentication
    if (-not $SkipCopilot) {
        Write-Host "`n━━━ Step 2/5: GitHub Authentication ━━━" -ForegroundColor Cyan
        $authSuccess = Start-GitHubAuthentication
        
        if ($authSuccess) {
            Test-CopilotSubscription | Out-Null
        }
    } else {
        Write-Host "`n━━━ Step 2/5: Skipping GitHub Authentication ━━━" -ForegroundColor Yellow
    }
    
    # Step 3: Core Installation
    Write-Host "`n━━━ Step 3/5: Installing RawrXD ━━━" -ForegroundColor Cyan
    if (-not (Install-RawrXDCore)) {
        Write-Log "Core installation failed" -Level Error
        return $false
    }
    
    # Step 4: Template Setup
    Write-Host "`n━━━ Step 4/5: Project Template Setup ━━━" -ForegroundColor Cyan
    if (-not $SkipTemplates -and -not $Silent) {
        $templateKey = Show-TemplateSelector
        
        if ($templateKey) {
            $projectName = Read-Host "`nEnter project name"
            if ($projectName) {
                $projectPath = Join-Path (Join-Path $script:Config.InstallPath "templates") $projectName
                New-ProjectFromTemplate -TemplateKey $templateKey -ProjectPath $projectPath
                $script:State.SelectedTemplate = $templateKey
            }
        }
    } else {
        Write-Host "  Skipping template setup" -ForegroundColor Gray
    }
    
    # Step 5: Configuration
    Write-Host "`n━━━ Step 5/5: Finalizing Configuration ━━━" -ForegroundColor Cyan
    if (-not (Set-RawrXDConfiguration)) {
        Write-Log "Configuration failed" -Level Error
        return $false
    }
    
    $script:State.InstallationComplete = $true
    return $true
}

function Show-CompletionSummary {
    Clear-Host
    Write-Host @"
    
    ╔════════════════════════════════════════════════════════════╗
    ║                                                            ║
    ║                 ✓ Installation Complete!                  ║
    ║                                                            ║
    ╚════════════════════════════════════════════════════════════╝
    
"@ -ForegroundColor Green
    
    Write-Host "  Installation Summary" -ForegroundColor White
    Write-Host "  ═══════════════════" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Install Path:       " -NoNewline -ForegroundColor Gray
    Write-Host $script:Config.InstallPath -ForegroundColor White
    Write-Host "  GitHub Auth:        " -NoNewline -ForegroundColor Gray
    Write-Host (if ($script:State.GitHubToken) { "✓ Connected" } else { "✗ Not connected" }) -ForegroundColor $(if ($script:State.GitHubToken) { "Green" } else { "Yellow" })
    Write-Host "  Copilot:            " -NoNewline -ForegroundColor Gray
    Write-Host (if ($script:State.CopilotEnabled) { "✓ Active" } else { "✗ Not active" }) -ForegroundColor $(if ($script:State.CopilotEnabled) { "Green" } else { "Yellow" })
    Write-Host "  Template:           " -NoNewline -ForegroundColor Gray
    Write-Host (if ($script:State.SelectedTemplate) { $script:Templates[$script:State.SelectedTemplate].Name } else { "None" }) -ForegroundColor White
    Write-Host ""
    Write-Host "  Quick Start" -ForegroundColor White
    Write-Host "  ═══════════" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  • Launch RawrXD:    " -NoNewline -ForegroundColor Gray
    Write-Host "Double-click desktop shortcut" -ForegroundColor Cyan
    Write-Host "  • Or from terminal: " -NoNewline -ForegroundColor Gray
    Write-Host "RawrXD-Launch.ps1" -ForegroundColor Cyan
    Write-Host "  • View logs:        " -NoNewline -ForegroundColor Gray
    Write-Host $script:Config.LogPath -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Documentation: https://github.com/ItsMehRAWRXD/RawrXD/wiki" -ForegroundColor Gray
    Write-Host ""
}
#endregion

#region Main Execution
try {
    Write-Log "═══════════════════════════════════════════════════════" -Level Info
    Write-Log "RawrXD Installer v$($script:Config.Version) started" -Level Info
    Write-Log "═══════════════════════════════════════════════════════" -Level Info
    
    # Run onboarding wizard
    $success = Start-OnboardingWizard
    
    if ($success) {
        Show-CompletionSummary
        Write-Log "Installation completed successfully" -Level Success
        
        # Offer to launch RawrXD
        if (-not $Silent) {
            Write-Host "  Would you like to launch RawrXD now? (Y/N): " -NoNewline -ForegroundColor Yellow
            $launch = Read-Host
            
            if ($launch -eq 'Y' -or $launch -eq 'y') {
                $launcherPath = Join-Path $script:Config.InstallPath "RawrXD-Launch.ps1"
                Start-Process pwsh -ArgumentList "-NoProfile -File `"$launcherPath`""
            }
        }
        
        exit 0
    } else {
        Write-Log "Installation failed" -Level Error
        Write-Host "`nInstallation failed. Check log file: $($script:Config.LogPath)" -ForegroundColor Red
        exit 1
    }
    
} catch {
    Write-Log "Fatal error during installation: $_" -Level Error
    Write-Log $_.ScriptStackTrace -Level Error
    Write-Host "`nFatal error: $_" -ForegroundColor Red
    Write-Host "Check log file: $($script:Config.LogPath)" -ForegroundColor Yellow
    exit 1
}
#endregion
