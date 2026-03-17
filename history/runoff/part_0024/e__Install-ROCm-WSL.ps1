# Install ROCm on Windows via WSL
# This script automates the ROCm installation process for Windows using WSL and Ubuntu 22.04

param(
    [switch]$SkipWSLInstall,
    [switch]$SkipROCmInstall,
    [switch]$VerifyOnly
)

Write-Host "=== ROCm Installation for Windows via WSL ===" -ForegroundColor Cyan
Write-Host ""

# Function to check if running as administrator
function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Check administrator privileges
if (-not (Test-Administrator)) {
    Write-Host "ERROR: This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Please right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Step 1: Install WSL
if (-not $SkipWSLInstall -and -not $VerifyOnly) {
    Write-Host "[Step 1] Installing Windows Subsystem for Linux..." -ForegroundColor Green
    try {
        wsl --install -d Ubuntu-22.04
        Write-Host "WSL and Ubuntu 22.04 installed successfully!" -ForegroundColor Green
        Write-Host "NOTE: You may need to restart your computer and run this script again with -SkipWSLInstall flag" -ForegroundColor Yellow
        Write-Host ""
        
        # Check if restart is needed
        $wslStatus = wsl --status 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Please restart your computer and then run:" -ForegroundColor Yellow
            Write-Host "  .\Install-ROCm-WSL.ps1 -SkipWSLInstall" -ForegroundColor Cyan
            exit 0
        }
    }
    catch {
        Write-Host "Error installing WSL: $_" -ForegroundColor Red
        exit 1
    }
}

# Verify WSL is available
Write-Host "[Checking] Verifying WSL installation..." -ForegroundColor Green
$wslCheck = wsl --list --verbose 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: WSL is not properly installed. Please run without -SkipWSLInstall flag" -ForegroundColor Red
    exit 1
}
Write-Host "WSL is available" -ForegroundColor Green
Write-Host ""

# Step 2: Install ROCm in WSL
if (-not $SkipROCmInstall -and -not $VerifyOnly) {
    Write-Host "[Step 2] Installing ROCm in WSL Ubuntu..." -ForegroundColor Green
    Write-Host "This will execute the following commands in WSL:" -ForegroundColor Yellow
    Write-Host "  1. Update package list (sudo apt update)" -ForegroundColor Gray
    Write-Host "  2. Download AMD ROCm installer" -ForegroundColor Gray
    Write-Host "  3. Install the package" -ForegroundColor Gray
    Write-Host "  4. Install ROCm with WSL support" -ForegroundColor Gray
    Write-Host ""
    
    # Execute commands directly in WSL instead of creating a script file
    try {
        Write-Host "Executing installation in WSL..." -ForegroundColor Cyan
        
        Write-Host "  → Updating package list..." -ForegroundColor Gray
        wsl sudo apt update
        
        Write-Host "  → Downloading AMD ROCm installer..." -ForegroundColor Gray
        wsl wget https://repo.radeon.com/amdgpu-install/6.1.3/ubuntu/jammy/amdgpu-install_6.1.60103-1_all.deb
        
        Write-Host "  → Installing ROCm package..." -ForegroundColor Gray
        wsl sudo apt install -y ./amdgpu-install_6.1.60103-1_all.deb
        
        Write-Host "  → Installing ROCm with WSL support (this may take several minutes)..." -ForegroundColor Gray
        wsl sudo amdgpu-install -y --usecase=wsl,rocm --no-dkms
        
        Write-Host "  → Cleaning up..." -ForegroundColor Gray
        wsl rm -f amdgpu-install_6.1.60103-1_all.deb
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "ROCm installed successfully!" -ForegroundColor Green
        }
        else {
            Write-Host ""
            Write-Host "Installation encountered errors. Exit code: $LASTEXITCODE" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "Error during ROCm installation: $_" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

# Step 3: Verify ROCm installation
Write-Host "[Step 3] Verifying ROCm installation..." -ForegroundColor Green
try {
    Write-Host "Running rocm-smi to check ROCm status..." -ForegroundColor Cyan
    wsl rocm-smi
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "=== ROCm Installation Successful! ===" -ForegroundColor Green
        Write-Host ""
        Write-Host "ROCm is properly installed and working." -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  - To enter WSL Ubuntu: wsl" -ForegroundColor White
        Write-Host "  - To check ROCm: wsl rocm-smi" -ForegroundColor White
        Write-Host "  - To install PyTorch with ROCm: refer to ROCm documentation" -ForegroundColor White
        Write-Host "  - ROCm documentation: https://rocm.docs.amd.com/" -ForegroundColor White
    }
    else {
        Write-Host ""
        Write-Host "WARNING: rocm-smi returned exit code $LASTEXITCODE" -ForegroundColor Yellow
        Write-Host "ROCm may not be fully configured or compatible with your GPU" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "Error verifying ROCm: $_" -ForegroundColor Red
    Write-Host "ROCm may not be properly installed" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "=== Installation Complete ===" -ForegroundColor Cyan
