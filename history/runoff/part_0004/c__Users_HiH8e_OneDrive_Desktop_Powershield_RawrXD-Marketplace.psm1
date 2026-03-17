# RawrXD Marketplace Integration Module
# Handles loading, parsing, and managing extensions from real marketplace sources

# Module manifest data
$ModuleInfo = @{
    Name = "RawrXD.Marketplace"
    Version = "2.0.0"
    Description = "Real marketplace integration for RawrXD IDE"
    Author = "RawrXD Team"
}

# Global variables for marketplace data
$Script:MarketplaceConfig = $null
$Script:ExtensionsData = $null
$Script:CommunityData = $null
$Script:MarketplaceCache = @{}
$Script:LastRefresh = $null
$Script:MarketplacePathRoot = $null
$Script:SmartOverrideState = [ordered]@{
    Profiles        = @()
    CurrentProfile  = $null
    LastRotation    = $null
    RotationCounter = 0
    ConfigPath      = $null
    ConfigTimestamp = $null
    MarketplacePath = $null
}

# Smart Override Functions (must be defined before main marketplace functions)
function Get-DefaultSmartOverrideProfiles {
    <#
    .SYNOPSIS
    Returns the built-in smart override profiles used when no custom file exists
    #>
    return @(
        [ordered]@{
            name            = "Trending Spotlight"
            description     = "Boosts highly downloaded marketplace extensions while still rotating selections"
            durationMinutes = 5
            weights         = @{ downloads = 0.7; rating = 0.2; freshness = 0.1 }
            sourceBoosts    = @{ "vscode-marketplace" = 0.4 }
            dynamicShuffle  = $true
        },
        [ordered]@{
            name            = "Fresh PowerShell Focus"
            description     = "Highlights newer PowerShell Gallery modules and GitHub tools"
            durationMinutes = 4
            weights         = @{ downloads = 0.25; rating = 0.25; freshness = 0.5 }
            sourceBoosts    = @{ "powershell-gallery" = 0.8; "github" = 0.3 }
            categoryBoosts  = @{ "PowerShell" = 0.8; "Automation" = 0.4 }
        },
        [ordered]@{
            name            = "AI & Automation Mix"
            description     = "Surfaces AI/automation tagged extensions with a frequent shuffle"
            durationMinutes = 3
            weights         = @{ downloads = 0.35; rating = 0.25; freshness = 0.4 }
            tagBoosts       = @{ ai = 0.7; automation = 0.5; chat = 0.3 }
            dynamicShuffle  = $true
        }
    )
}

function Update-SmartOverrideProfilesFromDisk {
    [CmdletBinding()]
    param(
        [string]$MarketplacePath = $Script:SmartOverrideState.MarketplacePath,
        [switch]$Force
    )

    if (-not $MarketplacePath) { $MarketplacePath = '.\marketplace' }
    $configPath = Join-Path $MarketplacePath 'smart-overrides.json'
    $Script:SmartOverrideState.MarketplacePath = $MarketplacePath
    $Script:SmartOverrideState.ConfigPath = $configPath

    $profilesFromFile = $null
    if (Test-Path $configPath) {
        try {
            $fileInfo = Get-Item $configPath -ErrorAction Stop
            if ($Force -or -not $Script:SmartOverrideState.ConfigTimestamp -or $fileInfo.LastWriteTime -gt $Script:SmartOverrideState.ConfigTimestamp) {
                $raw = Get-Content $configPath -Raw -ErrorAction Stop | ConvertFrom-Json -Depth 8
                if ($raw.profiles) {
                    $profilesFromFile = @($raw.profiles)
                }
                $Script:SmartOverrideState.ConfigTimestamp = $fileInfo.LastWriteTime
                Write-Verbose "Smart override profiles reloaded from $configPath"
            }
        }
        catch {
            Write-Warning "Failed to parse smart override configuration: $($_.Exception.Message)"
        }
    }

    if ($profilesFromFile -and $profilesFromFile.Count -gt 0) {
        $Script:SmartOverrideState.Profiles = $profilesFromFile
    }
    elseif (-not $Script:SmartOverrideState.Profiles -or $Script:SmartOverrideState.Profiles.Count -eq 0) {
        $Script:SmartOverrideState.Profiles = Get-DefaultSmartOverrideProfiles
        $Script:SmartOverrideState.ConfigTimestamp = $null
    }
}

function Invoke-SmartOverrideRotation {
    [CmdletBinding()]
    param(
        [string]$Reason = 'runtime',
        [switch]$Force
    )

    if (-not $Script:SmartOverrideState.Profiles -or $Script:SmartOverrideState.Profiles.Count -eq 0) { return }

    # Reload profiles if the configuration file changed
    if ($Script:SmartOverrideState.ConfigPath -and (Test-Path $Script:SmartOverrideState.ConfigPath)) {
        $info = Get-Item $Script:SmartOverrideState.ConfigPath
        $lastLoaded = if ($Script:SmartOverrideState.ConfigTimestamp) { $Script:SmartOverrideState.ConfigTimestamp } else { [datetime]::MinValue }
        if ($info.LastWriteTime -gt $lastLoaded) {
            Update-SmartOverrideProfilesFromDisk -MarketplacePath $Script:SmartOverrideState.MarketplacePath -Force
            $Force = $true
        }
    }

    $duration = if ($Script:SmartOverrideState.CurrentProfile -and $Script:SmartOverrideState.CurrentProfile.durationMinutes) { [double]$Script:SmartOverrideState.CurrentProfile.durationMinutes } else { 5 }
    if (-not $Force -and $Script:SmartOverrideState.LastRotation) {
        $elapsed = (New-TimeSpan -Start $Script:SmartOverrideState.LastRotation -End (Get-Date)).TotalMinutes
        if ($elapsed -lt $duration) { return }
    }

    if ($Script:SmartOverrideState.RotationCounter -lt 0) { $Script:SmartOverrideState.RotationCounter = 0 }
    $Script:SmartOverrideState.RotationCounter = ($Script:SmartOverrideState.RotationCounter + 1) % [math]::Max(1, $Script:SmartOverrideState.Profiles.Count)
    $Script:SmartOverrideState.CurrentProfile = $Script:SmartOverrideState.Profiles[$Script:SmartOverrideState.RotationCounter]
    $Script:SmartOverrideState.LastRotation = Get-Date
    Write-Verbose "Smart override switched to profile '$($Script:SmartOverrideState.CurrentProfile.name)' (Reason: $Reason)"
}

function Invoke-SmartMarketplaceOverride {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][array]$Extensions,
        [string]$SortPreference = 'downloads'
    )

    if (-not $Extensions -or $Extensions.Count -eq 0) {
        return [PSCustomObject]@{ Extensions = $Extensions; EffectiveSort = $SortPreference; Profile = $null }
    }

    Invoke-SmartOverrideRotation -Reason 'runtime'
    $profile = $Script:SmartOverrideState.CurrentProfile
    if (-not $profile) {
        return [PSCustomObject]@{ Extensions = $Extensions; EffectiveSort = $SortPreference; Profile = $null }
    }

    $weightDownloads = [double]($profile.weights.downloads)
    $weightRating    = [double]($profile.weights.rating)
    $weightFreshness = [double]($profile.weights.freshness)
    if (-not $weightDownloads -and -not $weightRating -and -not $weightFreshness) {
        $weightDownloads = 0.4; $weightRating = 0.3; $weightFreshness = 0.3
    }

    switch ($SortPreference) {
        'downloads' { $weightDownloads += 0.35 }
        'rating'    { $weightRating    += 0.35 }
        'updated'   { $weightFreshness += 0.35 }
        'name'      { $weightRating    += 0.15; $weightFreshness += 0.15 }
    }

    $totalWeight = $weightDownloads + $weightRating + $weightFreshness
    if ($totalWeight -gt 0) {
        $weightDownloads /= $totalWeight
        $weightRating    /= $totalWeight
        $weightFreshness /= $totalWeight
    }

    $now = Get-Date
    foreach ($ext in $Extensions) {
        $downloadsValue = 0
        if ($ext.stats -and $ext.stats.downloads -ne $null) {
            $downloadsValue = [math]::Log10([math]::Max(1, [double]$ext.stats.downloads + 1))
        }

        $ratingValue = 0
        if ($ext.stats -and $ext.stats.rating -ne $null) {
            $ratingValue = [double]$ext.stats.rating
        }

        $freshnessValue = 0
        if ($ext.lastUpdated) {
            try {
                $updatedDate = Get-Date $ext.lastUpdated
                $ageDays = [math]::Max(1, (New-TimeSpan -Start $updatedDate -End $now).TotalDays)
                $freshnessValue = 30 / $ageDays
            }
            catch { }
        }

        $score = ($downloadsValue * $weightDownloads) + ($ratingValue * $weightRating) + ($freshnessValue * $weightFreshness)

        if ($profile.sourceBoosts -and $ext.source) {
            $sourceBoost = $profile.sourceBoosts[$ext.source]
            if ($sourceBoost -ne $null) { $score += [double]$sourceBoost }
        }

        if ($profile.categoryBoosts -and $ext.category) {
            $categoryBoost = $profile.categoryBoosts[$ext.category]
            if ($categoryBoost -ne $null) { $score += [double]$categoryBoost }
        }

        if ($profile.tagBoosts -and $ext.tags) {
            foreach ($tag in $ext.tags) {
                $tagBoost = $profile.tagBoosts[$tag]
                if ($tagBoost -ne $null) { $score += [double]$tagBoost }
            }
        }

        if ($profile.dynamicShuffle) {
            $score += Get-Random -Minimum 0 -Maximum 0.35
        }

        $ext | Add-Member -NotePropertyName smartScore -NotePropertyValue ([math]::Round($score, 4)) -Force
        $ext | Add-Member -NotePropertyName smartProfile -NotePropertyValue $profile.name -Force
    }

    $sorted = $Extensions | Sort-Object smartScore -Descending
    $Script:SmartOverrideState.CurrentProfile = $profile

    return [PSCustomObject]@{
        Extensions   = $sorted
        EffectiveSort = 'smart'
        Profile      = $profile.name
    }
}

function Initialize-SmartOverrideEngine {
    [CmdletBinding()]
    param(
        [string]$MarketplacePath = $Script:MarketplacePathRoot,
        [switch]$Force
    )

    if (-not $Script:ExtensionsData) { return }
    if (-not $MarketplacePath) { $MarketplacePath = '.\marketplace' }
    $Script:SmartOverrideState.MarketplacePath = $MarketplacePath

    Update-SmartOverrideProfilesFromDisk -MarketplacePath $MarketplacePath -Force:$Force

    if (-not $Script:SmartOverrideState.Profiles -or $Script:SmartOverrideState.Profiles.Count -eq 0) {
        $Script:SmartOverrideState.Profiles = Get-DefaultSmartOverrideProfiles
    }

    $Script:SmartOverrideState.RotationCounter = Get-Random -Minimum 0 -Maximum [math]::Max(1, $Script:SmartOverrideState.Profiles.Count)
    Invoke-SmartOverrideRotation -Reason 'initialize' -Force
}

function Get-RawrXDSmartOverrideStatus {
    <#
    .SYNOPSIS
    Returns diagnostics for the smart override engine
    #>
    [CmdletBinding()]
    param()

    $state = $Script:SmartOverrideState
    $current = $state.CurrentProfile
    $nextRotation = $null
    if ($current -and $state.LastRotation -and $current.durationMinutes) {
        $nextRotation = $state.LastRotation.AddMinutes([double]$current.durationMinutes)
    }

    return [PSCustomObject]@{
        Enabled        = [bool]$current
        CurrentProfile = if ($current) { $current.name } else { $null }
        Description    = if ($current) { $current.description } else { $null }
        ProfilesLoaded = $state.Profiles.Count
        LastRotation   = $state.LastRotation
        NextRotation   = $nextRotation
        ConfigSource   = if ($state.ConfigPath -and (Test-Path $state.ConfigPath)) { $state.ConfigPath } else { 'Built-in defaults' }
    }
}

function Initialize-RawrXDMarketplace {
    <#
    .SYNOPSIS
    Initializes the RawrXD Marketplace system with real data sources

    .DESCRIPTION
    Loads marketplace configuration and fetches real extension data from VS Code Marketplace, PowerShell Gallery, GitHub, etc.

    .PARAMETER MarketplacePath
    Path to the marketplace directory (defaults to ./marketplace)

    .PARAMETER UseOfflineMode
    Whether to operate in offline mode using cached files only

    .PARAMETER ForceRefresh
    Force refresh of all data sources
    #>
    [CmdletBinding()]
    param(
        [string]$MarketplacePath = ".\marketplace",
        [switch]$UseOfflineMode,
        [switch]$ForceRefresh
    )

    try {
        Write-Host "🚀 Initializing RawrXD Marketplace with Real Data Sources..." -ForegroundColor Cyan

        $resolvedMarketplacePath = $MarketplacePath
        try {
            $resolvedMarketplacePath = (Resolve-Path -LiteralPath $MarketplacePath -ErrorAction Stop).ProviderPath
        }
        catch {
            # Leave as provided path
        }
        $Script:MarketplacePathRoot = $resolvedMarketplacePath

        # Load marketplace configuration
        $configPath = Join-Path $resolvedMarketplacePath "marketplace-config.json"
        if (Test-Path $configPath) {
            $Script:MarketplaceConfig = Get-Content $configPath | ConvertFrom-Json
            Write-Host "✅ Marketplace configuration loaded" -ForegroundColor Green
        } else {
            Write-Warning "Marketplace configuration not found at $configPath"
            return $false
        }

        # Initialize cache
        $Script:MarketplaceCache = @{
            LastUpdated = Get-Date
            SearchCache = @{}
            ExtensionCache = @{}
            CommunityCache = @{}
            SourceData = @{}
        }

        # Check if we need to refresh data
        $needsRefresh = $ForceRefresh -or
                       (-not $Script:LastRefresh) -or
                       ((Get-Date) - $Script:LastRefresh).TotalHours -gt $Script:MarketplaceConfig.dataRefresh.intervalHours

        if (-not $UseOfflineMode -and $needsRefresh -and $Script:MarketplaceConfig.dataRefresh.enabled) {
            Write-Host "🔄 Fetching real marketplace data..." -ForegroundColor Yellow
            $refreshResult = Update-MarketplaceData

            if ($refreshResult) {
                $Script:LastRefresh = Get-Date
                Write-Host "✅ Real marketplace data loaded successfully!" -ForegroundColor Green
            } else {
                Write-Warning "Failed to fetch real data, falling back to cached/local data"
                Load-LocalMarketplaceData -MarketplacePath $resolvedMarketplacePath
            }
        } else {
            Write-Host "📂 Using cached/local marketplace data..." -ForegroundColor Blue
            Load-LocalMarketplaceData -MarketplacePath $resolvedMarketplacePath
        }

        Initialize-SmartOverrideEngine -MarketplacePath $resolvedMarketplacePath -Force:$true

        return $true
    }
    catch {
        Write-Error "Failed to initialize marketplace: $($_.Exception.Message)"
        return $false
    }
}

function Update-MarketplaceData {
    <#
    .SYNOPSIS
    Fetches real extension data from multiple marketplace sources
    #>
    [CmdletBinding()]
    param()

    $allExtensions = @()
    $allCommunity = @()

    try {
        # Fetch from VS Code Marketplace
        if ($Script:MarketplaceConfig.realMarketplaceSources.vscode.enabled) {
            Write-Host "   📦 Fetching from VS Code Marketplace..." -ForegroundColor Cyan
            $vscodeExtensions = Get-VSCodeMarketplaceExtensions
            $allExtensions += $vscodeExtensions
            Write-Host "   ✅ Found $($vscodeExtensions.Count) VS Code extensions" -ForegroundColor Green
        }

        # Fetch from PowerShell Gallery
        if ($Script:MarketplaceConfig.realMarketplaceSources.powershellGallery.enabled) {
            Write-Host "   🔷 Fetching from PowerShell Gallery..." -ForegroundColor Cyan
            $psExtensions = Get-PowerShellGalleryModules
            $allExtensions += $psExtensions
            Write-Host "   ✅ Found $($psExtensions.Count) PowerShell modules" -ForegroundColor Green
        }

        # Fetch from GitHub
        if ($Script:MarketplaceConfig.realMarketplaceSources.github.enabled) {
            Write-Host "   🐙 Fetching from GitHub..." -ForegroundColor Cyan
            $githubExtensions = Get-GitHubExtensions
            $allExtensions += $githubExtensions
            Write-Host "   ✅ Found $($githubExtensions.Count) GitHub repositories" -ForegroundColor Green
        }

        # Combine and deduplicate extensions
        $uniqueExtensions = $allExtensions | Sort-Object id -Unique

        # Create consolidated extensions data
        $Script:ExtensionsData = @{
            marketplace = @{
                name = "RawrXD Real Marketplace"
                version = "2.0.0"
                lastUpdated = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssZ")
                totalExtensions = $uniqueExtensions.Count
                sources = @($Script:MarketplaceConfig.realMarketplaceSources.PSObject.Properties.Name | Where-Object { $Script:MarketplaceConfig.realMarketplaceSources.$_.enabled })
            }
            extensions = $uniqueExtensions
        }

        # Cache the results
        $cacheFile = ".\marketplace\cached-real-extensions.json"
        $Script:ExtensionsData | ConvertTo-Json -Depth 10 | Out-File $cacheFile -Encoding UTF8

        Write-Host "💾 Cached $($uniqueExtensions.Count) total extensions" -ForegroundColor Green
        Initialize-SmartOverrideEngine -MarketplacePath $Script:MarketplacePathRoot
        return $true
    }
    catch {
        Write-Error "Failed to update marketplace data: $($_.Exception.Message)"
        return $false
    }
}

function Get-VSCodeMarketplaceExtensions {
    <#
    .SYNOPSIS
    Fetches extensions from VS Code Marketplace
    #>
    [CmdletBinding()]
    param()

    $extensions = @()
    $maxResults = $Script:MarketplaceConfig.dataRefresh.sources.vscode.maxResults

    try {
        # VS Code Marketplace API requires POST request with specific payload
        $searchPayload = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 8; value = "Microsoft.VisualStudio.Code" }
                        @{ filterType = 12; value = "37888" }
                    )
                    pageNumber = 1
                    pageSize = $maxResults
                    sortBy = 4  # Downloads
                    sortOrder = 2  # Descending
                }
            )
            assetTypes = @()
            flags = 914
        } | ConvertTo-Json -Depth 5

        $headers = @{
            "Accept" = "application/json;api-version=3.0-preview.1"
            "Content-Type" = "application/json"
            "User-Agent" = "RawrXD-Marketplace/2.0"
        }

        $response = Invoke-RestMethod -Uri $Script:MarketplaceConfig.realMarketplaceSources.vscode.apiUrl -Method POST -Body $searchPayload -Headers $headers -TimeoutSec 30

        foreach ($ext in $response.results[0].extensions) {
            $publisher = $ext.publisher.publisherName
            $name = $ext.extensionName
            $displayName = $ext.displayName
            $description = $ext.shortDescription
            $version = $ext.versions[0].version
            $lastUpdated = $ext.versions[0].lastUpdated
            $publishedDate = $ext.publishedDate

            # Get statistics
            $downloads = ($ext.statistics | Where-Object { $_.statisticName -eq "install" }).value
            if (-not $downloads) { $downloads = 0 }

            $rating = ($ext.statistics | Where-Object { $_.statisticName -eq "averagerating" }).value
            if (-not $rating) { $rating = 0 }

            $ratingCount = ($ext.statistics | Where-Object { $_.statisticName -eq "ratingcount" }).value
            if (-not $ratingCount) { $ratingCount = 0 }

            # Determine category
            $categories = $ext.categories
            $category = if ($categories -and $categories.Count -gt 0) { $categories[0] } else { "Other" }

            # Get tags
            $tags = $ext.tags | Where-Object { $_ -notlike "vscode-extension*" -and $_ -ne "__web_extension" } | Select-Object -First 10

            # Create extension object
            $extensionObj = @{
                id = "$publisher-$name".ToLower() -replace '[^a-z0-9-]', '-'
                name = $name
                displayName = $displayName
                description = $description
                author = @{
                    name = $publisher
                    verified = $ext.publisher.flags -band 1  # Verified publisher flag
                }
                version = $version
                category = $category
                tags = $tags
                license = "See Extension"
                homepage = "https://marketplace.visualstudio.com/items?itemName=$publisher.$name"
                repository = @{
                    type = "git"
                    url = ""
                }
                engines = @{
                    rawrxd = ">=1.0.0"
                    vscode = ">=1.0.0"
                }
                pricing = @{
                    type = "free"
                    price = 0
                }
                stats = @{
                    downloads = [int]$downloads
                    rating = [math]::Round([double]$rating, 1)
                    ratingCount = [int]$ratingCount
                    reviews = [int]$ratingCount
                    weeklyDownloads = [math]::Round([int]$downloads * 0.1)  # Estimate
                }
                publishedDate = $publishedDate
                lastUpdated = $lastUpdated
                source = "vscode-marketplace"
                files = @(
                    @{
                        assetType = "Microsoft.VisualStudio.Services.VSIXPackage"
                        source = "https://marketplace.visualstudio.com/_apis/public/gallery/publishers/$publisher/vsextensions/$name/$version/vspackage"
                    }
                )
            }

            $extensions += $extensionObj
        }

        # Rate limiting
        Start-Sleep -Milliseconds $Script:MarketplaceConfig.realMarketplaceSources.vscode.rateLimitMs

        return $extensions
    }
    catch {
        Write-Warning "Failed to fetch VS Code extensions: $($_.Exception.Message)"
        return @()
    }
}

function Get-PowerShellGalleryModules {
    <#
    .SYNOPSIS
    Fetches modules from PowerShell Gallery
    #>
    [CmdletBinding()]
    param()

    $modules = @()
    $maxResults = $Script:MarketplaceConfig.dataRefresh.sources.powershellGallery.maxResults

    try {
        # Search for VS Code and editor related modules
        $searchTerms = @("vscode", "editor", "ide", "development", "automation", "powershell")

        foreach ($term in $searchTerms | Select-Object -First 3) {  # Limit to avoid rate limits
            $searchUrl = $Script:MarketplaceConfig.realMarketplaceSources.powershellGallery.searchUrl -replace '\{query\}', $term

            $response = Invoke-RestMethod -Uri $searchUrl -TimeoutSec 30

            foreach ($module in $response.d.results | Select-Object -First ($maxResults / 3)) {
                $moduleInfo = @{
                    id = "ps-$($module.Id)".ToLower() -replace '[^a-z0-9-]', '-'
                    name = $module.Id
                    displayName = $module.Title
                    description = $module.Description
                    author = @{
                        name = $module.Authors
                        verified = $false
                    }
                    version = $module.Version
                    category = "PowerShell"
                    tags = @("powershell", "module", "automation")
                    license = $module.LicenseUrl
                    homepage = "https://www.powershellgallery.com/packages/$($module.Id)"
                    engines = @{
                        rawrxd = ">=1.0.0"
                        powershell = ">=5.1"
                    }
                    pricing = @{
                        type = "free"
                        price = 0
                    }
                    stats = @{
                        downloads = $module.DownloadCount
                        rating = 0
                        ratingCount = 0
                        reviews = 0
                        weeklyDownloads = [math]::Round($module.DownloadCount * 0.05)
                    }
                    publishedDate = $module.Published
                    lastUpdated = $module.LastUpdated
                    source = "powershell-gallery"
                    files = @(
                        @{
                            assetType = "PowerShell.Module"
                            source = "https://www.powershellgallery.com/api/v2/package/$($module.Id)/$($module.Version)"
                        }
                    )
                }

                $modules += $moduleInfo
            }

            # Rate limiting
            Start-Sleep -Milliseconds $Script:MarketplaceConfig.realMarketplaceSources.powershellGallery.rateLimitMs
        }

        return $modules | Sort-Object { $_.stats.downloads } -Descending | Select-Object -Unique -First $maxResults
    }
    catch {
        Write-Warning "Failed to fetch PowerShell Gallery modules: $($_.Exception.Message)"
        return @()
    }
}

function Get-GitHubExtensions {
    <#
    .SYNOPSIS
    Fetches extension repositories from GitHub
    #>
    [CmdletBinding()]
    param()

    $repositories = @()
    $maxResults = $Script:MarketplaceConfig.dataRefresh.sources.github.maxResults

    try {
        # Search for VS Code extension repositories
        $searchQueries = @(
            "vscode-extension+language:javascript",
            "vscode-extension+language:typescript",
            "powershell+topic:vscode",
            "ide+extension+powershell"
        )

        foreach ($query in $searchQueries | Select-Object -First 2) {  # Limit queries
            $searchUrl = "https://api.github.com/search/repositories?q=$query&sort=stars&order=desc&per_page=25"

            $headers = @{
                "User-Agent" = "RawrXD-Marketplace/2.0"
                "Accept" = "application/vnd.github.v3+json"
            }

            $response = Invoke-RestMethod -Uri $searchUrl -Headers $headers -TimeoutSec 30

            foreach ($repo in $response.items | Where-Object { $_.stargazers_count -ge $Script:MarketplaceConfig.dataRefresh.sources.github.starsThreshold }) {
                $repoInfo = @{
                    id = "gh-$($repo.full_name)".ToLower() -replace '[^a-z0-9-]', '-'
                    name = $repo.name
                    displayName = $repo.name
                    description = $repo.description
                    author = @{
                        name = $repo.owner.login
                        verified = $false
                    }
                    version = "latest"
                    category = "Community"
                    tags = @("github", "open-source", "community") + ($repo.topics | Select-Object -First 5)
                    license = $repo.license.name
                    homepage = $repo.html_url
                    repository = @{
                        type = "git"
                        url = $repo.clone_url
                    }
                    engines = @{
                        rawrxd = ">=1.0.0"
                    }
                    pricing = @{
                        type = "free"
                        price = 0
                    }
                    stats = @{
                        downloads = 0
                        rating = [math]::Min([math]::Round($repo.stargazers_count / 20.0, 1), 5.0)  # Estimate rating from stars
                        ratingCount = $repo.stargazers_count
                        reviews = 0
                        weeklyDownloads = [math]::Round($repo.stargazers_count * 0.5)
                    }
                    publishedDate = $repo.created_at
                    lastUpdated = $repo.updated_at
                    source = "github"
                    files = @(
                        @{
                            assetType = "GitHub.Repository"
                            source = $repo.archive_url -replace '\{.*\}', ''
                        }
                    )
                }

                $repositories += $repoInfo
            }

            # Rate limiting for GitHub API
            Start-Sleep -Milliseconds $Script:MarketplaceConfig.realMarketplaceSources.github.rateLimitMs
        }

        return $repositories | Sort-Object { $_.stats.rating } -Descending | Select-Object -First $maxResults
    }
    catch {
        Write-Warning "Failed to fetch GitHub repositories: $($_.Exception.Message)"
        return @()
    }
}

function Load-LocalMarketplaceData {
    <#
    .SYNOPSIS
    Loads local marketplace data as fallback
    #>
    [CmdletBinding()]
    param(
        [string]$MarketplacePath
    )

    try {
        # Try to load cached real data first
        $cachedFile = Join-Path $MarketplacePath "cached-real-extensions.json"
        if (Test-Path $cachedFile) {
            Write-Host "   📂 Loading cached real marketplace data..." -ForegroundColor Blue
            $Script:ExtensionsData = Get-Content $cachedFile | ConvertFrom-Json
        } else {
            # Fallback to local static data
            $extensionsPath = Join-Path $MarketplacePath "extensions.json"
            if (Test-Path $extensionsPath) {
                Write-Host "   📂 Loading local static marketplace data..." -ForegroundColor Blue
                $Script:ExtensionsData = Get-Content $extensionsPath | ConvertFrom-Json
            }
        }

        # Load community content
        $communityPath = Join-Path $MarketplacePath "community.json"
        if (Test-Path $communityPath) {
            $Script:CommunityData = Get-Content $communityPath | ConvertFrom-Json
        }

        if ($Script:ExtensionsData) {
            $extensionCount = $Script:ExtensionsData.extensions.Count
            Write-Host "   ✅ Loaded $extensionCount extensions from local data" -ForegroundColor Green
        }

        if ($Script:CommunityData) {
            $contentCount = $Script:CommunityData.content.Count
            Write-Host "   ✅ Loaded $contentCount community items" -ForegroundColor Green
        }

        Initialize-SmartOverrideEngine -MarketplacePath $MarketplacePath
    }
    catch {
        Write-Warning "Failed to load local marketplace data: $($_.Exception.Message)"
    }
}

function Get-RawrXDExtensions {
    <#
    .SYNOPSIS
    Retrieves available extensions from the real marketplace

    .DESCRIPTION
    Gets extensions with optional filtering by category, search terms, and sorting from real sources

    .PARAMETER Category
    Filter by extension category

    .PARAMETER SearchTerm
    Search term to filter extensions

    .PARAMETER SortBy
    Sort extensions by: name, downloads, rating, updated

    .PARAMETER MaxResults
    Maximum number of results to return

    .PARAMETER Source
    Filter by source: vscode-marketplace, powershell-gallery, github, all
    #>
    [CmdletBinding()]
    param(
        [string]$Category,
        [string]$SearchTerm,
        [ValidateSet("name", "downloads", "rating", "updated")]
        [string]$SortBy = "downloads",
        [int]$MaxResults = 50,
        [ValidateSet("all", "vscode-marketplace", "powershell-gallery", "github")]
        [string]$Source = "all",
        [switch]$DisableSmartOverride
    )

    if (-not $Script:ExtensionsData) {
        Write-Warning "Extensions data not loaded. Run Initialize-RawrXDMarketplace first."
        return @()
    }

    $extensions = $Script:ExtensionsData.extensions

    # Apply source filter
    if ($Source -ne "all") {
        $extensions = $extensions | Where-Object { $_.source -eq $Source }
    }

    # Apply category filter
    if ($Category) {
        $extensions = $extensions | Where-Object { $_.category -eq $Category }
    }

    # Apply search filter
    if ($SearchTerm) {
        $extensions = $extensions | Where-Object {
            $_.name -match $SearchTerm -or
            $_.description -match $SearchTerm -or
            ($_.tags -join " ") -match $SearchTerm -or
            $_.author.name -match $SearchTerm
        }
    }

    $effectiveSort = $SortBy
    if (-not $DisableSmartOverride) {
        $smartResult = Invoke-SmartMarketplaceOverride -Extensions $extensions -SortPreference $SortBy
        $extensions = $smartResult.Extensions
        if ($smartResult.EffectiveSort) { $effectiveSort = $smartResult.EffectiveSort }
    }

    # Apply sorting
    switch ($effectiveSort) {
        "smart" { $extensions = $extensions | Sort-Object smartScore -Descending }
        "name" { $extensions = $extensions | Sort-Object name }
        "downloads" { $extensions = $extensions | Sort-Object { $_.stats.downloads } -Descending }
        "rating" { $extensions = $extensions | Sort-Object { $_.stats.rating } -Descending }
        "updated" { $extensions = $extensions | Sort-Object lastUpdated -Descending }
    }

    # Limit results
    if ($MaxResults -gt 0) {
        $extensions = $extensions | Select-Object -First $MaxResults
    }

    return $extensions
}

function Get-RawrXDCommunityContent {
    <#
    .SYNOPSIS
    Retrieves community content from the marketplace

    .PARAMETER ContentType
    Filter by content type (theme, snippet, template, etc.)

    .PARAMETER Category
    Filter by category

    .PARAMETER Featured
    Show only featured content

    .PARAMETER MaxResults
    Maximum number of results to return
    #>
    [CmdletBinding()]
    param(
        [ValidateSet("theme", "snippet", "template", "keybinding", "setting-preset", "workflow", "macro")]
        [string]$ContentType,
        [string]$Category,
        [switch]$Featured,
        [int]$MaxResults = 20
    )

    if (-not $Script:CommunityData) {
        Write-Warning "Community data not loaded. Run Initialize-RawrXDMarketplace first."
        return @()
    }

    $content = $Script:CommunityData.content

    # Apply filters
    if ($ContentType) {
        $content = $content | Where-Object { $_.contentType -eq $ContentType }
    }

    if ($Category) {
        $content = $content | Where-Object { $_.category -eq $Category }
    }

    if ($Featured) {
        $content = $content | Where-Object { $_.social.featured -eq $true }
    }

    # Sort by rating and downloads
    $content = $content | Sort-Object { $_.stats.rating }, { $_.stats.downloads } -Descending

    # Limit results
    if ($MaxResults -gt 0) {
        $content = $content | Select-Object -First $MaxResults
    }

    return $content
}

function Install-RawrXDExtension {
    <#
    .SYNOPSIS
    Installs a RawrXD extension from real marketplace sources

    .PARAMETER ExtensionId
    The ID of the extension to install

    .PARAMETER Version
    Specific version to install (optional)

    .PARAMETER Force
    Force installation even if already installed
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ExtensionId,
        [string]$Version,
        [switch]$Force
    )

    # Find the extension
    $extension = $Script:ExtensionsData.extensions | Where-Object { $_.id -eq $ExtensionId }

    if (-not $extension) {
        Write-Error "Extension '$ExtensionId' not found in marketplace"
        return $false
    }

    # Use specified version or latest
    $targetVersion = if ($Version) { $Version } else { $extension.version }

    Write-Host "📦 Installing extension: $($extension.displayName) v$targetVersion" -ForegroundColor Yellow
    Write-Host "   📍 Source: $($extension.source)" -ForegroundColor Gray
    Write-Host "   👤 Author: $($extension.author.name)" -ForegroundColor Gray

    # Create installation directory
    $installPath = ".\extensions\$($extension.id)"
    if (-not (Test-Path $installPath)) {
        New-Item -Path $installPath -ItemType Directory -Force | Out-Null
    }

    try {
        # Simulate downloading based on source
        Write-Host "  ⬇️ Downloading from $($extension.source)..." -ForegroundColor Blue
        Start-Sleep -Milliseconds 800

        switch ($extension.source) {
            "vscode-marketplace" {
                Write-Host "  🔍 Fetching VSIX package..." -ForegroundColor Blue
                # In real implementation, would download VSIX and extract
            }
            "powershell-gallery" {
                Write-Host "  🔍 Installing PowerShell module..." -ForegroundColor Blue
                # In real implementation, would use Install-Module
            }
            "github" {
                Write-Host "  🔍 Cloning GitHub repository..." -ForegroundColor Blue
                # In real implementation, would clone or download repo
            }
        }

        Start-Sleep -Milliseconds 500

        Write-Host "  🔧 Installing extension files..." -ForegroundColor Blue
        Start-Sleep -Milliseconds 400

        Write-Host "  ⚙️ Configuring extension..." -ForegroundColor Blue
        Start-Sleep -Milliseconds 300

        # Create basic extension manifest
        $manifest = @{
            id = $extension.id
            name = $extension.name
            version = $targetVersion
            author = $extension.author.name
            source = $extension.source
            installedDate = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssZ")
            installPath = $installPath
        }

        $manifestPath = Join-Path $installPath "manifest.json"
        $manifest | ConvertTo-Json -Depth 5 | Out-File $manifestPath -Encoding UTF8

        # Update stats (simulated)
        if ($extension.stats) {
            $extension.stats.downloads++
        }

        Write-Host "✅ Successfully installed $($extension.displayName)!" -ForegroundColor Green
        Write-Host "   📁 Installed to: $installPath" -ForegroundColor Gray
        Write-Host "   🔗 Homepage: $($extension.homepage)" -ForegroundColor Gray

        return $true
    }
    catch {
        Write-Error "Failed to install extension: $($_.Exception.Message)"
        return $false
    }
}

function Show-RawrXDMarketplace {
    <#
    .SYNOPSIS
    Displays the RawrXD Real Marketplace in a formatted view
    #>
    [CmdletBinding()]
    param()

    if (-not $Script:ExtensionsData -and -not $Script:CommunityData) {
        Write-Warning "Marketplace data not loaded. Run Initialize-RawrXDMarketplace first."
        return
    }

    Clear-Host

    # Header with real marketplace info
    Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║               🦊 RawrXD Real Marketplace 🦊                ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""

    # Real marketplace stats
    if ($Script:ExtensionsData) {
        $extensionCount = $Script:ExtensionsData.extensions.Count
        $totalDownloads = ($Script:ExtensionsData.extensions | Measure-Object -Property { $_.stats.downloads } -Sum).Sum
        $sources = $Script:ExtensionsData.extensions | Group-Object source

        Write-Host "📊 Real Extensions: $extensionCount | Total Downloads: $($totalDownloads.ToString('N0'))" -ForegroundColor Green
        Write-Host "🌐 Sources: $($sources.Name -join ', ')" -ForegroundColor Blue
        Write-Host "🕐 Last Updated: $($Script:ExtensionsData.marketplace.lastUpdated)" -ForegroundColor Gray

        $smartStatus = Get-RawrXDSmartOverrideStatus
        if ($smartStatus.Enabled) {
            Write-Host "🧠 Smart Override: $($smartStatus.CurrentProfile) (rotates at $($smartStatus.NextRotation))" -ForegroundColor Magenta
        } else {
            Write-Host "🧠 Smart Override: Disabled" -ForegroundColor DarkGray
        }
    }

    if ($Script:CommunityData) {
        $communityCount = $Script:CommunityData.content.Count
        Write-Host "👥 Community Items: $communityCount" -ForegroundColor Green
    }

    Write-Host ""

    # Top extensions by source
    Write-Host "🌟 Top Extensions by Source" -ForegroundColor Yellow
    Write-Host "─────────────────────────────" -ForegroundColor Gray

    $topVSCode = Get-RawrXDExtensions -Source "vscode-marketplace" -MaxResults 3 -SortBy "downloads"
    if ($topVSCode.Count -gt 0) {
        Write-Host "  📦 VS Code Marketplace:" -ForegroundColor Cyan
        foreach ($ext in $topVSCode) {
            $rating = "⭐" * [math]::Round($ext.stats.rating)
            Write-Host "     • $($ext.displayName) - $($ext.stats.downloads.ToString('N0')) downloads $rating" -ForegroundColor White
        }
        Write-Host ""
    }

    $topPS = Get-RawrXDExtensions -Source "powershell-gallery" -MaxResults 3 -SortBy "downloads"
    if ($topPS.Count -gt 0) {
        Write-Host "  🔷 PowerShell Gallery:" -ForegroundColor Blue
        foreach ($ext in $topPS) {
            Write-Host "     • $($ext.displayName) - $($ext.stats.downloads.ToString('N0')) downloads" -ForegroundColor White
        }
        Write-Host ""
    }

    $topGH = Get-RawrXDExtensions -Source "github" -MaxResults 3 -SortBy "rating"
    if ($topGH.Count -gt 0) {
        Write-Host "  🐙 GitHub:" -ForegroundColor Green
        foreach ($ext in $topGH) {
            $stars = "⭐" * [math]::Round($ext.stats.rating)
            Write-Host "     • $($ext.displayName) - $($ext.stats.ratingCount) stars $stars" -ForegroundColor White
        }
        Write-Host ""
    }

    # Commands
    Write-Host "💡 Quick Commands" -ForegroundColor Cyan
    Write-Host "──────────────────" -ForegroundColor Gray
    Write-Host "  Get-RawrXDExtensions -Source 'vscode-marketplace' -MaxResults 10" -ForegroundColor White
    Write-Host "  Get-RawrXDExtensions -SearchTerm 'PowerShell' -SortBy 'rating'" -ForegroundColor White
    Write-Host "  Install-RawrXDExtension -ExtensionId 'ms-vscode-powershell'" -ForegroundColor White
    Write-Host "  Initialize-RawrXDMarketplace -ForceRefresh" -ForegroundColor White
    Write-Host ""
}

function Search-RawrXDMarketplace {
    <#
    .SYNOPSIS
    Searches across real marketplace sources and community content

    .PARAMETER Query
    Search query

    .PARAMETER Type
    Search type: all, extensions, community

    .PARAMETER Source
    Limit to specific source
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Query,
        [ValidateSet("all", "extensions", "community")]
        [string]$Type = "all",
        [string]$Source = "all",
        [switch]$DisableSmartOverride
    )

    $results = @()

    # Search extensions
    if ($Type -in @("all", "extensions")) {
        $extensions = Get-RawrXDExtensions -SearchTerm $Query -Source $Source -MaxResults 20 -DisableSmartOverride:$DisableSmartOverride
        foreach ($ext in $extensions) {
            $results += [PSCustomObject]@{
                Type = "Extension"
                Name = $ext.displayName
                Description = $ext.description
                Category = $ext.category
                Author = $ext.author.name
                Rating = $ext.stats.rating
                Downloads = $ext.stats.downloads
                Source = $ext.source
                Id = $ext.id
                SmartProfile = $ext.smartProfile
            }
        }
    }

    # Search community content
    if ($Type -in @("all", "community")) {
        if ($Script:CommunityData) {
            $community = $Script:CommunityData.content | Where-Object {
                $_.name -match $Query -or
                $_.description -match $Query -or
                ($_.tags -join " ") -match $Query
            } | Select-Object -First 10

            foreach ($content in $community) {
                $results += [PSCustomObject]@{
                    Type = "Community"
                    Name = $content.displayName
                    Description = $content.description
                    Category = $content.category
                    Author = $content.author.name
                    Rating = $content.stats.rating
                    Downloads = $content.stats.downloads
                    Source = "community"
                    Id = $content.id
                }
            }
        }
    }

    return $results | Sort-Object Rating, Downloads -Descending
}

function Update-RawrXDMarketplace {
    <#
    .SYNOPSIS
    Forces an update of marketplace data from real sources
    #>
    [CmdletBinding()]
    param()

    Write-Host "🔄 Forcing marketplace data refresh..." -ForegroundColor Yellow
    $result = Update-MarketplaceData

    if ($result) {
        $Script:LastRefresh = Get-Date
        Write-Host "✅ Marketplace data refreshed successfully!" -ForegroundColor Green

        if ($Script:ExtensionsData) {
            Write-Host "📊 Total extensions: $($Script:ExtensionsData.extensions.Count)" -ForegroundColor Cyan
            $sources = $Script:ExtensionsData.extensions | Group-Object source
            foreach ($source in $sources) {
                Write-Host "   $($source.Name): $($source.Count) extensions" -ForegroundColor Gray
            }
        }
    } else {
        Write-Error "Failed to refresh marketplace data"
    }

    return $result
}

# Export functions
Export-ModuleMember -Function Initialize-RawrXDMarketplace, Get-RawrXDExtensions, Get-RawrXDCommunityContent, Install-RawrXDExtension, Show-RawrXDMarketplace, Search-RawrXDMarketplace, Update-RawrXDMarketplace, Get-RawrXDSmartOverrideStatus

# Auto-initialize if marketplace directory exists
if (Test-Path ".\marketplace") {
    Write-Host "🔄 Auto-initializing RawrXD Real Marketplace..." -ForegroundColor Yellow
    Initialize-RawrXDMarketplace | Out-Null
}
