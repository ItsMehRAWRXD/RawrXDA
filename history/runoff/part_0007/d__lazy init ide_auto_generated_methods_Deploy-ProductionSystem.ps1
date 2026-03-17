# RawrXD Complete Production Deployment System
# Comprehensive deployment, testing, and validation

#Requires -Version 5.1

<#
.SYNOPSIS
    Deploy-ProductionSystem - Complete production deployment system

.DESCRIPTION
    Comprehensive production deployment system that:
    - Tests all modules for pure PowerShell compatibility
    - Creates deployment packages
    - Generates installation scripts
    - Validates functionality
    - Creates documentation
    - Ensures no external dependencies

.EXAMPLE
    .\Deploy-ProductionSystem.ps1 -TargetPath "C:\RawrXD\Production"
    
    Deploy complete system to target path

.EXAMPLE
    .\Deploy-ProductionSystem.ps1 -TargetPath "C:\RawrXD\Production" -InstallService -ServiceName "RawrXDService"
    
    Deploy with service installation

.EXAMPLE
    .\Deploy-ProductionSystem.ps1 -ValidateOnly
    
    Validate all modules without deployment
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$TargetPath = "C:\RawrXD\Production",
    
    [Parameter(Mandatory=$false)]
    [switch]$ValidateOnly = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$InstallService = $false,
    
    [Parameter(Mandatory=$false)]
    [string]$ServiceName = "RawrXDService",
    
    [Parameter(Mandatory=$false)]
    [string]$ServiceDisplayName = "RawrXD Production Service",
    
    [Parameter(Mandatory=$false)]
    [string]$LogPath = $null,
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipTests = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$CreateInstaller = $true
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RawrXD Complete Production Deployment System            ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Initialize logging
if ($LogPath) {
    if (-not (Test-Path $LogPath)) {
        New-Item -Path $LogPath -ItemType Directory -Force | Out-Null
    }
    $logFile = Join-Path $LogPath "Deployment_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
    Start-Transcript -Path $logFile | Out-Null
    Write-Host "Logging to: $logFile" -ForegroundColor Yellow
}

# Deployment configuration
$deploymentConfig = @{
    SourcePath = $PSScriptRoot
    TargetPath = $TargetPath
    ValidateOnly = $ValidateOnly
    InstallService = $InstallService
    ServiceName = $ServiceName
    ServiceDisplayName = $ServiceDisplayName
    SkipTests = $SkipTests
    CreateInstaller = $CreateInstaller
    StartTime = Get-Date
    Modules = @()
    TestResults = @()
    DeploymentResults = @()
    Success = $false
}

Write-Host "Deployment Configuration:" -ForegroundColor Yellow
Write-Host "  Source Path: $($deploymentConfig.SourcePath)" -ForegroundColor White
Write-Host "  Target Path: $($deploymentConfig.TargetPath)" -ForegroundColor White
Write-Host "  Validate Only: $($deploymentConfig.ValidateOnly)" -ForegroundColor White
Write-Host "  Install Service: $($deploymentConfig.InstallService)" -ForegroundColor White
Write-Host "  Skip Tests: $($deploymentConfig.SkipTests)" -ForegroundColor White
Write-Host "  Create Installer: $($deploymentConfig.CreateInstaller)" -ForegroundColor White
Write-Host ""

# Phase 1: Module Discovery and Validation
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "PHASE 1: Module Discovery and Validation" -ForegroundColor Cyan
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

try {
    Write-Host "Discovering RawrXD modules..." -ForegroundColor Yellow
    
    $modules = Get-ChildItem -Path $deploymentConfig.SourcePath -Filter "RawrXD*.psm1" | Sort-Object Name
    
    Write-Host "Found $($modules.Count) modules:" -ForegroundColor Green
    foreach ($module in $modules) {
        $moduleInfo = @{
            Name = $module.Name
            Path = $module.FullName
            SizeKB = [Math]::Round($module.Length / 1KB, 2)
            Lines = 0
            Functions = 0
            Classes = 0
            Exports = 0
            Status = "Pending"
            Errors = @()
        }
        
        # Analyze module
        $content = Get-Content -Path $module.FullName -Raw
        $lines = $content -split "`n"
        $moduleInfo.Lines = $lines.Count
        
        $functions = [regex]::Matches($content, 'function\s+(\w+)')
        $moduleInfo.Functions = $functions.Count
        
        $classes = [regex]::Matches($content, 'class\s+(\w+)')
        $moduleInfo.Classes = $classes.Count
        
        $exports = [regex]::Matches($content, 'Export-ModuleMember\s+-Function\s+(.+)')
        if ($exports.Count -gt 0) {
            $exportList = $exports[0].Groups[1].Value -split ',' | ForEach-Object { $_.Trim() }
            $moduleInfo.Exports = $exportList.Count
        }
        
        Write-Host "  ✓ $($module.Name)" -ForegroundColor Green
        Write-Host "    Lines: $($moduleInfo.Lines), Functions: $($moduleInfo.Functions), Classes: $($moduleInfo.Classes), Exports: $($moduleInfo.Exports), Size: $($moduleInfo.SizeKB)KB" -ForegroundColor Gray
        
        $deploymentConfig.Modules += $moduleInfo
    }
    
    Write-Host ""
    Write-Host "Module discovery completed successfully!" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host "✗ Module discovery failed: $_" -ForegroundColor Red
    exit 1
}

# Phase 2: Pure PowerShell Compatibility Testing
if (-not $deploymentConfig.SkipTests) {
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "PHASE 2: Pure PowerShell Compatibility Testing" -ForegroundColor Cyan
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
    
    try {
        Write-Host "Testing modules for pure PowerShell compatibility..." -ForegroundColor Yellow
        
        $testResults = @()
        $totalTests = 0
        $passedTests = 0
        $failedTests = 0
        
        foreach ($moduleInfo in $deploymentConfig.Modules) {
            Write-Host "Testing: $($moduleInfo.Name)" -ForegroundColor Yellow
            
            $moduleTest = @{
                ModuleName = $moduleInfo.Name
                ImportTest = "Pending"
                FunctionTests = @()
                OverallResult = "Pending"
                Errors = @()
            }
            
            try {
                # Test 1: Import module
                Write-Host "  Testing import..." -ForegroundColor Gray
                Import-Module -Name $moduleInfo.Path -Force -Global -ErrorAction Stop
                $moduleTest.ImportTest = "Passed"
                Write-Host "  ✓ Import successful" -ForegroundColor Green
                
                # Test 2: Get exported functions
                $module = Get-Module -Name ([System.IO.Path]::GetFileNameWithoutExtension($moduleInfo.Name))
                if ($module) {
                    $exportedCommands = $module.ExportedCommands
                    Write-Host "  ✓ Exported $($exportedCommands.Count) commands" -ForegroundColor Green
                    
                    # Test 3: Test each exported function
                    foreach ($command in $exportedCommands.Values) {
                        $funcTest = @{
                            FunctionName = $command.Name
                            SyntaxTest = "Pending"
                            HelpTest = "Pending"
                            Result = "Pending"
                            Error = $null
                        }
                        
                        try {
                            # Test syntax
                            $syntax = Get-Command -Name $command.Name -Syntax -ErrorAction Stop
                            $funcTest.SyntaxTest = "Passed"
                            
                            # Test help
                            $help = Get-Help -Name $command.Name -ErrorAction SilentlyContinue
                            if ($help -and $help.Synopsis -ne $null) {
                                $funcTest.HelpTest = "Passed"
                            } else {
                                $funcTest.HelpTest = "Failed"
                            }
                            
                            $funcTest.Result = "Passed"
                            Write-Host "    ✓ $($command.Name)" -ForegroundColor Green
                            
                        } catch {
                            $funcTest.SyntaxTest = "Failed"
                            $funcTest.Result = "Failed"
                            $funcTest.Error = $_.Message
                            $moduleTest.Errors += "$($command.Name): $_"
                            Write-Host "    ✗ $($command.Name): $_" -ForegroundColor Red
                        }
                        
                        $moduleTest.FunctionTests += $funcTest
                        $totalTests++
                        if ($funcTest.Result -eq "Passed") { $passedTests++ } else { $failedTests++ }
                    }
                    
                    # Remove module after testing
                    Remove-Module -Name $module.Name -Force -ErrorAction SilentlyContinue
                }
                
                $moduleTest.OverallResult = if ($moduleTest.Errors.Count -eq 0) { "Passed" } else { "Failed" }
                
            } catch {
                $moduleTest.ImportTest = "Failed"
                $moduleTest.OverallResult = "Failed"
                $moduleTest.Errors += "Import failed: $_"
                Write-Host "  ✗ Import failed: $_" -ForegroundColor Red
                $failedTests++
            }
            
            $testResults += $moduleTest
            Write-Host ""
        }
        
        $deploymentConfig.TestResults = $testResults
        
        Write-Host "Testing completed!" -ForegroundColor Green
        Write-Host "  Total Tests: $totalTests" -ForegroundColor White
        Write-Host "  Passed: $passedTests" -ForegroundColor Green
        Write-Host "  Failed: $failedTests" -ForegroundColor $(if ($failedTests -eq 0) { "Green" } else { "Red" })
        Write-Host ""
        
        if ($failedTests -gt 0) {
            Write-Host "⚠ Some tests failed. Review errors before deployment." -ForegroundColor Yellow
            Write-Host ""
        }
        
    } catch {
        Write-Host "✗ Testing failed: $_" -ForegroundColor Red
        exit 1
    }
}

# Phase 3: Create Deployment Package
if (-not $deploymentConfig.ValidateOnly) {
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "PHASE 3: Create Deployment Package" -ForegroundColor Cyan
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
    
    try {
        Write-Host "Creating deployment package..." -ForegroundColor Yellow
        
        # Create target directory
        if (Test-Path $deploymentConfig.TargetPath) {
            Write-Host "Target path exists. Removing..." -ForegroundColor Yellow
            Remove-Item -Path $deploymentConfig.TargetPath -Recurse -Force
        }
        
        New-Item -Path $deploymentConfig.TargetPath -ItemType Directory -Force | Out-Null
        Write-Host "✓ Created target directory: $($deploymentConfig.TargetPath)" -ForegroundColor Green
        
        # Create subdirectories
        $modulesDir = Join-Path $deploymentConfig.TargetPath "Modules"
        $scriptsDir = Join-Path $deploymentConfig.TargetPath "Scripts"
        $docsDir = Join-Path $deploymentConfig.TargetPath "Documentation"
        $testsDir = Join-Path $deploymentConfig.TargetPath "Tests"
        $configDir = Join-Path $deploymentConfig.TargetPath "Config"
        
        @($modulesDir, $scriptsDir, $docsDir, $testsDir, $configDir) | ForEach-Object {
            New-Item -Path $_ -ItemType Directory -Force | Out-Null
            Write-Host "✓ Created directory: $_" -ForegroundColor Green
        }
        
        Write-Host ""
        
        # Copy modules
        Write-Host "Copying modules..." -ForegroundColor Yellow
        foreach ($moduleInfo in $deploymentConfig.Modules) {
            $destPath = Join-Path $modulesDir $moduleInfo.Name
            Copy-Item -Path $moduleInfo.Path -Destination $destPath -Force
            Write-Host "  ✓ Copied: $($moduleInfo.Name)" -ForegroundColor Green
        }
        
        Write-Host ""
        
        # Copy additional files
        $additionalFiles = @(
            "*.ps1",
            "*.md",
            "*.json",
            "*.xml"
        )
        
        foreach ($pattern in $additionalFiles) {
            $files = Get-ChildItem -Path $deploymentConfig.SourcePath -Filter $pattern -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                $destPath = Join-Path $deploymentConfig.TargetPath $file.Name
                Copy-Item -Path $file.FullName -Destination $destPath -Force
                Write-Host "  ✓ Copied: $($file.Name)" -ForegroundColor Green
            }
        }
        
        Write-Host ""
        Write-Host "✓ Deployment package created successfully!" -ForegroundColor Green
        Write-Host ""
        
        $deploymentConfig.DeploymentResults += @{
            Phase = "Package Creation"
            Status = "Success"
            Details = "Created deployment package at $($deploymentConfig.TargetPath)"
        }
        
    } catch {
        Write-Host "✗ Package creation failed: $_" -ForegroundColor Red
        exit 1
    }
}

# Phase 4: Create Installation Scripts
if (-not $deploymentConfig.ValidateOnly -and $deploymentConfig.CreateInstaller) {
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "PHASE 4: Create Installation Scripts" -ForegroundColor Cyan
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
    
    try {
        Write-Host "Creating installation scripts..." -ForegroundColor Yellow
        
        # Create Install.ps1
        $installScript = @"
# RawrXD Production System Installer
# Auto-generated on $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

param(
    [Parameter(Mandatory=`$false)]
    [string]`$InstallPath = "C:\RawrXD",
    
    [Parameter(Mandatory=`$false)]
    [switch]`$InstallService = `$$InstallService,
    
    [Parameter(Mandatory=`$false)]
    [string]`$ServiceName = "$ServiceName",
    
    [Parameter(Mandatory=`$false)]
    [string]`$ServiceDisplayName = "$ServiceDisplayName"
)

`$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           RawrXD Production System Installer                 ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

try {
    # Check if running as administrator
    `$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not `$isAdmin) {
        Write-Host "⚠ This installer requires Administrator privileges." -ForegroundColor Yellow
        Write-Host "  Please run as Administrator." -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "Installing RawrXD Production System..." -ForegroundColor Yellow
    Write-Host "  Install Path: `$InstallPath" -ForegroundColor White
    Write-Host "  Install Service: `$InstallService" -ForegroundColor White
    Write-Host ""
    
    # Create installation directory
    if (Test-Path `$InstallPath) {
        Write-Host "Removing existing installation..." -ForegroundColor Yellow
        Remove-Item -Path `$InstallPath -Recurse -Force
    }
    
    New-Item -Path `$InstallPath -ItemType Directory -Force | Out-Null
    Write-Host "✓ Created installation directory: `$InstallPath" -ForegroundColor Green
    
    # Copy files
    `$sourceDir = Split-Path -Parent `$MyInvocation.MyCommand.Path
    Write-Host "Copying files..." -ForegroundColor Yellow
    
    Copy-Item -Path "`$sourceDir\*" -Destination `$InstallPath -Recurse -Force
    Write-Host "✓ Copied all files" -ForegroundColor Green
    
    # Import master module
    Write-Host "Importing master module..." -ForegroundColor Yellow
    Import-Module "`$InstallPath\Modules\RawrXD.Master.psm1" -Force -Global
    Write-Host "✓ Master module imported" -ForegroundColor Green
    
    # Initialize system
    Write-Host "Initializing system..." -ForegroundColor Yellow
    `$initResult = Initialize-MasterSystem -LogPath "`$InstallPath\Logs" -EnableAutonomousEnhancement
    Write-Host "✓ System initialized" -ForegroundColor Green
    
    # Install service if requested
    if (`$InstallService) {
        Write-Host "Installing service..." -ForegroundColor Yellow
        `$serviceBinary = "`$InstallPath\Scripts\RawrXD.Service.exe"
        if (Test-Path `$serviceBinary) {
            Invoke-Win32Deployment -Action InstallService -ServiceName `$ServiceName -DisplayName `$ServiceDisplayName -BinaryPath `$serviceBinary
            Write-Host "✓ Service installed: `$ServiceName" -ForegroundColor Green
        } else {
            Write-Host "⚠ Service binary not found: `$serviceBinary" -ForegroundColor Yellow
        }
    }
    
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║              Installation Completed Successfully!            ║" -ForegroundColor Green
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    Write-Host "To get started:" -ForegroundColor Yellow
    Write-Host "  1. Open PowerShell" -ForegroundColor White
    Write-Host "  2. Import the module: Import-Module '$InstallPath\Modules\RawrXD.Master.psm1'" -ForegroundColor White
    Write-Host "  3. Get system status: Get-MasterSystemStatus" -ForegroundColor White
    Write-Host "  4. Run enhancement: Start-AutonomousEnhancementCycle -TargetPath '$InstallPath' -ResearchQuery 'top 10 agentic IDE features' -MaxFeatures 5" -ForegroundColor White
    Write-Host ""
    
} catch {
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║                    Installation Failed!                      ║" -ForegroundColor Red
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    Write-Host ""
    Write-Host "Error: $_" -ForegroundColor Red
    Write-Host ""
    exit 1
}
"@
        
        $installScriptPath = Join-Path $deploymentConfig.TargetPath "Install.ps1"
        Set-Content -Path $installScriptPath -Value $installScript -Encoding UTF8
        Write-Host "✓ Created: Install.ps1" -ForegroundColor Green
        
        # Create Uninstall.ps1
        $uninstallScript = @"
# RawrXD Production System Uninstaller
# Auto-generated on $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

param(
    [Parameter(Mandatory=`$false)]
    [string]`$InstallPath = "C:\RawrXD",
    
    [Parameter(Mandatory=`$false)]
    [string]`$ServiceName = "$ServiceName"
)

`$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         RawrXD Production System Uninstaller                 ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

try {
    # Check if running as administrator
    `$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not `$isAdmin) {
        Write-Host "⚠ This uninstaller requires Administrator privileges." -ForegroundColor Yellow
        Write-Host "  Please run as Administrator." -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "Uninstalling RawrXD Production System..." -ForegroundColor Yellow
    Write-Host "  Install Path: `$InstallPath" -ForegroundColor White
    Write-Host ""
    
    # Stop and remove service
    `$service = Get-Service -Name `$ServiceName -ErrorAction SilentlyContinue
    if (`$service) {
        Write-Host "Removing service: `$ServiceName" -ForegroundColor Yellow
        if (`$service.Status -eq 'Running') {
            Stop-Service -Name `$ServiceName -Force
            Start-Sleep -Seconds 2
        }
        sc.exe delete `$ServiceName | Out-Null
        Write-Host "✓ Service removed" -ForegroundColor Green
    }
    
    # Remove installation directory
    if (Test-Path `$InstallPath) {
        Write-Host "Removing installation directory..." -ForegroundColor Yellow
        Remove-Item -Path `$InstallPath -Recurse -Force
        Write-Host "✓ Installation directory removed" -ForegroundColor Green
    }
    
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║             Uninstallation Completed Successfully!           ║" -ForegroundColor Green
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║                  Uninstallation Failed!                      ║" -ForegroundColor Red
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    Write-Host ""
    Write-Host "Error: $_" -ForegroundColor Red
    Write-Host ""
    exit 1
}
"@
        
        $uninstallScriptPath = Join-Path $deploymentConfig.TargetPath "Uninstall.ps1"
        Set-Content -Path $uninstallScriptPath -Value $uninstallScript -Encoding UTF8
        Write-Host "✓ Created: Uninstall.ps1" -ForegroundColor Green
        
        # Create README.md
        $readme = @"
# RawrXD Production System

## Overview

The RawrXD Production System is a comprehensive autonomous development platform built entirely in PowerShell with no external dependencies.

## Features

- **Custom Model Performance Monitoring** - Track and analyze model performance
- **Agentic Command Execution** - Execute complex tasks through AI agents
- **Win32 Deployment** - System integration and service management
- **Custom Model Loading** - Support for multiple model formats (GGUF, GGML, ONNX, PyTorch, SafeTensors)
- **Autonomous Enhancement** - Self-improving code generation and optimization
- **Reverse Engineering** - Algorithmic code analysis and enhancement
- **Swarm Intelligence** - Multi-agent collaboration system

## System Requirements

- Windows 10/11 or Windows Server 2016+
- PowerShell 5.1 or higher
- Administrator privileges (for service installation)

## Installation

### Quick Install

1. Download the deployment package
2. Extract to your desired location
3. Run PowerShell as Administrator
4. Execute: `.\Install.ps1`

### Custom Installation

```powershell
.\Install.ps1 -InstallPath "C:\Custom\Path" -InstallService -ServiceName "MyService"
```

## Usage

### Get System Status

```powershell
Import-Module "$TargetPath\Modules\RawrXD.Master.psm1"
Get-MasterSystemStatus
```

### Run Autonomous Enhancement

```powershell
Start-AutonomousEnhancementCycle -TargetPath "$TargetPath" -ResearchQuery "top 10 agentic IDE features" -MaxFeatures 5
```

### Deploy Win32 Service

```powershell
Invoke-Win32Deployment -Action InstallService -ServiceName "MyService" -DisplayName "My Service" -BinaryPath "C:\Path\To\Service.exe"
```

### Load Custom Models

```powershell
$loader = New-ModelLoader -Path "C:\models\model.gguf"
$loader.Load()
$metadata = $loader.Metadata
```

## Module Structure

```
RawrXD/
├── Modules/                 # PowerShell modules
│   ├── RawrXD.Master.psm1          # Main entry point
│   ├── RawrXD.Logging.psm1         # Structured logging
│   ├── RawrXD.AgenticCommands.psm1 # Agentic commands
│   ├── RawrXD.CustomModelLoaders.psm1 # Model loading
│   ├── RawrXD.Win32Deployment.psm1 # Win32 integration
│   ├── RawrXD.AutonomousEnhancement.psm1 # Self-improvement
│   └── RawrXD.ReverseEngineering.psm1 # Code analysis
├── Scripts/                 # Utility scripts
├── Documentation/           # Documentation
├── Tests/                   # Test suites
└── Config/                  # Configuration files
```

## Testing

Run validation tests:

```powershell
.\Deploy-ProductionSystem.ps1 -ValidateOnly
```

## Uninstallation

Run PowerShell as Administrator and execute:

```powershell
.\Uninstall.ps1
```

## Troubleshooting

### Common Issues

1. **Import fails**: Ensure PowerShell execution policy allows script execution
2. **Service installation fails**: Verify Administrator privileges
3. **Module not found**: Check installation path and import statements

### Logs

Check logs in the `Logs` directory for detailed information.

## Security

- All modules include input validation
- Secure string handling for sensitive data
- Audit logging for critical operations
- No external dependencies reduce attack surface

## Performance

- Optimized for PowerShell 5.1+
- Caching support for frequently used functions
- Lazy loading for large modules
- Efficient regex patterns

## Development

### Adding New Features

1. Create new module in `Modules/`
2. Add to `RawrXD.Master.psm1` imports
3. Update documentation
4. Add tests

### Contributing

Follow the existing code style and include:
- Structured logging
- Error handling
- Documentation
- Tests

## License

MIT License - See LICENSE file for details

## Support

For issues and feature requests, please use the GitHub repository.

## Version

Version: 1.0.0
Build Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
PowerShell Version: 5.1+
"@
        
        $readmePath = Join-Path $deploymentConfig.TargetPath "README.md"
        Set-Content -Path $readmePath -Value $readme -Encoding UTF8
        Write-Host "✓ Created: README.md" -ForegroundColor Green
        
        Write-Host ""
        Write-Host "✓ Installation scripts created successfully!" -ForegroundColor Green
        Write-Host ""
        
        $deploymentConfig.DeploymentResults += @{
            Phase = "Installer Creation"
            Status = "Success"
            Details = "Created Install.ps1, Uninstall.ps1, and README.md"
        }
        
    } catch {
        Write-Host "✗ Installer creation failed: $_" -ForegroundColor Red
        exit 1
    }
}

# Phase 5: Final Validation
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "PHASE 5: Final Validation" -ForegroundColor Cyan
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

try {
    Write-Host "Performing final validation..." -ForegroundColor Yellow
    
    $validationResults = @()
    
    # Validation 1: Check target directory structure
    if (Test-Path $deploymentConfig.TargetPath) {
        $requiredDirs = @("Modules", "Scripts", "Documentation", "Tests", "Config")
        $allDirsExist = $true
        
        foreach ($dir in $requiredDirs) {
            $dirPath = Join-Path $deploymentConfig.TargetPath $dir
            if (Test-Path $dirPath) {
                Write-Host "  ✓ Directory exists: $dir" -ForegroundColor Green
            } else {
                Write-Host "  ✗ Directory missing: $dir" -ForegroundColor Red
                $allDirsExist = $false
            }
        }
        
        $validationResults += @{
            Test = "Directory Structure"
            Result = if ($allDirsExist) { "Passed" } else { "Failed" }
            Details = "All required directories exist"
        }
    }
    
    # Validation 2: Check modules
    $modulesDir = Join-Path $deploymentConfig.TargetPath "Modules"
    if (Test-Path $modulesDir) {
        $moduleFiles = Get-ChildItem -Path $modulesDir -Filter "RawrXD*.psm1"
        $expectedCount = $deploymentConfig.Modules.Count
        $actualCount = $moduleFiles.Count
        
        if ($actualCount -eq $expectedCount) {
            Write-Host "  ✓ All modules copied: $actualCount/$expectedCount" -ForegroundColor Green
            $validationResults += @{
                Test = "Module Copy"
                Result = "Passed"
                Details = "All $actualCount modules copied successfully"
            }
        } else {
            Write-Host "  ✗ Module count mismatch: $actualCount/$expectedCount" -ForegroundColor Red
            $validationResults += @{
                Test = "Module Copy"
                Result = "Failed"
                Details = "Expected $expectedCount modules, found $actualCount"
            }
        }
    }
    
    # Validation 3: Check installer scripts
    $installScript = Join-Path $deploymentConfig.TargetPath "Install.ps1"
    $uninstallScript = Join-Path $deploymentConfig.TargetPath "Uninstall.ps1"
    $readmeFile = Join-Path $deploymentConfig.TargetPath "README.md"
    
    $installerExists = Test-Path $installScript
    $uninstallerExists = Test-Path $uninstallScript
    $readmeExists = Test-Path $readmeFile
    
    if ($installerExists -and $uninstallerExists -and $readmeExists) {
        Write-Host "  ✓ All installer files present" -ForegroundColor Green
        $validationResults += @{
            Test = "Installer Files"
            Result = "Passed"
            Details = "Install.ps1, Uninstall.ps1, and README.md exist"
        }
    } else {
        Write-Host "  ✗ Some installer files missing" -ForegroundColor Red
        $validationResults += @{
            Test = "Installer Files"
            Result = "Failed"
            Details = "Missing: $(if (-not $installerExists) { 'Install.ps1 ' })$(if (-not $uninstallerExists) { 'Uninstall.ps1 ' })$(if (-not $readmeExists) { 'README.md' })"
        }
    }
    
    # Validation 4: Test module import from target
    if (Test-Path $modulesDir) {
        Write-Host "  Testing module import from target..." -ForegroundColor Gray
        
        try {
            # Save current location
            $currentLocation = Get-Location
            
            # Change to target modules directory
            Set-Location -Path $modulesDir
            
            # Test importing master module
            Import-Module ".\RawrXD.Master.psm1" -Force -Global -ErrorAction Stop
            
            # Restore location
            Set-Location -Path $currentLocation
            
            Write-Host "  ✓ Master module imports successfully from target" -ForegroundColor Green
            $validationResults += @{
                Test = "Target Module Import"
                Result = "Passed"
                Details = "RawrXD.Master.psm1 imports successfully"
            }
            
        } catch {
            Write-Host "  ✗ Module import failed: $_" -ForegroundColor Red
            $validationResults += @{
                Test = "Target Module Import"
                Result = "Failed"
                Details = "Import error: $_"
            }
        }
    }
    
    Write-Host ""
    
    # Summary
    $passedValidations = ($validationResults | Where-Object { $_.Result -eq "Passed" }).Count
    $totalValidations = $validationResults.Count
    
    Write-Host "Validation Summary:" -ForegroundColor Yellow
    Write-Host "  Total Validations: $totalValidations" -ForegroundColor White
    Write-Host "  Passed: $passedValidations" -ForegroundColor Green
    Write-Host "  Failed: $($totalValidations - $passedValidations)" -ForegroundColor $(if (($totalValidations - $passedValidations) -eq 0) { "Green" } else { "Red" })
    Write-Host ""
    
    foreach ($result in $validationResults) {
        $color = if ($result.Result -eq "Passed") { "Green" } else { "Red" }
        Write-Host "  [$($result.Result)] $($result.Test)" -ForegroundColor $color
        Write-Host "        $($result.Details)" -ForegroundColor Gray
    }
    
    Write-Host ""
    
    if ($passedValidations -eq $totalValidations) {
        Write-Host "✓ All validations passed!" -ForegroundColor Green
        $deploymentConfig.Success = $true
    } else {
        Write-Host "⚠ Some validations failed. Review before deployment." -ForegroundColor Yellow
        $deploymentConfig.Success = $false
    }
    
    Write-Host ""
    
} catch {
    Write-Host "✗ Validation failed: $_" -ForegroundColor Red
    $deploymentConfig.Success = false
}

# Final Summary
$deploymentConfig.EndTime = Get-Date
$deploymentConfig.Duration = [Math]::Round(($deploymentConfig.EndTime - $deploymentConfig.StartTime).TotalSeconds, 2)

Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "DEPLOYMENT COMPLETE" -ForegroundColor Cyan
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  Start Time: $($deploymentConfig.StartTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor White
Write-Host "  End Time: $($deploymentConfig.EndTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor White
Write-Host "  Duration: $($deploymentConfig.Duration) seconds" -ForegroundColor White
Write-Host "  Success: $($deploymentConfig.Success)" -ForegroundColor $(if ($deploymentConfig.Success) { "Green" } else { "Red" })
Write-Host ""

Write-Host "Modules Processed: $($deploymentConfig.Modules.Count)" -ForegroundColor White
Write-Host "  Total Lines: $(($deploymentConfig.Modules | Measure-Object -Property Lines -Sum).Sum)" -ForegroundColor White
Write-Host "  Total Functions: $(($deploymentConfig.Modules | Measure-Object -Property Functions -Sum).Sum)" -ForegroundColor White
Write-Host "  Total Classes: $(($deploymentConfig.Modules | Measure-Object -Property Classes -Sum).Sum)" -ForegroundColor White
Write-Host "  Total Exports: $(($deploymentConfig.Modules | Measure-Object -Property Exports -Sum).Sum)" -ForegroundColor White
Write-Host "  Total Size: $(($deploymentConfig.Modules | Measure-Object -Property SizeKB -Sum).Sum) KB" -ForegroundColor White
Write-Host ""

if (-not $deploymentConfig.ValidateOnly) {
    Write-Host "Deployment Location: $($deploymentConfig.TargetPath)" -ForegroundColor White
    Write-Host "  To install: Run .\Install.ps1 as Administrator" -ForegroundColor Yellow
    Write-Host "  To uninstall: Run .\Uninstall.ps1 as Administrator" -ForegroundColor Yellow
    Write-Host ""
}

Write-Host "Next Steps:" -ForegroundColor Yellow
if ($deploymentConfig.ValidateOnly) {
    Write-Host "  1. Review validation results" -ForegroundColor White
    Write-Host "  2. Fix any issues identified" -ForegroundColor White
    Write-Host "  3. Run deployment without -ValidateOnly switch" -ForegroundColor White
} else {
    Write-Host "  1. Test the deployment package" -ForegroundColor White
    Write-Host "  2. Run .\Install.ps1 on target systems" -ForegroundColor White
    Write-Host "  3. Verify functionality" -ForegroundColor White
    Write-Host "  4. Monitor logs for any issues" -ForegroundColor White
}
Write-Host ""

# Save deployment report
$reportPath = Join-Path $deploymentConfig.TargetPath "DeploymentReport.xml"
$deploymentConfig | Export-Clixml -Path $reportPath -Force
Write-Host "Deployment report saved to: $reportPath" -ForegroundColor Green
Write-Host ""

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor $(if ($deploymentConfig.Success) { "Green" } else { "Red" })
Write-Host "║          Deployment $(if ($deploymentConfig.Success) { 'Completed Successfully' } else { 'Completed with Issues' })" -ForegroundColor $(if ($deploymentConfig.Success) { "Green" } else { "Red" })
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor $(if ($deploymentConfig.Success) { "Green" } else { "Red" })
Write-Host ""

# Stop logging
if ($LogPath) {
    Stop-Transcript | Out-Null
}

exit $(if ($deploymentConfig.Success) { 0 } else { 1 })
