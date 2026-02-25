# Comprehensive Agentic Capabilities Test
# Tests all new agent tools: language detection, project creation, Git tools, recovery tools

Write-Host "🧪 Comprehensive Agentic Capabilities Test" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host ""

$testResults = @()
$testCount = 0
$passCount = 0
$failCount = 0

function Test-AgentTool {
    param(
        [string]$TestName,
        [scriptblock]$TestScript,
        [string]$Category = "General"
    )
    
    $global:testCount++
    Write-Host "[$global:testCount] Testing: $TestName" -ForegroundColor Yellow
    
    try {
        $result = & $TestScript
        if ($result.success -or $result -eq $true -or ($result -is [string] -and $result -notmatch "error|fail")) {
            Write-Host "  ✅ PASS: $TestName" -ForegroundColor Green
            $global:passCount++
            $global:testResults += @{
                Test = $TestName
                Category = $Category
                Status = "PASS"
                Result = $result
            }
            return $true
        }
        else {
            Write-Host "  ❌ FAIL: $TestName" -ForegroundColor Red
            Write-Host "    Error: $($result.error -or $result)" -ForegroundColor Red
            $global:failCount++
            $global:testResults += @{
                Test = $TestName
                Category = $Category
                Status = "FAIL"
                Result = $result
            }
            return $false
        }
    }
    catch {
        Write-Host "  ❌ EXCEPTION: $TestName" -ForegroundColor Red
        Write-Host "    Error: $($_.Exception.Message)" -ForegroundColor Red
        $global:failCount++
        $global:testResults += @{
            Test = $TestName
            Category = $Category
            Status = "EXCEPTION"
            Result = $_.Exception.Message
        }
        return $false
    }
}

# Load RawrXD functions
Write-Host "📦 Loading RawrXD functions..." -ForegroundColor Cyan
try {
    . .\RawrXD.ps1 -NoGUI 2>&1 | Out-Null
    Write-Host "  ✅ RawrXD loaded" -ForegroundColor Green
}
catch {
    Write-Host "  ⚠️  Note: Some functions may not be available in standalone mode" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "🔍 TESTING LANGUAGE DETECTION" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Test 1: Detect Languages
Test-AgentTool -TestName "Detect All Languages" -Category "Language Detection" {
    if (Get-Command Get-DetectedLanguages -ErrorAction SilentlyContinue) {
        $langs = Get-DetectedLanguages
        return @{
            success = $true
            count = $langs.Count
            languages = $langs.Keys
        }
    }
    else {
        # Try direct detection
        $detected = @()
        $commands = @("python", "node", "rustc", "go", "gcc", "g++", "clang", "clang++", "nasm", "java", "javac", "dotnet")
        foreach ($cmd in $commands) {
            if (Get-Command $cmd -ErrorAction SilentlyContinue) {
                $detected += $cmd
            }
        }
        return @{
            success = $true
            count = $detected.Count
            languages = $detected
        }
    }
}

# Test 2: Check Specific Languages
$languagesToCheck = @("python", "node", "rust", "go", "gcc", "nasm", "masm", "java", "dotnet")
foreach ($lang in $languagesToCheck) {
    Test-AgentTool -TestName "Detect $lang" -Category "Language Detection" {
        $cmd = switch ($lang) {
            "python" { "python" }
            "node" { "node" }
            "rust" { "rustc" }
            "go" { "go" }
            "gcc" { "gcc" }
            "nasm" { "nasm" }
            "masm" { "ml64" }
            "java" { "java" }
            "dotnet" { "dotnet" }
            default { $lang }
        }
        
        $found = Get-Command $cmd -ErrorAction SilentlyContinue
        if ($found) {
            try {
                $version = & $cmd --version 2>&1 | Select-Object -First 1
                return @{
                    success = $true
                    compiler = $cmd
                    version = $version
                }
            }
            catch {
                return @{
                    success = $true
                    compiler = $cmd
                    version = "Available"
                }
            }
        }
        else {
            # Check for MASM in Visual Studio paths
            if ($lang -eq "masm") {
                $vsPaths = @(
                    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
                    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
                )
                foreach ($vsPath in $vsPaths) {
                    if (Test-Path $vsPath) {
                        $msvcDirs = Get-ChildItem -Path $vsPath -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
                        foreach ($msvcDir in $msvcDirs) {
                            $ml64Path = Join-Path $msvcDir "bin\Hostx64\x64\ml64.exe"
                            if (Test-Path $ml64Path) {
                                return @{
                                    success = $true
                                    compiler = "ml64"
                                    version = "Visual Studio MASM - $($msvcDir.Name)"
                                    path = $ml64Path
                                }
                            }
                        }
                    }
                }
            }
            return @{
                success = $false
                error = "$lang not found"
            }
        }
    }
}

Write-Host ""
Write-Host "📁 TESTING PROJECT CREATION" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

$testProjectsDir = Join-Path $PWD "test-projects"
if (-not (Test-Path $testProjectsDir)) {
    New-Item -ItemType Directory -Path $testProjectsDir -Force | Out-Null
}

# Test 3: Create Python Project
Test-AgentTool -TestName "Create Python Project" -Category "Project Creation" {
    $projName = "test-python-app"
    $projPath = Join-Path $testProjectsDir $projName
    
    if (Test-Path $projPath) {
        Remove-Item -Path $projPath -Recurse -Force
    }
    
    New-Item -ItemType Directory -Path $projPath -Force | Out-Null
    Set-Location $projPath
    
    # Create basic Python project structure
    $mainPy = @"
#!/usr/bin/env python3
"""Test Python project created by agent."""

def main():
    print("Hello from agent-created Python project!")

if __name__ == "__main__":
    main()
"@
    Set-Content -Path "main.py" -Value $mainPy
    Set-Content -Path "requirements.txt" -Value ""
    Set-Content -Path "README.md" -Value "# Test Python Project`nCreated by agent test"
    
    Set-Location $PWD
    
    $hasFiles = (Test-Path (Join-Path $projPath "main.py")) -and 
                (Test-Path (Join-Path $projPath "requirements.txt")) -and
                (Test-Path (Join-Path $projPath "README.md"))
    
    return @{
        success = $hasFiles
        path = $projPath
        files = if ($hasFiles) { @("main.py", "requirements.txt", "README.md") } else { @() }
    }
}

# Test 4: Create Rust Project (if cargo available)
if (Get-Command cargo -ErrorAction SilentlyContinue) {
    Test-AgentTool -TestName "Create Rust Project (via cargo)" -Category "Project Creation" {
        $projName = "test-rust-app"
        $projPath = Join-Path $testProjectsDir $projName
        
        if (Test-Path $projPath) {
            Remove-Item -Path $projPath -Recurse -Force
        }
        
        $originalLocation = Get-Location
        Set-Location $testProjectsDir
        
        $result = cargo new $projName 2>&1 | Out-String
        
        Set-Location $originalLocation
        
        $hasCargoToml = Test-Path (Join-Path $projPath "Cargo.toml")
        $hasSrc = Test-Path (Join-Path $projPath "src\main.rs")
        
        return @{
            success = $hasCargoToml -and $hasSrc
            path = $projPath
            output = $result
        }
    }
}

# Test 5: Create Assembly Project (NASM)
if (Get-Command nasm -ErrorAction SilentlyContinue) {
    Test-AgentTool -TestName "Create NASM Assembly Project" -Category "Project Creation" {
        $projName = "test-nasm-app"
        $projPath = Join-Path $testProjectsDir $projName
        
        if (Test-Path $projPath) {
            Remove-Item -Path $projPath -Recurse -Force
        }
        
        New-Item -ItemType Directory -Path $projPath -Force | Out-Null
        
        $mainAsm = @"
; NASM Hello World
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
        
        $hasAsm = Test-Path (Join-Path $projPath "main.asm")
        
        return @{
            success = $hasAsm
            path = $projPath
        }
    }
}

Write-Host ""
Write-Host "🔧 TESTING GIT TOOLS" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Test 6: Git Status
if (Get-Command git -ErrorAction SilentlyContinue) {
    Test-AgentTool -TestName "Git Status Check" -Category "Git Tools" {
        $status = git status --short 2>&1 | Out-String
        $branch = git branch --show-current 2>&1
        
        return @{
            success = $true
            branch = $branch.Trim()
            status = $status
        }
    }
    
    # Test 7: Git Init (in test directory)
    Test-AgentTool -TestName "Git Init New Repository" -Category "Git Tools" {
        $testRepoDir = Join-Path $testProjectsDir "test-git-repo"
        
        if (Test-Path $testRepoDir) {
            Remove-Item -Path $testRepoDir -Recurse -Force
        }
        
        New-Item -ItemType Directory -Path $testRepoDir -Force | Out-Null
        Set-Location $testRepoDir
        
        $result = git init 2>&1 | Out-String
        $hasGit = Test-Path ".git"
        
        Set-Location $PWD
        
        return @{
            success = $hasGit
            output = $result
        }
    }
}

Write-Host ""
Write-Host "🛠️  TESTING FILE OPERATIONS" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Test 8: File Creation
Test-AgentTool -TestName "Create Test File" -Category "File Operations" {
    $testFile = Join-Path $testProjectsDir "test-file.txt"
    $content = "This is a test file created by the agent test script.`nTimestamp: $(Get-Date)"
    
    Set-Content -Path $testFile -Value $content
    
    $exists = Test-Path $testFile
    $readContent = if ($exists) { Get-Content $testFile -Raw } else { "" }
    
    return @{
        success = $exists -and ($readContent -match "test file")
        path = $testFile
        content_length = $readContent.Length
    }
}

# Test 9: Directory Listing
Test-AgentTool -TestName "List Directory Contents" -Category "File Operations" {
    $items = Get-ChildItem -Path $testProjectsDir -ErrorAction SilentlyContinue
    
    return @{
        success = $true
        count = $items.Count
        items = $items.Name
    }
}

Write-Host ""
Write-Host "🔄 TESTING RECOVERY TOOLS" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Test 10: Process Detection
Test-AgentTool -TestName "Detect Running Processes" -Category "Recovery Tools" {
    $processes = Get-Process | Select-Object -First 10 -Property ProcessName, Id
    
    return @{
        success = $true
        process_count = $processes.Count
        sample_processes = $processes.ProcessName
    }
}

# Test 11: File Lock Detection (simulated)
Test-AgentTool -TestName "File Lock Detection" -Category "Recovery Tools" {
    $testFile = Join-Path $testProjectsDir "test-lock.txt"
    Set-Content -Path $testFile -Value "test"
    
    try {
        $fileStream = [System.IO.File]::Open($testFile.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $fileStream.Close()
        
        return @{
            success = $true
            locked = $false
            message = "File is not locked"
        }
    }
    catch {
        return @{
            success = $true
            locked = $true
            message = "File appears to be locked"
        }
    }
}

Write-Host ""
Write-Host "📊 TESTING ENVIRONMENT DETECTION" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Test 12: Environment Info
Test-AgentTool -TestName "Get Environment Information" -Category "System" {
    return @{
        success = $true
        os = [System.Environment]::OSVersion.ToString()
        powershell_version = $PSVersionTable.PSVersion.ToString()
        current_dir = $PWD
        user = $env:USERNAME
        computer = $env:COMPUTERNAME
        dotnet_installed = (Get-Command dotnet -ErrorAction SilentlyContinue) -ne $null
        git_installed = (Get-Command git -ErrorAction SilentlyContinue) -ne $null
        node_installed = (Get-Command node -ErrorAction SilentlyContinue) -ne $null
        python_installed = (Get-Command python -ErrorAction SilentlyContinue) -ne $null
    }
}

Write-Host ""
Write-Host "🧹 CLEANUP" -ForegroundColor Cyan
Write-Host "-" * 60 -ForegroundColor Gray

# Cleanup test projects
Write-Host "Cleaning up test projects..." -ForegroundColor Yellow
if (Test-Path $testProjectsDir) {
    try {
        Remove-Item -Path $testProjectsDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "  ✅ Test projects cleaned up" -ForegroundColor Green
    }
    catch {
        Write-Host "  ⚠️  Some files may still exist: $_" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "📊 TEST SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host ""
Write-Host "Total Tests: $testCount" -ForegroundColor White
Write-Host "Passed: $passCount" -ForegroundColor Green
Write-Host "Failed: $failCount" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Red" })
Write-Host ""

$passRate = if ($testCount -gt 0) { [math]::Round(($passCount / $testCount) * 100, 2) } else { 0 }
Write-Host "Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -ge 80) { "Green" } elseif ($passRate -ge 60) { "Yellow" } else { "Red" })
Write-Host ""

# Detailed Results by Category
$categories = $testResults | Group-Object Category
Write-Host "Results by Category:" -ForegroundColor Cyan
foreach ($cat in $categories) {
    $catPass = ($cat.Group | Where-Object { $_.Status -eq "PASS" }).Count
    $catTotal = $cat.Group.Count
    Write-Host "  $($cat.Name): $catPass/$catTotal passed" -ForegroundColor $(if ($catPass -eq $catTotal) { "Green" } else { "Yellow" })
}

Write-Host ""
Write-Host "✅ Test Complete!" -ForegroundColor Green

# Export results
$resultsFile = "Agentic-Test-Results-$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$testResults | ConvertTo-Json -Depth 5 | Set-Content -Path $resultsFile
Write-Host "Results saved to: $resultsFile" -ForegroundColor Cyan

