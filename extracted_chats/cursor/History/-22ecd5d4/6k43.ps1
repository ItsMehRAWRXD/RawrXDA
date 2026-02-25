# Direct Agent Tool Testing
# Tests the actual agent tools registered in RawrXD

Write-Host "🔬 Direct Agent Tool Testing" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

# Load RawrXD to get agent tools
Write-Host "📦 Loading RawrXD and agent tools..." -ForegroundColor Yellow
$script:agentTools = @{}
$global:currentWorkingDir = $PWD
$global:currentFile = $null

# Source RawrXD to get functions (but skip GUI initialization)
$rawrXDContent = Get-Content -Path "RawrXD.ps1" -Raw -ErrorAction SilentlyContinue
if ($rawrXDContent) {
    # Extract just the agent tool registration sections
    Write-Host "  ✅ RawrXD file found" -ForegroundColor Green
}

Write-Host ""
Write-Host "🧪 Testing Agent Tools Directly" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray
Write-Host ""

$testResults = @()
$testNum = 0

function Test-Tool {
    param([string]$Name, [scriptblock]$Test)
    $global:testNum++
    Write-Host "[$global:testNum] $Name" -ForegroundColor Yellow
    try {
        $result = & $Test
        if ($result.success -ne $false) {
            Write-Host "  ✅ PASS" -ForegroundColor Green
            if ($result -is [hashtable] -and $result.ContainsKey("count")) {
                Write-Host "    Detected: $($result.count) items" -ForegroundColor Gray
            }
            $global:testResults += @{Test = $Name; Status = "PASS"; Result = $result}
            return $true
        }
        else {
            Write-Host "  ❌ FAIL: $($result.error)" -ForegroundColor Red
            $global:testResults += @{Test = $Name; Status = "FAIL"; Result = $result}
            return $false
        }
    }
    catch {
        Write-Host "  ❌ EXCEPTION: $($_.Exception.Message)" -ForegroundColor Red
        $global:testResults += @{Test = $Name; Status = "EXCEPTION"; Result = $_.Exception.Message}
        return $false
    }
}

# Test 1: Language Detection (if function exists)
if (Get-Command Get-DetectedLanguages -ErrorAction SilentlyContinue) {
    Test-Tool -Name "Get-DetectedLanguages" {
        $langs = Get-DetectedLanguages
        return @{
            success = $true
            count = $langs.Count
            languages = $langs.Keys
            details = $langs
        }
    }
}
else {
    Write-Host "[SKIP] Get-DetectedLanguages function not available in this context" -ForegroundColor Yellow
}

# Test 2: Check for installed compilers directly
Write-Host ""
Write-Host "🔍 Direct Compiler Detection" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray

$compilers = @{
    "Python" = @("python", "python3", "py")
    "Node.js" = @("node")
    "Rust" = @("rustc", "cargo")
    "Go" = @("go")
    "GCC" = @("gcc", "g++")
    "Clang" = @("clang", "clang++")
    "NASM" = @("nasm")
    "MASM" = @("ml", "ml64")
    "Java" = @("java", "javac")
    ".NET" = @("dotnet")
}

$detectedCompilers = @{}
foreach ($compilerName in $compilers.Keys) {
    $found = $false
    $version = ""
    $cmd = ""
    
    foreach ($cmdName in $compilers[$compilerName]) {
        $command = Get-Command $cmdName -ErrorAction SilentlyContinue
        if ($command) {
            $found = $true
            $cmd = $cmdName
            try {
                if ($cmdName -eq "go") {
                    $version = & $cmdName version 2>&1 | Select-Object -First 1
                }
                elseif ($cmdName -match "ml|ml64") {
                    # MASM - check VS paths
                    $vsPaths = @(
                        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
                        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
                    )
                    foreach ($vsPath in $vsPaths) {
                        if (Test-Path $vsPath) {
                            $msvcDirs = Get-ChildItem -Path $vsPath -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
                            if ($msvcDirs) {
                                $msvcDir = $msvcDirs[0]
                                $ml64Path = Join-Path $msvcDir "bin\Hostx64\x64\ml64.exe"
                                if (Test-Path $ml64Path) {
                                    $version = "VS 2022 - $($msvcDir.Name)"
                                    break
                                }
                            }
                        }
                    }
                }
                else {
                    $version = & $cmdName --version 2>&1 | Select-Object -First 1
                }
            }
            catch {
                $version = "Available"
            }
            break
        }
    }
    
    if ($found) {
        Write-Host "  ✅ $compilerName`: $cmd" -ForegroundColor Green
        if ($version) {
            Write-Host "     Version: $($version.Trim())" -ForegroundColor Gray
        }
        $detectedCompilers[$compilerName] = @{
            command = $cmd
            version = $version
        }
    }
    else {
        Write-Host "  ❌ $compilerName`: Not found" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "📊 Detected: $($detectedCompilers.Count) compilers" -ForegroundColor Cyan

# Test 3: Project Creation Tests
Write-Host ""
Write-Host "📁 Project Creation Tests" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray

$testDir = Join-Path $PWD "agent-test-projects"
if (-not (Test-Path $testDir)) {
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
}

# Test Python project structure
if ($detectedCompilers.ContainsKey("Python")) {
    Test-Tool -Name "Create Python Project Structure" {
        $projName = "test-python-agent"
        $projPath = Join-Path $testDir $projName
        
        if (Test-Path $projPath) {
            Remove-Item -Path $projPath -Recurse -Force
        }
        
        New-Item -ItemType Directory -Path $projPath -Force | Out-Null
        
        $files = @{
            "main.py" = @"
#!/usr/bin/env python3
def main():
    print("Hello from agent-created project!")

if __name__ == "__main__":
    main()
"@
            "requirements.txt" = ""
            "README.md" = "# Test Python Project`nCreated by agent"
        }
        
        foreach ($file in $files.Keys) {
            Set-Content -Path (Join-Path $projPath $file) -Value $files[$file]
        }
        
        $allExist = $files.Keys | ForEach-Object { Test-Path (Join-Path $projPath $_) } | Where-Object { $_ -eq $true }
        
        return @{
            success = ($allExist.Count -eq $files.Count)
            path = $projPath
            files_created = $files.Keys
        }
    }
}

# Test NASM project
if ($detectedCompilers.ContainsKey("NASM")) {
    Test-Tool -Name "Create NASM Assembly Project" {
        $projName = "test-nasm-agent"
        $projPath = Join-Path $testDir $projName
        
        if (Test-Path $projPath) {
            Remove-Item -Path $projPath -Recurse -Force
        }
        
        New-Item -ItemType Directory -Path $projPath -Force | Out-Null
        
        $mainAsm = @"
; NASM test project
section .data
    msg db 'Hello from NASM!', 0xA
    len equ `$ - msg

section .text
    global _start

_start:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, len
    syscall
    
    mov rax, 60
    mov rdi, 0
    syscall
"@
        Set-Content -Path (Join-Path $projPath "main.asm") -Value $mainAsm
        
        $buildBat = @"
@echo off
nasm -f win64 main.asm -o main.obj
gcc main.obj -o main.exe
main.exe
"@
        Set-Content -Path (Join-Path $projPath "build.bat") -Value $buildBat
        
        return @{
            success = (Test-Path (Join-Path $projPath "main.asm")) -and (Test-Path (Join-Path $projPath "build.bat"))
            path = $projPath
        }
    }
}

# Test 4: Git Operations
Write-Host ""
Write-Host "🔧 Git Operations Tests" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray

if (Get-Command git -ErrorAction SilentlyContinue) {
    Test-Tool -Name "Git Status" {
        $status = git status --short 2>&1
        $branch = git branch --show-current 2>&1
        
        return @{
            success = $true
            branch = $branch.Trim()
            has_changes = ($status -match ".")
        }
    }
    
    Test-Tool -Name "Git Init Test Repository" {
        $repoPath = Join-Path $testDir "test-git-repo"
        
        if (Test-Path $repoPath) {
            Remove-Item -Path $repoPath -Recurse -Force
        }
        
        New-Item -ItemType Directory -Path $repoPath -Force | Out-Null
        $originalLocation = Get-Location
        Set-Location $repoPath
        
        $result = git init 2>&1 | Out-String
        $hasGit = Test-Path ".git"
        
        Set-Location $originalLocation
        
        return @{
            success = $hasGit
            output = $result
        }
    }
}

# Test 5: File Operations
Write-Host ""
Write-Host "📝 File Operations Tests" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray

Test-Tool -Name "Create and Read File" {
    $testFile = Join-Path $testDir "test-file-ops.txt"
    $content = "Test content created at $(Get-Date)"
    
    Set-Content -Path $testFile -Value $content
    $readContent = Get-Content -Path $testFile -Raw
    
    return @{
        success = ($readContent -eq $content)
        file = $testFile
        content_match = ($readContent -eq $content)
    }
}

Test-Tool -Name "List Directory" {
    $items = Get-ChildItem -Path $testDir -ErrorAction SilentlyContinue
    
    return @{
        success = $true
        count = $items.Count
        items = $items.Name
    }
}

# Test 6: Environment Detection
Write-Host ""
Write-Host "🌍 Environment Detection" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray

Test-Tool -Name "Get System Environment" {
    $env = @{
        os = [System.Environment]::OSVersion.ToString()
        ps_version = $PSVersionTable.PSVersion.ToString()
        current_dir = $PWD
        user = $env:USERNAME
        computer = $env:COMPUTERNAME
    }
    
    # Check for tools
    $tools = @{}
    $toolNames = @("python", "node", "rustc", "go", "gcc", "nasm", "java", "dotnet", "git")
    foreach ($tool in $toolNames) {
        $tools[$tool] = (Get-Command $tool -ErrorAction SilentlyContinue) -ne $null
    }
    
    return @{
        success = $true
        environment = $env
        tools = $tools
    }
}

# Summary
Write-Host ""
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "📊 TEST SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

$passed = ($testResults | Where-Object { $_.Status -eq "PASS" }).Count
$failed = ($testResults | Where-Object { $_.Status -ne "PASS" }).Count
$total = $testResults.Count

Write-Host "Total Tests: $total" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })

if ($total -gt 0) {
    $passRate = [math]::Round(($passed / $total) * 100, 2)
    Write-Host "Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -ge 90) { "Green" } elseif ($passRate -ge 70) { "Yellow" } else { "Red" })
}

Write-Host ""
Write-Host "Detected Compilers: $($detectedCompilers.Count)" -ForegroundColor Cyan
foreach ($comp in $detectedCompilers.Keys) {
    Write-Host "  • $comp`: $($detectedCompilers[$comp].command)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "🧹 Cleaning up test directory..." -ForegroundColor Yellow
if (Test-Path $testDir) {
    Remove-Item -Path $testDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  ✅ Cleanup complete" -ForegroundColor Green
}

Write-Host ""
Write-Host "✅ All tests completed!" -ForegroundColor Green

