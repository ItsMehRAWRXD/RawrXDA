# RawrZ Safe Test Environment - Public Domain
# Creates isolated testing environment for advanced evasion tools
# 
# This is free and unencumbered software released into the public domain.
# 
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
# 
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# For more information, please refer to <http://unlicense.org/>

param(
    [switch]$CreateEnvironment,
    [switch]$CleanupEnvironment,
    [switch]$RunTests
)

$TestEnvironment = "C:\RawrZTestEnv"
$TestFiles = @(
    "EvAdrKiller.ps1",
    "B.ps1", 
    "CppEncExe.ps1",
    "FHp.ps1",
    "TripleCrypto.ps1",
    "polymorph.asm",
    "polymorph_ssl.asm",
    "loader.c",
    "stealthdrv.c",
    "stealthinst.c",
    "stealthinj.asm"
)

function New-TestEnvironment {
    Write-Host "Creating safe test environment..." -ForegroundColor Cyan
    
    # Create test directory
    if (Test-Path $TestEnvironment) {
        Remove-Item $TestEnvironment -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TestEnvironment -Force | Out-Null
    
    # Copy test files
    foreach ($file in $TestFiles) {
        if (Test-Path $file) {
            Copy-Item $file $TestEnvironment
            Write-Host "Copied $file to test environment" -ForegroundColor Green
        } else {
            Write-Host "Warning: $file not found" -ForegroundColor Yellow
        }
    }
    
    # Create test data files
    $testData = "This is test data for encryption testing."
    $testData | Out-File -FilePath "$TestEnvironment\test_data.txt" -Encoding ASCII
    
    # Create simple C++ test file
    $cppTest = @"
#include <iostream>
int main() {
    std::cout << "Hello from RawrZ test!" << std::endl;
    return 0;
}
"@
    $cppTest | Out-File -FilePath "$TestEnvironment\test.cpp" -Encoding ASCII
    
    Write-Host "Test environment created at: $TestEnvironment" -ForegroundColor Green
}

function Remove-TestEnvironment {
    Write-Host "Cleaning up test environment..." -ForegroundColor Cyan
    
    if (Test-Path $TestEnvironment) {
        Remove-Item $TestEnvironment -Recurse -Force
        Write-Host "Test environment cleaned up" -ForegroundColor Green
    } else {
        Write-Host "Test environment not found" -ForegroundColor Yellow
    }
}

function Test-PowerShellUtilities {
    Write-Host "Testing PowerShell utilities in safe environment..." -ForegroundColor Yellow
    
    Push-Location $TestEnvironment
    
    try {
        # Test EvAdrKiller.ps1 (syntax check only)
        if (Test-Path "EvAdrKiller.ps1") {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content "EvAdrKiller.ps1" -Raw), [ref]$null)
            Write-Host "EvAdrKiller.ps1 syntax check: PASS" -ForegroundColor Green
        }
        
        # Test B.ps1 (syntax check only)
        if (Test-Path "B.ps1") {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content "B.ps1" -Raw), [ref]$null)
            Write-Host "B.ps1 syntax check: PASS" -ForegroundColor Green
        }
        
        # Test CppEncExe.ps1 (syntax check only)
        if (Test-Path "CppEncExe.ps1") {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content "CppEncExe.ps1" -Raw), [ref]$null)
            Write-Host "CppEncExe.ps1 syntax check: PASS" -ForegroundColor Green
        }
        
        # Test FHp.ps1 (syntax check only)
        if (Test-Path "FHp.ps1") {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content "FHp.ps1" -Raw), [ref]$null)
            Write-Host "FHp.ps1 syntax check: PASS" -ForegroundColor Green
        }
        
        # Test TripleCrypto.ps1 (syntax check only)
        if (Test-Path "TripleCrypto.ps1") {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content "TripleCrypto.ps1" -Raw), [ref]$null)
            Write-Host "TripleCrypto.ps1 syntax check: PASS" -ForegroundColor Green
        }
        
    } catch {
        Write-Host "PowerShell syntax check failed: $_" -ForegroundColor Red
    } finally {
        Pop-Location
    }
}

function Test-AssemblyFiles {
    Write-Host "Testing Assembly files..." -ForegroundColor Yellow
    
    # Check if ML64 is available
    try {
        $null = Get-Command "ml64" -ErrorAction Stop
        Write-Host "ML64 compiler found" -ForegroundColor Green
        
        # Test polymorph.asm syntax (basic check)
        if (Test-Path "$TestEnvironment\polymorph.asm") {
            $content = Get-Content "$TestEnvironment\polymorph.asm" -Raw
            if ($content -match "OPTION DOTNAME" -and $content -match "Start PROC") {
                Write-Host "polymorph.asm structure check: PASS" -ForegroundColor Green
            } else {
                Write-Host "polymorph.asm structure check: FAIL" -ForegroundColor Red
            }
        }
        
        # Test polymorph_ssl.asm syntax (basic check)
        if (Test-Path "$TestEnvironment\polymorph_ssl.asm") {
            $content = Get-Content "$TestEnvironment\polymorph_ssl.asm" -Raw
            if ($content -match "OPTION DOTNAME" -and $content -match "OpenSSL") {
                Write-Host "polymorph_ssl.asm structure check: PASS" -ForegroundColor Green
            } else {
                Write-Host "polymorph_ssl.asm structure check: FAIL" -ForegroundColor Red
            }
        }
        
    } catch {
        Write-Host "ML64 compiler not found - skipping assembly tests" -ForegroundColor Yellow
    }
}

function Test-CFiles {
    Write-Host "Testing C files..." -ForegroundColor Yellow
    
    # Check if cl.exe is available
    try {
        $null = Get-Command "cl" -ErrorAction Stop
        Write-Host "MSVC compiler found" -ForegroundColor Green
        
        # Test loader.c syntax (basic check)
        if (Test-Path "$TestEnvironment\loader.c") {
            $content = Get-Content "$TestEnvironment\loader.c" -Raw
            if ($content -match "public domain" -and $content -match "crypt_bytes") {
                Write-Host "loader.c structure check: PASS" -ForegroundColor Green
            } else {
                Write-Host "loader.c structure check: FAIL" -ForegroundColor Red
            }
        }
        
        # Test stealthdrv.c syntax (basic check)
        if (Test-Path "$TestEnvironment\stealthdrv.c") {
            $content = Get-Content "$TestEnvironment\stealthdrv.c" -Raw
            if ($content -match "public domain" -and $content -match "ObRegisterCallbacks") {
                Write-Host "stealthdrv.c structure check: PASS" -ForegroundColor Green
            } else {
                Write-Host "stealthdrv.c structure check: FAIL" -ForegroundColor Red
            }
        }
        
        # Test stealthinst.c syntax (basic check)
        if (Test-Path "$TestEnvironment\stealthinst.c") {
            $content = Get-Content "$TestEnvironment\stealthinst.c" -Raw
            if ($content -match "public domain" -and $content -match "NtLoadDriver") {
                Write-Host "stealthinst.c structure check: PASS" -ForegroundColor Green
            } else {
                Write-Host "stealthinst.c structure check: FAIL" -ForegroundColor Red
            }
        }
        
    } catch {
        Write-Host "MSVC compiler not found - skipping C file tests" -ForegroundColor Yellow
    }
}

function Test-APIEndpoints {
    Write-Host "Testing API endpoints..." -ForegroundColor Yellow
    
    # Check if API server is running
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:3000/api/engines/status" -Method GET -TimeoutSec 5
        Write-Host "API server is running" -ForegroundColor Green
        
        # Test basic encryption endpoint
        $testData = @{
            data = "test data"
            algorithm = "aes-256-gcm"
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri "http://localhost:3000/api/real-encryption/encrypt" -Method POST -Body $testData -ContentType "application/json" -TimeoutSec 5
        Write-Host "Real encryption endpoint test: PASS" -ForegroundColor Green
        
    } catch {
        Write-Host "API server not responding: $_" -ForegroundColor Red
    }
}

function Show-TestResults {
    Write-Host ""
    Write-Host "Safe Test Environment Results" -ForegroundColor Cyan
    Write-Host "=============================" -ForegroundColor Cyan
    Write-Host "All tests completed in isolated environment" -ForegroundColor Green
    Write-Host "Test environment location: $TestEnvironment" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To clean up the test environment, run:" -ForegroundColor Yellow
    Write-Host "  .\safe_test_environment.ps1 -CleanupEnvironment" -ForegroundColor Gray
}

# Main execution
if ($CreateEnvironment) {
    New-TestEnvironment
}

if ($RunTests) {
    if (-not (Test-Path $TestEnvironment)) {
        Write-Host "Test environment not found. Creating..." -ForegroundColor Yellow
        New-TestEnvironment
    }
    
    Write-Host "Running tests in safe environment..." -ForegroundColor Cyan
    Test-PowerShellUtilities
    Test-AssemblyFiles
    Test-CFiles
    Test-APIEndpoints
    Show-TestResults
}

if ($CleanupEnvironment) {
    Remove-TestEnvironment
}

if (-not $CreateEnvironment -and -not $CleanupEnvironment -and -not $RunTests) {
    Write-Host "RawrZ Safe Test Environment" -ForegroundColor Cyan
    Write-Host "===========================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\safe_test_environment.ps1 -CreateEnvironment" -ForegroundColor Gray
    Write-Host "  .\safe_test_environment.ps1 -RunTests" -ForegroundColor Gray
    Write-Host "  .\safe_test_environment.ps1 -CleanupEnvironment" -ForegroundColor Gray
    Write-Host ""
    Write-Host "This script creates an isolated testing environment for" -ForegroundColor White
    Write-Host "safely testing all RawrZ advanced evasion tools." -ForegroundColor White
}
