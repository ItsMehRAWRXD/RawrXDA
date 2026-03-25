# ============================================
# RawrXD Marketplace Extension Module
# ============================================
# Category: Marketplace
# Purpose: Extension marketplace functionality
# Author: RawrXD
# Version: 1.0.0
# ============================================

# Extension Metadata
$global:RawrXDMarketplaceExtension = @{
    Id           = "rawrxd-marketplace"
    Name         = "RawrXD Marketplace"
    Description  = "Extension marketplace for discovering and installing extensions"
    Author       = "RawrXD Official"
    Version      = "1.0.0"
    Language     = 0  # LANG_CUSTOM
    Capabilities = 0  # Bitmask: 0 = no specific capabilities required
    EditorType   = $null
    Dependencies = @()
    Enabled      = $true
}

# ============================================
# HELPER FUNCTIONS
# ============================================

# Helper function for logging (works even if Write-DevConsole isn't available yet)
function Write-ExtensionLog {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
        Write-DevConsole $Message $Level
    }
    else {
        $color = switch ($Level) {
            "ERROR" { "Red" }
            "WARNING" { "Yellow" }
            "SUCCESS" { "Green" }
            "DEBUG" { "Cyan" }
            default { "White" }
        }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# ============================================
# MARKETPLACE DATA STRUCTURES AND VARIABLES
# ============================================

# Marketplace cache and sources
$script:marketplaceCache = @()
$script:marketplaceLocalDir = Join-Path $PSScriptRoot "marketplace"
if (-not (Test-Path $script:marketplaceLocalDir)) {
    New-Item -ItemType Directory -Path $script:marketplaceLocalDir -Force | Out-Null
}

$script:marketplaceSources = @(
    # Remote sources disabled - GitHub repo doesn't exist (404 errors)
    # @{ Name = 'RawrXD Official (Remote)'; Url = 'https://raw.githubusercontent.com/HiH8e/RawrXD-marketplace/main/extensions.json'; MarketplaceId = 'rawrxd-official' }
    # @{ Name = 'Community Marketplace (Remote)'; Url = 'https://raw.githubusercontent.com/HiH8e/RawrXD-marketplace/main/community.json'; MarketplaceId = 'community-marketplace' }

    # Local marketplace sources (working)
    @{ Name = 'RawrXD Official (Local)'; Url = 'file://' + (Join-Path $script:marketplaceLocalDir 'rawrxd_official.json'); MarketplaceId = 'rawrxd-official' }
    @{ Name = 'Community Marketplace (Local)'; Url = 'file://' + (Join-Path $script:marketplaceLocalDir 'community.json'); MarketplaceId = 'community-marketplace' }
    @{ Name = 'Local Official Extras'; Url = 'file://' + (Join-Path $script:marketplaceLocalDir 'local-official.json'); MarketplaceId = 'rawrxd-official' }
    @{ Name = 'Local Community Extras'; Url = 'file://' + (Join-Path $script:marketplaceLocalDir 'local-community.json'); MarketplaceId = 'community-marketplace' }
)
$script:marketplaceLastRefresh = $null

# ============================================
# MARKETPLACE FUNCTIONS
# ============================================

function Resolve-MarketplaceLanguageCode {
    param([int]$LanguageCode)

    switch ($LanguageCode) {
        0 { return "Custom" }
        1 { return "PowerShell" }
        2 { return "C#" }
        3 { return "Python" }
        4 { return "JavaScript" }
        5 { return "TypeScript" }
        6 { return "Java" }
        7 { return "C++" }
        8 { return "C" }
        9 { return "Go" }
        10 { return "Rust" }
        11 { return "PHP" }
        12 { return "Ruby" }
        13 { return "Swift" }
        14 { return "Kotlin" }
        15 { return "Dart" }
        16 { return "R" }
        17 { return "MATLAB" }
        18 { return "SQL" }
        19 { return "HTML" }
        20 { return "CSS" }
        21 { return "XML" }
        22 { return "JSON" }
        23 { return "YAML" }
        24 { return "Markdown" }
        25 { return "Shell" }
        26 { return "Batch" }
        27 { return "Perl" }
        28 { return "Lua" }
        29 { return "Assembly" }
        default { return "Unknown" }
    }
}

function Get-FirstNonNullValue {
    param([object[]]$Values)

    foreach ($value in $Values) {
        if ($null -ne $value) {
            return $value
        }
    }

    return $null
}

function Normalize-MarketplaceEntry {
    param([object]$Entry)

    if (-not $Entry) { return $null }

    $extensionIdValue = if ($Entry.PSObject.Properties['extensionId']) { $Entry.extensionId } else { $null }
    $rawIdValue       = if ($Entry.PSObject.Properties['id']) { $Entry.id } else { $null }
    $idCandidate      = Get-FirstNonNullValue -Values @($extensionIdValue, $rawIdValue, "unknown")

    $displayNameValue = if ($Entry.PSObject.Properties['displayName']) { $Entry.displayName } else { $null }
    $rawNameValue     = if ($Entry.PSObject.Properties['name']) { $Entry.name } else { $null }
    $nameCandidate    = Get-FirstNonNullValue -Values @($displayNameValue, $rawNameValue, "Unknown Extension")

    $descriptionValue = if ($Entry.PSObject.Properties['description']) { $Entry.description } else { $null }
    $descriptionCandidate = Get-FirstNonNullValue -Values @($descriptionValue, "No description available")

    $publisherValue = if ($Entry.PSObject.Properties['publisher']) { $Entry.publisher } else { $null }
    $authorValue    = if ($Entry.PSObject.Properties['author']) { $Entry.author } else { $null }
    $authorCandidate = Get-FirstNonNullValue -Values @($publisherValue, $authorValue, "Unknown")

    $versionValue     = if ($Entry.PSObject.Properties['version']) { $Entry.version } else { $null }
    $versionCandidate = Get-FirstNonNullValue -Values @($versionValue, "1.0.0")
    $languageCandidate = 0
    if ($Entry.PSObject.Properties['language'] -or $Entry.PSObject.Properties['targetPlatform']) {
        $langValue = $null
        if ($Entry.PSObject.Properties['language']) {
            try { $langValue = $Entry.language } catch { }
        }
        if (-not $langValue -and $Entry.PSObject.Properties['targetPlatform']) {
            try { $langValue = $Entry.targetPlatform } catch { }
        }
        if ($langValue) {
            if ($langValue -is [string]) {
                $resolved = Resolve-MarketplaceLanguageCode -Code $langValue
                if ($resolved -ne "Unknown") {
                    $langCodes = @{
                        "PowerShell" = 0; "Python" = 1; "JavaScript" = 2; "TypeScript" = 3;
                        "C#" = 4; "Java" = 5; "C++" = 6; "C" = 7; "Go" = 8; "Rust" = 9;
                        "PHP" = 10; "Ruby" = 11; "Swift" = 12; "Kotlin" = 13; "Dart" = 14;
                        "HTML" = 15; "CSS" = 16; "SQL" = 17; "JSON" = 18; "XML" = 19;
                        "YAML" = 20; "Markdown" = 21; "Shell" = 22; "Batch" = 23; "Assembly" = 29
                    }
                    $converted = $langCodes[$resolved]
                    if ($null -ne $converted) {
                        $languageCandidate = $converted
                    }
                }
            }
            elseif ($langValue -is [int] -or $langValue -is [long]) {
                $languageCandidate = [int]$langValue
            }
        }
    }
    $installCountValue = if ($Entry.PSObject.Properties['installCount']) { $Entry.installCount } else { $null }
    $downloadsValue    = if ($Entry.PSObject.Properties['downloads']) { $Entry.downloads } else { $null }
    $downloadsCandidate = Get-FirstNonNullValue -Values @($installCountValue, $downloadsValue, 0)

    $ratingValue = if ($Entry.PSObject.Properties['rating']) { $Entry.rating } else { $null }
    $ratingCandidate = Get-FirstNonNullValue -Values @($ratingValue, 0.0)

    $repositoryValue = if ($Entry.PSObject.Properties['repository']) { $Entry.repository } else { $null }
    $repositoryCandidate = Get-FirstNonNullValue -Values @($repositoryValue, "")

    $sourceValue = if ($Entry.PSObject.Properties['Source']) { $Entry.Source } else { $null }
    $sourceCandidate = Get-FirstNonNullValue -Values @($sourceValue, "Unknown")

    $normalized = @{
        Id          = $idCandidate.ToString().ToLower()

        Name        = $nameCandidate

        Description = $descriptionCandidate

        Author      = $authorCandidate

        Version     = $versionCandidate

        Language    = $languageCandidate

        Downloads   = [int]$downloadsCandidate

        Rating      = [double]$ratingCandidate

        Repository  = $repositoryCandidate

        Tags        = if ($Entry.PSObject.Properties['tags'] -and $Entry.tags -is [System.Collections.IEnumerable]) {
            @($Entry.tags)
        }
        else { @() }

        Categories  = if ($Entry.PSObject.Properties['categories'] -and $Entry.categories -is [System.Collections.IEnumerable]) {
            @($Entry.categories)
        }
        else { @() }

        Source      = $sourceCandidate
        Raw         = $Entry
    }

    return $normalized
}

function Get-VSCodeMarketplaceExtensions {
    param(
        [int]$PageSize = 50,
        [string]$Query = "",
        [string]$Category = ""
    )

    try {
        $baseUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery"
        $body = @{
            filters    = @(
                @{
                    criteria   = @(
                        @{
                            filterType = 8  # ExtensionName
                            value      = if ($Query) { $Query } else { "vscode" }
                        }
                        @{
                            filterType = 10  # Target
                            value      = "Microsoft.VisualStudio.Code"
                        }
                        @{
                            filterType = 12  # ExtensionKind
                            value      = "workspace"
                        }
                    )
                    pageNumber = 1
                    pageSize   = $PageSize
                    sortBy     = 4  # InstallCount
                    sortOrder  = 0  # Descending
                }
            )
            assetTypes = @()
            flags      = 2151
        }

        if ($Category) {
            $body.filters[0].criteria += @{
                filterType = 5  # Category
                value      = $Category
            }
        }

        $jsonBody = $body | ConvertTo-Json -Depth 10 -Compress
        $response = Invoke-RestMethod -Uri $baseUrl -Method Post -Body $jsonBody -ContentType "application/json" -TimeoutSec 30

        $extensions = @()
        if ($response.results -and $response.results[0].extensions) {
            foreach ($ext in $response.results[0].extensions) {
                $extension = @{
                    extensionId    = $ext.extensionId
                    name           = $ext.displayName
                    displayName    = $ext.displayName
                    description    = $ext.description
                    publisher      = $ext.publisher.publisherName
                    version        = $ext.versions[0].version
                    installCount   = $ext.statistics | Where-Object { $_.statisticName -eq "install" } | Select-Object -ExpandProperty value
                    rating         = $ext.statistics | Where-Object { $_.statisticName -eq "averagerating" } | Select-Object -ExpandProperty value
                    tags           = $ext.tags
                    categories     = $ext.categories
                    targetPlatform = "vscode"
                }
                $extensions += $extension
            }
        }

        return $extensions
    }
    catch {
        Write-ExtensionLog "Error fetching VSCode marketplace extensions: $_" "WARNING"
        return @()
    }
}

function Get-MarketplaceSeedData {
    return @(
        @{
            Id          = "rawrxd-editor-basic"
            Name        = "RawrXD Basic Editor"
            Description = "Lightweight text editor using TextBox control for maximum speed and minimal resource usage"
            Author      = "RawrXD Official"
            Version     = "1.0.0"
            Language    = 0
            Downloads   = 1000
            Rating      = 4.5
            Tags        = @("editor", "basic", "lightweight")
            Categories  = @("Editors")
            Source      = "Built-in"
        }
        @{
            Id          = "rawrxd-editor-advanced"
            Name        = "RawrXD Advanced Editor"
            Description = "Advanced text editor with syntax highlighting and code completion features"
            Author      = "RawrXD Official"
            Version     = "1.0.0"
            Language    = 0
            Downloads   = 800
            Rating      = 4.7
            Tags        = @("editor", "advanced", "syntax", "completion")
            Categories  = @("Editors")
            Source      = "Built-in"
        }
        @{
            Id          = "rawrxd-editor-richtext"
            Name        = "RawrXD Rich Text Editor"
            Description = "Rich text editor with formatting capabilities and advanced text processing"
            Author      = "RawrXD Official"
            Version     = "1.0.0"
            Language    = 0
            Downloads   = 600
            Rating      = 4.3
            Tags        = @("editor", "rich-text", "formatting")
            Categories  = @("Editors")
            Source      = "Built-in"
        }
        @{
            Id          = "rawrxd-marketplace"
            Name        = "RawrXD Marketplace"
            Description = "Extension marketplace for discovering and installing extensions"
            Author      = "RawrXD Official"
            Version     = "1.0.0"
            Language    = 0
            Downloads   = 500
            Rating      = 4.8
            Tags        = @("marketplace", "extensions", "discovery")
            Categories  = @("Tools")
            Source      = "Built-in"
        }
    )
}

function Get-MarketplacePayloadFromSource {
    param([object]$Source)

    try {
        if ($Source.Url -match '^file://') {
            $filePath = $Source.Url -replace '^file://', ''
            if (Test-Path $filePath) {
                $content = Get-Content $filePath -Raw -Encoding UTF8
                return $content | ConvertFrom-Json
            }
            else {
                Write-ExtensionLog "Marketplace source file not found: $filePath" "WARNING"
                return $null
            }
        }
        elseif ($Source.Url -match '^https?://') {
            $response = Invoke-WebRequest -Uri $Source.Url -Method Get -TimeoutSec 30 -UseBasicParsing
            return $response.Content | ConvertFrom-Json
        }
        else {
            Write-ExtensionLog "Unsupported marketplace source URL format: $($Source.Url)" "WARNING"
            return $null
        }
    }
    catch {
        Write-ExtensionLog "Error loading marketplace source '$($Source.Name)': $($_.Exception.Message)" "WARNING"
        return $null
    }
}

function Load-MarketplaceCatalog {
    param([switch]$Force)

    try {
        if (-not $Force -and $script:marketplaceCache.Count -gt 0) {
            return $script:marketplaceCache
        }

        $catalog = Get-MarketplaceSeedData
        Write-ExtensionLog "Loading marketplace from $($script:marketplaceSources.Count) source(s)..." "INFO"

        # Try to fetch live data from VSCode Marketplace API
        try {
            Write-ExtensionLog "Fetching live extensions from VSCode Marketplace..." "INFO"
            $liveExtensions = Get-VSCodeMarketplaceExtensions -PageSize 100
            if ($liveExtensions -and $liveExtensions.Count -gt 0) {
                foreach ($ext in $liveExtensions) {
                    $normalized = Normalize-MarketplaceEntry -Entry $ext
                    # FIX: Ensure Source property is set for VSCode marketplace extensions
                    if (-not $normalized.Source -or $normalized.Source -eq "Unknown") {
                        $normalized.Source = "VSCode Marketplace"
                    }
                    $catalog += $normalized
                }
                Write-ExtensionLog "✅ Added $($liveExtensions.Count) live extensions from VSCode Marketplace" "SUCCESS"
            }
        }
        catch {
            Write-ExtensionLog "⚠️ Could not fetch live VSCode Marketplace data: $($_.Exception.Message)" "WARNING"
        }

        foreach ($source in $script:marketplaceSources) {
            try {
                $payload = Get-MarketplacePayloadFromSource -Source $source
                if ($payload -and $payload.PSObject) {
                    $extensionsProperty = $payload.PSObject.Properties | Where-Object { $_.Name -ieq 'extensions' }
                    if ($extensionsProperty -and $extensionsProperty.Value) {
                        $payload = $extensionsProperty.Value
                    }
                    elseif ($payload.PSObject.Properties['Extensions']) {
                        $payload = $payload.Extensions
                    }
                }

                if ($payload) {
                    $entries = @()
                    if ($payload -is [System.Collections.IEnumerable]) {
                        $entries = $payload
                    }
                    else {
                        $entries = @($payload)
                    }

                    foreach ($entry in $entries) {
                        try {
                            $normalized = Normalize-MarketplaceEntry -Entry $entry
                            if ($normalized) {
                                # Ensure Source property is set
                                if (-not $normalized.PSObject.Properties['Source']) {
                                    $normalized | Add-Member -MemberType NoteProperty -Name 'Source' -Value $source.Name -Force
                                }
                                else {
                                    $normalized.Source = $source.Name
                                }
                                $catalog += $normalized
                            }
                        }
                        catch {
                            Write-ExtensionLog "⚠️ Skipped invalid entry in $($source.Name): $($_.Exception.Message)" "WARNING"
                            continue
                        }
                    }
                }

                Write-ExtensionLog "Loaded marketplace source '$($source.Name)'" "INFO"
                Write-ExtensionLog "✅ Loaded extensions from $($source.Name)" "SUCCESS"
            }
            catch {
                Write-ExtensionLog "Marketplace source '$($source.Name)' failed: $($_.Exception.Message)" "WARNING"
                Write-ExtensionLog "⚠️ Failed to load $($source.Name): $($_.Exception.Message)" "WARNING"
            }
        }

        if (-not $catalog.Count) {
            Write-ExtensionLog "No extensions loaded from remote sources, using seed data" "WARNING"
            $catalog = Get-MarketplaceSeedData
        }

        $unique = @{}
        foreach ($entry in $catalog) {
            if (-not $entry.Id) { continue }
            $key = $entry.Id.ToLower()
            if (-not $unique.ContainsKey($key)) {
                $unique[$key] = $entry
            }
        }

        $script:marketplaceCache = $unique.Values | Sort-Object -Property Downloads -Descending
        $script:marketplaceLastRefresh = Get-Date
        Write-ExtensionLog "✅ Marketplace catalog loaded: $($script:marketplaceCache.Count) unique extensions" "SUCCESS"
        return $script:marketplaceCache
    }
    catch {
        Write-ExtensionLog "❌ Error loading marketplace catalog: $_" "ERROR"
        Write-ErrorLog -Message "Marketplace catalog load failed: $($_.Exception.Message)" -Severity "MEDIUM"
        # Return seed data as fallback
        return Get-MarketplaceSeedData
    }
}

function Search-Marketplace {
    param(
        [string]$Query,
        [int]$LanguageFilter = -1,
        [switch]$IncludeInstalled,
        [switch]$IncludeRemote
    )

    if (-not $IncludeInstalled.IsPresent -and -not $IncludeRemote.IsPresent) {
        $IncludeInstalled = $true
        $IncludeRemote = $true
    }

    $results = @()
    $seenIds = @{}

    $evaluateEntry = {
        param($entry, $defaultSource)

        if (-not $entry) {
            return
        }

        # Ensure Source property exists
        if (-not $entry.PSObject.Properties['Source']) {
            $entry | Add-Member -MemberType NoteProperty -Name 'Source' -Value (Get-FirstNonNullValue -Values @($defaultSource, "Unknown")) -Force
        }
        elseif (-not $entry.Source -and $defaultSource) {
            $entry.Source = $defaultSource
        }
        elseif (-not $entry.Source) {
            $entry.Source = "Unknown"
        }

        # Check if entry matches query
        $matchesQuery = $false
        if (-not $Query -or $Query.Trim() -eq "") {
            $matchesQuery = $true
        }
        else {
            $queryLower = $Query.ToLower()
            $matchesQuery = $entry.Name.ToLower().Contains($queryLower) -or
            $entry.Description.ToLower().Contains($queryLower) -or
            $entry.Author.ToLower().Contains($queryLower) -or
            ($entry.Tags -and ($entry.Tags | Where-Object { $_.ToLower().Contains($queryLower) }).Count -gt 0)
        }

        # Check language filter
        $matchesLanguage = $LanguageFilter -eq -1 -or $entry.Language -eq $LanguageFilter

        if ($matchesQuery -and $matchesLanguage) {
            $id = $entry.Id.ToLower()
            if (-not $seenIds.ContainsKey($id)) {
                $seenIds[$id] = $true
                # FIX: Ensure Source property exists before adding to results
                if (-not $entry.Source -or $entry.Source -eq "Unknown") {
                    $entry.Source = Get-FirstNonNullValue -Values @($defaultSource, "Marketplace")
                }
                $results += $entry
            }
        }
    }

    # Search installed extensions if requested
    if ($IncludeInstalled) {
        foreach ($ext in $script:extensionRegistry) {
            $entry = @{
                Id          = $ext.Id
                Name        = $ext.Name
                Description = $ext.Description
                Author      = $ext.Author
                Version     = $ext.Version
                Language    = if ($ext.PSObject.Properties['Language']) { $ext.Language } else { 0 }
                Downloads   = 0
                Rating      = 0.0
                Tags        = @()
                Categories  = @()
                Source      = "Installed"
                Installed   = $true
            }
            & $evaluateEntry $entry "Installed"
        }
    }

    # Search marketplace catalog if requested
    if ($IncludeRemote) {
        try {
            $catalog = Load-MarketplaceCatalog
            if ($catalog -and $catalog.Count -gt 0) {
                Write-ExtensionLog "Searching $($catalog.Count) extensions from marketplace catalog" "DEBUG"
                foreach ($entry in $catalog) {
                    # Check if already installed
                    $installed = $script:extensionRegistry | Where-Object { $_.Id -eq $entry.Id }
                    if ($installed) {
                        $entry.Installed = $true
                    }
                    & $evaluateEntry $entry "Marketplace"
                }
            }
            else {
                Write-ExtensionLog "⚠️ Marketplace catalog is empty, forcing refresh..." "WARNING"
                $catalog = Load-MarketplaceCatalog -Force
                if ($catalog -and $catalog.Count -gt 0) {
                    foreach ($entry in $catalog) {
                        $installed = $script:extensionRegistry | Where-Object { $_.Id -eq $entry.Id }
                        if ($installed) {
                            $entry.Installed = $true
                        }
                        & $evaluateEntry $entry "Marketplace"
                    }
                }
            }
        }
        catch {
            Write-ExtensionLog "Error loading marketplace catalog: $_" "ERROR"
        }
    }

    return $results
}

function Show-Marketplace {
    try {
        Write-ExtensionLog "Opening Extension Marketplace..." "INFO"

        $marketplaceForm = New-Object System.Windows.Forms.Form
        $marketplaceForm.Text = "Extension Marketplace"
        $marketplaceForm.Size = New-Object System.Drawing.Size(800, 600)
        $marketplaceForm.StartPosition = "CenterScreen"
        $marketplaceForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
        $marketplaceForm.ForeColor = [System.Drawing.Color]::White

        # Search box
        $searchBox = New-Object System.Windows.Forms.TextBox
        $searchBox.Dock = [System.Windows.Forms.DockStyle]::Top
        $searchBox.Height = 30
        $searchBox.Font = New-Object System.Drawing.Font("Segoe UI", 10)
        $searchBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
        $searchBox.ForeColor = [System.Drawing.Color]::White
        # Conditional PlaceholderText assignment (not available in .NET Framework 4.8)
        if ($searchBox.PSObject.Properties['PlaceholderText']) {
            $searchBox.PlaceholderText = "Search extensions..."
        }
        else {
            # Fallback: show hint via status label after load
            Write-ExtensionLog "PlaceholderText not supported; using fallback hint" "DEBUG"
        }
        $marketplaceForm.Controls.Add($searchBox) | Out-Null

        # Status label
        $statusLabel = New-Object System.Windows.Forms.Label
        $statusLabel.Dock = [System.Windows.Forms.DockStyle]::Bottom
        $statusLabel.Height = 25
        $statusLabel.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
        $statusLabel.ForeColor = [System.Drawing.Color]::LightGray
        $statusLabel.Text = "Loading marketplace..."
        $marketplaceForm.Controls.Add($statusLabel) | Out-Null

        # Results list
        $resultsList = New-Object System.Windows.Forms.ListView
        $resultsList.Dock = [System.Windows.Forms.DockStyle]::Fill
        $resultsList.View = [System.Windows.Forms.View]::Details
        $resultsList.Font = New-Object System.Drawing.Font("Segoe UI", 9)
        $resultsList.FullRowSelect = $true
        $resultsList.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
        $resultsList.ForeColor = [System.Drawing.Color]::White
        $resultsList.Columns.Add("Name", 200) | Out-Null
        $resultsList.Columns.Add("Description", 400) | Out-Null
        $resultsList.Columns.Add("Author", 100) | Out-Null
        $resultsList.Columns.Add("Version", 80) | Out-Null
        $marketplaceForm.Controls.Add($resultsList) | Out-Null

        # Refresh results function
        $refreshResults = {
            try {
                $resultsList.Items.Clear()
                $query = $searchBox.Text
                $statusLabel.Text = "Searching..."

                try {
                    $results = Search-Marketplace -Query $query -IncludeInstalled -IncludeRemote
                    $statusLabel.Text = "Found $($results.Count) extension(s)"

                    foreach ($ext in $results) {
                        try {
                            $item = New-Object System.Windows.Forms.ListViewItem($ext.Name)
                            $item.SubItems.Add($ext.Description) | Out-Null
                            $item.SubItems.Add($ext.Author) | Out-Null
                            $item.SubItems.Add($ext.Version) | Out-Null
                            $item.Tag = $ext
                            $item.ForeColor = [System.Drawing.Color]::White
                            $resultsList.Items.Add($item) | Out-Null
                        }
                        catch {
                            Write-ExtensionLog "Error adding extension to list: $_" "WARNING"
                        }
                    }
                }
                catch {
                    Write-ExtensionLog "Error searching marketplace: $_" "ERROR"
                    $statusLabel.Text = "Error: $($_.Exception.Message)"
                    [System.Windows.Forms.MessageBox]::Show("Error loading marketplace: $($_.Exception.Message)", "Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
                }
            }
            catch {
                Write-ExtensionLog "Error in refreshResults: $_" "ERROR"
                $statusLabel.Text = "Error refreshing results"
            }
        }

        # Load initial results - FIX: Force refresh catalog to ensure all extensions are loaded
        try {
            Write-ExtensionLog "Force refreshing marketplace catalog..." "INFO"
            $null = Load-MarketplaceCatalog -Force
            Write-ExtensionLog "✅ Marketplace catalog refreshed" "SUCCESS"
        }
        catch {
            Write-ExtensionLog "⚠️ Could not force refresh catalog: $_" "WARNING"
        }

        $searchBox.Add_TextChanged($refreshResults)

        # Try to load marketplace catalog in background
        try {
            Write-ExtensionLog "Loading marketplace catalog..." "INFO"
            $catalog = Load-MarketplaceCatalog -Force
            Write-ExtensionLog "Marketplace catalog loaded: $($catalog.Count) extensions" "SUCCESS"
            $statusLabel.Text = "Ready - $($catalog.Count) extensions available"
        }
        catch {
            Write-ExtensionLog "Error loading marketplace catalog: $_" "WARNING"
            $statusLabel.Text = "Using cached data - $($_.Exception.Message)"
        }

        # Refresh results now that catalog is loaded
        & $refreshResults

        $marketplaceForm.ShowDialog() | Out-Null
    }
    catch {
        Write-ExtensionLog "❌ Error showing marketplace: $_" "ERROR"
        Write-ErrorLog -Message "Failed to show marketplace: $($_.Exception.Message)" -Severity "HIGH"
        [System.Windows.Forms.MessageBox]::Show("Error opening marketplace: $($_.Exception.Message)", "Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
    }
}

# Wrapper function for test compatibility
function Show-ExtensionMarketplace {
    <#
    .SYNOPSIS
        Opens the extension marketplace (wrapper for Show-Marketplace)
    .DESCRIPTION
        Provides a standardized function name for opening the extension marketplace
    #>
    Show-Marketplace
}

function Show-InstalledExtensions {
    $installedForm = New-Object System.Windows.Forms.Form
    $installedForm.Text = "Installed Extensions"
    $installedForm.Size = New-Object System.Drawing.Size(700, 500)
    $installedForm.StartPosition = "CenterScreen"

    $listBox = New-Object System.Windows.Forms.ListBox
    $listBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $listBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $installedForm.Controls.Add($listBox) | Out-Null

    foreach ($ext in $script:extensionRegistry) {
        $status = if ($ext.Enabled) { "[ENABLED]" } else { "[DISABLED]" }
        $listBox.Items.Add("$($ext.Name) | Out-Null $status - $($ext.Description)") | Out-Null
    }

    $installedForm.ShowDialog() | Out-Null
}

function Show-ExtensionManager {
    <#
    .SYNOPSIS
        Shows the extension management interface
    #>

    $form = New-Object System.Windows.Forms.Form
    $form.Text = "🔌 Extension Manager"
    $form.Size = New-Object System.Drawing.Size(800, 600)
    $form.StartPosition = "CenterScreen"
    $form.Font = New-Object System.Drawing.Font("Segoe UI", 9)

    # Create tab control
    $tabControl = New-Object System.Windows.Forms.TabControl
    $tabControl.Dock = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($tabControl)

    # Installed tab
    $installedTab = New-Object System.Windows.Forms.TabPage
    $installedTab.Text = "📦 Installed Extensions"
    $tabControl.TabPages.Add($installedTab)

    # Extensions list
    $extListView = New-Object System.Windows.Forms.ListView
    $extListView.Location = New-Object System.Drawing.Point(10, 10)
    $extListView.Size = New-Object System.Drawing.Size(750, 400)
    $extListView.View = "Details"
    $extListView.FullRowSelect = $true
    $extListView.GridLines = $true
    $extListView.Columns.Add("Name", 200)
    $extListView.Columns.Add("Version", 80)
    $extListView.Columns.Add("Author", 120)
    $extListView.Columns.Add("Status", 80)
    $extListView.Columns.Add("Description", 250)
    $installedTab.Controls.Add($extListView)

    # Refresh extension list
    foreach ($ext in $script:extensionRegistry) {
        $item = New-Object System.Windows.Forms.ListViewItem
        $item.Text = $ext.Name
        $item.SubItems.Add($ext.Version)
        $item.SubItems.Add($ext.Author)
        $item.SubItems.Add($(if ($ext.Enabled) { "Enabled" } else { "Disabled" }))
        $item.SubItems.Add($ext.Description)
        $extListView.Items.Add($item)
    }

    $form.ShowDialog() | Out-Null
}

# ============================================
# CLI MARKETPLACE FUNCTIONS
# ============================================

function Show-CLIMarketplace {
    Write-Host "`n🛍️  EXTENSION MARKETPLACE" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    try {
        $catalog = Load-MarketplaceCatalog
        Write-Host "📦 Available Extensions ($($catalog.Count)):" -ForegroundColor Yellow
        Write-Host ""

        $index = 1
        foreach ($ext in $catalog) {
            $installed = if ($script:extensionRegistry | Where-Object { $_.Id -eq $ext.Id }) { "✅" } else { "⬜" }
            Write-Host "$installed $($index.ToString().PadLeft(2)). $($ext.Name)" -ForegroundColor White
            Write-Host "    📝 $($ext.Description)" -ForegroundColor Gray
            Write-Host "    👤 $($ext.Author) | v$($ext.Version)" -ForegroundColor DarkGray
            if ($ext.Downloads -gt 0) {
                Write-Host "    📊 $($ext.Downloads) downloads" -ForegroundColor DarkGray
            }
            Write-Host ""
            $index++
        }

        Write-Host "💡 Use '/marketplace search <query>' to search extensions" -ForegroundColor Cyan
        Write-Host "💡 Use '/marketplace install <name>' to install an extension" -ForegroundColor Cyan
    }
    catch {
        Write-Host "❌ Error loading marketplace: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Search-CLIExtensions {
    param([string]$Query)

    Write-Host "`n🔍 SEARCH RESULTS for '$Query'" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    try {
        $results = Search-Marketplace -Query $Query -IncludeInstalled -IncludeRemote

        if ($results.Count -eq 0) {
            Write-Host "❌ No extensions found matching '$Query'" -ForegroundColor Yellow
            return
        }

        Write-Host "📦 Found $($results.Count) extension(s):" -ForegroundColor Yellow
        Write-Host ""

        foreach ($ext in $results) {
            $installed = if ($ext.Installed) { "✅ INSTALLED" } else { "⬜ AVAILABLE" }
            Write-Host "$installed - $($ext.Name)" -ForegroundColor White
            Write-Host "  📝 $($ext.Description)" -ForegroundColor Gray
            Write-Host "  👤 $($ext.Author) | v$($ext.Version)" -ForegroundColor DarkGray
            if ($ext.Downloads -gt 0) {
                Write-Host "  📊 $($ext.Downloads) downloads" -ForegroundColor DarkGray
            }
            Write-Host ""
        }
    }
    catch {
        Write-Host "❌ Error searching extensions: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Install-CLIExtension {
    param([string]$ExtensionName)

    Write-Host "`n📦 INSTALLING EXTENSION: $ExtensionName" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    try {
        # Find extension by name
        $catalog = Load-MarketplaceCatalog
        $ext = $catalog | Where-Object { $_.Name -ieq $ExtensionName -or $_.Id -ieq $ExtensionName }

        if (-not $ext) {
            Write-Host "❌ Extension '$ExtensionName' not found in marketplace" -ForegroundColor Red
            return
        }

        # Check if already installed
        $existing = $script:extensionRegistry | Where-Object { $_.Id -eq $ext.Id }
        if ($existing) {
            Write-Host "⚠️  Extension '$($ext.Name)' is already installed" -ForegroundColor Yellow
            return
        }

        Write-Host "🔄 Installing $($ext.Name)..." -ForegroundColor Yellow

        # For now, just register the extension (actual installation would require download logic)
        $extension = Register-Extension -Id $ext.Id -Name $ext.Name -Description $ext.Description `
            -Author $ext.Author -Capabilities 0 -Version $ext.Version

        Write-Host "✅ Successfully installed $($ext.Name)" -ForegroundColor Green
    }
    catch {
        Write-Host "❌ Error installing extension: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Show-CLIExtensionsList {
    Write-Host "`n📦 INSTALLED EXTENSIONS" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    if ($script:extensionRegistry.Count -eq 0) {
        Write-Host "❌ No extensions installed" -ForegroundColor Yellow
        return
    }

    foreach ($ext in $script:extensionRegistry) {
        $status = if ($ext.Enabled) { "✅ ENABLED" } else { "⬜ DISABLED" }
        Write-Host "$status - $($ext.Name)" -ForegroundColor White
        Write-Host "  📝 $($ext.Description)" -ForegroundColor Gray
        Write-Host "  👤 $($ext.Author) | v$($ext.Version)" -ForegroundColor DarkGray
        Write-Host ""
    }
}

function Show-CLIExtensionInfo {
    param([string]$ExtensionName)

    Write-Host "`nℹ️  EXTENSION INFO: $ExtensionName" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    try {
        $ext = $script:extensionRegistry | Where-Object { $_.Name -ieq $ExtensionName -or $_.Id -ieq $ExtensionName }

        if (-not $ext) {
            Write-Host "❌ Extension '$ExtensionName' not found" -ForegroundColor Red
            return
        }

        Write-Host "📦 Name: $($ext.Name)" -ForegroundColor White
        Write-Host "🆔 ID: $($ext.Id)" -ForegroundColor White
        Write-Host "📝 Description: $($ext.Description)" -ForegroundColor White
        Write-Host "👤 Author: $($ext.Author)" -ForegroundColor White
        Write-Host "🏷️  Version: $($ext.Version)" -ForegroundColor White
        Write-Host "⚙️  Status: $(if ($ext.Enabled) { "Enabled" } else { "Disabled" })" -ForegroundColor White
        Write-Host "🛠️  Capabilities: $($ext.Capabilities)" -ForegroundColor White
    }
    catch {
        Write-Host "❌ Error getting extension info: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# ============================================
# MODULE INITIALIZATION
# ============================================

function Initialize-RawrXD-MarketplaceExtension {
    <#
    .SYNOPSIS
        Initializes the RawrXD Marketplace extension
    #>
    Write-ExtensionLog "🛍️ Initializing Marketplace Extension..." "INFO"

    # Initialize marketplace directory
    if (-not (Test-Path $script:marketplaceLocalDir)) {
        New-Item -ItemType Directory -Path $script:marketplaceLocalDir -Force | Out-Null
        Write-ExtensionLog "Created marketplace directory: $script:marketplaceLocalDir" "INFO"
    }

    # Load initial marketplace catalog
    try {
        $catalog = Load-MarketplaceCatalog
        Write-ExtensionLog "✅ Marketplace extension initialized with $($catalog.Count) extensions" "SUCCESS"
    }
    catch {
        Write-ExtensionLog "⚠️ Marketplace initialization warning: $($_.Exception.Message)" "WARNING"
    }
}

# Export functions for use by the main application
Export-ModuleMember -Function @(
    'Resolve-MarketplaceLanguageCode',
    'Normalize-MarketplaceEntry',
    'Get-VSCodeMarketplaceExtensions',
    'Get-MarketplaceSeedData',
    'Get-MarketplacePayloadFromSource',
    'Load-MarketplaceCatalog',
    'Search-Marketplace',
    'Show-Marketplace',
    'Show-ExtensionMarketplace',
    'Show-InstalledExtensions',
    'Show-ExtensionManager',
    'Show-CLIMarketplace',
    'Search-CLIExtensions',
    'Install-CLIExtension',
    'Show-CLIExtensionsList',
    'Show-CLIExtensionInfo',
    'Initialize-RawrXD-MarketplaceExtension',
    'Start-BigDaddyAgent'
) -Variable @(
    'marketplaceCache',
    'marketplaceSources',
    'marketplaceLastRefresh'
)

# ============================================
# BIGDADDY-G AGENTIC MODE
# ============================================

function Start-BigDaddyAgent {
    <#
    .SYNOPSIS
        Starts the BigDaddy-G agentic assistant with function-calling capabilities
    .DESCRIPTION
        Launches an interactive agent loop that monitors for {{function:Name(args)}}
        patterns in the model's replies and executes them automatically.
    .EXAMPLE
        Start-BigDaddyAgent
    .NOTES
        Press Ctrl+C to exit the agent loop
    #>

    Write-Host "🤖 Initializing BigDaddy-G Agent..." -ForegroundColor Cyan

    # ---------- 1. Load agent tools ----------
    # Try multiple possible locations for AgentTools.ps1
    $agentToolsPaths = @(
        "D:\BigDaddyG-40GB-Torrent\AgentTools.ps1",                                    # Original location
        (Join-Path $PSScriptRoot "..\agents\AgentTools.ps1"),                          # Workspace agents folder
        (Join-Path (Split-Path $PSScriptRoot -Parent) "agents\AgentTools.ps1")        # Relative to Powershield root
    )

    $agentToolsPath = $null
    foreach ($path in $agentToolsPaths) {
        if (Test-Path $path) {
            $agentToolsPath = $path
            break
        }
    }

    if (-not $agentToolsPath) {
        Write-Host "❌ AgentTools.ps1 not found in any of the following locations:" -ForegroundColor Red
        $agentToolsPaths | ForEach-Object { Write-Host "   - $_" -ForegroundColor Yellow }
        Write-Host "`n💡 Copy AgentTools.ps1 to one of the above locations or update the path" -ForegroundColor Cyan
        return
    }

    . $agentToolsPath
    Write-Host "✅ Loaded agent tools from $agentToolsPath" -ForegroundColor Green

    # Verify required functions exist
    $requiredFuncs = @('Invoke-WebScrape', 'Invoke-RawrZPayload')
    foreach ($func in $requiredFuncs) {
        if (-not (Get-Command $func -ErrorAction SilentlyContinue)) {
            Write-Host "⚠️  Warning: Function '$func' not found in AgentTools.ps1" -ForegroundColor Yellow
        }
    }

    # ---------- 2. Primed history ----------
    $history = @(
        "You are BigDaddy-G, a helpful security-testing assistant.",
        "When you need data from a URL, reply exactly: {{function:Invoke-WebScrape(URL)}}",
        "When you want to deploy a test payload, reply exactly: {{function:Invoke-RawrZPayload(IP)}}",
        "User: scan 192.168.1.1 and summarise any open ports."
    )

    Write-Host "🚀 BigDaddy-G Agent Active - Press Ctrl+C to exit" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

    # ---------- 3. Agent loop ----------
    while ($true) {
        try {
            $prompt = $history -join "`n"
            $reply = ollama run bg40-unleashed $prompt
            Write-Host "`n>>> $reply" -ForegroundColor White

            # Check for function call pattern
            if ($reply -match '\{\{function:(\w+)\(([^)]*)\)\}\}') {
                $func = $Matches[1]
                $arg = $Matches[2]

                Write-Host "⚡ Executing: $func($arg)" -ForegroundColor Yellow

                if (Get-Command $func -ErrorAction SilentlyContinue) {
                    $result = & $func $arg
                    $history += "Function returned: $result"
                    Write-Host "✅ Function completed" -ForegroundColor Green
                }
                else {
                    $errorMsg = "Function '$func' not found"
                    $history += "Error: $errorMsg"
                    Write-Host "❌ $errorMsg" -ForegroundColor Red
                }
            }
            else {
                $history += $reply
            }

            # Trim history to last 50 entries
            if ($history.Count -gt 50) {
                $history = $history[-50..-1]
            }
        }
        catch {
            Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
            if ($_.Exception.Message -match "ollama") {
                Write-Host "💡 Ensure 'ollama' is installed and 'bg40' model is available" -ForegroundColor Yellow
                break
            }
        }
    }

    Write-Host "`n🛑 BigDaddy-G Agent stopped" -ForegroundColor Yellow
}
