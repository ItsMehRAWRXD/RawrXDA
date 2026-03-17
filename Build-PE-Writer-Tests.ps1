# Build-PE-Writer-Tests.ps1
# PowerShell script to build and run PE Writer integration tests

param(
    [switch]$Clean,
    [switch]$Test,
    [switch]$Verbose,
    [string]$OutputDir = ".\build"
)

Write-Host "=== RawrXD PE Writer Build & Test System ===" -ForegroundColor Green

# Configuration
$MasmPath = "C:\masm32\bin\ml64.exe"
$LinkPath = "C:\masm32\bin\link64.exe"
$IncludePath = "C:\masm32\include64"
$LibPath = "C:\masm32\lib64"

$SourceFiles = @(
    "RawrXD_PE_Writer.asm",
    "PE_Writer_Integration_Tests.asm", 
    "PE_Test_Helpers.asm"
)

$OutputExe = "PE_Integration_Tests.exe"
$TestOutputDir = "$OutputDir\test_output"
$ReportFile = "$OutputDir\integration_report.txt"

# Helper function to run commands with error checking
function Invoke-BuildCommand {
    param([string]$Command, [string]$Description)
    
    if ($Verbose) {
        Write-Host "Executing: $Command" -ForegroundColor Yellow
    }
    
    Write-Host "  $Description..." -NoNewline
    
    $result = Invoke-Expression $Command 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        Write-Host " SUCCESS" -ForegroundColor Green
        return $true
    } else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "Error output:" -ForegroundColor Red
        Write-Host $result -ForegroundColor Red
        return $false
    }
}

# Cleanup function
function Clear-BuildArtifacts {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Cyan
    
    $cleanTargets = @("*.obj", "*.exe", "*.lib", "*.exp", "*.pdb", "$OutputDir\*")
    foreach ($target in $cleanTargets) {
        if (Test-Path $target) {
            Remove-Item $target -Force -Recurse
            if ($Verbose) { Write-Host "  Removed: $target" }
        }
    }
}

# Validate environment
function Test-BuildEnvironment {
    Write-Host "Validating build environment..." -ForegroundColor Cyan
    
    $tools = @{
        "MASM64" = $MasmPath
        "LINK64" = $LinkPath
        "Include" = $IncludePath
        "Lib" = $LibPath
    }
    
    $allValid = $true
    foreach ($tool in $tools.GetEnumerator()) {
        if (Test-Path $tool.Value) {
            Write-Host "  ✓ $($tool.Key): $($tool.Value)" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $($tool.Key): $($tool.Value) NOT FOUND" -ForegroundColor Red
            $allValid = $false
        }
    }
    
    foreach ($file in $SourceFiles) {
        if (Test-Path $file) {
            Write-Host "  ✓ Source: $file" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Source: $file NOT FOUND" -ForegroundColor Red
            $allValid = $false
        }
    }
    
    return $allValid
}

# Build PE Writer
function Build-PEWriter {
    Write-Host "Building PE Writer..." -ForegroundColor Cyan
    
    # Create output directory
    if (!(Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
    }
    
    if (!(Test-Path $TestOutputDir)) {
        New-Item -ItemType Directory -Path $TestOutputDir | Out-Null
    }
    
    # Compile each source file
    $objectFiles = @()
    foreach ($sourceFile in $SourceFiles) {
        $objFile = [System.IO.Path]::GetFileNameWithoutExtension($sourceFile) + ".obj"
        $objectFiles += $objFile
        
        $masmCmd = "`"$MasmPath`" /c /Cp /W3 /I`"$IncludePath`" /Fo`"$objFile`" `"$sourceFile`""
        
        if (-not (Invoke-BuildCommand $masmCmd "Compiling $sourceFile")) {
            return $false
        }
    }
    
    # Link the executable
    $objList = $objectFiles -join " "
    $linkCmd = "`"$LinkPath`" /SUBSYSTEM:CONSOLE /ENTRY:main /LIBPATH:`"$LibPath`" /OUT:`"$OutputDir\$OutputExe`" $objList kernel32.lib user32.lib"
    
    if (-not (Invoke-BuildCommand $linkCmd "Linking executable")) {
        return $false
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    return $true
}

# Run integration tests
function Run-IntegrationTests {
    Write-Host "Running integration tests..." -ForegroundColor Cyan
    
    $testExe = "$OutputDir\$OutputExe"
    if (!(Test-Path $testExe)) {
        Write-Host "  Test executable not found: $testExe" -ForegroundColor Red
        return $false
    }
    
    # Change to test output directory
    $originalLocation = Get-Location
    Set-Location $TestOutputDir
    
    Write-Host "  Executing integration tests..."
    
    try {
        # Run the test executable and capture output
        $testOutput = & $testExe 2>&1
        $testExitCode = $LASTEXITCODE
        
        # Save test output to report
        $testOutput | Out-File -FilePath $ReportFile -Encoding UTF8
        
        Write-Host "  Test execution completed with exit code: $testExitCode"
        
        # Display test results
        Write-Host ""
        Write-Host "=== TEST RESULTS ===" -ForegroundColor Yellow
        Write-Host $testOutput
        
        # Analyze results
        $passCount = ($testOutput | Select-String "\[PASS\]").Count
        $failCount = ($testOutput | Select-String "\[FAIL\]").Count
        
        Write-Host ""
        Write-Host "=== SUMMARY ===" -ForegroundColor Yellow
        Write-Host "Tests Passed: $passCount" -ForegroundColor Green
        Write-Host "Tests Failed: $failCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
        Write-Host "Overall: $(if ($testExitCode -eq 0 -and $failCount -eq 0) { "SUCCESS" } else { "FAILURE" })" -ForegroundColor $(if ($testExitCode -eq 0 -and $failCount -eq 0) { "Green" } else { "Red" })
        
        # Check for generated executables
        Write-Host ""
        Write-Host "=== GENERATED FILES ===" -ForegroundColor Yellow
        $generatedFiles = @("simple_test.exe", "msgbox_test.exe", "test_complex.exe")
        foreach ($file in $generatedFiles) {
            if (Test-Path $file) {
                $size = (Get-Item $file).Length
                Write-Host "  ✓ $file ($size bytes)" -ForegroundColor Green
                
                # Basic file validation
                try {
                    $header = Get-Content $file -Encoding Byte -TotalCount 2
                    if ($header[0] -eq 0x4D -and $header[1] -eq 0x5A) {  # 'MZ'
                        Write-Host "    ✓ Valid PE signature" -ForegroundColor Green
                    } else {
                        Write-Host "    ✗ Invalid PE signature" -ForegroundColor Red
                    }
                } catch {
                    Write-Host "    ? Could not validate signature" -ForegroundColor Yellow
                }
            } else {
                Write-Host "  ✗ $file (not generated)" -ForegroundColor Red
            }
        }
        
        return ($testExitCode -eq 0 -and $failCount -eq 0)
        
    } finally {
        Set-Location $originalLocation
    }
}

# Generate comprehensive report
function New-IntegrationReport {
    Write-Host "Generating integration report..." -ForegroundColor Cyan
    
    $reportContent = @"
# RawrXD PE Writer Integration Test Report
Generated: $(Get-Date)

## Build Environment
- MASM Path: $MasmPath
- Link Path: $LinkPath
- Include Path: $IncludePath
- Library Path: $LibPath

## Source Files
$(foreach ($file in $SourceFiles) { "- $file`n" })

## Test Results
$(if (Test-Path $ReportFile) { Get-Content $ReportFile } else { "No test output available" })

## Generated Files
$(foreach ($file in @("simple_test.exe", "msgbox_test.exe", "test_complex.exe")) {
    if (Test-Path "$TestOutputDir\$file") {
        $size = (Get-Item "$TestOutputDir\$file").Length
        "- $file ($size bytes) ✓"
    } else {
        "- $file (not generated) ✗"
    }
})

## Recommendations
1. All generated executables should have proper PE structure
2. Files should be executable on target Windows systems  
3. Import tables should be complete and functional
4. Code generation should produce valid x64 machine code
5. Memory management should be robust with no leaks

## Technical Notes
- The PE writer generates Windows x64 PE32+ executables
- Import table supports kernel32.dll and user32.dll
- Machine code emitter supports basic x64 instruction set
- File alignment is set to 512 bytes (200h)
- Section alignment is set to 4096 bytes (1000h)

"@

    $reportContent | Out-File -FilePath "$OutputDir\comprehensive_report.md" -Encoding UTF8
    Write-Host "  Report saved to: $OutputDir\comprehensive_report.md" -ForegroundColor Green
}

# Main execution
try {
    # Handle cleanup
    if ($Clean) {
        Clear-BuildArtifacts
        return
    }
    
    # Validate environment
    if (-not (Test-BuildEnvironment)) {
        Write-Host "Build environment validation failed!" -ForegroundColor Red
        exit 1
    }
    
    # Build the project
    if (-not (Build-PEWriter)) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    
    # Run tests if requested
    if ($Test) {
        if (-not (Run-IntegrationTests)) {
            Write-Host "Integration tests failed!" -ForegroundColor Red
            exit 1
        }
        
        New-IntegrationReport
    }
    
    Write-Host ""
    Write-Host "=== BUILD COMPLETE ===" -ForegroundColor Green
    Write-Host "Executable: $OutputDir\$OutputExe"
    Write-Host "Test Output: $TestOutputDir"
    Write-Host "Report: $OutputDir\comprehensive_report.md"
    
} catch {
    Write-Host "An error occurred: $_" -ForegroundColor Red
    exit 1
}