# Full Integration Test - Simulates Agent Tool Calls
# Tests the complete agentic workflow

Write-Host "🚀 Full Agentic Integration Test" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

# Simulate agent tool calls
function Invoke-AgentTool-Simulated {
    param(
        [string]$ToolName,
        [hashtable]$Parameters = @{}
    )

    Write-Host "  🔧 Calling: $ToolName" -ForegroundColor Gray

    switch ($ToolName) {
        "detect_languages" {
            $languages = @{}

            # Check each language
            $langChecks = @{
                "python" = @("python", "python3", "py")
                "node" = @("node")
                "javascript" = @("node")
                "rust" = @("rustc", "cargo")
                "go" = @("go")
                "c" = @("gcc", "clang", "cl")
                "cpp" = @("g++", "clang++", "cl")
                "nasm" = @("nasm")
                "masm" = @("ml", "ml64")
                "java" = @("java", "javac")
                "csharp" = @("dotnet")
                "typescript" = @("tsc")
            }

            foreach ($lang in $langChecks.Keys) {
                $detected = $false
                $compiler = $null
                $version = ""

                foreach ($cmd in $langChecks[$lang]) {
                    $command = Get-Command $cmd -ErrorAction SilentlyContinue
                    if ($command) {
                        $detected = $true
                        $compiler = $cmd
                        try {
                            if ($cmd -eq "go") {
                                $version = & $cmd version 2>&1 | Select-Object -First 1
                            }
                            else {
                                $version = & $cmd --version 2>&1 | Select-Object -First 1
                            }
                        }
                        catch {
                            $version = "Available"
                        }
                        break
                    }
                }

                # Special MASM detection
                if (-not $detected -and $lang -eq "masm") {
                    $vsPaths = @(
                        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
                        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
                        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
                        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
                    )

                    foreach ($vsPath in $vsPaths) {
                        if (Test-Path $vsPath) {
                            $msvcDirs = Get-ChildItem -Path $vsPath -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
                            foreach ($msvcDir in $msvcDirs) {
                                $ml64Path = Join-Path $msvcDir "bin\Hostx64\x64\ml64.exe"
                                $mlPath = Join-Path $msvcDir "bin\Hostx64\x86\ml.exe"

                                if (Test-Path $ml64Path) {
                                    $detected = $true
                                    $compiler = "ml64"
                                    $version = "Visual Studio MASM (64-bit) - $($msvcDir.Name)"
                                    break
                                }
                                elseif (Test-Path $mlPath) {
                                    $detected = $true
                                    $compiler = "ml"
                                    $version = "Visual Studio MASM (32-bit) - $($msvcDir.Name)"
                                    break
                                }
                            }
                            if ($detected) { break }
                        }
                    }
                }

                if ($detected) {
                    $languages[$lang] = @{
                        name = $lang
                        compiler = $compiler
                        version = $version.Trim()
                        available = $true
                    }
                }
            }

            return @{
                success = $true
                languages = $languages
                count = $languages.Count
                detected = $languages.Keys
            }
        }

        "create_project" {
            $projectName = $Parameters.project_name
            $language = $Parameters.language
            $directory = if ($Parameters.directory) { $Parameters.directory } else { $PWD }
            $useCli = if ($Parameters.use_cli) { $Parameters.use_cli } else { $true }

            $projectPath = Join-Path $directory $projectName

            if (Test-Path $projectPath) {
                return @{
                    success = $false
                    error = "Directory already exists: $projectPath"
                }
            }

            New-Item -ItemType Directory -Path $projectPath -Force | Out-Null
            $originalLocation = Get-Location
            Set-Location $projectPath

            $filesCreated = @()

            # Create basic project structure based on language
            switch ($language.ToLower()) {
                "python" {
                    Set-Content -Path "main.py" -Value @"
#!/usr/bin/env python3
def main():
    print("Hello from $projectName!")

if __name__ == "__main__":
    main()
"@
                    Set-Content -Path "requirements.txt" -Value ""
                    Set-Content -Path "README.md" -Value "# $projectName`nPython project created by agent"
                    $filesCreated = @("main.py", "requirements.txt", "README.md")
                }
                "nasm" {
                    Set-Content -Path "main.asm" -Value @"
; NASM Hello World
section .data
    msg db 'Hello from $projectName!', 0xA
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
                    Set-Content -Path "build.bat" -Value @"
@echo off
nasm -f win64 main.asm -o main.obj
gcc main.obj -o main.exe
main.exe
"@
                    Set-Content -Path "README.md" -Value "# $projectName`nNASM project created by agent"
                    $filesCreated = @("main.asm", "build.bat", "README.md")
                }
                "masm" {
                    Set-Content -Path "main.asm" -Value @"
; MASM Hello World
.code
main proc
    sub rsp, 28h
    mov rcx, -11
    call GetStdHandle
    mov rcx, rax
    lea rdx, msg
    mov r8, lengthof msg
    mov r9, 0
    push 0
    call WriteFile
    mov ecx, 0
    call ExitProcess
main endp

.data
    msg db 'Hello from $projectName!', 0Dh, 0Ah

end
"@
                    Set-Content -Path "build.bat" -Value @"
@echo off
ml64 /c main.asm
link /subsystem:console main.obj kernel32.lib /entry:main
main.exe
"@
                    Set-Content -Path "README.md" -Value "# $projectName`nMASM project created by agent"
                    $filesCreated = @("main.asm", "build.bat", "README.md")
                }
                default {
                    Set-Content -Path "main.$language" -Value "# $projectName`nCreated by agent"
                    Set-Content -Path "README.md" -Value "# $projectName`n$language project created by agent"
                    $filesCreated = @("main.$language", "README.md")
                }
            }

            Set-Location $originalLocation

            return @{
                success = $true
                project_name = $projectName
                path = $projectPath
                language = $language
                files_created = $filesCreated
            }
        }

        "git_init" {
            $repoPath = if ($Parameters.repository_path) { $Parameters.repository_path } else { $PWD }

            $originalLocation = Get-Location
            Set-Location $repoPath

            $result = git init 2>&1 | Out-String
            $hasGit = Test-Path ".git"

            Set-Location $originalLocation

            return @{
                success = $hasGit
                path = $repoPath
                output = $result
            }
        }

        "get_environment" {
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

        default {
            return @{
                success = $false
                error = "Tool not found: $ToolName"
            }
        }
    }
}

# Test Suite
Write-Host "🧪 Running Agent Tool Tests" -ForegroundColor Cyan
Write-Host "-" * 70 -ForegroundColor Gray
Write-Host ""

$testResults = @()
$testCount = 0
$passCount = 0

function Run-Test {
    param([string]$Name, [scriptblock]$Test)
    $global:testCount++
    Write-Host "[$global:testCount] $Name" -ForegroundColor Yellow

    try {
        $result = & $Test
        if ($result.success -ne $false) {
            Write-Host "  ✅ PASS" -ForegroundColor Green
            if ($result.count) { Write-Host "    Count: $($result.count)" -ForegroundColor Gray }
            if ($result.detected) { Write-Host "    Detected: $($result.detected -join ', ')" -ForegroundColor Gray }
            $global:passCount++
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

# Test 1: Detect Languages
Run-Test -Name "detect_languages" {
    Invoke-AgentTool-Simulated -ToolName "detect_languages"
}

# Test 2: Get Environment
Run-Test -Name "get_environment" {
    Invoke-AgentTool-Simulated -ToolName "get_environment"
}

# Test 3: Create Python Project
Run-Test -Name "create_project (Python)" {
    $testDir = Join-Path $PWD "agent-integration-tests"
    if (-not (Test-Path $testDir)) {
        New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    }

    $result = Invoke-AgentTool-Simulated -ToolName "create_project" -Parameters @{
        project_name = "test-python-agent"
        language = "python"
        directory = $testDir
    }

    # Verify files exist
    if ($result.success) {
        $allExist = $result.files_created | ForEach-Object {
            Test-Path (Join-Path $result.path $_)
        } | Where-Object { $_ -eq $true }

        $result.success = ($allExist.Count -eq $result.files_created.Count)
    }

    return $result
}

# Test 4: Create NASM Project
Run-Test -Name "create_project (NASM)" {
    $testDir = Join-Path $PWD "agent-integration-tests"

    $result = Invoke-AgentTool-Simulated -ToolName "create_project" -Parameters @{
        project_name = "test-nasm-agent"
        language = "nasm"
        directory = $testDir
    }

    if ($result.success) {
        $allExist = $result.files_created | ForEach-Object {
            Test-Path (Join-Path $result.path $_)
        } | Where-Object { $_ -eq $true }

        $result.success = ($allExist.Count -eq $result.files_created.Count)
    }

    return $result
}

# Test 5: Create MASM Project
Run-Test -Name "create_project (MASM)" {
    $testDir = Join-Path $PWD "agent-integration-tests"

    $result = Invoke-AgentTool-Simulated -ToolName "create_project" -Parameters @{
        project_name = "test-masm-agent"
        language = "masm"
        directory = $testDir
    }

    if ($result.success) {
        $allExist = $result.files_created | ForEach-Object {
            Test-Path (Join-Path $result.path $_)
        } | Where-Object { $_ -eq $true }

        $result.success = ($allExist.Count -eq $result.files_created.Count)
    }

    return $result
}

# Test 6: Git Init
if (Get-Command git -ErrorAction SilentlyContinue) {
    Run-Test -Name "git_init" {
        $testDir = Join-Path $PWD "agent-integration-tests"
        $repoPath = Join-Path $testDir "test-git-repo"

        if (Test-Path $repoPath) {
            Remove-Item -Path $repoPath -Recurse -Force
        }
        New-Item -ItemType Directory -Path $repoPath -Force | Out-Null

        $result = Invoke-AgentTool-Simulated -ToolName "git_init" -Parameters @{
            repository_path = $repoPath
        }

        return $result
    }
}

# Summary
Write-Host ""
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "📊 TEST SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

Write-Host "Total Tests: $testCount" -ForegroundColor White
Write-Host "Passed: $passCount" -ForegroundColor Green
Write-Host "Failed: $($testCount - $passCount)" -ForegroundColor $(if (($testCount - $passCount) -eq 0) { "Green" } else { "Red" })

if ($testCount -gt 0) {
    $passRate = [math]::Round(($passCount / $testCount) * 100, 2)
    Write-Host "Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -ge 90) { "Green" } elseif ($passRate -ge 70) { "Yellow" } else { "Red" })
}

# Show detected languages from first test
if ($testResults[0].Result.detected) {
    Write-Host ""
    Write-Host "Detected Languages: $($testResults[0].Result.count)" -ForegroundColor Cyan
    foreach ($lang in $testResults[0].Result.detected) {
        $langInfo = $testResults[0].Result.languages[$lang]
        Write-Host "  • $lang`: $($langInfo.compiler) - $($langInfo.version)" -ForegroundColor Gray
    }
}

# Cleanup
Write-Host ""
Write-Host "🧹 Cleaning up..." -ForegroundColor Yellow
$testDir = Join-Path $PWD "agent-integration-tests"
if (Test-Path $testDir) {
    Remove-Item -Path $testDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  ✅ Cleanup complete" -ForegroundColor Green
}

Write-Host ""
Write-Host "✅ Integration test complete!" -ForegroundColor Green

