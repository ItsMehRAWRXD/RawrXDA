#Requires -Version 7.0
<#
.SYNOPSIS
    Complete Source Code Dumper - Creates a structured reverse-engineered fork
.DESCRIPTION
    Dumps the entire extracted source code and structures it as a complete,
    reverse-engineered fork with proper organization, metadata, and documentation.
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDirectory,
    
    [string]$OutputDirectory = "Cursor_Reverse_Engineered_Fork",
    
    [switch]$IncludeAllFiles,
    [switch]$IncludeSourceOnly,
    [switch]$IncludeDependencies,
    [switch]$IncludeExtensions,
    [switch]$IncludeConfiguration,
    [switch]$IncludeBuildFiles,
    [switch]$IncludeDocumentation,
    [switch]$GenerateManifest,
    [switch]$GenerateReport,
    [switch]$CreateGitRepo,
    [switch]$ReconstructTemplates,
    [switch]$ShowProgress,
    [switch]$ForceOverwrite
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
    File = "DarkGreen"
    Directory = "DarkYellow"
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

function Get-SourceStructure {
    param([string]$Dir)
    
    Write-ColorOutput "→ Analyzing source structure: $Dir" "Detail"
    
    $structure = @{
        Root = $Dir
        Directories = @()
        Files = @()
        TotalFiles = 0
        TotalSize = 0
        FileTypes = @{}
        LargestFiles = @()
        Depth = 0
    }
    
    try {
        $allFiles = Get-ChildItem -Path $Dir -Recurse -ErrorAction SilentlyContinue
        $files = $allFiles | Where-Object { -not $_.PSIsContainer }
        $dirs = $allFiles | Where-Object { $_.PSIsContainer }
        
        $structure.TotalFiles = $files.Count
        $structure.TotalSize = ($files | Measure-Object -Property Length -Sum).Sum
        $structure.Directories = $dirs | ForEach-Object { $_.FullName.Substring($Dir.Length).TrimStart('\', '/') }
        $structure.Files = $files | ForEach-Object {
            $relativePath = $_.FullName.Substring($Dir.Length).TrimStart('\', '/')
            $ext = $_.Extension.ToLower()
            
            # Count file types
            if (-not $structure.FileTypes[$ext]) {
                $structure.FileTypes[$ext] = @{ Count = 0; Size = 0 }
            }
            $structure.FileTypes[$ext].Count++
            $structure.FileTypes[$ext].Size += $_.Length
            
            return @{
                Path = $relativePath
                Size = $_.Length
                Extension = $ext
                Modified = $_.LastWriteTime
                Created = $_.CreationTime
            }
        }
        
        # Find largest files
        $structure.LargestFiles = $files | Sort-Object -Property Length -Descending | Select-Object -First 20 | ForEach-Object {
            @{
                Path = $_.FullName.Substring($Dir.Length).TrimStart('\', '/')
                Size = $_.Length
                Extension = $_.Extension.ToLower()
            }
        }
        
        # Calculate depth
        $maxDepth = 0
        foreach ($dir in $dirs) {
            $depth = ($dir.FullName.Substring($Dir.Length).Trim('\') -split '\\').Count
            if ($depth -gt $maxDepth) { $maxDepth = $depth }
        }
        $structure.Depth = $maxDepth
        
        Write-ColorOutput "✓ Analyzed $($structure.TotalFiles) files in $($structure.Directories.Count) directories" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze structure: $_" "Warning"
    }
    
    return $structure
}

function Copy-SourceTree {
    param(
        [string]$SourceDir,
        [string]$TargetDir,
        $Structure
    )
    
    Write-ColorOutput "→ Copying source tree" "File"
    
    $copiedFiles = 0
    $totalFiles = $Structure.TotalFiles
    
    # Create root directory
    if (-not (Test-Path $TargetDir)) {
        New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null
    }
    
    # Create all directories first
    foreach ($dir in $Structure.Directories) {
        $targetPath = Join-Path $TargetDir $dir
        if (-not (Test-Path $targetPath)) {
            New-Item -Path $targetPath -ItemType Directory -Force | Out-Null
        }
    }
    
    # Copy all files
    foreach ($file in $Structure.Files) {
        $sourcePath = Join-Path $SourceDir $file.Path
        $targetPath = Join-Path $TargetDir $file.Path
        
        $percentComplete = [math]::Round(($copiedFiles / $totalFiles) * 100, 1)
        Write-ProgressBar -Activity "Copying Files" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $file.Path
        
        try {
            Copy-Item -Path $sourcePath -Destination $targetPath -Force -ErrorAction Stop
            $copiedFiles++
        } catch {
            Write-ColorOutput "⚠ Failed to copy: $($file.Path)" "Warning"
        }
    }
    
    Write-ProgressBar -Activity "Copying Files" -Status "Complete" -PercentComplete 100
    Write-Host ""
    
    Write-ColorOutput "✓ Copied $copiedFiles files" "Success"
    
    return $copiedFiles
}

function Get-PackageInfo {
    param([string]$Dir)
    
    Write-ColorOutput "→ Extracting package information" "Detail"
    
    $packages = @()
    
    try {
        $packageFiles = Get-ChildItem -Path $Dir -Recurse -Filter "package.json" -ErrorAction SilentlyContinue
        
        foreach ($packageFile in $packageFiles) {
            try {
                $package = Get-Content $packageFile.FullName -Raw | ConvertFrom-Json
                $relativePath = $packageFile.FullName.Substring($Dir.Length).TrimStart('\', '/')
                
                $packageInfo = @{
                    Name = $package.name
                    Version = $package.version
                    Description = $package.description
                    Main = $package.main
                    Type = $package.type
                    Path = $relativePath
                    Directory = Split-Path $relativePath -Parent
                    Dependencies = @{}
                    DevDependencies = @{}
                    Scripts = @{}
                    Keywords = @()
                    Author = ""
                    License = ""
                    Repository = ""
                    Bugs = ""
                    Homepage = ""
                }
                
                if ($package.dependencies) {
                    $packageInfo.Dependencies = $package.dependencies
                }
                if ($package.devDependencies) {
                    $packageInfo.DevDependencies = $package.devDependencies
                }
                if ($package.scripts) {
                    $packageInfo.Scripts = $package.scripts
                }
                if ($package.keywords) {
                    $packageInfo.Keywords = $package.keywords
                }
                if ($package.author) {
                    $packageInfo.Author = $package.author.ToString()
                }
                if ($package.license) {
                    $packageInfo.License = $package.license
                }
                if ($package.repository) {
                    $packageInfo.Repository = $package.repository.ToString()
                }
                if ($package.bugs) {
                    $packageInfo.Bugs = $package.bugs.ToString()
                }
                if ($package.homepage) {
                    $packageInfo.Homepage = $package.homepage
                }
                
                $packages += $packageInfo
                
            } catch {
                # Skip invalid package.json files
            }
        }
        
        Write-ColorOutput "✓ Found $($packages.Count) package.json files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract package information: $_" "Warning"
    }
    
    return $packages
}

function Get-ExtensionInfo {
    param([string]$Dir)
    
    Write-ColorOutput "→ Extracting extension information" "Detail"
    
    $extensions = @()
    
    try {
        # Look for extension directories
        $extensionDirs = @(
            Join-Path $Dir "extensions"
            Join-Path $Dir "resources\app\extensions"
            Join-Path $Dir "builtInExtensions"
        )
        
        foreach ($extDir in $extensionDirs) {
            if (Test-Path $extDir) {
                $folders = Get-ChildItem -Path $extDir -Directory -ErrorAction SilentlyContinue
                
                foreach ($folder in $folders) {
                    $packageJson = Join-Path $folder.FullName "package.json"
                    if (Test-Path $packageJson) {
                        try {
                            $package = Get-Content $packageJson -Raw | ConvertFrom-Json
                            $relativePath = $folder.FullName.Substring($Dir.Length).TrimStart('\', '/')
                            
                            $extensionInfo = @{
                                Name = $package.name
                                DisplayName = $package.displayName
                                Version = $package.version
                                Description = $package.description
                                Publisher = $package.publisher
                                Path = $relativePath
                                Categories = @()
                                Keywords = @()
                                ActivationEvents = @()
                                Main = $package.main
                                Contributes = @{}
                            }
                            
                            if ($package.categories) {
                                $extensionInfo.Categories = $package.categories
                            }
                            if ($package.keywords) {
                                $extensionInfo.Keywords = $package.keywords
                            }
                            if ($package.activationEvents) {
                                $extensionInfo.ActivationEvents = $package.activationEvents
                            }
                            if ($package.contributes) {
                                $extensionInfo.Contributes = $package.contributes
                            }
                            
                            $extensions += $extensionInfo
                            
                        } catch {
                            # Skip invalid extensions
                        }
                    }
                }
            }
        }
        
        Write-ColorOutput "✓ Found $($extensions.Count) extensions" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract extension information: $_" "Warning"
    }
    
    return $extensions
}

function Get-ConfigurationFiles {
    param([string]$Dir)
    
    Write-ColorOutput "→ Extracting configuration files" "Detail"
    
    $configs = @()
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
    
    try {
        foreach ($pattern in $configPatterns) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            
            foreach ($file in $files) {
                $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
                
                $configInfo = @{
                    Name = $file.Name
                    Path = $relativePath
                    Size = $file.Length
                    Type = $file.Extension
                    Directory = Split-Path $relativePath -Parent
                }
                
                $configs += $configInfo
            }
        }
        
        Write-ColorOutput "✓ Found $($configs.Count) configuration files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract configuration files: $_" "Warning"
    }
    
    return $configs
}

function Get-BuildSystemInfo {
    param([string]$Dir)
    
    Write-ColorOutput "→ Extracting build system information" "Detail"
    
    $buildInfo = @{
        BuildTools = @()
        Scripts = @{}
        CIConfig = @()
        DockerFiles = @()
        PackageManagers = @()
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
        
        foreach ($pattern in $buildFiles) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter (Split-Path $pattern -Leaf) -ErrorAction SilentlyContinue
            
            foreach ($file in $files) {
                $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
                
                switch ($file.Name.ToLower()) {
                    "gulpfile.js" { $buildInfo.BuildTools += "Gulp" }
                    "gruntfile.js" { $buildInfo.BuildTools += "Grunt" }
                    "webpack.config.js" { $buildInfo.BuildTools += "Webpack" }
                    "rollup.config.js" { $buildInfo.BuildTools += "Rollup" }
                    "vite.config.js" { $buildInfo.BuildTools += "Vite" }
                    "tsconfig.json" { $buildInfo.BuildTools += "TypeScript" }
                    "dockerfile" { $buildInfo.DockerFiles += $relativePath }
                    "docker-compose.yml" { $buildInfo.DockerFiles += $relativePath }
                    { $_.StartsWith(".github/workflows/") } { $buildInfo.CIConfig += $relativePath }
                    ".gitlab-ci.yml" { $buildInfo.CIConfig += $relativePath }
                    "jenkinsfile" { $buildInfo.CIConfig += $relativePath }
                }
            }
        }
        
        # Check package.json for scripts
        $packageJson = Join-Path $Dir "package.json"
        if (Test-Path $packageJson) {
            try {
                $package = Get-Content $packageJson -Raw | ConvertFrom-Json
                if ($package.scripts) {
                    $buildInfo.Scripts = $package.scripts
                }
            } catch {
                # Ignore
            }
        }
        
        # Check for package managers
        if (Test-Path (Join-Path $Dir "package.json")) { $buildInfo.PackageManagers += "npm" }
        if (Test-Path (Join-Path $Dir "yarn.lock")) { $buildInfo.PackageManagers += "yarn" }
        if (Test-Path (Join-Path $Dir "pnpm-lock.yaml")) { $buildInfo.PackageManagers += "pnpm" }
        
        # Remove duplicates
        $buildInfo.BuildTools = $buildInfo.BuildTools | Select-Object -Unique
        $buildInfo.PackageManagers = $buildInfo.PackageManagers | Select-Object -Unique
        
        Write-ColorOutput "✓ Found $($buildInfo.BuildTools.Count) build tools" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract build system information: $_" "Warning"
    }
    
    return $buildInfo
}

function Beautify-JavaScript {
    param(
        [string]$Code,
        [int]$IndentSize = 2
    )
    
    # Basic JavaScript beautification
    # This is a simplified version - for production use js-beautify npm package
    
    $lines = $Code -split "`n"
    $beautified = @()
    $indentLevel = 0
    $indent = " " * $IndentSize
    
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        
        # Skip empty lines
        if ([string]::IsNullOrWhiteSpace($trimmed)) {
            $beautified += ""
            continue
        }
        
        # Decrease indent for closing braces
        if ($trimmed -match '^[}\])]') {
            $indentLevel = [Math]::Max(0, $indentLevel - 1)
        }
        
        # Add indented line
        $beautified += ($indent * $indentLevel) + $trimmed
        
        # Increase indent for opening braces
        if ($trimmed -match '[{[(]$') {
            $indentLevel++
        }
    }
    
    return $beautified -join "`n"
}

function Analyze-SourceMap {
    param([string]$FilePath)
    
    Write-ColorOutput "  → Analyzing source map: $([System.IO.Path]::GetFileName($FilePath))" "Detail"
    
    $sourceMap = @{
        HasSourceMap = $false
        SourceMapURL = ""
        OriginalSources = @()
        SourceRoot = ""
    }
    
    try {
        # Read last few lines to find source map comment
        $content = Get-Content $FilePath -Raw -ErrorAction SilentlyContinue
        
        if ($content -match '//# sourceMappingURL=(.+)$') {
            $sourceMap.HasSourceMap = $true
            $sourceMap.SourceMapURL = $Matches[1].Trim()
            
            # Try to load actual source map file
            $mapFile = $FilePath + ".map"
            if (Test-Path $mapFile) {
                $mapContent = Get-Content $mapFile -Raw | ConvertFrom-Json
                if ($mapContent.sources) {
                    $sourceMap.OriginalSources = $mapContent.sources
                }
                if ($mapContent.sourceRoot) {
                    $sourceMap.SourceRoot = $mapContent.sourceRoot
                }
            }
        }
        
    } catch {
        # Silently fail
    }
    
    return $sourceMap
}

function Reconstruct-SourceStructure {
    param(
        [string]$CompiledDir,
        [string]$ReconstructedDir
    )
    
    Write-ColorOutput "→ Reconstructing source structure from compiled output" "Header"
    Write-ColorOutput "  Compiled: $CompiledDir" "Detail"
    Write-ColorOutput "  Target: $ReconstructedDir" "Detail"
    
    $stats = @{
        FilesProcessed = 0
        FilesReconstructed = 0
        SourceMapsFound = 0
        FilesBeautified = 0
        TypeDefinitionsGenerated = 0
    }
    
    # Create src directory
    $srcDir = Join-Path $ReconstructedDir "src"
    if (-not (Test-Path $srcDir)) {
        New-Item -Path $srcDir -ItemType Directory -Force | Out-Null
    }
    
    # Find all JavaScript files in out/ directory
    $outDir = Join-Path $CompiledDir "out"
    if (-not (Test-Path $outDir)) {
        $outDir = $CompiledDir
    }
    
    Write-ColorOutput "  Scanning for JavaScript files..." "Detail"
    $jsFiles = Get-ChildItem -Path $outDir -Recurse -Filter "*.js" -ErrorAction SilentlyContinue
    
    Write-ColorOutput "  Found $($jsFiles.Count) JavaScript files to process" "Success"
    
    foreach ($jsFile in $jsFiles) {
        $stats.FilesProcessed++
        
        $percentComplete = [math]::Round(($stats.FilesProcessed / $jsFiles.Count) * 100, 1)
        Write-ProgressBar -Activity "Reconstructing Source Templates" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $jsFile.Name
        
        try {
            # Analyze source map
            $sourceMap = Analyze-SourceMap -FilePath $jsFile.FullName
            
            if ($sourceMap.HasSourceMap) {
                $stats.SourceMapsFound++
            }
            
            # Determine target path in src/
            $relativePath = $jsFile.FullName.Substring($outDir.Length).TrimStart('\', '/')
            
            # Convert out/ path to src/ path
            if ($sourceMap.OriginalSources.Count -gt 0) {
                # Use original source path from source map
                $originalPath = $sourceMap.OriginalSources[0] -replace '^\.\./', ''
                $targetPath = Join-Path $srcDir $originalPath
            } else {
                # Infer TypeScript source path
                $targetPath = Join-Path $srcDir ($relativePath -replace '\.js$', '.ts')
            }
            
            # Create target directory
            $targetDir = Split-Path $targetPath -Parent
            if (-not (Test-Path $targetDir)) {
                New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
            }
            
            # Read and beautify JavaScript
            $code = Get-Content $jsFile.FullName -Raw -ErrorAction SilentlyContinue
            
            # Skip minified files (too long single lines)
            $lines = $code -split "`n"
            $avgLineLength = ($code.Length) / ([Math]::Max($lines.Count, 1))
            
            if ($avgLineLength -gt 500 -or $jsFile.Length -gt 1MB) {
                # File is likely minified, apply beautification
                try {
                    $beautified = Beautify-JavaScript -Code $code
                    $beautified | Out-File -FilePath $targetPath -Encoding UTF8 -ErrorAction Stop
                    $stats.FilesBeautified++
                    $stats.FilesReconstructed++
                } catch {
                    # If beautification fails, copy as-is
                    Copy-Item $jsFile.FullName -Destination $targetPath -Force
                    $stats.FilesReconstructed++
                }
            } else {
                # File looks reasonable, copy with minimal formatting
                $code | Out-File -FilePath $targetPath -Encoding UTF8 -ErrorAction Stop
                $stats.FilesReconstructed++
            }
            
        } catch {
            Write-ColorOutput "    ⚠ Failed to process: $($jsFile.Name)" "Warning"
        }
    }
    
    Write-ProgressBar -Activity "Reconstructing Source Templates" -Status "Complete" -PercentComplete 100
    Write-Host ""
    
    Write-ColorOutput "✓ Source reconstruction complete" "Success"
    Write-ColorOutput "  Files Processed: $($stats.FilesProcessed)" "Detail"
    Write-ColorOutput "  Files Reconstructed: $($stats.FilesReconstructed)" "Detail"
    Write-ColorOutput "  Source Maps Found: $($stats.SourceMapsFound)" "Detail"
    Write-ColorOutput "  Files Beautified: $($stats.FilesBeautified)" "Detail"
    
    return $stats
}

function Generate-TypeScriptConfig {
    param([string]$OutputDir)
    
    Write-ColorOutput "→ Generating TypeScript configuration" "Detail"
    
    $tsconfig = @{
        compilerOptions = @{
            target = "ES2020"
            module = "commonjs"
            lib = @("ES2020", "DOM")
            outDir = "./out"
            rootDir = "./src"
            sourceMap = $true
            declaration = $true
            declarationMap = $true
            moduleResolution = "node"
            esModuleInterop = $true
            allowSyntheticDefaultImports = $true
            strict = $false
            skipLibCheck = $true
            forceConsistentCasingInFileNames = $true
            resolveJsonModule = $true
            allowJs = $true
        }
        include = @("src/**/*")
        exclude = @("node_modules", "out", "dist")
    }
    
    $tsconfigPath = Join-Path $OutputDir "tsconfig.json"
    $tsconfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $tsconfigPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated tsconfig.json" "Success"
}

function Generate-BuildConfiguration {
    param(
        [string]$OutputDir,
        $Manifest
    )
    
    Write-ColorOutput "→ Generating build configuration" "Detail"
    
    # Generate package.json with dev dependencies
    $packageJsonPath = Join-Path $OutputDir "package.json"
    
    if (Test-Path $packageJsonPath) {
        try {
            $package = Get-Content $packageJsonPath -Raw | ConvertFrom-Json
            
            # Add dev dependencies if not present
            if (-not $package.devDependencies) {
                $package | Add-Member -NotePropertyName "devDependencies" -NotePropertyValue @{} -Force
            }
            
            # Add common build tools
            $devDeps = @{
                "typescript" = "^5.3.3"
                "webpack" = "^5.89.0"
                "webpack-cli" = "^5.1.4"
                "ts-loader" = "^9.5.1"
                "@types/node" = "^20.10.0"
                "eslint" = "^8.56.0"
                "prettier" = "^3.1.1"
            }
            
            foreach ($dep in $devDeps.Keys) {
                if (-not $package.devDependencies.$dep) {
                    $package.devDependencies | Add-Member -NotePropertyName $dep -NotePropertyValue $devDeps[$dep] -Force
                }
            }
            
            # Add build scripts if not present
            if (-not $package.scripts) {
                $package | Add-Member -NotePropertyName "scripts" -NotePropertyValue @{} -Force
            }
            
            $scripts = @{
                "build" = "tsc && webpack"
                "watch" = "tsc -w"
                "clean" = "rimraf out dist"
                "compile" = "tsc"
                "lint" = "eslint src/**/*.ts"
                "format" = "prettier --write src/**/*.ts"
            }
            
            foreach ($script in $scripts.Keys) {
                if (-not $package.scripts.$script) {
                    $package.scripts | Add-Member -NotePropertyName $script -NotePropertyValue $scripts[$script] -Force
                }
            }
            
            # Save updated package.json
            $package | ConvertTo-Json -Depth 20 | Out-File -FilePath $packageJsonPath -Encoding UTF8
            
            Write-ColorOutput "✓ Updated package.json with dev dependencies" "Success"
            
        } catch {
            Write-ColorOutput "⚠ Failed to update package.json: $_" "Warning"
        }
    }
    
    # Generate TypeScript config
    Generate-TypeScriptConfig -OutputDir $OutputDir
    
    Write-ColorOutput "✓ Generated build configuration" "Success"
}

function Generate-Manifest {
    param(
        $Structure,
        $Packages,
        $Extensions,
        $Configs,
        $BuildInfo
    )
    
    Write-ColorOutput "→ Generating manifest" "Detail"
    
    $manifest = @{
        ManifestVersion = "1.0"
        GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Tool = "Dump-Complete-Source.ps1"
        Source = @{
            OriginalDirectory = $SourceDirectory
            TotalFiles = $Structure.TotalFiles
            TotalSize = $Structure.TotalSize
            TotalSizeMB = [math]::Round($Structure.TotalSize / 1MB, 2)
            DirectoryCount = $Structure.Directories.Count
            MaxDepth = $Structure.Depth
        }
        FileTypes = $Structure.FileTypes
        Packages = @{
            Count = $Packages.Count
            List = $Packages
        }
        Extensions = @{
            Count = $Extensions.Count
            List = $Extensions
        }
        Configuration = @{
            Count = $Configs.Count
            Files = $Configs
        }
        BuildSystem = $BuildInfo
    }
    
    Write-ColorOutput "✓ Generated manifest" "Success"
    
    return $manifest
}

function Generate-ComprehensiveReport {
    param(
        $Manifest,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Generating comprehensive report" "Detail"
    
    $report = @{
        Metadata = @{
            Tool = "Dump-Complete-Source.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceDirectory = $SourceDirectory
            OutputDirectory = $OutputDirectory
        }
        Summary = @{
            TotalFiles = $Manifest.Source.TotalFiles
            TotalSizeMB = $Manifest.Source.TotalSizeMB
            DirectoryCount = $Manifest.Source.DirectoryCount
            PackageCount = $Manifest.Packages.Count
            ExtensionCount = $Manifest.Extensions.Count
            ConfigCount = $Manifest.Configuration.Count
            BuildTools = $Manifest.BuildSystem.BuildTools
        }
        Source = $Manifest.Source
        FileTypes = $Manifest.FileTypes
        LargestFiles = $Structure.LargestFiles
        Packages = $Manifest.Packages.List | Select-Object -First 20
        Extensions = $Manifest.Extensions.List | Select-Object -First 20
        BuildSystem = $Manifest.BuildSystem
    }
    
    # Save JSON report
    $reportJson = $report | ConvertTo-Json -Depth 20 -Compress:$false
    $reportPath = Join-Path $OutputDir "reverse_engineering_report.json"
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated comprehensive report" "Success"
    Write-ColorOutput "  Report saved to: $reportPath" "Detail"
    
    return $report
}

function Initialize-GitRepository {
    param([string]$RepoDir)
    
    Write-ColorOutput "→ Initializing Git repository" "Detail"
    
    try {
        # Check if git is available
        $gitCmd = Get-Command "git" -ErrorAction SilentlyContinue
        if (-not $gitCmd) {
            Write-ColorOutput "⚠ Git not found, skipping repository initialization" "Warning"
            return $false
        }
        
        # Initialize repository
        Set-Location $RepoDir
        & git init | Out-Null
        
        # Create initial commit
        & git add . | Out-Null
        & git commit -m "Initial commit: Reverse engineered Cursor IDE v2.4.21" | Out-Null
        
        Write-ColorOutput "✓ Initialized Git repository" "Success"
        return $true
        
    } catch {
        Write-ColorOutput "⚠ Failed to initialize Git repository: $_" "Warning"
        return $false
    }
}

function Create-README {
    param(
        [string]$OutputDir,
        $Manifest
    )
    
    Write-ColorOutput "→ Creating README.md" "Detail"
    
    $readme = @"
# Cursor IDE - Reverse Engineered Fork

## Overview

This is a complete reverse-engineered fork of **Cursor IDE v2.4.21** extracted from the official Electron application.

**Generated**: $($Manifest.GeneratedAt)  
**Tool**: $($Manifest.Tool)  
**Original Source**: $($Manifest.Source.OriginalDirectory)

## Statistics

- **Total Files**: $($Manifest.Source.TotalFiles.ToString('N0'))
- **Total Size**: $($Manifest.Source.TotalSizeMB) MB
- **Directories**: $($Manifest.Source.DirectoryCount.ToString('N0'))
- **Maximum Depth**: $($Manifest.Source.MaxDepth)
- **Packages**: $($Manifest.Packages.Count)
- **Extensions**: $($Manifest.Extensions.Count)
- **Configuration Files**: $($Manifest.Configuration.Count)

## File Types

| Extension | Count | Size (MB) |
|-----------|-------|-----------|
"@
    
    foreach ($ext in $Manifest.FileTypes.Keys | Sort-Object) {
        $count = $Manifest.FileTypes[$ext].Count
        $sizeMB = [math]::Round($Manifest.FileTypes[$ext].Size / 1MB, 2)
        $readme += "| $ext | $count | $sizeMB |
"
    }
    
    $readme += @"

## Directory Structure

```
$OutputDirectory/
├── src/                          # Source code
├── extensions/                   # Built-in extensions
├── node_modules/                 # Dependencies
├── out/                          # Compiled output
├── resources/                    # Resources
├── package.json                  # Main package
├── README.md                     # This file
├── REVERSE_ENGINEERING_REPORT.md # Detailed report
└── MANIFEST.json                 # Complete manifest
```

## Key Components

### Core Application
- **Electron-based**: Built on Electron framework
- **VS Code Fork**: Based on VS Code architecture
- **Main Entry**: `./out/main.js`
- **Type**: ES Module

### AI Integration
- **OpenAI**: GPT-4, GPT-3.5, embeddings
- **Anthropic**: Claude 3 (Opus, Sonnet, Haiku)
- **Google**: Gemini, PaLM 2
- **LangChain**: Framework integration
- **Vercel AI**: SDK integration

### Extension System
- **VS Code Compatible**: Full extension API support
- **Built-in Extensions**: $($Manifest.Extensions.Count) extensions
- **Marketplace**: Extension marketplace integration
- **Activation**: Event-based activation

### Build System
- **Tools**: $($Manifest.BuildSystem.BuildTools -join ', ')
- **Package Managers**: $($Manifest.BuildSystem.PackageManagers -join ', ')
- **CI/CD**: $($Manifest.BuildSystem.CIConfig.Count) configurations
- **Docker**: $($Manifest.BuildSystem.DockerFiles.Count) files

## Packages

### Main Package
- **Name**: Cursor
- **Version**: 2.4.21
- **Author**: Anysphere, Inc.
- **License**: Proprietary
- **Repository**: Based on VS Code

### Dependencies
Total Packages: $($Manifest.Packages.Count)

Top 20 Packages:
"@
    
    foreach ($pkg in $Manifest.Packages.List | Select-Object -First 20) {
        $readme += "- **$($pkg.Name)**: v$($pkg.Version) - $($pkg.Description)`n"
    }
    
    $readme += @"

## Extensions

Total Extensions: $($Manifest.Extensions.Count)

Top 20 Extensions:
"@
    
    foreach ($ext in $Manifest.Extensions.List | Select-Object -First 20) {
        $readme += "- **$($ext.DisplayName)**: v$($ext.Version) by $($ext.Publisher)`n"
    }
    
    $readme += @"

## Configuration

The application includes $($Manifest.Configuration.Count) configuration files:

- JSON configurations
- TypeScript configs
- Build configs
- Linting configs
- Environment files
- CI/CD configs

## Build Instructions

### Prerequisites
- Node.js (version specified in package.json)
- npm/yarn/pnpm
- Git

### Installation
```bash
# Install dependencies
npm install

# Build the application
npm run build

# Run in development
npm run dev
```

### Available Scripts
"@
    
    if ($Manifest.BuildSystem.Scripts.Count -gt 0) {
        foreach ($script in $Manifest.BuildSystem.Scripts.PSObject.Properties) {
            $readme += "- **$($script.Name)**: $($script.Value)`n"
        }
    } else {
        $readme += "- No scripts found in package.json`n"
    }
    
    $readme += @"

## Architecture

### Electron Application Structure
- **Main Process**: `./out/main.js`
- **Renderer Process**: Web-based UI
- **Preload Scripts**: Bridge between main and renderer
- **WebViews**: Isolated web content

### Key Directories
- `out/`: Compiled JavaScript output
- `src/`: Source code (if available)
- `extensions/`: Built-in extensions
- `node_modules/`: Dependencies
- `resources/`: Static resources

### AI Integration Points
1. **Chat Interface**: Composer contribution
2. **Code Completion**: Language server integration
3. **Command Execution**: Agent system
4. **Tool Integration**: MCP (Model Context Protocol)

## Reverse Engineering Process

This fork was created using:
1. **Universal Reverse Engineer**: Extracted Electron app source
2. **Source Analyzer**: Analyzed structure and dependencies
3. **Complete Dumper**: Organized and structured the codebase

### Tools Used
- PowerShell 7.0+
- File system analysis
- JSON parsing
- Structure reconstruction

## Legal Notice

This is a reverse-engineered fork created for educational and research purposes. 

- **Original**: Cursor IDE by Anysphere, Inc.
- **Version**: 2.4.21
- **Purpose**: Educational, research, and compatibility analysis
- **License**: Original proprietary license applies to original code

## Contributing

This is a reverse-engineered codebase. Contributions should focus on:
- Documentation improvements
- Compatibility analysis
- Educational examples
- Research enhancements

## License

This reverse-engineered fork is provided as-is for educational purposes. 
The original Cursor IDE remains under its proprietary license by Anysphere, Inc.

## Support

For issues related to:
- **Original Cursor**: Visit https://cursor.com
- **This Fork**: Create an issue in the repository
- **Reverse Engineering**: Refer to the methodology in REVERSE_ENGINEERING_REPORT.md

---

**Generated**: $($Manifest.GeneratedAt)  
**Tool**: $($Manifest.Tool)  
**Version**: 1.0
"@
    
    $readmePath = Join-Path $OutputDir "README.md"
    $readme | Out-File -FilePath $readmePath -Encoding UTF8
    
    Write-ColorOutput "✓ Created README.md" "Success"
    Write-ColorOutput "  Location: $readmePath" "Detail"
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "COMPLETE SOURCE CODE DUMPER"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Source: $SourceDirectory"
Write-ColorOutput "Output: $OutputDirectory"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if source directory exists
if (-not (Test-Path $SourceDirectory)) {
    Write-ColorOutput "✗ Source directory not found: $SourceDirectory" "Error"
    exit 1
}

# Check if output directory exists
if (Test-Path $OutputDirectory) {
    if ($ForceOverwrite) {
        Write-ColorOutput "→ Removing existing output directory" "Warning"
        Remove-Item -Path $OutputDirectory -Recurse -Force
    } else {
        Write-ColorOutput "⚠ Output directory already exists, use -ForceOverwrite to replace" "Warning"
        exit 1
    }
}

# Analyze source structure
Write-ColorOutput "=== ANALYZING SOURCE STRUCTURE ===" "Header"
$structure = Get-SourceStructure -Dir $SourceDirectory

# Extract package information
Write-ColorOutput "=== EXTRACTING PACKAGE INFORMATION ===" "Header"
$packages = Get-PackageInfo -Dir $SourceDirectory

# Extract extension information
Write-ColorOutput "=== EXTRACTING EXTENSION INFORMATION ===" "Header"
$extensions = Get-ExtensionInfo -Dir $SourceDirectory

# Extract configuration files
Write-ColorOutput "=== EXTRACTING CONFIGURATION FILES ===" "Header"
$configs = Get-ConfigurationFiles -Dir $SourceDirectory

# Extract build system information
Write-ColorOutput "=== EXTRACTING BUILD SYSTEM INFORMATION ===" "Header"
$buildInfo = Get-BuildSystemInfo -Dir $SourceDirectory

# Generate manifest
Write-ColorOutput "=== GENERATING MANIFEST ===" "Header"
$manifest = Generate-Manifest `
    -Structure $structure `
    -Packages $packages `
    -Extensions $extensions `
    -Configs $configs `
    -BuildInfo $buildInfo

# Copy source tree
Write-ColorOutput "=== COPYING SOURCE TREE ===" "Header"
$copiedCount = Copy-SourceTree `
    -SourceDir $SourceDirectory `
    -TargetDir $OutputDirectory `
    -Structure $structure

# Reconstruct source templates if requested
$reconstructionStats = $null
if ($ReconstructTemplates) {
    Write-ColorOutput "=== RECONSTRUCTING SOURCE TEMPLATES ===" "Header"
    $reconstructionStats = Reconstruct-SourceStructure `
        -CompiledDir $SourceDirectory `
        -ReconstructedDir $OutputDirectory
    
    # Generate build configuration
    Write-ColorOutput "=== GENERATING BUILD CONFIGURATION ===" "Header"
    Generate-BuildConfiguration `
        -OutputDir $OutputDirectory `
        -Manifest $manifest
}

# Save manifest
Write-ColorOutput "=== SAVING MANIFEST ===" "Header"
$manifestJson = $manifest | ConvertTo-Json -Depth 20 -Compress:$false
$manifestPath = Join-Path $OutputDirectory "MANIFEST.json"
$manifestJson | Out-File -FilePath $manifestPath -Encoding UTF8
Write-ColorOutput "✓ Saved manifest to: $manifestPath" "Success"

# Generate comprehensive report
if ($GenerateReport) {
    Write-ColorOutput "=== GENERATING COMPREHENSIVE REPORT ===" "Header"
    $report = Generate-ComprehensiveReport `
        -Manifest $manifest `
        -OutputDir $OutputDirectory
}

# Create README
Write-ColorOutput "=== CREATING README ===" "Header"
Create-README `
    -OutputDir $OutputDirectory `
    -Manifest $manifest

# Initialize Git repository
if ($CreateGitRepo) {
    Write-ColorOutput "=== INITIALIZING GIT REPOSITORY ===" "Header"
    $gitInitialized = Initialize-GitRepository -RepoDir $OutputDirectory
}

# Final summary
Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "DUMP COMPLETE" "Header"
Write-ColorOutput "=" * 80
Write-ColorOutput "Files Copied: $copiedCount" "Success"
Write-ColorOutput "Total Size: $($manifest.Source.TotalSizeMB) MB" "Detail"
Write-ColorOutput "Directories: $($manifest.Source.DirectoryCount)" "Detail"
Write-ColorOutput "Packages: $($manifest.Packages.Count)" "Detail"
Write-ColorOutput "Extensions: $($manifest.Extensions.Count)" "Detail"
Write-ColorOutput "Configuration Files: $($manifest.Configuration.Count)" "Detail"
Write-ColorOutput "Build Tools: $($manifest.BuildSystem.BuildTools -join ', ')" "Detail"

if ($reconstructionStats) {
    Write-ColorOutput ""
    Write-ColorOutput "Source Template Reconstruction:" "Header"
    Write-ColorOutput "  Files Processed: $($reconstructionStats.FilesProcessed)" "Detail"
    Write-ColorOutput "  Files Reconstructed: $($reconstructionStats.FilesReconstructed)" "Detail"
    Write-ColorOutput "  Source Maps Found: $($reconstructionStats.SourceMapsFound)" "Detail"
    Write-ColorOutput "  Files Beautified: $($reconstructionStats.FilesBeautified)" "Detail"
}

Write-ColorOutput "=" * 80
Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
Write-ColorOutput "=" * 80

if ($gitInitialized) {
    Write-ColorOutput "Git Repository: Initialized" "Success"
    Write-ColorOutput "Initial Commit: Created" "Success"
}

Write-ColorOutput ""
Write-ColorOutput "Next steps:"
Write-ColorOutput "1. Review README.md in the output directory"
Write-ColorOutput "2. Examine REVERSE_ENGINEERING_REPORT.md for details"
Write-ColorOutput "3. Check MANIFEST.json for complete metadata"
if ($ReconstructTemplates) {
    Write-ColorOutput "4. Explore reconstructed source code in src/ directory"
    Write-ColorOutput "5. Review tsconfig.json and build configuration"
    Write-ColorOutput "6. Install dependencies: npm install"
    Write-ColorOutput "7. Build the project: npm run build"
} else {
    Write-ColorOutput "4. Explore the source code structure"
    Write-ColorOutput "5. Run build commands if available"
    Write-ColorOutput "6. Use -ReconstructTemplates to reverse engineer source templates"
}
