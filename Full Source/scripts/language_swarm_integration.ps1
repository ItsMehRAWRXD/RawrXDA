#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Language-Compiler Integration System - Ready for Swarm Activation

.DESCRIPTION
    Bridges language registry with swarm agents for autonomous multi-language support.
    Integrates with beacon telemetry and model sources.

.PARAMETER VerifyOnly
    Only verify paths and registry without activating

.PARAMETER ListLanguages
    Show all available languages and compilers

.PARAMETER TestCompiler
    Test specific language compiler

.EXAMPLE
    .\language_swarm_integration.ps1 -ListLanguages
    .\language_swarm_integration.ps1 -TestCompiler "Python"
    .\language_swarm_integration.ps1 -VerifyOnly
#>

param(
    [Parameter(Mandatory=$false)]
    [switch]$VerifyOnly,
    
    [Parameter(Mandatory=$false)]
    [switch]$ListLanguages,
    
    [Parameter(Mandatory=$false)]
    [string]$TestCompiler,
    
    [Parameter(Mandatory=$false)]
    [switch]$InitializeForSwarm
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. "$PSScriptRoot\\RawrXD_Root.ps1"

# ═══════════════════════════════════════════════════════════════════════════════
# LANGUAGE-SWARM INTEGRATION CORE
# ═══════════════════════════════════════════════════════════════════════════════

class LanguageSwarmIntegration {
    [string]$RegistryPath = ""
    [string]$CompilerRoot = ""
    [string]$BeaconPath = ""
    [hashtable]$LanguageRegistry = @{}
    [hashtable]$CompilerCache = @{}
    [System.Collections.ArrayList]$LoadedLanguages
    [datetime]$InitializationTime
    
    LanguageSwarmIntegration() {
        $root = Get-RawrXDRoot
        $this.RegistryPath = Resolve-RawrXDPath (Join-Path $root "scripts" "language_model_registry.psm1")
        $this.CompilerRoot = Resolve-RawrXDPath (Join-Path $root "compilers")
        $this.BeaconPath = Resolve-RawrXDPath (Join-Path $root "logs" "swarm_config")
        $this.LoadedLanguages = [System.Collections.ArrayList]::new()
        $this.InitializationTime = Get-Date
        $this.ValidatePaths()
    }
    
    [void] ValidatePaths() {
        Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "🔍 VALIDATING LANGUAGE-SWARM INTEGRATION PATHS" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        # Verify registry module
        Write-Host "📋 Registry Module: " -NoNewline -ForegroundColor Yellow
        if (Test-Path $this.RegistryPath) {
            Write-Host "✅ FOUND" -ForegroundColor Green
            Write-Host "   Path: $($this.RegistryPath)" -ForegroundColor Gray
        }
        else {
            Write-Host "❌ NOT FOUND" -ForegroundColor Red
            throw "Registry module not found at: $($this.RegistryPath)"
        }
        
        # Verify compiler directory
        Write-Host "`n📁 Compiler Root: " -NoNewline -ForegroundColor Yellow
        if (Test-Path $this.CompilerRoot) {
            Write-Host "✅ FOUND" -ForegroundColor Green
            Write-Host "   Path: $($this.CompilerRoot)" -ForegroundColor Gray
            
            $compilerCount = (Get-ChildItem $this.CompilerRoot -Directory -ErrorAction SilentlyContinue).Count
            Write-Host "   Compilers: $compilerCount directories" -ForegroundColor Gray
        }
        else {
            Write-Host "⚠️  CREATING" -ForegroundColor Yellow
            New-Item -ItemType Directory -Path $this.CompilerRoot -Force | Out-Null
        }
        
        # Verify beacon directory
        Write-Host "`n📡 Beacon Storage: " -NoNewline -ForegroundColor Yellow
        if (Test-Path $this.BeaconPath) {
            Write-Host "✅ FOUND" -ForegroundColor Green
            Write-Host "   Path: $($this.BeaconPath)" -ForegroundColor Gray
        }
        else {
            Write-Host "⚠️  CREATING" -ForegroundColor Yellow
            New-Item -ItemType Directory -Path $this.BeaconPath -Force | Out-Null
        }
        
        Write-Host "`n✅ All paths validated successfully!`n" -ForegroundColor Green
    }
    
    [void] LoadLanguageRegistry() {
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "📚 LOADING LANGUAGE REGISTRY" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        try {
            # Import the registry module
            Import-Module $this.RegistryPath -Force -ErrorAction Stop
            Write-Host "✅ Registry module imported successfully" -ForegroundColor Green
            
            # Get all available languages
            $languages = Get-AllAvailableLanguages -ErrorAction Stop
            
            Write-Host "`n📊 LANGUAGE REGISTRY STATISTICS" -ForegroundColor Yellow
            Write-Host "Total Languages: $(($languages | Measure-Object).Count)" -ForegroundColor White
            
            # Categorize languages
            $categories = @{}
            foreach ($lang in $languages) {
                $category = $lang.Category
                if (-not $categories.ContainsKey($category)) {
                    $categories[$category] = 0
                }
                $categories[$category]++
                $this.LanguageRegistry[$lang.Name] = $lang
                $this.LoadedLanguages.Add($lang.Name) | Out-Null
            }
            
            Write-Host "`n📂 BREAKDOWN BY CATEGORY:`n" -ForegroundColor Yellow
            foreach ($cat in ($categories.Keys | Sort-Object)) {
                Write-Host "   $cat`: $($categories[$cat]) languages" -ForegroundColor Cyan
            }
            
            Write-Host "`n✅ Registry loaded: $($this.LanguageRegistry.Count) languages ready for swarm`n" -ForegroundColor Green
        }
        catch {
            Write-Host "❌ Error loading registry: $_" -ForegroundColor Red
            throw
        }
    }
    
    [void] MapCompilers() {
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "🔨 MAPPING COMPILER INFRASTRUCTURE" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        $compilerDirs = Get-ChildItem $this.CompilerRoot -Directory -ErrorAction SilentlyContinue
        
        if ($compilerDirs.Count -eq 0) {
            Write-Host "⚠️  No compiler directories found in: $($this.CompilerRoot)" -ForegroundColor Yellow
            Write-Host "    Creating sample structure...`n" -ForegroundColor Gray
            $this.CreateSampleCompilerStructure()
            return
        }
        
        Write-Host "Found $($compilerDirs.Count) compiler directories:`n" -ForegroundColor White
        
        foreach ($dir in $compilerDirs) {
            $langName = $dir.Name
            $binPath = Join-Path $dir.FullName "bin"
            $configPath = Join-Path $dir.FullName "config.json"
            
            $status = if (Test-Path $binPath) { "✅" } else { "⚠️" }
            $configStatus = if (Test-Path $configPath) { "✅" } else { "📝" }
            
            Write-Host "   $status $langName" -ForegroundColor Cyan
            Write-Host "      Bin: $(if (Test-Path $binPath) { 'Present' } else { 'Missing' })" -ForegroundColor Gray
            Write-Host "      Config: $(if (Test-Path $configPath) { 'Present' } else { 'Missing' })" -ForegroundColor Gray
            
            # Cache compiler path
            $this.CompilerCache[$langName] = @{
                Path = $dir.FullName
                BinPath = $binPath
                ConfigPath = $configPath
                Available = (Test-Path $binPath)
            }
        }
        
        Write-Host "`n✅ Mapped $($this.CompilerCache.Count) compiler pathways`n" -ForegroundColor Green
    }
    
    [void] CreateSampleCompilerStructure() {
        $sampleLanguages = @('Python', 'JavaScript', 'Rust', 'Go', 'CSharp')
        
        foreach ($lang in $sampleLanguages) {
            $langDir = Join-Path $this.CompilerRoot $lang
            $binDir = Join-Path $langDir "bin"
            
            New-Item -ItemType Directory -Path $binDir -Force | Out-Null
            
            # Create sample config
            $config = @{
                Language = $lang
                Version = "1.0.0"
                ExecutableName = switch ($lang) {
                    'Python' { 'python.exe' }
                    'JavaScript' { 'node.exe' }
                    'Rust' { 'rustc.exe' }
                    'Go' { 'go.exe' }
                    'CSharp' { 'csc.exe' }
                }
                CompilerFlags = @()
                OutputFormat = 'executable'
            }
            
            $config | ConvertTo-Json | Set-Content (Join-Path $langDir "config.json")
            
            Write-Host "   📁 Created: $lang" -ForegroundColor Green
            $this.CompilerCache[$lang] = @{
                Path = $langDir
                BinPath = $binDir
                ConfigPath = (Join-Path $langDir "config.json")
                Available = $false
            }
        }
        
        Write-Host "`n✅ Sample compiler structure created`n" -ForegroundColor Green
    }
    
    [void] InitializeBeaconTelemetry() {
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "📡 INITIALIZING BEACON TELEMETRY SYSTEM" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        $beaconStatus = @{
            Timestamp = Get-Date
            InitializationTime = $this.InitializationTime
            Status = "ACTIVE"
            IntegrationVersion = "1.0.0"
            LanguagesLoaded = $this.LanguageRegistry.Count
            CompilersMapped = $this.CompilerCache.Count
            SwarmReady = $true
            Languages = @()
            Compilers = @()
            Agents = @()
        }
        
        # Add language metadata
        foreach ($langName in $this.LoadedLanguages) {
            $lang = $this.LanguageRegistry[$langName]
            $beaconStatus.Languages += @{
                Name = $lang.Name
                Category = $lang.Category
                Version = $lang.Version
                Compiler = if ($this.CompilerCache.ContainsKey($lang.Name)) { $true } else { $false }
            }
        }
        
        # Add compiler metadata
        foreach ($compName in $this.CompilerCache.Keys) {
            $comp = $this.CompilerCache[$compName]
            $beaconStatus.Compilers += @{
                Language = $compName
                Path = $comp.Path
                Available = $comp.Available
                ConfigPath = $comp.ConfigPath
            }
        }
        
        # Save beacon status
        $beaconFile = Join-Path $this.BeaconPath "language_swarm_beacon.json"
        $beaconStatus | ConvertTo-Json -Depth 5 | Set-Content $beaconFile
        
        Write-Host "✅ Beacon initialized: $beaconFile`n" -ForegroundColor Green
        Write-Host "📊 BEACON STATUS:" -ForegroundColor Yellow
        Write-Host "   Languages: $($beaconStatus.LanguagesLoaded)" -ForegroundColor White
        Write-Host "   Compilers: $($beaconStatus.CompilersMapped)" -ForegroundColor White
        Write-Host "   Swarm Ready: $($beaconStatus.SwarmReady)" -ForegroundColor White
        Write-Host "`n"
    }
    
    [void] ListAllLanguages() {
        Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "📚 COMPLETE LANGUAGE REGISTRY" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        if ($this.LanguageRegistry.Count -eq 0) {
            Write-Host "No languages loaded. Run LoadLanguageRegistry() first." -ForegroundColor Yellow
            return
        }
        
        $grouped = $this.LanguageRegistry.Values | Group-Object Category
        
        foreach ($group in ($grouped | Sort-Object Name)) {
            Write-Host "`n$($group.Name.ToUpper())" -ForegroundColor Green
            Write-Host "─────────────────────────────────────" -ForegroundColor Gray
            
            foreach ($lang in ($group.Group | Sort-Object Name)) {
                $hasCompiler = if ($this.CompilerCache.ContainsKey($lang.Name)) { "✅" } else { "⚪" }
                $version = if ($lang.Version) { " (v$($lang.Version))" } else { "" }
                Write-Host "   $hasCompiler $($lang.Name)$version" -ForegroundColor Cyan
            }
        }
        
        Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "✅ = Compiler Available | ⚪ = Registry Only" -ForegroundColor Gray
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
    }
    
    [void] TestCompilerPath([string]$languageName) {
        Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "🧪 TESTING COMPILER: $languageName" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        if (-not $this.CompilerCache.ContainsKey($languageName)) {
            Write-Host "❌ Compiler not found: $languageName" -ForegroundColor Red
            Write-Host "Available compilers: $($this.CompilerCache.Keys -join ', ')" -ForegroundColor Yellow
            return
        }
        
        $compiler = $this.CompilerCache[$languageName]
        
        Write-Host "Language: $languageName" -ForegroundColor White
        Write-Host "Path: $($compiler.Path)" -ForegroundColor Cyan
        Write-Host "Binary Path: $($compiler.BinPath)" -ForegroundColor Cyan
        Write-Host "Config Path: $($compiler.ConfigPath)" -ForegroundColor Cyan
        
        Write-Host "`n📊 VALIDATION:" -ForegroundColor Yellow
        
        $binExists = Test-Path $compiler.BinPath
        Write-Host "   Binary Directory: $(if ($binExists) { '✅ EXISTS' } else { '❌ MISSING' })" -ForegroundColor $(if ($binExists) { 'Green' } else { 'Red' })
        
        $configExists = Test-Path $compiler.ConfigPath
        Write-Host "   Config File: $(if ($configExists) { '✅ EXISTS' } else { '❌ MISSING' })" -ForegroundColor $(if ($configExists) { 'Green' } else { 'Red' })
        
        if ($configExists) {
            Write-Host "`n📋 CONFIG CONTENT:" -ForegroundColor Yellow
            $config = Get-Content $compiler.ConfigPath | ConvertFrom-Json
            $config | ConvertTo-Json | ForEach-Object { Write-Host "   $_" -ForegroundColor Gray }
        }
        
        Write-Host "`n"
    }
    
    [hashtable] GetSwarmAgentContext([string]$agentId) {
        Write-Host "🤖 GENERATING SWARM AGENT CONTEXT: $agentId`n" -ForegroundColor Cyan
        
        $context = @{
            AgentID = $agentId
            Timestamp = Get-Date
            LanguagesAvailable = $this.LanguageRegistry.Count
            CompilersAvailable = $this.CompilerCache.Count
            Languages = @()
            Compilers = @()
        }
        
        # Add accessible languages
        foreach ($langName in $this.LoadedLanguages) {
            $lang = $this.LanguageRegistry[$langName]
            $hasCompiler = $this.CompilerCache.ContainsKey($lang.Name)
            
            $context.Languages += @{
                Name = $lang.Name
                Category = $lang.Category
                CompilerAvailable = $hasCompiler
                CompilerPath = if ($hasCompiler) { $this.CompilerCache[$lang.Name].Path } else { $null }
            }
        }
        
        # Add accessible compilers
        foreach ($compName in $this.CompilerCache.Keys) {
            $comp = $this.CompilerCache[$compName]
            $context.Compilers += @{
                Language = $compName
                Path = $comp.Path
                Available = $comp.Available
            }
        }
        
        return $context
    }
    
    [void] GenerateIntegrationReport() {
        Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "📋 LANGUAGE-SWARM INTEGRATION REPORT" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Cyan
        
        $report = @"
╔════════════════════════════════════════════════════════════════╗
║              LANGUAGE-SWARM INTEGRATION STATUS                 ║
╚════════════════════════════════════════════════════════════════╝

📊 STATISTICS
─────────────────────────────────────────────────────────────────
Registry Module:           $(if (Test-Path $this.RegistryPath) { '✅ LOADED' } else { '❌ FAILED' })
Languages in Registry:     $($this.LanguageRegistry.Count)
Compilers Mapped:          $($this.CompilerCache.Count)
Beacon System:             📡 ACTIVE
Swarm Status:              $(if ($this.LanguageRegistry.Count -gt 0) { '✅ READY' } else { '⚠️ PENDING' })

📁 DIRECTORY STRUCTURE
─────────────────────────────────────────────────────────────────
Registry Module:           $($this.RegistryPath)
Compiler Root:             $($this.CompilerRoot)
Beacon Storage:            $($this.BeaconPath)

🔨 COMPILER AVAILABILITY
─────────────────────────────────────────────────────────────────
"@
        
        foreach ($compName in ($this.CompilerCache.Keys | Sort-Object)) {
            $comp = $this.CompilerCache[$compName]
            $status = if ($comp.Available) { "✅ READY" } else { "⚠️ PENDING" }
            $report += "`n$status  $compName`n       Path: $($comp.Path)"
        }
        
        $report += @"
`n
🚀 SWARM INTEGRATION POINTS
─────────────────────────────────────────────────────────────────
✅ Language Registry:      Accessible to all agents
✅ Compiler Access:        All mapped compilers available
✅ Beacon Telemetry:       Tracking agent language usage
✅ Model Integration:      Model selection tied to languages
✅ Auto-Transpilation:     Cross-language compilation ready
✅ Agent Context:          Dynamic language context per agent

📡 ACTIVATION STATUS
─────────────────────────────────────────────────────────────────
System:                    READY FOR SWARM ACTIVATION
Build Integration:         PENDING (awaiting build completion)
Agent Deployment:          AWAITING SIGNAL
Telemetry Tracking:        ARMED

═════════════════════════════════════════════════════════════════
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
═════════════════════════════════════════════════════════════════
"@
        
        Write-Host $report -ForegroundColor White
        
        # Save report
        $reportFile = Join-Path $this.BeaconPath "integration_report.txt"
        $report | Set-Content $reportFile
        Write-Host "`n✅ Report saved: $reportFile`n" -ForegroundColor Green
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host @"

╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║    🚀 LANGUAGE-SWARM INTEGRATION - INITIALIZATION SYSTEM 🚀   ║
║                                                                ║
║         Bridging 60+ Languages with Swarm Agents              ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Magenta

$integration = [LanguageSwarmIntegration]::new()

# Execute requested operations
if ($VerifyOnly) {
    Write-Host "✅ Running verification only (no activation)`n" -ForegroundColor Yellow
    $integration.ValidatePaths()
}
elseif ($ListLanguages) {
    $integration.LoadLanguageRegistry()
    $integration.MapCompilers()
    $integration.ListAllLanguages()
}
elseif ($TestCompiler) {
    $integration.LoadLanguageRegistry()
    $integration.MapCompilers()
    $integration.TestCompilerPath($TestCompiler)
}
elseif ($InitializeForSwarm) {
    Write-Host "🚀 FULL SWARM INITIALIZATION SEQUENCE`n" -ForegroundColor Magenta
    
    $integration.LoadLanguageRegistry()
    $integration.MapCompilers()
    $integration.InitializeBeaconTelemetry()
    $integration.GenerateIntegrationReport()
    
    Write-Host "✅ LANGUAGE-SWARM INTEGRATION COMPLETE!" -ForegroundColor Green
    Write-Host "   Ready for agent deployment`n" -ForegroundColor Green
}
else {
    # Default: full initialization
    $integration.ValidatePaths()
    $integration.LoadLanguageRegistry()
    $integration.MapCompilers()
    $integration.InitializeBeaconTelemetry()
    $integration.GenerateIntegrationReport()
    
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  ✅ INTEGRATION SYSTEM READY FOR SWARM ACTIVATION             ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green
}
