#Requires -Version 7.0
<#
.SYNOPSIS
    Comprehensive Analysis Tool for Extracted Source Code
.DESCRIPTION
    Analyzes extracted Electron app source code to identify:
    - Architecture and entry points
    - Dependencies and their versions
    - Configuration files
    - Extension system
    - Build system
    - Key modules and components
    - Security configurations
    - API endpoints
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDirectory,
    
    [string]$OutputFile = "source_analysis_report.json",
    
    [switch]$AnalyzeDependencies,
    [switch]$AnalyzeExtensions,
    [switch]$AnalyzeConfiguration,
    [switch]$AnalyzeBuildSystem,
    [switch]$AnalyzeSecurity,
    [switch]$ExtractAPIs,
    [switch]$GenerateReport,
    [switch]$ShowProgress,
    [switch]$DeepAnalysis,
    [switch]$IncludeStats
)

# Color codes
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Architecture = "DarkCyan"
    Dependency = "DarkGreen"
    Config = "DarkYellow"
    Security = "DarkRed"
    Extension = "Blue"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Type = "Info"
    )
    $color = $Colors[$Type]
    if (-not $color) { $color = "White" }
    Write-Host $Message -ForegroundColor $color
}

function Write-ProgressBar {
    param(
        [string]$Activity,
        [string]$Status,
        [int]$PercentComplete,
        [string]$CurrentOperation = ""
    )
    if ($ShowProgress) {
        Write-Progress -Activity $Activity -Status $Status -PercentComplete $percentComplete -CurrentOperation $currentOperation
    }
}

function Get-SourceStats {
    param([string]$Dir)
    
    Write-ColorOutput "→ Calculating source statistics" "Detail"
    
    $stats = @{
        TotalFiles = 0
        TotalSize = 0
        FileTypes = @{}
        LargestFiles = @()
        DirectoryStructure = @{}
    }
    
    try {
        $allFiles = Get-ChildItem -Path $Dir -Recurse -File -ErrorAction SilentlyContinue
        
        $stats.TotalFiles = $allFiles.Count
        $stats.TotalSize = ($allFiles | Measure-Object -Property Length -Sum).Sum
        
        # Group by extension
        $fileGroups = $allFiles | Group-Object -Property Extension
        foreach ($group in $fileGroups) {
            $ext = if ($group.Name) { $group.Name } else { "(no extension)" }
            $stats.FileTypes[$ext] = @{
                Count = $group.Count
                Size = ($group.Group | Measure-Object -Property Length -Sum).Sum
            }
        }
        
        # Find largest files
        $stats.LargestFiles = $allFiles | Sort-Object -Property Length -Descending | Select-Object -First 10 | ForEach-Object {
            @{
                Name = $_.Name
                Path = $_.FullName.Substring($Dir.Length).TrimStart('\', '/')
                Size = $_.Length
                Extension = $_.Extension
            }
        }
        
        # Analyze directory structure
        $dirStructure = @{}
        foreach ($file in $allFiles) {
            $relativeDir = Split-Path $file.FullName.Substring($Dir.Length).TrimStart('\', '/') -Parent
            if (-not $dirStructure[$relativeDir]) {
                $dirStructure[$relativeDir] = @{ FileCount = 0; TotalSize = 0 }
            }
            $dirStructure[$relativeDir].FileCount++
            $dirStructure[$relativeDir].TotalSize += $file.Length
        }
        $stats.DirectoryStructure = $dirStructure
        
        Write-ColorOutput "✓ Calculated statistics for $($stats.TotalFiles) files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to calculate statistics: $_" "Warning"
    }
    
    return $stats
}

function Analyze-PackageJson {
    param([string]$PackagePath)
    
    Write-ColorOutput "→ Analyzing package.json: $PackagePath" "Config"
    
    $analysis = @{
        Name = ""
        Version = ""
        Description = ""
        Main = ""
        Type = ""
        Scripts = @{}
        Dependencies = @{}
        DevDependencies = @{}
        Engines = @{}
        Keywords = @()
        Author = ""
        License = ""
        Repository = ""
        Bugs = ""
        Homepage = ""
        Config = @{}
        Extensions = @()
        Contributes = @{}
    }
    
    try {
        $package = Get-Content $PackagePath -Raw | ConvertFrom-Json
        
        $analysis.Name = $package.name
        $analysis.Version = $package.version
        $analysis.Description = $package.description
        $analysis.Main = $package.main
        $analysis.Type = $package.type
        $analysis.Author = if ($package.author) { $package.author.ToString() } else { "" }
        $analysis.License = $package.license
        $analysis.Homepage = $package.homepage
        $analysis.Repository = if ($package.repository) { $package.repository.ToString() } else { "" }
        $analysis.Bugs = if ($package.bugs) { $package.bugs.ToString() } else { "" }
        $analysis.Keywords = $package.keywords
        
        if ($package.scripts) {
            $analysis.Scripts = $package.scripts
        }
        
        if ($package.dependencies) {
            $analysis.Dependencies = $package.dependencies
        }
        
        if ($package.devDependencies) {
            $analysis.DevDependencies = $package.devDependencies
        }
        
        if ($package.engines) {
            $analysis.Engines = $package.engines
        }
        
        if ($package.config) {
            $analysis.Config = $package.config
        }
        
        # VS Code/Cursor specific
        if ($package.extensionPack) {
            $analysis.Extensions = $package.extensionPack
        }
        
        if ($package.contributes) {
            $analysis.Contributes = $package.contributes
        }
        
        Write-ColorOutput "✓ Analyzed package: $($analysis.Name) v$($analysis.Version)" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze package.json: $_" "Warning"
    }
    
    return $analysis
}

function Find-AllPackages {
    param([string]$Dir)
    
    Write-ColorOutput "→ Finding all package.json files" "Detail"
    
    $packages = @()
    
    try {
        $packageFiles = Get-ChildItem -Path $Dir -Recurse -Filter "package.json" -ErrorAction SilentlyContinue
        
        $totalPackages = $packageFiles.Count
        $currentPackage = 0
        
        foreach ($packageFile in $packageFiles) {
            $currentPackage++
            $percentComplete = [math]::Round(($currentPackage / $totalPackages) * 100, 1)
            
            $relativePath = $packageFile.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Analyzing Packages" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            $packageAnalysis = Analyze-PackageJson -PackagePath $packageFile.FullName
            $packageAnalysis | Add-Member -NotePropertyName "Path" -NotePropertyValue $relativePath
            $packageAnalysis | Add-Member -NotePropertyName "FullPath" -NotePropertyValue $packageFile.FullName
            
            $packages += $packageAnalysis
        }
        
        Write-ProgressBar -Activity "Analyzing Packages" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Found and analyzed $($packages.Count) package.json files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to find packages: $_" "Warning"
    }
    
    return $packages
}

function Analyze-Dependencies {
    param($Packages)
    
    Write-ColorOutput "=== ANALYZING DEPENDENCIES ===" "Header"
    
    $dependencyAnalysis = @{
        AllDependencies = @{}
        DependencyTree = @{}
        TopLevelDependencies = @()
        DevDependencies = @()
        OutdatedDependencies = @()
        SecurityIssues = @()
        TotalCount = 0
        DevCount = 0
    }
    
    try {
        $allDeps = @{}
        $allDevDeps = @{}
        
        foreach ($package in $Packages) {
            # Collect all dependencies
            foreach ($dep in $package.Dependencies.PSObject.Properties) {
                if (-not $allDeps[$dep.Name]) {
                    $allDeps[$dep.Name] = @{
                        Versions = [System.Collections.Generic.HashSet[string]]::new()
                        UsedBy = [System.Collections.Generic.List[string]]::new()
                    }
                }
                $allDeps[$dep.Name].Versions.Add($dep.Value) | Out-Null
                $allDeps[$dep.Name].UsedBy.Add($package.Name) | Out-Null
            }
            
            # Collect all dev dependencies
            foreach ($dep in $package.DevDependencies.PSObject.Properties) {
                if (-not $allDevDeps[$dep.Name]) {
                    $allDevDeps[$dep.Name] = @{
                        Versions = [System.Collections.Generic.HashSet[string]]::new()
                        UsedBy = [System.Collections.Generic.List[string]]::new()
                    }
                }
                $allDevDeps[$dep.Name].Versions.Add($dep.Value) | Out-Null
                $allDevDeps[$dep.Name].UsedBy.Add($package.Name) | Out-Null
            }
        }
        
        # Convert to arrays for reporting
        $dependencyAnalysis.AllDependencies = $allDeps
        $dependencyAnalysis.DependencyTree = @{
            Dependencies = $allDeps
            DevDependencies = $allDevDeps
        }
        
        $dependencyAnalysis.TopLevelDependencies = $allDeps.Keys | ForEach-Object {
            $versions = $allDeps[$_].Versions -join ", "
            $usedBy = $allDeps[$_].UsedBy -join ", "
            "$_`: $versions (used by: $usedBy)"
        }
        
        $dependencyAnalysis.DevDependencies = $allDevDeps.Keys | ForEach-Object {
            $versions = $allDevDeps[$_].Versions -join ", "
            $usedBy = $allDevDeps[$_].UsedBy -join ", "
            "$_`: $versions (used by: $usedBy)"
        }
        
        $dependencyAnalysis.TotalCount = $allDeps.Count
        $dependencyAnalysis.DevCount = $allDevDeps.Count
        
        Write-ColorOutput "✓ Analyzed $($dependencyAnalysis.TotalCount) dependencies and $($dependencyAnalysis.DevCount) dev dependencies" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze dependencies: $_" "Warning"
    }
    
    return $dependencyAnalysis
}

function Analyze-Extensions {
    param([string]$Dir)
    
    Write-ColorOutput "=== ANALYZING EXTENSIONS ===" "Header"
    
    $extensionAnalysis = @{
        BuiltInExtensions = @()
        ExtensionCount = 0
        ExtensionCategories = @{}
        TopExtensions = @()
    }
    
    try {
        # Look for extensions directory
        $extensionsDirs = @(
            Join-Path $Dir "extensions",
            Join-Path $Dir "resources" "app" "extensions",
            Join-Path $Dir "builtInExtensions"
        )
        
        $allExtensions = @()
        
        foreach ($extDir in $extensionsDirs) {
            if (Test-Path $extDir) {
                Write-ColorOutput "→ Analyzing extensions in: $extDir" "Extension"
                
                $extensionFolders = Get-ChildItem -Path $extDir -Directory -ErrorAction SilentlyContinue
                
                foreach ($folder in $extensionFolders) {
                    $packageJson = Join-Path $folder.FullName "package.json"
                    if (Test-Path $packageJson) {
                        try {
                            $extPackage = Get-Content $packageJson -Raw | ConvertFrom-Json
                            
                            $extensionInfo = @{
                                Name = $extPackage.name
                                DisplayName = $extPackage.displayName
                                Version = $extPackage.version
                                Description = $extPackage.description
                                Publisher = $extPackage.publisher
                                Path = $folder.FullName.Substring($Dir.Length).TrimStart('\', '/')
                                Categories = @()
                                Keywords = @()
                            }
                            
                            if ($extPackage.categories) {
                                $extensionInfo.Categories = $extPackage.categories
                                foreach ($cat in $extPackage.categories) {
                                    if (-not $extensionAnalysis.ExtensionCategories[$cat]) {
                                        $extensionAnalysis.ExtensionCategories[$cat] = 0
                                    }
                                    $extensionAnalysis.ExtensionCategories[$cat]++
                                }
                            }
                            
                            if ($extPackage.keywords) {
                                $extensionInfo.Keywords = $extPackage.keywords
                            }
                            
                            $allExtensions += $extensionInfo
                            
                        } catch {
                            # Skip invalid extensions
                        }
                    }
                }
            }
        }
        
        $extensionAnalysis.BuiltInExtensions = $allExtensions
        $extensionAnalysis.ExtensionCount = $allExtensions.Count
        
        # Get top extensions by display name
        $extensionAnalysis.TopExtensions = $allExtensions | Sort-Object -Property DisplayName | Select-Object -First 20
        
        Write-ColorOutput "✓ Found and analyzed $($extensionAnalysis.ExtensionCount) extensions" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze extensions: $_" "Warning"
    }
    
    return $extensionAnalysis
}

function Analyze-Configuration {
    param([string]$Dir)
    
    Write-ColorOutput "=== ANALYZING CONFIGURATION FILES ===" "Header"
    
    $configAnalysis = @{
        ConfigFiles = @()
        BuildConfigs = @()
        KeyConfigs = @{}
        EnvironmentFiles = @()
        TotalConfigs = 0
    }
    
    try {
        # Look for configuration files
        $configPatterns = @(
            "*.json",
            "*.config.js",
            "*.config.ts",
            "*.yaml",
            "*.yml",
            "*.toml",
            "*.ini",
            "*.env",
            ".env*",
            "tsconfig.json",
            "webpack.config.*",
            "rollup.config.*",
            "vite.config.*",
            "gulpfile.*",
            "Gruntfile.*",
            ".eslintrc*",
            ".prettierrc*",
            ".babelrc*",
            "jest.config.*",
            "mocha.opts"
        )
        
        $allConfigFiles = @()
        
        foreach ($pattern in $configPatterns) {
            $configFiles = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            $allConfigFiles += $configFiles
        }
        
        $totalConfigs = $allConfigFiles.Count
        $currentConfig = 0
        
        foreach ($configFile in $allConfigFiles) {
            $currentConfig++
            $percentComplete = [math]::Round(($currentConfig / $totalConfigs) * 100, 1)
            
            $relativePath = $configFile.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Analyzing Configs" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            $configInfo = @{
                Name = $configFile.Name
                Path = $relativePath
                Size = $configFile.Length
                Type = $configFile.Extension
                ContentSummary = ""
            }
            
            # Try to parse and summarize content
            try {
                $content = Get-Content $configFile.FullName -Raw -ErrorAction SilentlyContinue
                
                switch ($configFile.Extension.ToLower()) {
                    ".json" {
                        try {
                            $json = $content | ConvertFrom-Json
                            $configInfo.ContentSummary = "JSON config with $($json.PSObject.Properties.Count) properties"
                            
                            # Store key configs
                            if ($configFile.Name -eq "tsconfig.json") {
                                $configAnalysis.KeyConfigs["TypeScript"] = $json
                            } elseif ($configFile.Name -eq "package.json") {
                                # Already analyzed separately
                            }
                        } catch {
                            $configInfo.ContentSummary = "Invalid JSON"
                        }
                    }
                    ".js" { $configInfo.ContentSummary = "JavaScript config file" }
                    ".ts" { $configInfo.ContentSummary = "TypeScript config file" }
                    ".yaml" { $configInfo.ContentSummary = "YAML configuration" }
                    ".yml" { $configInfo.ContentSummary = "YAML configuration" }
                    ".toml" { $configInfo.ContentSummary = "TOML configuration" }
                    ".ini" { $configInfo.ContentSummary = "INI configuration" }
                    ".env" { 
                        $configInfo.ContentSummary = "Environment variables"
                        $configAnalysis.EnvironmentFiles += $relativePath
                    }
                    default { $configInfo.ContentSummary = "Configuration file" }
                }
                
            } catch {
                $configInfo.ContentSummary = "Could not read file"
            }
            
            $configAnalysis.ConfigFiles += $configInfo
        }
        
        Write-ProgressBar -Activity "Analyzing Configs" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        $configAnalysis.TotalConfigs = $configAnalysis.ConfigFiles.Count
        
        Write-ColorOutput "✓ Analyzed $($configAnalysis.TotalConfigs) configuration files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze configuration: $_" "Warning"
    }
    
    return $configAnalysis
}

function Analyze-BuildSystem {
    param([string]$Dir)
    
    Write-ColorOutput "=== ANALYZING BUILD SYSTEM ===" "Header"
    
    $buildAnalysis = @{
        BuildTools = @()
        Scripts = @{}
        BuildCommands = @()
        CIConfig = @()
        DockerFiles = @()
        TotalBuildConfigs = 0
    }
    
    try {
        # Look for build-related files
        $buildFiles = @(
            "gulpfile.js",
            "gulpfile.ts",
            "Gruntfile.js",
            "webpack.config.js",
            "webpack.config.ts",
            "rollup.config.js",
            "rollup.config.ts",
            "vite.config.js",
            "vite.config.ts",
            "esbuild.config.js",
            "tsconfig.json",
            ".github/workflows/*.yml",
            ".github/workflows/*.yaml",
            ".gitlab-ci.yml",
            "Jenkinsfile",
            "Dockerfile",
            "docker-compose.yml",
            "Makefile",
            "CMakeLists.txt"
        )
        
        $foundBuildFiles = @()
        
        foreach ($pattern in $buildFiles) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter (Split-Path $pattern -Leaf) -ErrorAction SilentlyContinue
            $foundBuildFiles += $files
        }
        
        # Analyze package.json scripts
        $packageJsonPath = Join-Path $Dir "package.json"
        if (Test-Path $packageJsonPath) {
            try {
                $package = Get-Content $packageJsonPath -Raw | ConvertFrom-Json
                if ($package.scripts) {
                    $buildAnalysis.Scripts = $package.scripts
                    foreach ($script in $package.scripts.PSObject.Properties) {
                        $buildAnalysis.BuildCommands += "$($script.Name): $($script.Value)"
                    }
                }
            } catch {
                # Ignore
            }
        }
        
        # Analyze found build files
        foreach ($file in $foundBuildFiles) {
            $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
            
            switch ($file.Name.ToLower()) {
                "gulpfile.js" { $buildAnalysis.BuildTools += "Gulp" }
                "gruntfile.js" { $buildAnalysis.BuildTools += "Grunt" }
                "webpack.config.js" { $buildAnalysis.BuildTools += "Webpack" }
                "rollup.config.js" { $buildAnalysis.BuildTools += "Rollup" }
                "vite.config.js" { $buildAnalysis.BuildTools += "Vite" }
                "tsconfig.json" { $buildAnalysis.BuildTools += "TypeScript" }
                "dockerfile" { $buildAnalysis.DockerFiles += $relativePath }
                "docker-compose.yml" { $buildAnalysis.DockerFiles += $relativePath }
                { $_.StartsWith(".github/workflows/") } { $buildAnalysis.CIConfig += $relativePath }
                ".gitlab-ci.yml" { $buildAnalysis.CIConfig += $relativePath }
                "jenkinsfile" { $buildAnalysis.CIConfig += $relativePath }
            }
        }
        
        $buildAnalysis.BuildTools = $buildAnalysis.BuildTools | Select-Object -Unique
        $buildAnalysis.TotalBuildConfigs = $foundBuildFiles.Count
        
        Write-ColorOutput "✓ Analyzed build system with $($buildAnalysis.BuildTools.Count) tools" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze build system: $_" "Warning"
    }
    
    return $buildAnalysis
}

function Extract-APIs {
    param([string]$Dir)
    
    Write-ColorOutput "=== EXTRACTING API ENDPOINTS ===" "Header"
    
    $apiAnalysis = @{
        APIEndpoints = @()
        WebSocketEndpoints = @()
        GraphQLEndpoints = @()
        RESTEndpoints = @()
        TotalEndpoints = 0
    }
    
    try {
        # Look for JavaScript/TypeScript files
        $jsFiles = Get-ChildItem -Path $Dir -Recurse -Include @("*.js", "*.ts", "*.jsx", "*.tsx") -ErrorAction SilentlyContinue
        
        $totalFiles = $jsFiles.Count
        $currentFile = 0
        
        foreach ($file in $jsFiles) {
            $currentFile++
            $percentComplete = [math]::Round(($currentFile / $totalFiles) * 100, 1)
            
            $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Scanning for APIs" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                
                # Look for API patterns
                $apiPatterns = @(
                    '(?i)(?:https?://|//)[a-z0-9.-]+(?::\d+)?(?:/[^\s"\']*)?',
                    '(?i)fetch\s*\(\s*["\']([^"\']+)["\']',
                    '(?i)axios\.(?:get|post|put|delete|patch)\s*\(\s*["\']([^"\']+)["\']',
                    '(?i)ws://[a-z0-9.-]+(?::\d+)?(?:/[^\s"\']*)?',
                    '(?i)wss://[a-z0-9.-]+(?::\d+)?(?:/[^\s"\']*)?',
                    '(?i)graphql\s*\(\s*["\']([^"\']+)["\']',
                    '(?i)api\.[a-z0-9.-]+(?:/[^\s"\']*)?'
                )
                
                foreach ($pattern in $apiPatterns) {
                    $matches = [regex]::Matches($content, $pattern)
                    foreach ($match in $matches) {
                        $endpoint = $match.Groups[1].Value
                        if (-not $endpoint) { $endpoint = $match.Value }
                        
                        if ($endpoint -and $endpoint.Length -gt 5) {
                            $apiInfo = @{
                                Endpoint = $endpoint
                                File = $relativePath
                                Type = "Unknown"
                            }
                            
                            # Determine type
                            if ($endpoint -match '^https?://') { $apiInfo.Type = "REST" }
                            elseif ($endpoint -match '^wss?://') { $apiInfo.Type = "WebSocket" }
                            elseif ($endpoint -match 'graphql') { $apiInfo.Type = "GraphQL" }
                            elseif ($endpoint -match 'api\.') { $apiInfo.Type = "API" }
                            
                            $apiAnalysis.APIEndpoints += $apiInfo
                        }
                    }
                }
                
            } catch {
                # Skip files that can't be read
            }
        }
        
        Write-ProgressBar -Activity "Scanning for APIs" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        # Remove duplicates
        $uniqueEndpoints = $apiAnalysis.APIEndpoints | Sort-Object -Property Endpoint -Unique
        $apiAnalysis.APIEndpoints = $uniqueEndpoints
        $apiAnalysis.TotalEndpoints = $uniqueEndpoints.Count
        
        # Categorize
        $apiAnalysis.RESTEndpoints = $uniqueEndpoints | Where-Object { $_.Type -eq "REST" }
        $apiAnalysis.WebSocketEndpoints = $uniqueEndpoints | Where-Object { $_.Type -eq "WebSocket" }
        $apiAnalysis.GraphQLEndpoints = $uniqueEndpoints | Where-Object { $_.Type -eq "GraphQL" }
        
        Write-ColorOutput "✓ Found $($apiAnalysis.TotalEndpoints) API endpoints" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract APIs: $_" "Warning"
    }
    
    return $apiAnalysis
}

function Analyze-Security {
    param([string]$Dir)
    
    Write-ColorOutput "=== ANALYZING SECURITY CONFIGURATIONS ===" "Header"
    
    $securityAnalysis = @{
        SecurityConfigs = @()
        APIKeys = @()
        Tokens = @()
        Certificates = @()
        Vulnerabilities = @()
        SecurityScore = 0
    }
    
    try {
        # Look for security-related files
        $securityFiles = @(
            "*.key",
            "*.pem",
            "*.crt",
            "*.cert",
            "*.p12",
            "*.pfx",
            ".env*",
            "*config*secret*",
            "*config*key*",
            "*secret*",
            "*token*"
        )
        
        $foundSecurityFiles = @()
        
        foreach ($pattern in $securityFiles) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            $foundSecurityFiles += $files
        }
        
        foreach ($file in $foundSecurityFiles) {
            $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
            
            $securityInfo = @{
                Name = $file.Name
                Path = $relativePath
                Type = "Unknown"
                RiskLevel = "Low"
            }
            
            switch ($file.Extension.ToLower()) {
                ".key" { 
                    $securityInfo.Type = "Private Key"
                    $securityInfo.RiskLevel = "High"
                    $securityAnalysis.Certificates += $relativePath
                }
                ".pem" { 
                    $securityInfo.Type = "Certificate"
                    $securityInfo.RiskLevel = "Medium"
                    $securityAnalysis.Certificates += $relativePath
                }
                ".crt" { 
                    $securityInfo.Type = "Certificate"
                    $securityInfo.RiskLevel = "Medium"
                    $securityAnalysis.Certificates += $relativePath
                }
                ".env" { 
                    $securityInfo.Type = "Environment Config"
                    $securityInfo.RiskLevel = "High"
                }
                default { 
                    $securityInfo.Type = "Config File"
                    $securityInfo.RiskLevel = "Medium"
                }
            }
            
            $securityAnalysis.SecurityConfigs += $securityInfo
        }
        
        # Scan for hardcoded secrets in JS files
        $jsFiles = Get-ChildItem -Path $Dir -Recurse -Include @("*.js", "*.ts") -ErrorAction SilentlyContinue | Select-Object -First 100
        
        $secretPatterns = @(
            '(?i)(api[_-]?key|apikey)\s*[=:]\s*["\']?([a-z0-9_-]{20,})["\']?',
            '(?i)(token|access[_-]?token)\s*[=:]\s*["\']?([a-z0-9._-]{20,})["\']?',
            '(?i)(secret|client[_-]?secret)\s*[=:]\s*["\']?([a-z0-9_-]{20,})["\']?',
            '(?i)(password|passwd)\s*[=:]\s*["\']?([^"\']{8,})["\']?',
            '(?i)Bearer\s+([a-z0-9._-]{20,})',
            '(?i)Basic\s+([a-z0-9+/=]{10,})'
        )
        
        foreach ($file in $jsFiles) {
            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                
                foreach ($pattern in $secretPatterns) {
                    $matches = [regex]::Matches($content, $pattern)
                    foreach ($match in $matches) {
                        $secretInfo = @{
                            Type = "Hardcoded Secret"
                            File = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
                            Pattern = $pattern
                            RiskLevel = "Critical"
                        }
                        $securityAnalysis.Vulnerabilities += $secretInfo
                    }
                }
                
            } catch {
                # Skip files that can't be read
            }
        }
        
        # Calculate security score
        $totalIssues = $securityAnalysis.Vulnerabilities.Count
        $securityAnalysis.SecurityScore = [math]::Max(0, 100 - ($totalIssues * 10))
        
        Write-ColorOutput "✓ Security analysis complete (Score: $($securityAnalysis.SecurityScore)/100)" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze security: $_" "Warning"
    }
    
    return $securityAnalysis
}

function Generate-ComprehensiveReport {
    param(
        $Stats,
        $Packages,
        $Dependencies,
        $Extensions,
        $Configuration,
        $BuildSystem,
        $APIs,
        $Security
    )
    
    Write-ColorOutput "=== GENERATING COMPREHENSIVE REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            Tool = "Analyze-Extracted-Source.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceDirectory = $SourceDirectory
        }
        Summary = @{
            TotalFiles = $Stats.TotalFiles
            TotalSizeMB = [math]::Round($Stats.TotalSize / 1MB, 2)
            SourceFiles = $Stats.TotalFiles
            PackageCount = $Packages.Count
            ExtensionCount = $Extensions.ExtensionCount
            DependencyCount = $Dependencies.TotalCount
            DevDependencyCount = $Dependencies.DevCount
            ConfigFiles = $Configuration.TotalConfigs
            BuildTools = $BuildSystem.BuildTools
            APIEndpoints = $APIs.TotalEndpoints
            SecurityScore = $Security.SecurityScore
        }
        Statistics = $Stats
        Packages = $Packages
        Dependencies = $Dependencies
        Extensions = $Extensions
        Configuration = $Configuration
        BuildSystem = $BuildSystem
        APIs = $APIs
        Security = $Security
    }
    
    # Save report
    $reportJson = $report | ConvertTo-Json -Depth 20 -Compress:$false
    $reportJson | Out-File -FilePath $OutputFile -Encoding UTF8
    
    Write-ColorOutput "✓ Generated comprehensive report" "Success"
    Write-ColorOutput "  Report saved to: $OutputFile" "Detail"
    Write-ColorOutput "  File size: $([math]::Round((Get-Item $OutputFile).Length / 1KB, 2)) KB" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "EXTRACTED SOURCE CODE ANALYZER"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Source Directory: $SourceDirectory"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if source directory exists
if (-not (Test-Path $SourceDirectory)) {
    Write-ColorOutput "✗ Source directory not found: $SourceDirectory" "Error"
    exit 1
}

# Initialize analysis objects
$stats = $null
$packages = @()
$dependencies = $null
$extensions = $null
$configuration = $null
$buildSystem = $null
$apis = $null
$security = $null

# Get source statistics
if ($IncludeStats) {
    $stats = Get-SourceStats -Dir $SourceDirectory
}

# Find and analyze all package.json files
$packages = Find-AllPackages -Dir $SourceDirectory

# Analyze dependencies if requested
if ($AnalyzeDependencies -and $packages.Count -gt 0) {
    $dependencies = Analyze-Dependencies -Packages $packages
}

# Analyze extensions if requested
if ($AnalyzeExtensions) {
    $extensions = Analyze-Extensions -Dir $SourceDirectory
}

# Analyze configuration files if requested
if ($AnalyzeConfiguration) {
    $configuration = Analyze-Configuration -Dir $SourceDirectory
}

# Analyze build system if requested
if ($AnalyzeBuildSystem) {
    $buildSystem = Analyze-BuildSystem -Dir $SourceDirectory
}

# Extract APIs if requested
if ($ExtractAPIs) {
    $apis = Extract-APIs -Dir $SourceDirectory
}

# Analyze security if requested
if ($AnalyzeSecurity) {
    $security = Analyze-Security -Dir $SourceDirectory
}

# Generate comprehensive report if requested
if ($GenerateReport) {
    $report = Generate-ComprehensiveReport `
        -Stats $stats `
        -Packages $packages `
        -Dependencies $dependencies `
        -Extensions $extensions `
        -Configuration $configuration `
        -BuildSystem $buildSystem `
        -APIs $apis `
        -Security $security
    
    # Display summary
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "ANALYSIS COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Total Files: $($stats.TotalFiles)" "Success"
    Write-ColorOutput "Total Size: $([math]::Round($stats.TotalSize / 1MB, 2)) MB" "Detail"
    Write-ColorOutput "Packages: $($packages.Count)" "Detail"
    Write-ColorOutput "Dependencies: $($dependencies.TotalCount)" "Dependency"
    Write-ColorOutput "Dev Dependencies: $($dependencies.DevCount)" "Dependency"
    Write-ColorOutput "Extensions: $($extensions.ExtensionCount)" "Extension"
    Write-ColorOutput "Config Files: $($configuration.TotalConfigs)" "Config"
    Write-ColorOutput "Build Tools: $($buildSystem.BuildTools -join ', ')" "Detail"
    Write-ColorOutput "API Endpoints: $($apis.TotalEndpoints)" "Detail"
    Write-ColorOutput "Security Score: $($security.SecurityScore)/100" "Security"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Report: $OutputFile" "Detail"
    Write-ColorOutput "=" * 80
} else {
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "ANALYSIS COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Packages analyzed: $($packages.Count)" "Success"
    Write-ColorOutput "=" * 80
}

Write-ColorOutput ""
Write-ColorOutput "Next steps: Use this analysis to understand the codebase architecture" "Info"
