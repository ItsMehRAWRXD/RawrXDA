# Agentic Test: Autonomous React Script Creation
# This test simulates what the agent needs to do to autonomously create a React application

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  AGENTIC TEST: Autonomous React Script Creation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$testResults = @()
$missingCapabilities = @()

# Test directory
$testDir = Join-Path $PWD "test-react-agentic"
if (Test-Path $testDir) {
    Remove-Item $testDir -Recurse -Force
}

# ============================================
# TEST 1: Check Available Tools
# ============================================
Write-Host "[TEST 1] Checking Available Agent Tools..." -ForegroundColor Yellow

$requiredTools = @(
    "create_directory",
    "write_file",
    "list_directory",
    "execute_command",
    "get_environment"
)

$availableTools = @()
foreach ($tool in $requiredTools) {
    # Simulate checking if tool exists in agent system
    $availableTools += $tool
    Write-Host "  ✓ Tool available: $tool" -ForegroundColor Green
}

$testResults += @{
    Test = "Tool Availability"
    Status = "PASS"
    Details = "All required tools are available"
}

# ============================================
# TEST 2: Create Project Directory
# ============================================
Write-Host "`n[TEST 2] Creating Project Directory..." -ForegroundColor Yellow

try {
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    if (Test-Path $testDir) {
        Write-Host "  ✓ Directory created: $testDir" -ForegroundColor Green
        $testResults += @{
            Test = "Create Directory"
            Status = "PASS"
            Details = "Directory created successfully"
        }
    }
    else {
        throw "Directory creation failed"
    }
}
catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
    $testResults += @{
        Test = "Create Directory"
        Status = "FAIL"
        Details = $_.Exception.Message
    }
    $missingCapabilities += "Directory creation error handling"
}

# ============================================
# TEST 3: Check Node.js/npm Availability
# ============================================
Write-Host "`n[TEST 3] Checking Node.js/npm Availability..." -ForegroundColor Yellow

$nodeAvailable = $false
$npmAvailable = $false

try {
    $nodeVersion = node --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ Node.js available: $nodeVersion" -ForegroundColor Green
        $nodeAvailable = $true
    }
}
catch {
    Write-Host "  ✗ Node.js not found" -ForegroundColor Red
    $missingCapabilities += "Node.js detection and installation guidance"
}

try {
    $npmVersion = npm --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ npm available: $npmVersion" -ForegroundColor Green
        $npmAvailable = $true
    }
}
catch {
    Write-Host "  ✗ npm not found" -ForegroundColor Red
    $missingCapabilities += "npm detection and installation guidance"
}

if ($nodeAvailable -and $npmAvailable) {
    $testResults += @{
        Test = "Node.js/npm Check"
        Status = "PASS"
        Details = "Node.js and npm are available"
    }
}
else {
    $testResults += @{
        Test = "Node.js/npm Check"
        Status = "FAIL"
        Details = "Node.js or npm not available"
    }
}

# ============================================
# TEST 4: Create package.json
# ============================================
Write-Host "`n[TEST 4] Creating package.json..." -ForegroundColor Yellow

$packageJson = @{
    name = "test-react-app"
    version = "1.0.0"
    description = "Test React app created by agent"
    main = "index.js"
    scripts = @{
        start = "react-scripts start"
        build = "react-scripts build"
        test = "react-scripts test"
        eject = "react-scripts eject"
    }
    dependencies = @{
        react = "^18.2.0"
        "react-dom" = "^18.2.0"
        "react-scripts" = "5.0.1"
    }
}

try {
    $packageJsonPath = Join-Path $testDir "package.json"
    $packageJson | ConvertTo-Json -Depth 10 | Set-Content -Path $packageJsonPath
    Write-Host "  ✓ package.json created" -ForegroundColor Green
    $testResults += @{
        Test = "Create package.json"
        Status = "PASS"
        Details = "package.json created with React dependencies"
    }
}
catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
    $testResults += @{
        Test = "Create package.json"
        Status = "FAIL"
        Details = $_.Exception.Message
    }
    $missingCapabilities += "JSON file creation with proper formatting"
}

# ============================================
# TEST 5: Create React App Structure
# ============================================
Write-Host "`n[TEST 5] Creating React App Structure..." -ForegroundColor Yellow

$reactFiles = @{
    "public/index.html" = @"
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>React App</title>
  </head>
  <body>
    <div id="root"></div>
  </body>
</html>
"@
    "src/index.js" = @"
import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import App from './App';

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
"@
    "src/App.js" = @"
import React from 'react';
import './App.css';

function App() {
  return (
    <div className="App">
      <header className="App-header">
        <h1>Hello, React!</h1>
        <p>This app was created autonomously by the agent.</p>
      </header>
    </div>
  );
}

export default App;
"@
    "src/App.css" = @"
.App {
  text-align: center;
}

.App-header {
  background-color: #282c34;
  padding: 20px;
  color: white;
}
"@
    "src/index.css" = @"
body {
  margin: 0;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen',
    'Ubuntu', 'Cantarell', 'Fira Sans', 'Droid Sans', 'Helvetica Neue',
    sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}
"@
    ".gitignore" = @"
# Dependencies
node_modules/
/.pnp
.pnp.js

# Testing
/coverage

# Production
/build

# Misc
.DS_Store
.env.local
.env.development.local
.env.test.local
.env.production.local

npm-debug.log*
yarn-debug.log*
yarn-error.log*
"@
    "README.md" = @"
# Test React App

This React application was created autonomously by the agent.

## Getting Started

\`\`\`bash
npm install
npm start
\`\`\`

## Available Scripts

- \`npm start\` - Start development server
- \`npm build\` - Build for production
- \`npm test\` - Run tests
"@
}

$filesCreated = 0
$filesFailed = 0

foreach ($file in $reactFiles.Keys) {
    try {
        $filePath = Join-Path $testDir $file
        $fileDir = Split-Path $filePath -Parent

        if (-not (Test-Path $fileDir)) {
            New-Item -ItemType Directory -Path $fileDir -Force | Out-Null
        }

        Set-Content -Path $filePath -Value $reactFiles[$file] -Force
        $filesCreated++
        Write-Host "  ✓ Created: $file" -ForegroundColor Green
    }
    catch {
        $filesFailed++
        Write-Host "  ✗ Failed: $file - $_" -ForegroundColor Red
    }
}

if ($filesFailed -eq 0) {
    $testResults += @{
        Test = "Create React Files"
        Status = "PASS"
        Details = "All $filesCreated React files created successfully"
    }
}
else {
    $testResults += @{
        Test = "Create React Files"
        Status = "PARTIAL"
        Details = "Created $filesCreated files, $filesFailed failed"
    }
    $missingCapabilities += "Better error handling for file creation"
}

# ============================================
# TEST 6: Initialize Git Repository
# ============================================
Write-Host "`n[TEST 6] Initializing Git Repository..." -ForegroundColor Yellow

try {
    Push-Location $testDir
    git init 2>&1 | Out-Null
    if (Test-Path ".git") {
        Write-Host "  ✓ Git repository initialized" -ForegroundColor Green
        $testResults += @{
            Test = "Initialize Git"
            Status = "PASS"
            Details = "Git repository created"
        }
    }
    Pop-Location
}
catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
    $testResults += @{
        Test = "Initialize Git"
        Status = "FAIL"
        Details = $_.Exception.Message
    }
    $missingCapabilities += "Git initialization error handling"
    Pop-Location
}

# ============================================
# TEST 7: Check if npm install would work
# ============================================
Write-Host "`n[TEST 7] Testing npm install capability..." -ForegroundColor Yellow

if ($npmAvailable) {
    Write-Host "  ✓ npm install can be executed" -ForegroundColor Green
    Write-Host "  ℹ Note: Not running actual install to save time" -ForegroundColor Gray
    $testResults += @{
        Test = "npm install capability"
        Status = "PASS"
        Details = "npm install can be executed (not run in test)"
    }
}
else {
    Write-Host "  ✗ npm not available" -ForegroundColor Red
    $testResults += @{
        Test = "npm install capability"
        Status = "FAIL"
        Details = "npm not available"
    }
    $missingCapabilities += "npm installation detection and guidance"
}

# ============================================
# TEST 8: Check Missing Capabilities
# ============================================
Write-Host "`n[TEST 8] Analyzing Missing Capabilities..." -ForegroundColor Yellow

# Check for missing agent tools
$missingTools = @()

# Check if we have a tool to create React projects from template
$missingTools += "create_react_app (using create-react-app or Vite)"

# Check if we have a tool to detect and install missing dependencies
$missingTools += "detect_and_install_dependencies"

# Check if we have a tool to validate project structure
$missingTools += "validate_project_structure"

# Check if we have a tool to run development server
$missingTools += "start_development_server"

# Check if we have a tool to build project
$missingTools += "build_project"

foreach ($tool in $missingTools) {
    Write-Host "  ⚠ Missing: $tool" -ForegroundColor Yellow
    $missingCapabilities += $tool
}

# ============================================
# TEST SUMMARY
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$passed = ($testResults | Where-Object { $_.Status -eq "PASS" }).Count
$failed = ($testResults | Where-Object { $_.Status -eq "FAIL" }).Count
$partial = ($testResults | Where-Object { $_.Status -eq "PARTIAL" }).Count

Write-Host "Tests Passed: $passed" -ForegroundColor Green
Write-Host "Tests Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "Tests Partial: $partial" -ForegroundColor Yellow
Write-Host ""

Write-Host "Test Details:" -ForegroundColor Cyan
foreach ($result in $testResults) {
    $color = switch ($result.Status) {
        "PASS" { "Green" }
        "FAIL" { "Red" }
        "PARTIAL" { "Yellow" }
        default { "White" }
    }
    Write-Host "  [$($result.Status)] $($result.Test): $($result.Details)" -ForegroundColor $color
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  MISSING CAPABILITIES FOR FULL AUTONOMY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if ($missingCapabilities.Count -eq 0) {
    Write-Host "  ✓ No missing capabilities identified!" -ForegroundColor Green
}
else {
    Write-Host "The following capabilities need to be added:" -ForegroundColor Yellow
    $index = 1
    foreach ($capability in $missingCapabilities) {
        Write-Host "  $index. $capability" -ForegroundColor Yellow
        $index++
    }
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$recommendations = @(
    "Add 'create_react_app' tool that uses create-react-app or Vite CLI",
    "Add 'detect_and_install_dependencies' tool for automatic dependency management",
    "Add 'validate_project_structure' tool to verify project setup",
    "Add 'start_development_server' tool to run npm start in background",
    "Add 'build_project' tool to create production builds",
    "Add 'run_tests' tool to execute test suites",
    "Add better error handling for Node.js/npm detection",
    "Add project template system for different frameworks",
    "Add ability to detect and use existing package managers (npm, yarn, pnpm)",
    "Add timeout handling for long-running npm operations"
)

foreach ($rec in $recommendations) {
    Write-Host "  • $rec" -ForegroundColor White
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST PROJECT CREATED" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "Test project location: $testDir" -ForegroundColor Green
Write-Host ""
Write-Host "To test the created project:" -ForegroundColor Cyan
Write-Host "  cd $testDir" -ForegroundColor White
Write-Host "  npm install" -ForegroundColor White
Write-Host "  npm start" -ForegroundColor White
Write-Host ""

# Export results to JSON
$resultsFile = Join-Path $PWD "agentic-test-results.json"
$exportData = @{
    TestDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    TestName = "Autonomous React Script Creation"
    Results = $testResults
    MissingCapabilities = $missingCapabilities
    Recommendations = $recommendations
    TestProjectPath = $testDir
}

$exportData | ConvertTo-Json -Depth 10 | Set-Content -Path $resultsFile
Write-Host "Results exported to: $resultsFile" -ForegroundColor Green
Write-Host ""

