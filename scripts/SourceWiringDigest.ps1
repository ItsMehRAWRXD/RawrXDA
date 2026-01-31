#!/usr/bin/env pwsh
<#[
.SYNOPSIS
    Source wiring digest for IDE modules
.DESCRIPTION
    Scans PowerShell source files to determine which modules are wired (imported/used)
    and which are not referenced. Produces a summary and detailed report.
.EXAMPLE
    .\SourceWiringDigest.ps1 -RootPath "d:\lazy init ide" -ModuleDir "d:\lazy init ide\scripts"
#>

[CmdletBinding()]
param(
    [string]$RootPath = "d:\lazy init ide",
    [string]$ModuleDir = "d:\lazy init ide\scripts",
    [string]$ReportPath = (Join-Path $PSScriptRoot "..\WIRING_DIGEST_REPORT.md"),
    [string]$GraphPath = (Join-Path $PSScriptRoot "..\WIRING_DEPENDENCY_GRAPH.dot"),
    [string]$CallGraphPath = (Join-Path $PSScriptRoot "..\WIRING_CALL_GRAPH.dot"),
    [string[]]$IncludeExtensions = @('ps1','psm1','ts','js','tsx','jsx','cpp','cxx','c','h','hpp','cmake','txt','md','json','toml'),
    [string[]]$ExcludeDirectories = @('.git','.vs','.vscode','node_modules','dist','build','out','bin','obj','target','.idea','.cache','__pycache__'),
    [string]$ExcludeConfigPath = (Join-Path $RootPath ".wiringdigestignore"),
    [string]$RulesConfigPath = (Join-Path $RootPath ".wiringdigestrules.json"),
    [string]$RulesProfile = "default",
    [ValidateSet('BaseFirst','ProfileFirst')]
    [string]$ModuleOverridePrecedence = 'BaseFirst',
    [ValidateSet('GlobFirst','RegexFirst')]
    [string]$PathMatchPrecedence = 'RegexFirst',
    [ValidateSet('Low','Medium','High')]
    [string]$RiskThreshold = 'High',
    [switch]$WarnOnly,
    [int]$MaxSecuritySignals = -1,
    [int]$MaxSecrets = -1,
    [int]$MaxBinaryBlobs = -1,
    [int]$MaxLargeFiles = -1,
    [int]$MaxUnusedFiles = -1
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$rules = $null
$moduleOverrides = $null
$inheritModuleOverrides = $true
$overrideMode = 'merge'

function Get-RulePropertyValue {
    param(
        $Object,
        [string]$Name
    )

    if (-not $Object) { return $null }
    $prop = $Object.PSObject.Properties[$Name]
    if ($prop) { return $prop.Value }
    return $null
}

function Merge-Override {
    param(
        [hashtable]$Existing,
        $Incoming,
        [string]$Mode
    )

    if (-not $Existing -or $Mode -eq 'replace') {
        return $Incoming
    }

    $result = $Existing
    foreach ($prop in $Incoming.PSObject.Properties.Name) {
        $incomingValue = $Incoming.$prop
        if ($Mode -eq 'append' -and $incomingValue -is [System.Array]) {
            $existingValue = $result.$prop
            $result.$prop = @($existingValue + $incomingValue)
        } else {
            $result.$prop = $incomingValue
        }
    }

    return $result
}

if (Test-Path $RulesConfigPath) {
    try {
        $rules = Get-Content $RulesConfigPath -Raw | ConvertFrom-Json

        $resolvedRules = $rules
        $profileRules = Get-RulePropertyValue -Object $rules.profiles -Name $RulesProfile
        if ($profileRules) { $resolvedRules = $profileRules }

        if ($resolvedRules.IncludeExtensions) { $IncludeExtensions = @($resolvedRules.IncludeExtensions) }
        if ($resolvedRules.ExcludeDirectories) { $ExcludeDirectories = @($resolvedRules.ExcludeDirectories) }
        if ($resolvedRules.ExcludeConfigPath) { $ExcludeConfigPath = $resolvedRules.ExcludeConfigPath }
        $resolvedExcludeProfiles = Get-RulePropertyValue -Object $resolvedRules -Name 'ExcludeConfigPathProfiles'
        $resolvedExcludeProfile = Get-RulePropertyValue -Object $resolvedExcludeProfiles -Name $RulesProfile
        if ($resolvedExcludeProfile) {
            $ExcludeConfigPath = $resolvedExcludeProfile
        } else {
            $baseExcludeProfiles = Get-RulePropertyValue -Object $rules -Name 'ExcludeConfigPathProfiles'
            $baseExcludeProfile = Get-RulePropertyValue -Object $baseExcludeProfiles -Name $RulesProfile
            if ($baseExcludeProfile) { $ExcludeConfigPath = $baseExcludeProfile }
        }

        if ($resolvedRules.PathMatchPrecedence) { $PathMatchPrecedence = $resolvedRules.PathMatchPrecedence }
        if ($resolvedRules.ModuleOverridePrecedence) { $ModuleOverridePrecedence = $resolvedRules.ModuleOverridePrecedence }
        if ($null -ne $resolvedRules.InheritModuleOverrides) { $inheritModuleOverrides = [bool]$resolvedRules.InheritModuleOverrides }
        if ($resolvedRules.OverrideMode) { $overrideMode = $resolvedRules.OverrideMode }
        if ($resolvedRules.RiskThreshold) { $RiskThreshold = $resolvedRules.RiskThreshold }
        if ($null -ne $resolvedRules.WarnOnly) { $WarnOnly = [bool]$resolvedRules.WarnOnly }
        if ($null -ne $resolvedRules.MaxSecuritySignals) { $MaxSecuritySignals = [int]$resolvedRules.MaxSecuritySignals }
        if ($null -ne $resolvedRules.MaxSecrets) { $MaxSecrets = [int]$resolvedRules.MaxSecrets }
        if ($null -ne $resolvedRules.MaxBinaryBlobs) { $MaxBinaryBlobs = [int]$resolvedRules.MaxBinaryBlobs }
        if ($null -ne $resolvedRules.MaxLargeFiles) { $MaxLargeFiles = [int]$resolvedRules.MaxLargeFiles }
        if ($null -ne $resolvedRules.MaxUnusedFiles) { $MaxUnusedFiles = [int]$resolvedRules.MaxUnusedFiles }

        $baseOverrides = @{}
        $profileOverrides = @{}

        if ($inheritModuleOverrides) {
            $baseOverrideCandidates = Get-RulePropertyValue -Object $rules -Name 'moduleOverrides'
            if ($baseOverrideCandidates) { $baseOverrides = $baseOverrideCandidates }
        }
        $baseOverridesProfiles = Get-RulePropertyValue -Object $rules -Name 'moduleOverridesProfiles'
        $baseProfileOverrides = Get-RulePropertyValue -Object $baseOverridesProfiles -Name $RulesProfile
        if ($baseProfileOverrides) {
            foreach ($key in $baseProfileOverrides.PSObject.Properties.Name) {
                $profileOverrides[$key] = Get-RulePropertyValue -Object $baseProfileOverrides -Name $key
            }
        }
        $resolvedModuleOverrides = Get-RulePropertyValue -Object $resolvedRules -Name 'moduleOverrides'
        if ($resolvedModuleOverrides) {
            foreach ($key in $resolvedModuleOverrides.PSObject.Properties.Name) {
                $profileOverrides[$key] = Get-RulePropertyValue -Object $resolvedModuleOverrides -Name $key
            }
        }
        $resolvedOverridesProfiles = Get-RulePropertyValue -Object $resolvedRules -Name 'moduleOverridesProfiles'
        $resolvedProfileOverrides = Get-RulePropertyValue -Object $resolvedOverridesProfiles -Name $RulesProfile
        if ($resolvedProfileOverrides) {
            foreach ($key in $resolvedProfileOverrides.PSObject.Properties.Name) {
                $profileOverrides[$key] = Get-RulePropertyValue -Object $resolvedProfileOverrides -Name $key
            }
        }

        $moduleOverrides = @{}
        if ($ModuleOverridePrecedence -eq 'BaseFirst') {
            foreach ($key in $baseOverrides.PSObject.Properties.Name) {
                $moduleOverrides[$key] = Get-RulePropertyValue -Object $baseOverrides -Name $key
            }
            foreach ($key in $profileOverrides.Keys) {
                $mode = $overrideMode
                $currentOverride = $profileOverrides[$key]
                if ($currentOverride -and $currentOverride.OverrideMode) { $mode = $currentOverride.OverrideMode }
                $moduleOverrides[$key] = Merge-Override -Existing $moduleOverrides[$key] -Incoming $currentOverride -Mode $mode
            }
        } else {
            foreach ($key in $profileOverrides.Keys) {
                $moduleOverrides[$key] = $profileOverrides[$key]
            }
            foreach ($key in $baseOverrides.PSObject.Properties.Name) {
                $mode = $overrideMode
                $baseOverride = Get-RulePropertyValue -Object $baseOverrides -Name $key
                if ($baseOverride -and $baseOverride.OverrideMode) { $mode = $baseOverride.OverrideMode }
                $moduleOverrides[$key] = Merge-Override -Existing $moduleOverrides[$key] -Incoming $baseOverride -Mode $mode
            }
        }
    } catch {
        Write-Warning "Failed to parse rules config: $RulesConfigPath"
        Write-Warning "Parse error: $($_.Exception.Message)"
        if ($_.InvocationInfo) {
            Write-Warning "Error occurred at line $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
        }
        if ($_.ScriptStackTrace) {
            Write-Warning "Stack: $($_.ScriptStackTrace)"
        }
    }
}

function Get-ModuleInventory {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "ModuleDir not found: $Path"
    }

    Get-ChildItem -Path $Path -Filter *.psm1 | ForEach-Object {
        [PSCustomObject]@{
            Name = $_.BaseName
            Path = $_.FullName
        }
    }
}

function Get-SourceFiles {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "RootPath not found: $Path"
    }

    $patterns = $IncludeExtensions | ForEach-Object { "*.$_" }

    $projectExcludes = @()
    $projectExcludePatterns = @()
    $projectExcludeRegex = @()
    $profileIgnorePath = if ($RulesProfile -and $RulesProfile -ne 'default') { "$ExcludeConfigPath.$RulesProfile" } else { $null }
    $ignoreSources = @($ExcludeConfigPath)
    if ($profileIgnorePath) { $ignoreSources += $profileIgnorePath }

    foreach ($ignoreFile in $ignoreSources) {
        if (-not $ignoreFile) { continue }
        if (-not (Test-Path $ignoreFile)) { continue }

        $lines = Get-Content $ignoreFile -ErrorAction SilentlyContinue | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') }
        if ($lines.Count -eq 0) { continue }

        $projectExcludeRegex += $lines | Where-Object { $_ -like 'regex:*' } | ForEach-Object { $_.Substring(6) }
        $projectExcludePatterns += $lines | Where-Object { $_ -match '[\*\?]' -and $_ -notlike 'regex:*' }
        $projectExcludes += $lines | Where-Object { $_ -notmatch '[\*\?]' -and $_ -notlike 'regex:*' }
    }

    $allExcludes = @($ExcludeDirectories + $projectExcludes)
    $excludePattern = ($allExcludes | ForEach-Object { [regex]::Escape($_) }) -join '|'
    $excludeRegex = if ($excludePattern) { "[\\/]($excludePattern)[\\/]" } else { $null }

    function Test-ExcludePattern {
        param(
            [string]$FilePath,
            [string[]]$Patterns,
            [string[]]$RegexPatterns
        )

        if (-not $Patterns -or $Patterns.Count -eq 0) {
            return $false
        }

        $normalized = $FilePath.ToLowerInvariant().Replace('/', '\\')
        foreach ($pattern in $Patterns) {
            $p = $pattern.ToLowerInvariant().Replace('/', '\\')
            $p = $p -replace '\*\*', '*'
            if ($normalized -like "*$p*" -or ([System.IO.Path]::GetFileName($normalized) -like $p)) {
                return $true
            }
        }

        foreach ($regex in $RegexPatterns) {
            if ($normalized -match $regex) { return $true }
        }

        return $false
    }

    Get-ChildItem -Path $Path -Recurse -Include $patterns -File | Where-Object {
        if ($excludeRegex -and $_.FullName -match $excludeRegex) { return $false }
        if (Test-ExcludePattern -FilePath $_.FullName -Patterns $projectExcludePatterns -RegexPatterns $projectExcludeRegex) { return $false }
        return $true
    }
}

function Find-ModuleReferences {
    param(
        [System.IO.FileInfo[]]$Files,
        [string]$ModuleName,
        [string[]]$ExcludePatterns = @(),
        [string[]]$ExcludeRegex = @()
    )
    $escapedName = [regex]::Escape($ModuleName)
    $strongPatterns = @(
        ('Import-Module\s+[''\"]?[^''\"]*{0}\.psm1' -f $escapedName),
        ('Import-Module\s+{0}\b' -f $escapedName),
        ('using\s+module\s+[''\"]?[^''\"]*{0}\.psm1' -f $escapedName)
    )
    $weakPatterns = @(
        ('Get-Module\s+.*{0}' -f $escapedName),
        ('Get-Command\s+.*-Module\s+{0}' -f $escapedName),
        ('{0}' -f $escapedName)
    )

    $hits = @()
    foreach ($file in $Files) {
        if ($ExcludePatterns -and $file.FullName) {
            $skip = $false
            foreach ($pattern in $ExcludePatterns) {
                if ($file.FullName -like "*$pattern*") { $skip = $true; break }
            }
            if ($skip) { continue }
        }
        if ($ExcludeRegex -and $file.FullName) {
            $skip = $false
            foreach ($pattern in $ExcludeRegex) {
                if ($file.FullName -match $pattern) { $skip = $true; break }
            }
            if ($skip) { continue }
        }
        foreach ($pattern in $strongPatterns) {
            $matches = Select-String -Path $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
            if ($matches) {
                $hits += $matches | ForEach-Object {
                    [PSCustomObject]@{
                        Path = $_.Path
                        LineNumber = $_.LineNumber
                        Line = $_.Line.Trim()
                        Strength = 'Strong'
                    }
                }
            }
        }
        foreach ($pattern in $weakPatterns) {
            $matches = Select-String -Path $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
            if ($matches) {
                $hits += $matches | ForEach-Object {
                    [PSCustomObject]@{
                        Path = $_.Path
                        LineNumber = $_.LineNumber
                        Line = $_.Line.Trim()
                        Strength = 'Weak'
                    }
                }
            }
        }
    }

    return $hits
}

function Get-LanguageBreakdown {
    param([System.IO.FileInfo[]]$Files)

    $groups = $Files | Group-Object Extension
    $breakdown = @{}
    foreach ($group in $groups) {
        $ext = if ($group.Name) { $group.Name.TrimStart('.').ToLowerInvariant() } else { 'unknown' }
        $breakdown[$ext] = $group.Count
    }

    return $breakdown
}

function Get-ExportedFunctions {
    param([string]$ModulePath)

    if (-not (Test-Path $ModulePath)) {
        return @()
    }

    $content = Get-Content $ModulePath -Raw
    $exportBlocks = @()
    $matches = [regex]::Matches($content, 'Export-ModuleMember\s+-Function\s+@\((?<block>[^\)]*)\)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

    foreach ($match in $matches) {
        $block = $match.Groups['block'].Value
        if ($block) {
            $exportBlocks += $block
        }
    }

    $exports = @()
    foreach ($block in $exportBlocks) {
        $items = $block -split '[,\r\n]+' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
        foreach ($item in $items) {
            $clean = $item.Trim('"', "'", "`"")
            if ($clean) {
                $exports += $clean
            }
        }
    }

    return $exports | Sort-Object -Unique
}

function Find-ExportReferences {
    param(
        [System.IO.FileInfo[]]$Files,
        [string]$FunctionName
    )

    $escaped = [regex]::Escape($FunctionName)
    $pattern = "\b$escaped\b"
    $matches = Select-String -Path $Files.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
    return @($matches)
}

function Get-ModuleImportEdges {
    param([System.IO.FileInfo[]]$Files)

    $edges = @()
    $importPatterns = @(
        @{ Label = 'import'; Pattern = 'Import-Module\s+[''\"]?(?<name>[\w\.-]+)' },
        @{ Label = 'using'; Pattern = 'using\s+module\s+[''\"]?(?<name>[\w\.-]+)' }
    )

    foreach ($file in $Files) {
        foreach ($item in $importPatterns) {
            $matches = Select-String -Path $file.FullName -Pattern $item.Pattern -AllMatches -ErrorAction SilentlyContinue
            foreach ($match in $matches) {
                foreach ($m in $match.Matches) {
                    $name = $m.Groups['name'].Value
                    if ($name) {
                        $edges += [PSCustomObject]@{
                            Source = $file.FullName
                            Target = $name
                            Label = $item.Label
                        }
                    }
                }
            }
        }
    }

    return $edges
}

function Get-FunctionDefinitions {
    param([System.IO.FileInfo[]]$Files)

    $defs = @()
    $pattern = 'function\s+(?<name>[A-Za-z0-9_\-]+)\s*\{'

    foreach ($file in $Files) {
        if ($file.Extension -notin @('.ps1', '.psm1')) {
            continue
        }
        $matches = Select-String -Path $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            foreach ($m in $match.Matches) {
                $defs += [PSCustomObject]@{
                    File = $file.FullName
                    Name = $m.Groups['name'].Value
                }
            }
        }
    }

    return $defs
}

function Get-FunctionCallEdges {
    param(
        [System.IO.FileInfo[]]$Files,
        [string[]]$FunctionNames
    )

    $edges = @()
    if ($FunctionNames.Count -eq 0) {
        return $edges
    }

    $escapedNames = $FunctionNames | ForEach-Object { [regex]::Escape($_) }
    $callPattern = '\b(' + ($escapedNames -join '|') + ')\b'

    foreach ($file in $Files) {
        if ($file.Extension -notin @('.ps1', '.psm1')) {
            continue
        }
        $matches = Select-String -Path $file.FullName -Pattern $callPattern -AllMatches -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            foreach ($m in $match.Matches) {
                $edges += [PSCustomObject]@{
                    Source = $file.FullName
                    Target = $m.Value
                }
            }
        }
    }

    return $edges
}

function Get-GlobalWiringSignals {
    param([System.IO.FileInfo[]]$Files)

    $signals = @()

    $signalPatterns = @(
        @{ Label = 'CMake Targets'; Pattern = 'add_executable\(|add_library\(|target_link_libraries\(|target_include_directories\(' },
        @{ Label = 'CMake Subdir'; Pattern = 'add_subdirectory\(' },
        @{ Label = 'TS Imports'; Pattern = 'import\s+.*from\s+["\'']' },
        @{ Label = 'CommonJS Requires'; Pattern = 'require\s*\(' },
        @{ Label = 'VS Code Activation'; Pattern = '"activationEvents"\s*:' },
        @{ Label = 'VS Code Contributes'; Pattern = '"contributes"\s*:' },
        @{ Label = 'VS Code ExtensionDeps'; Pattern = '"extensionDependencies"\s*:' },
        @{ Label = 'Package Scripts'; Pattern = '"scripts"\s*:' },
        @{ Label = 'Feature Flags'; Pattern = 'FEATURE_FLAG|ENABLE_|DISABLE_' }
    )

    foreach ($file in $Files) {
        foreach ($item in $signalPatterns) {
            $matches = Select-String -Path $file.FullName -Pattern $item.Pattern -AllMatches -ErrorAction SilentlyContinue
            if ($matches) {
                $signals += $matches | ForEach-Object {
                    [PSCustomObject]@{
                        Label = $item.Label
                        Path = $_.Path
                        LineNumber = $_.LineNumber
                        Line = $_.Line.Trim()
                    }
                }
            }
        }
    }

    return $signals
}

function Get-FutureSignals {
    param([System.IO.FileInfo[]]$Files)

    $patterns = @(
        'TODO',
        'FIXME',
        'HACK',
        '@todo',
        '@fixme',
        'DEPRECATED',
        'DISABLED',
        'PENDING',
        'EXPERIMENTAL'
    )

    $hits = @()
    foreach ($file in $Files) {
        foreach ($pattern in $patterns) {
            $matches = Select-String -Path $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
            if ($matches) {
                $hits += $matches | ForEach-Object {
                    [PSCustomObject]@{
                        Tag = $pattern
                        Path = $_.Path
                        LineNumber = $_.LineNumber
                        Line = $_.Line.Trim()
                    }
                }
            }
        }
    }

    return $hits
}

function Get-ConfigSignals {
    param([System.IO.FileInfo[]]$Files)

    $signals = @()

    $configFiles = @(
        'package.json','tsconfig.json','jsconfig.json','cmake','CMakeLists.txt','vcpkg.json',
        'pyproject.toml','requirements.txt','Pipfile','Pipfile.lock','environment.yml',
        'Dockerfile','docker-compose.yml','docker-compose.yaml',
        '.env','.env.example','.gitignore','.gitattributes','README.md',
        '.github/workflows','azure-pipelines.yml','appveyor.yml','build.gradle','pom.xml'
    )

    foreach ($file in $Files) {
        $fileName = $file.Name
        if ($configFiles -contains $fileName -or $configFiles -contains $file.Extension.TrimStart('.')) {
            $signals += [PSCustomObject]@{
                Label = 'Config'
                Path = $file.FullName
                LineNumber = 0
                Line = $fileName
            }
        }
    }

    return $signals
}

function Get-BinaryBlobSignals {
    param([System.IO.FileInfo[]]$Files)

    $binaryExtensions = @(
        'exe','dll','bin','dat','pak','pdb','lib','a','so','dylib','zip','7z','rar','tar','gz',
        'png','jpg','jpeg','gif','bmp','ico','mp3','mp4','wav','flac','ogg','mov','avi','webm',
        'pdf','doc','docx','xls','xlsx','ppt','pptx'
    )

    $signals = @()
    foreach ($file in $Files) {
        $ext = $file.Extension.TrimStart('.').ToLowerInvariant()
        if ($binaryExtensions -contains $ext) {
            $sizeMB = [math]::Round(($file.Length / 1MB), 2)
            $signals += [PSCustomObject]@{
                Label = 'Binary'
                Path = $file.FullName
                Extension = $ext
                SizeMB = $sizeMB
            }
        }
    }

    return $signals
}

function Get-UnusedFileSignals {
    param(
        [System.IO.FileInfo[]]$Files,
        [System.IO.FileInfo[]]$ReferenceFiles,
        [string[]]$ExcludeRegex = @()
    )

    $referenceText = @()
    foreach ($file in $ReferenceFiles) {
        try {
            $referenceText += Get-Content $file.FullName -Raw -ErrorAction Stop
        } catch {
            continue
        }
    }

    $referenceBlob = $referenceText -join "\n"
    $signals = @()

    foreach ($file in $Files) {
        $fileName = $file.Name
        if (-not $referenceBlob.Contains($fileName)) {
            if ($ExcludeRegex -and $file.FullName) {
                $skip = $false
                foreach ($pattern in $ExcludeRegex) {
                    if ($file.FullName -match $pattern) { $skip = $true; break }
                }
                if ($skip) { continue }
            }
            $signals += [PSCustomObject]@{
                Path = $file.FullName
                Name = $fileName
            }
        }
    }

    return $signals
}

function Get-CICDHealthSignals {
    param([string]$Root)

    $signals = @()

    $githubWorkflows = Join-Path $Root ".github\workflows"
    if (-not (Test-Path $githubWorkflows)) {
        $signals += [PSCustomObject]@{ Label = 'Missing'; Path = $githubWorkflows; Detail = 'GitHub Actions workflows directory missing' }
    }

    $azurePipelines = Join-Path $Root "azure-pipelines.yml"
    if (-not (Test-Path $azurePipelines)) {
        $signals += [PSCustomObject]@{ Label = 'Missing'; Path = $azurePipelines; Detail = 'Azure Pipelines config missing' }
    }

    $appVeyor = Join-Path $Root "appveyor.yml"
    if (-not (Test-Path $appVeyor)) {
        $signals += [PSCustomObject]@{ Label = 'Missing'; Path = $appVeyor; Detail = 'AppVeyor config missing' }
    }

    $ciFiles = @(Get-ChildItem -Path $Root -Recurse -File -Include *.yml,*.yaml | Where-Object { $_.FullName -match 'workflows|ci|pipeline' })
    if ($ciFiles.Count -eq 0) {
        $signals += [PSCustomObject]@{ Label = 'Missing'; Path = $Root; Detail = 'No CI/CD configs detected' }
    }

    return $signals
}

function Get-SecuritySignals {
    param([System.IO.FileInfo[]]$Files)

    $signals = @()
    $patterns = @(
        @{ Label = 'Secret'; Pattern = '(?i)(api[_-]?key|secret|token|password)\s*[:=]\s*[^\s]+' },
        @{ Label = 'Network'; Pattern = '(?i)http[s]?://|Invoke-WebRequest|Invoke-RestMethod' },
        @{ Label = 'ProcessExec'; Pattern = '(?i)Start-Process|ProcessStartInfo|Invoke-Expression|\biex\b' },
        @{ Label = 'FileDelete'; Pattern = '(?i)Remove-Item\s+.*-Force' },
        @{ Label = 'Registry'; Pattern = '(?i)Registry::|Get-ItemProperty|Set-ItemProperty' }
    )

    foreach ($file in $Files) {
        foreach ($item in $patterns) {
            $matches = Select-String -Path $file.FullName -Pattern $item.Pattern -AllMatches -ErrorAction SilentlyContinue
            if ($matches) {
                $signals += $matches | ForEach-Object {
                    [PSCustomObject]@{
                        Label = $item.Label
                        Path = $_.Path
                        LineNumber = $_.LineNumber
                        Line = $_.Line.Trim()
                    }
                }
            }
        }
    }

    return $signals
}

function Get-LargeFileSignals {
    param([System.IO.FileInfo[]]$Files, [int]$ThresholdMB = 25)

    $signals = @()
    foreach ($file in $Files) {
        $sizeMB = [math]::Round(($file.Length / 1MB), 2)
        if ($sizeMB -ge $ThresholdMB) {
            $signals += [PSCustomObject]@{
                Label = 'LargeFile'
                Path = $file.FullName
                SizeMB = $sizeMB
            }
        }
    }
    return $signals
}

function Get-PackagingRiskScore {
    param(
        [int]$BinaryCount,
        [int]$LargeFileCount,
        [int]$SecuritySignalCount
    )

    $score = ($BinaryCount * 2) + ($LargeFileCount * 2) + ($SecuritySignalCount * 3)
    $level = if ($score -ge 20) { 'High' } elseif ($score -ge 8) { 'Medium' } else { 'Low' }

    return [PSCustomObject]@{
        Score = $score
        Level = $level
    }
}

$modules = @(Get-ModuleInventory -Path $ModuleDir)
$files = @(Get-SourceFiles -Path $RootPath)
$languageBreakdown = Get-LanguageBreakdown -Files $files

$results = @(
foreach ($module in $modules) {
    $moduleExcludePatterns = @()
    $moduleExcludeRegex = @()
    $moduleIgnorePath = Join-Path $RootPath ".wiringdigestignore.$($module.Name)"
    $moduleIgnoreProfilePath = if ($RulesProfile -and $RulesProfile -ne 'default') { Join-Path $RootPath ".wiringdigestignore.$($module.Name).$RulesProfile" } else { $null }
    if (Test-Path $moduleIgnorePath) {
        $lines = Get-Content $moduleIgnorePath -ErrorAction SilentlyContinue | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') }
        if ($lines.Count -gt 0) {
            $moduleExcludeRegex += $lines | Where-Object { $_ -like 'regex:*' } | ForEach-Object { $_.Substring(6) }
            $moduleExcludePatterns += $lines | Where-Object { $_ -match '[\*\?]' -and $_ -notlike 'regex:*' }
        }
    }
    if ($moduleIgnoreProfilePath -and (Test-Path $moduleIgnoreProfilePath)) {
        $lines = Get-Content $moduleIgnoreProfilePath -ErrorAction SilentlyContinue | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') }
        if ($lines.Count -gt 0) {
            $moduleExcludeRegex += $lines | Where-Object { $_ -like 'regex:*' } | ForEach-Object { $_.Substring(6) }
            $moduleExcludePatterns += $lines | Where-Object { $_ -match '[\*\?]' -and $_ -notlike 'regex:*' }
        }
    }
    $moduleOverrideConfig = $null
    if ($moduleOverrides -is [hashtable] -and $moduleOverrides.ContainsKey($module.Name)) {
        $moduleOverrideConfig = $moduleOverrides[$module.Name]
    }
    if ($moduleOverrideConfig -and $moduleOverrideConfig.ExcludePatterns) {
        $moduleExcludePatterns = @($moduleOverrideConfig.ExcludePatterns)
    }
    if ($moduleOverrideConfig -and $moduleOverrideConfig.ExcludeRegex) {
        $moduleExcludeRegex = @($moduleOverrideConfig.ExcludeRegex)
    }
    $hits = Find-ModuleReferences -Files $files -ModuleName $module.Name -ExcludePatterns $moduleExcludePatterns -ExcludeRegex $moduleExcludeRegex
    $strongHits = @($hits | Where-Object { $_.Strength -eq 'Strong' })
    $weakHits = @($hits | Where-Object { $_.Strength -eq 'Weak' })
    $score = ($strongHits.Count * 2) + ($weakHits.Count * 1)
    $signalLevel = if ($score -ge 6) { 'Strongly Wired' } elseif ($score -ge 2) { 'Weakly Wired' } else { 'Unwired' }

    $exports = @(Get-ExportedFunctions -ModulePath $module.Path)
    $exportRefs = @()
    $filesExcludingSelf = $files | Where-Object { $_.FullName -ne $module.Path }
    foreach ($fn in $exports) {
        $refs = @(Find-ExportReferences -Files $filesExcludingSelf -FunctionName $fn)
        $refCount = $refs.Count
        $exportRefs += [PSCustomObject]@{
            Function = $fn
            ReferenceCount = $refCount
        }
    }
    $unusedExports = @($exportRefs | Where-Object { $_.ReferenceCount -eq 0 })

    [PSCustomObject]@{
        Module = $module.Name
        Path = $module.Path
        ReferenceCount = ($hits | Measure-Object).Count
        StrongCount = $strongHits.Count
        WeakCount = $weakHits.Count
        SignalScore = $score
        SignalLevel = $signalLevel
        References = $hits | ForEach-Object { "$($_.Path):$($_.LineNumber) [$($_.Strength)]" }
        ExportCount = $exports.Count
        UnusedExports = @($unusedExports | ForEach-Object { $_.Function })
    }
}
)

$wired = @($results | Where-Object { $_.SignalLevel -ne 'Unwired' })
$unwired = @($results | Where-Object { $_.SignalLevel -eq 'Unwired' })
$globalSignals = @(Get-GlobalWiringSignals -Files $files)
$futureSignals = @(Get-FutureSignals -Files $files)
$configSignals = @(Get-ConfigSignals -Files $files)
$securitySignals = @(Get-SecuritySignals -Files $files)
$largeFileSignals = @(Get-LargeFileSignals -Files $files)
$binaryBlobSignals = @(Get-BinaryBlobSignals -Files $files)
$unusedFileSignals = @(Get-UnusedFileSignals -Files $files -ReferenceFiles $files)
$cicdSignals = @(Get-CICDHealthSignals -Root $RootPath)
$risk = Get-PackagingRiskScore -BinaryCount $binaryBlobSignals.Count -LargeFileCount $largeFileSignals.Count -SecuritySignalCount $securitySignals.Count
$secretSignals = @($securitySignals | Where-Object { $_.Label -eq 'Secret' })

$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

$report = @()
$report += "# Wiring Digest Report"
$report += "Generated: $timestamp"
$report += ""
$report += "## Summary"
$report += "- Total modules: $($results.Count)"
$report += "- Wired modules: $($wired.Count)"
$report += "- Unwired modules: $($unwired.Count)"
$report += "- Global wiring signals: $($globalSignals.Count)"
$report += "- Future-risk tags: $($futureSignals.Count)"
$report += "- Config signals: $($configSignals.Count)"
$report += "- Security signals: $($securitySignals.Count)"
$report += "- Large files (>=25MB): $($largeFileSignals.Count)"
$report += "- Binary blobs: $($binaryBlobSignals.Count)"
$report += "- Unused files: $($unusedFileSignals.Count)"
$report += "- CI/CD health signals: $($cicdSignals.Count)"
$report += "- Packaging risk score: $($risk.Score) ($($risk.Level))"
$report += "- Secrets detected: $($secretSignals.Count)"
$report += ""
$report += "## Per-Language Breakdown"
foreach ($key in ($languageBreakdown.Keys | Sort-Object)) {
    $report += "- .$($key): $($languageBreakdown[$key]) file(s)"
}
$report += ""
$report += "## Wired Modules"
foreach ($entry in $wired) {
    $report += "- $($entry.Module) ($($entry.ReferenceCount) reference(s), $($entry.SignalLevel), score=$($entry.SignalScore))"
}
$report += ""
$report += "## Unwired Modules"
foreach ($entry in $unwired) {
    $report += "- $($entry.Module)"
}
$report += ""
$report += "## Unused Exported Functions"
$unusedTotal = 0
foreach ($entry in $results) {
    if ($entry.UnusedExports.Count -gt 0) {
        $unusedTotal += $entry.UnusedExports.Count
        $report += "- $($entry.Module): $($entry.UnusedExports.Count) unused"
    }
}
if ($unusedTotal -eq 0) {
    $report += "- No unused exports detected"
}
$report += ""
$report += "## Global Wiring Signals"
if ($globalSignals.Count -eq 0) {
    $report += "- No global wiring signals found"
} else {
    foreach ($item in $globalSignals) {
        $report += "- [$($item.Label)] $($item.Path):$($item.LineNumber)"
    }
}
$report += ""
$report += "## Future-Risk Tags"
if ($futureSignals.Count -eq 0) {
    $report += "- No TODO/FIXME/DEPRECATED/EXPERIMENTAL tags found"
} else {
    foreach ($item in ($futureSignals | Select-Object -First 50)) {
        $report += "- [$($item.Tag)] $($item.Path):$($item.LineNumber)"
    }
    if ($futureSignals.Count -gt 50) {
        $report += "- ...($($futureSignals.Count - 50) more)"
    }
}
$report += ""
$report += "## Config & Manifest Signals"
if ($configSignals.Count -eq 0) {
    $report += "- No config signals found"
} else {
    foreach ($item in ($configSignals | Select-Object -First 50)) {
        $report += "- $($item.Path)"
    }
    if ($configSignals.Count -gt 50) {
        $report += "- ...($($configSignals.Count - 50) more)"
    }
}
$report += ""
$report += "## Security-Relevant Signals"
if ($securitySignals.Count -eq 0) {
    $report += "- No security signals found"
} else {
    foreach ($item in ($securitySignals | Select-Object -First 50)) {
        $report += "- [$($item.Label)] $($item.Path):$($item.LineNumber)"
    }
    if ($securitySignals.Count -gt 50) {
        $report += "- ...($($securitySignals.Count - 50) more)"
    }
}
$report += ""
$report += "## Large Files (>=25MB)"
if ($largeFileSignals.Count -eq 0) {
    $report += "- No large files detected"
} else {
    foreach ($item in ($largeFileSignals | Select-Object -First 50)) {
        $report += "- $($item.Path) ($($item.SizeMB) MB)"
    }
    if ($largeFileSignals.Count -gt 50) {
        $report += "- ...($($largeFileSignals.Count - 50) more)"
    }
}
$report += ""
$report += "## Binary Blob Signals"
if ($binaryBlobSignals.Count -eq 0) {
    $report += "- No binary blobs detected"
} else {
    foreach ($item in ($binaryBlobSignals | Select-Object -First 50)) {
        $report += "- $($item.Path) ($($item.SizeMB) MB, .$($item.Extension))"
    }
    if ($binaryBlobSignals.Count -gt 50) {
        $report += "- ...($($binaryBlobSignals.Count - 50) more)"
    }
}
$report += ""
$report += "## Unused Files (Name not referenced)"
if ($unusedFileSignals.Count -eq 0) {
    $report += "- No unused files detected"
} else {
    foreach ($item in ($unusedFileSignals | Select-Object -First 50)) {
        $report += "- $($item.Path)"
    }
    if ($unusedFileSignals.Count -gt 50) {
        $report += "- ...($($unusedFileSignals.Count - 50) more)"
    }
}
$report += ""
$report += "## CI/CD Health"
if ($cicdSignals.Count -eq 0) {
    $report += "- No CI/CD issues detected"
} else {
    foreach ($item in $cicdSignals) {
        $report += "- [$($item.Label)] $($item.Detail): $($item.Path)"
    }
}
$report += ""
$report += "## Packaging Risk Score"
$report += "- Score: $($risk.Score)"
$report += "- Level: $($risk.Level)"
$report += "- Factors: binary blobs ($($binaryBlobSignals.Count)), large files ($($largeFileSignals.Count)), security signals ($($securitySignals.Count))"
$report += "- Secrets detected: $($secretSignals.Count)"
$report += ""
$report += "## Module Overrides"
if ($rules) {
    $reportOverrides = Get-RulePropertyValue -Object $rules -Name 'moduleOverrides'
    if ($reportOverrides) {
        foreach ($key in $reportOverrides.PSObject.Properties.Name) {
            $override = Get-RulePropertyValue -Object $reportOverrides -Name $key
            $report += "- $($key): MaxUnusedExports=$($override.MaxUnusedExports)"
        }
    } else {
        $report += "- No module overrides defined"
    }
} else {
    $report += "- No module overrides defined"
}

function Resolve-ThresholdBreach {
    param(
        [string]$Label,
        [int]$Count,
        [int]$Max
    )

    if ($Max -lt 0) { return }
    if ($Count -gt $Max) {
        $message = "$Label threshold breached: $Count (max: $Max)"
        if ($WarnOnly) {
            Write-Warning $message
        } else {
            throw $message
        }
    }
}

$breached = $false
if ($RiskThreshold -eq 'Medium' -and $risk.Level -eq 'High') {
    $breached = $true
}
if ($RiskThreshold -eq 'Low' -and $risk.Level -in @('Medium','High')) {
    $breached = $true
}
if ($breached) {
    $message = "Packaging risk threshold breached: $($risk.Level) (threshold: $RiskThreshold)"
    if ($WarnOnly) {
        Write-Warning $message
    } else {
        throw $message
    }
}

if ($rules) {
    $rulesOverrides = Get-RulePropertyValue -Object $rules -Name 'moduleOverrides'
    if ($rulesOverrides) { $moduleOverrides = $rulesOverrides }
}

Resolve-ThresholdBreach -Label 'Security signals' -Count $securitySignals.Count -Max $MaxSecuritySignals
Resolve-ThresholdBreach -Label 'Secrets' -Count $secretSignals.Count -Max $MaxSecrets
Resolve-ThresholdBreach -Label 'Binary blobs' -Count $binaryBlobSignals.Count -Max $MaxBinaryBlobs
Resolve-ThresholdBreach -Label 'Large files' -Count $largeFileSignals.Count -Max $MaxLargeFiles
Resolve-ThresholdBreach -Label 'Unused files' -Count $unusedFileSignals.Count -Max $MaxUnusedFiles

if ($moduleOverrides) {
    foreach ($entry in $results) {
        if ($moduleOverrides -is [hashtable] -and $moduleOverrides.ContainsKey($entry.Module)) {
            $override = $moduleOverrides[$entry.Module]
            $pathRegex = @()
            $pathGlobs = @()
            if ($override.PathRegex) {
                $pathRegex = @($override.PathRegex)
            }
            if ($override.PathGlobs) {
                $pathGlobs = @($override.PathGlobs)
            }
            if ($pathRegex.Count -eq 0 -and $pathGlobs.Count -eq 0) {
                $pathRegex = @([regex]::Escape($entry.Module))
            }

            function Test-PathScope {
                param([string]$Path)
                $checkGlob = {
                    foreach ($glob in $pathGlobs) {
                        $normalized = $Path.ToLowerInvariant().Replace('/', '\\')
                        $g = $glob.ToLowerInvariant().Replace('/', '\\')
                        $g = $g -replace '\*\*', '*'
                        if ($normalized -like "*$g*" -or ([System.IO.Path]::GetFileName($normalized) -like $g)) { return $true }
                    }
                    return $false
                }
                $checkRegex = {
                    foreach ($pattern in $pathRegex) {
                        if ($Path -match $pattern) { return $true }
                    }
                    return $false
                }

                if ($PathMatchPrecedence -eq 'GlobFirst') {
                    if (& $checkGlob) { return $true }
                    if (& $checkRegex) { return $true }
                } else {
                    if (& $checkRegex) { return $true }
                    if (& $checkGlob) { return $true }
                }
                return $false
            }

            $scopedSecurity = @($securitySignals | Where-Object { Test-PathScope -Path $_.Path })

            $scopedSecrets = @($scopedSecurity | Where-Object { $_.Label -eq 'Secret' })
            $scopedBinaries = @($binaryBlobSignals | Where-Object {
                Test-PathScope -Path $_.Path
            })
            $scopedLargeFiles = @($largeFileSignals | Where-Object {
                Test-PathScope -Path $_.Path
            })
            $scopedUnusedFiles = @($unusedFileSignals | Where-Object {
                Test-PathScope -Path $_.Path
            })

            if ($null -ne $override.MaxUnusedExports) {
                $unusedCount = $entry.UnusedExports.Count
                if ($unusedCount -gt [int]$override.MaxUnusedExports) {
                    $message = "Module $($entry.Module) unused exports breached: $unusedCount (max: $($override.MaxUnusedExports))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
            if ($null -ne $override.MaxSecuritySignals) {
                if ($scopedSecurity.Count -gt [int]$override.MaxSecuritySignals) {
                    $message = "Module $($entry.Module) security signals breached: $($scopedSecurity.Count) (max: $($override.MaxSecuritySignals))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
            if ($null -ne $override.MaxSecrets) {
                if ($scopedSecrets.Count -gt [int]$override.MaxSecrets) {
                    $message = "Module $($entry.Module) secrets breached: $($scopedSecrets.Count) (max: $($override.MaxSecrets))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
            if ($null -ne $override.MaxBinaryBlobs) {
                if ($scopedBinaries.Count -gt [int]$override.MaxBinaryBlobs) {
                    $message = "Module $($entry.Module) binary blobs breached: $($scopedBinaries.Count) (max: $($override.MaxBinaryBlobs))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
            if ($null -ne $override.MaxLargeFiles) {
                if ($scopedLargeFiles.Count -gt [int]$override.MaxLargeFiles) {
                    $message = "Module $($entry.Module) large files breached: $($scopedLargeFiles.Count) (max: $($override.MaxLargeFiles))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
            if ($null -ne $override.MaxUnusedFiles) {
                if ($scopedUnusedFiles.Count -gt [int]$override.MaxUnusedFiles) {
                    $message = "Module $($entry.Module) unused files breached: $($scopedUnusedFiles.Count) (max: $($override.MaxUnusedFiles))"
                    if ($WarnOnly) { Write-Warning $message } else { throw $message }
                }
            }
        }
    }
}
$report += "## Reference Details"
foreach ($entry in $results) {
    $report += "### $($entry.Module)"
    if ($entry.ReferenceCount -eq 0) {
        $report += "- No references found"
    } else {
        foreach ($ref in $entry.References) {
            $report += "- $ref"
        }
    }
    $report += "- SignalLevel: $($entry.SignalLevel)"
    $report += "- Strong references: $($entry.StrongCount)"
    $report += "- Weak references: $($entry.WeakCount)"
    $report += "- Signal score: $($entry.SignalScore)"
    $report += "- Exported functions: $($entry.ExportCount)"
    if ($entry.UnusedExports.Count -gt 0) {
        $report += "- Unused exports: $($entry.UnusedExports -join ', ')"
    } else {
        $report += "- Unused exports: None"
    }
    $report += ""
}

$reportPathResolved = Resolve-Path (Split-Path $ReportPath -Parent) | Select-Object -ExpandProperty Path
$reportFullPath = Join-Path $reportPathResolved (Split-Path $ReportPath -Leaf)
$report | Set-Content -Path $reportFullPath -Encoding UTF8

$graphPathResolved = Resolve-Path (Split-Path $GraphPath -Parent) | Select-Object -ExpandProperty Path
$graphFullPath = Join-Path $graphPathResolved (Split-Path $GraphPath -Leaf)
$importEdges = Get-ModuleImportEdges -Files $files

$graphLines = @()
$graphLines += "digraph Wiring {"
$graphLines += "  rankdir=LR;"
$graphLines += "  node [shape=box];"
foreach ($edge in $importEdges) {
    $sourceLabel = [System.IO.Path]::GetFileName($edge.Source)
    $graphLines += ('  "{0}" -> "{1}" [label="{2}"];' -f $sourceLabel, $edge.Target, $edge.Label)
}
$graphLines += "}"
$graphLines | Set-Content -Path $graphFullPath -Encoding UTF8

$callGraphPathResolved = Resolve-Path (Split-Path $CallGraphPath -Parent) | Select-Object -ExpandProperty Path
$callGraphFullPath = Join-Path $callGraphPathResolved (Split-Path $CallGraphPath -Leaf)
$functionDefs = @(Get-FunctionDefinitions -Files $files)
$functionNames = @($functionDefs | Select-Object -ExpandProperty Name -Unique)
$callEdges = @(Get-FunctionCallEdges -Files $files -FunctionNames $functionNames)

$callGraphLines = @()
$callGraphLines += "digraph CallGraph {"
$callGraphLines += "  rankdir=LR;"
$callGraphLines += "  node [shape=ellipse];"
foreach ($edge in $callEdges) {
    $sourceLabel = [System.IO.Path]::GetFileName($edge.Source)
    $callGraphLines += ('  "{0}" -> "{1}";' -f $sourceLabel, $edge.Target)
}
$callGraphLines += "}"
$callGraphLines | Set-Content -Path $callGraphFullPath -Encoding UTF8

$dotExe = $null
try {
    $dotExe = Get-Command -Name dot -ErrorAction SilentlyContinue
} catch {
    $dotExe = $null
}

if ($dotExe) {
    $pngPath = [System.IO.Path]::ChangeExtension($graphFullPath, '.png')
    $svgPath = [System.IO.Path]::ChangeExtension($graphFullPath, '.svg')
    & $dotExe.Path -Tpng $graphFullPath -o $pngPath
    & $dotExe.Path -Tsvg $graphFullPath -o $svgPath

    $callPngPath = [System.IO.Path]::ChangeExtension($callGraphFullPath, '.png')
    $callSvgPath = [System.IO.Path]::ChangeExtension($callGraphFullPath, '.svg')
    & $dotExe.Path -Tpng $callGraphFullPath -o $callPngPath
    & $dotExe.Path -Tsvg $callGraphFullPath -o $callSvgPath
}

Write-Host "Wiring digest complete. Report: $reportFullPath" -ForegroundColor Green
Write-Host "Dependency graph: $graphFullPath" -ForegroundColor Green
Write-Host "Call graph: $callGraphFullPath" -ForegroundColor Green
if ($dotExe) {
    Write-Host "Graphviz PNG/SVG generated for dependency and call graphs" -ForegroundColor Green
}
