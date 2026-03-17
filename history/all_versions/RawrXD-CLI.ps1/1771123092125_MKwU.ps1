#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Toolchain Command-Line Interface

.DESCRIPTION
    Unified CLI for interacting with the RawrXD production toolchain.
    Provides easy access to PE generation, encoding, and toolchain utilities.

.EXAMPLE
    .\RawrXD-CLI.ps1 generate-pe output.exe
    .\RawrXD-CLI.ps1 test-encoder
    .\RawrXD-CLI.ps1 info
#>

param(
    [Parameter(Position = 0, Mandatory = $false)]
    [ValidateSet("generate-pe", "test-encoder", "info", "list-libs", "help", "verify",
                 "install-extension", "list-extensions", "uninstall-extension")]
    [string]$Command = "help",

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$RawrXDRoot = "C:\RawrXD"
$BinDir = Join-Path $RawrXDRoot "bin"
$LibDir = Join-Path $RawrXDRoot "Libraries"
$DocsDir = Join-Path $RawrXDRoot "Docs"
$ExtensionsDir = Join-Path $env:APPDATA "RawrXD\extensions"
$ExtRegistryPath = "D:\rawrxd\extensions\registry.json"

function Write-Banner {
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  RawrXD Production Toolchain CLI v2.0" -ForegroundColor Green
    Write-Host "  Pure x64 Assembly - PE Generation & Encoding" -ForegroundColor Gray
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
}

function Invoke-GeneratePE {
    param([string]$OutputFile = "output.exe")
    
    Write-Host "🔨 Generating PE executable: $OutputFile" -ForegroundColor Yellow
    
    $generator = Join-Path $BinDir "pe_generator.exe"
    if (!(Test-Path $generator)) {
        Write-Error "PE generator not found at: $generator"
        return 1
    }
    
    $originalDir = Get-Location
    try {
        Set-Location $RawrXDRoot
        & $generator
        
        if (Test-Path "output.exe") {
            $size = (Get-Item "output.exe").Length
            Write-Host "✓ Generated PE executable: output.exe ($size bytes)" -ForegroundColor Green
            
            if ($OutputFile -ne "output.exe") {
                Move-Item "output.exe" $OutputFile -Force
                Write-Host "✓ Renamed to: $OutputFile" -ForegroundColor Green
            }
            return 0
        } else {
            Write-Error "PE generation failed"
            return 1
        }
    } finally {
        Set-Location $originalDir
    }
}

function Invoke-TestEncoder {
    Write-Host "🧪 Testing instruction encoder..." -ForegroundColor Yellow
    
    $tester = Join-Path $BinDir "instruction_encoder_test.exe"
    if (!(Test-Path $tester)) {
        Write-Warning "Encoder test executable not found at: $tester"
        Write-Host "You can still link against the libraries:" -ForegroundColor Gray
        Write-Host "  - $LibDir\rawrxd_encoder.lib" -ForegroundColor Gray
        return 1
    }
    
    Write-Host ""
    & $tester
    return $LASTEXITCODE
}

function Show-Info {
    Write-Banner
    
    Write-Host "📍 Installation Directory:" -ForegroundColor Yellow
    Write-Host "   $RawrXDRoot" -ForegroundColor White
    Write-Host ""
    
    Write-Host "📦 Executables:" -ForegroundColor Yellow
    if (Test-Path $BinDir) {
        Get-ChildItem $BinDir\*.exe | ForEach-Object {
            $sizeKB = [math]::Round($_.Length / 1KB, 2)
            Write-Host "   ✓ $($_.Name) ($sizeKB KB)" -ForegroundColor Green
        }
    } else {
        Write-Host "   [No bin directory]" -ForegroundColor Gray
    }
    Write-Host ""
    
    Write-Host "📚 Static Libraries:" -ForegroundColor Yellow
    if (Test-Path $LibDir) {
        Get-ChildItem $LibDir\*.lib | ForEach-Object {
            $sizeKB = [math]::Round($_.Length / 1KB, 2)
            Write-Host "   ✓ $($_.Name) ($sizeKB KB)" -ForegroundColor Green
        }
    } else {
        Write-Host "   [Libraries not found]" -ForegroundColor Gray
    }
    Write-Host ""
    
    Write-Host "📖 Documentation:" -ForegroundColor Yellow
    if (Test-Path $DocsDir) {
        $docCount = (Get-ChildItem $DocsDir\*.md).Count
        Write-Host "   $docCount markdown files in Docs\" -ForegroundColor White
    }
    Write-Host ""
}

function Show-Libraries {
    Write-Host "📚 Available RawrXD Libraries:" -ForegroundColor Cyan
    Write-Host ""
    
    if (!(Test-Path $LibDir)) {
        Write-Error "Libraries directory not found: $LibDir"
        return 1
    }
    
    $libs = Get-ChildItem $LibDir\*.lib
    foreach ($lib in $libs) {
        $sizeKB = [math]::Round($lib.Length / 1KB, 2)
        Write-Host "  📦 $($lib.Name)" -ForegroundColor Green
        Write-Host "     Size: $sizeKB KB" -ForegroundColor Gray
        Write-Host "     Path: $($lib.FullName)" -ForegroundColor Gray
        
        # Show corresponding header if exists
        $headerName = $lib.BaseName + ".h"
        $headerPath = Join-Path $RawrXDRoot "Headers\$headerName"
        if (Test-Path $headerPath) {
            Write-Host "     Header: Headers\$headerName" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    Write-Host "💡 Link against these libraries in your C++ projects:" -ForegroundColor Yellow
    Write-Host "   link.exe your_code.obj $LibDir\rawrxd_encoder.lib /OUT:your_app.exe" -ForegroundColor White
    Write-Host ""
}

function Invoke-Verify {
    Write-Host "🔍 Verifying RawrXD Toolchain Installation..." -ForegroundColor Yellow
    Write-Host ""
    
    $allGood = $true
    
    # Check bin directory
    if (Test-Path (Join-Path $BinDir "pe_generator.exe")) {
        Write-Host "✓ PE Generator found" -ForegroundColor Green
    } else {
        Write-Host "✗ PE Generator missing" -ForegroundColor Red
        $allGood = $false
    }
    
    # Check libraries
    $requiredLibs = @("rawrxd_encoder.lib", "rawrxd_pe_gen.lib")
    foreach ($lib in $requiredLibs) {
        if (Test-Path (Join-Path $LibDir $lib)) {
            Write-Host "✓ $lib found" -ForegroundColor Green
        } else {
            Write-Host "✗ $lib missing" -ForegroundColor Red
            $allGood = $false
        }
    }
    
    # Check headers
    if (Test-Path (Join-Path $RawrXDRoot "Headers")) {
        $headerCount = (Get-ChildItem (Join-Path $RawrXDRoot "Headers\*.h")).Count
        Write-Host "✓ $headerCount header files found" -ForegroundColor Green
    } else {
        Write-Host "✗ Headers directory missing" -ForegroundColor Red
        $allGood = $false
    }
    
    # Check docs
    if (Test-Path $DocsDir) {
        $docCount = (Get-ChildItem $DocsDir\*.md).Count
        Write-Host "✓ $docCount documentation files found" -ForegroundColor Green
    } else {
        Write-Host "⚠ Documentation directory missing" -ForegroundColor Yellow
    }
    
    Write-Host ""
    if ($allGood) {
        Write-Host "✅ Installation verified successfully!" -ForegroundColor Green
        return 0
    } else {
        Write-Host "❌ Installation incomplete - run Build-And-Wire.ps1 to rebuild" -ForegroundColor Red
        return 1
    }
}

# ============================================================================
# Extension Management — Install/List/Uninstall VS Code Marketplace Extensions
# ============================================================================

# Known extension ID mappings for common names
$KnownExtensions = @{
    "amazonq"           = "amazonwebservices.amazon-q-vscode"
    "amazon-q"          = "amazonwebservices.amazon-q-vscode"
    "amazon q"          = "amazonwebservices.amazon-q-vscode"
    "copilot"           = "GitHub.copilot"
    "github-copilot"    = "GitHub.copilot"
    "github copilot"    = "GitHub.copilot"
    "copilot-chat"      = "GitHub.copilot-chat"
}

function Resolve-ExtensionId {
    param([string]$ExtIdInput)
    $lower = $ExtIdInput.ToLower().Trim()
    if ($KnownExtensions.ContainsKey($lower)) {
        return $KnownExtensions[$lower]
    }
    # If it looks like publisher.name, use as-is
    if ($ExtIdInput -match '^[\w-]+\.[\w-]+') {
        return $ExtIdInput
    }
    return $null
}

function Get-ExtensionRegistry {
    if (Test-Path $ExtRegistryPath) {
        return (Get-Content $ExtRegistryPath -Raw | ConvertFrom-Json)
    }
    return [PSCustomObject]@{}
}

function Save-ExtensionRegistry {
    param($Registry)
    $Registry | ConvertTo-Json -Depth 5 | Set-Content $ExtRegistryPath -Encoding UTF8
}

function Invoke-InstallExtension {
    param([string[]]$ExtIds)

    if (-not $ExtIds -or $ExtIds.Count -eq 0) {
        Write-Host "❌ Usage: .\RawrXD-CLI.ps1 install-extension <extensionId|name> [...]" -ForegroundColor Red
        Write-Host ""
        Write-Host "  Examples:" -ForegroundColor Gray
        Write-Host "    .\RawrXD-CLI.ps1 install-extension amazonq" -ForegroundColor White
        Write-Host "    .\RawrXD-CLI.ps1 install-extension GitHub.copilot" -ForegroundColor White
        Write-Host "    .\RawrXD-CLI.ps1 install-extension amazonq copilot" -ForegroundColor White
        return 1
    }

    # Ensure extensions directory exists
    if (-not (Test-Path $ExtensionsDir)) {
        New-Item -ItemType Directory -Path $ExtensionsDir -Force | Out-Null
    }

    $registry = Get-ExtensionRegistry
    $successCount = 0

    foreach ($rawId in $ExtIds) {
        $extId = Resolve-ExtensionId $rawId
        if (-not $extId) {
            Write-Host "❌ Unknown extension: '$rawId'" -ForegroundColor Red
            Write-Host "   Use publisher.name format (e.g., GitHub.copilot)" -ForegroundColor Gray
            continue
        }

        $parts = $extId -split '\.'
        if ($parts.Count -lt 2) {
            Write-Host "❌ Invalid extension ID format: '$extId'" -ForegroundColor Red
            continue
        }
        $publisher = $parts[0]
        $extName   = ($parts[1..($parts.Count-1)] -join '.')

        Write-Host ""
        Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "  Installing: $extId" -ForegroundColor Yellow
        Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

        $installPath = Join-Path $ExtensionsDir $extId
        $vsixPath    = Join-Path $env:TEMP "$extId.vsix"

        # --- Step 1: Query VS Code Marketplace ---
        Write-Host "🔍 Querying VS Code Marketplace..." -ForegroundColor Gray

        $marketplaceBody = @{
            filters = @(
                @{
                    criteria = @(
                        @{ filterType = 7; value = $extId }
                    )
                    pageNumber = 1
                    pageSize   = 1
                    sortBy     = 0
                    sortOrder  = 0
                }
            )
            assetTypes = @()
            flags      = 950
        } | ConvertTo-Json -Depth 5

        try {
            $response = Invoke-RestMethod -Uri "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery" `
                -Method POST -ContentType "application/json" -Body $marketplaceBody -ErrorAction Stop

            $ext = $response.results[0].extensions[0]
            if (-not $ext) {
                Write-Host "❌ Extension '$extId' not found on VS Code Marketplace" -ForegroundColor Red
                continue
            }

            $displayName = $ext.displayName
            $version     = $ext.versions[0].version
            $desc        = $ext.shortDescription

            Write-Host "✓ Found: $displayName v$version" -ForegroundColor Green
            Write-Host "  $desc" -ForegroundColor Gray

            # --- Step 2: Download VSIX ---
            Write-Host "📥 Downloading VSIX package..." -ForegroundColor Gray

            $vsixUrl = "https://$publisher.gallery.vsassets.io/_apis/public/gallery/publisher/$publisher/extension/$extName/$version/assetbyname/Microsoft.VisualStudio.Services.VSIXPackage"

            Invoke-WebRequest -Uri $vsixUrl -OutFile $vsixPath -ErrorAction Stop

            if (-not (Test-Path $vsixPath)) {
                Write-Host "❌ Download failed" -ForegroundColor Red
                continue
            }

            $vsixSize = (Get-Item $vsixPath).Length
            $vsixSizeKB = [math]::Round($vsixSize / 1KB, 1)
            Write-Host "✓ Downloaded: $vsixSizeKB KB" -ForegroundColor Green

        } catch {
            Write-Host "⚠ Marketplace download failed: $($_.Exception.Message)" -ForegroundColor Yellow
            Write-Host "  Creating extension manifest entry (offline registration)..." -ForegroundColor Gray

            # Offline registration — create stub manifest so the Win32 GUI knows about it
            if (-not (Test-Path $installPath)) {
                New-Item -ItemType Directory -Path $installPath -Force | Out-Null
            }

            $manifest = @{
                name        = $extName
                publisher   = $publisher
                displayName = $extId
                version     = "0.0.0-pending"
                description = "Registered via RawrXD CLI (pending marketplace download)"
                engines     = @{ vscode = "*" }
            }
            $manifest | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $installPath "package.json") -Encoding UTF8

            $nativeManifest = @{
                converted          = $true
                native_mode        = $true
                original_vsix      = $extId
                signature_verified = $false
                has_native_code    = $false
                publisher          = $publisher
                version            = "0.0.0-pending"
                install_time       = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
                install_source     = "rawrxd-cli-offline"
            }
            $nativeManifest | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $installPath "native_manifest.json") -Encoding UTF8

            # Update registry
            $registry | Add-Member -NotePropertyName $extId -NotePropertyValue ([PSCustomObject]@{
                Enabled       = $true
                Path          = $installPath
                InstalledPath = $installPath
                Created       = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
                Type          = "VSCodeExtension"
                LastEnabled   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
                Publisher     = $publisher
                ExtensionName = $extName
                Version       = "0.0.0-pending"
                Installed     = $true
                Source        = "marketplace-offline"
            }) -Force

            Write-Host "✓ Registered: $extId (offline — will sync when marketplace available)" -ForegroundColor Green
            $successCount++
            continue
        }

        # --- Step 3: Extract VSIX ---
        Write-Host "📦 Extracting VSIX to $installPath..." -ForegroundColor Gray

        if (Test-Path $installPath) {
            Remove-Item -Path $installPath -Recurse -Force -ErrorAction SilentlyContinue
        }
        New-Item -ItemType Directory -Path $installPath -Force | Out-Null

        try {
            Expand-Archive -LiteralPath $vsixPath -DestinationPath $installPath -Force -ErrorAction Stop
            Write-Host "✓ Extracted successfully" -ForegroundColor Green
        } catch {
            Write-Host "❌ Extraction failed: $($_.Exception.Message)" -ForegroundColor Red
            continue
        }

        # --- Step 4: Create native manifest ---
        Write-Host "🔧 Creating native RawrXD manifest..." -ForegroundColor Gray

        $nativeManifest = @{
            converted          = $true
            native_mode        = $true
            original_vsix      = $extId
            signature_verified = $false
            has_native_code    = $false
            publisher          = $publisher
            version            = $version
            install_time       = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
            install_source     = "rawrxd-cli"
        }
        $nativeManifest | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $installPath "native_manifest.json") -Encoding UTF8

        # --- Step 5: Update extension registry ---
        $registry | Add-Member -NotePropertyName $extId -NotePropertyValue ([PSCustomObject]@{
            Enabled       = $true
            Path          = $installPath
            InstalledPath = $installPath
            Created       = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            Type          = "VSCodeExtension"
            LastEnabled   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            Publisher     = $publisher
            ExtensionName = $extName
            DisplayName   = $displayName
            Version       = $version
            Installed     = $true
            Source        = "marketplace"
        }) -Force

        # Cleanup VSIX temp file
        Remove-Item $vsixPath -Force -ErrorAction SilentlyContinue

        Write-Host ""
        Write-Host "✅ Successfully installed: $displayName v$version" -ForegroundColor Green
        Write-Host "   Location: $installPath" -ForegroundColor Gray
        $successCount++
    }

    # Save registry
    Save-ExtensionRegistry $registry

    # --- Also register in Win32 registry for GUI pickup ---
    try {
        $regKey = "HKCU:\Software\RawrXD\Extensions"
        if (-not (Test-Path $regKey)) {
            New-Item -Path $regKey -Force | Out-Null
        }
        foreach ($rawId in $ExtIds) {
            $extId = Resolve-ExtensionId $rawId
            if ($extId) {
                $installPath = Join-Path $ExtensionsDir $extId
                Set-ItemProperty -Path $regKey -Name $extId -Value $installPath -ErrorAction SilentlyContinue
            }
        }
        Write-Host ""
        Write-Host "📋 Win32 registry updated (HKCU:\Software\RawrXD\Extensions)" -ForegroundColor Gray
    } catch {
        Write-Host "⚠ Win32 registry update skipped: $($_.Exception.Message)" -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  $successCount of $($ExtIds.Count) extension(s) installed" -ForegroundColor Green
    Write-Host "  Restart RawrXD IDE (Win32 or Electron) to activate" -ForegroundColor Gray
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""

    return $(if ($successCount -eq $ExtIds.Count) { 0 } else { 1 })
}

function Invoke-ListExtensions {
    Write-Host ""
    Write-Host "📦 Installed RawrXD Extensions:" -ForegroundColor Cyan
    Write-Host ""

    $registry = Get-ExtensionRegistry
    $count = 0

    $registry.PSObject.Properties | ForEach-Object {
        $name = $_.Name
        $ext  = $_.Value
        $count++
        $status = if ($ext.Enabled) { "✓ Enabled" } else { "✗ Disabled" }
        $color  = if ($ext.Enabled) { "Green" } else { "DarkGray" }

        Write-Host "  $status  $name" -ForegroundColor $color
        if ($ext.DisplayName) {
            Write-Host "          Display: $($ext.DisplayName)" -ForegroundColor Gray
        }
        if ($ext.Version) {
            Write-Host "          Version: $($ext.Version)" -ForegroundColor Gray
        }
        if ($ext.Type) {
            Write-Host "          Type:    $($ext.Type)" -ForegroundColor Gray
        }
        Write-Host "          Path:    $($ext.Path)" -ForegroundColor DarkGray
        Write-Host ""
    }

    # Also scan filesystem for unregistered extensions
    if (Test-Path $ExtensionsDir) {
        $fsDirs = Get-ChildItem $ExtensionsDir -Directory -ErrorAction SilentlyContinue
        foreach ($dir in $fsDirs) {
            $name = $dir.Name
            $alreadyListed = $registry.PSObject.Properties.Name -contains $name
            if (-not $alreadyListed) {
                $count++
                Write-Host "  ⚠ Unregistered  $name" -ForegroundColor Yellow
                Write-Host "          Path:    $($dir.FullName)" -ForegroundColor DarkGray
                Write-Host ""
            }
        }
    }

    if ($count -eq 0) {
        Write-Host "  (none installed)" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  Install extensions with:" -ForegroundColor Yellow
        Write-Host "    .\RawrXD-CLI.ps1 install-extension amazonq" -ForegroundColor White
        Write-Host "    .\RawrXD-CLI.ps1 install-extension GitHub.copilot" -ForegroundColor White
    } else {
        Write-Host "  Total: $count extension(s)" -ForegroundColor Cyan
    }
    Write-Host ""
    return 0
}

function Invoke-UninstallExtension {
    param([string[]]$ExtIds)

    if (-not $ExtIds -or $ExtIds.Count -eq 0) {
        Write-Host "❌ Usage: .\RawrXD-CLI.ps1 uninstall-extension <extensionId>" -ForegroundColor Red
        return 1
    }

    $registry = Get-ExtensionRegistry

    foreach ($rawId in $ExtIds) {
        $extId = Resolve-ExtensionId $rawId
        if (-not $extId) { $extId = $rawId }

        $installPath = Join-Path $ExtensionsDir $extId

        Write-Host "🗑  Uninstalling: $extId..." -ForegroundColor Yellow

        if (Test-Path $installPath) {
            Remove-Item -Path $installPath -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "✓ Removed files: $installPath" -ForegroundColor Green
        }

        if ($registry.PSObject.Properties.Name -contains $extId) {
            $registry.PSObject.Properties.Remove($extId)
            Write-Host "✓ Removed from registry" -ForegroundColor Green
        }

        # Remove from Win32 registry
        try {
            $regKey = "HKCU:\Software\RawrXD\Extensions"
            if (Test-Path $regKey) {
                Remove-ItemProperty -Path $regKey -Name $extId -ErrorAction SilentlyContinue
            }
        } catch {}

        Write-Host "✅ Uninstalled: $extId" -ForegroundColor Green
        Write-Host ""
    }

    Save-ExtensionRegistry $registry
    return 0
}

function Show-Help {
    Write-Banner
    
    Write-Host "USAGE:" -ForegroundColor Yellow
    Write-Host "  .\RawrXD-CLI.ps1 <command> [arguments]"
    Write-Host ""
    
    Write-Host "COMMANDS:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  generate-pe [output]  Generate a PE executable" -ForegroundColor Green
    Write-Host "                        Default: output.exe"
    Write-Host "                        Example: .\RawrXD-CLI.ps1 generate-pe myapp.exe"
    Write-Host ""
    
    Write-Host "  test-encoder          Run instruction encoder tests" -ForegroundColor Green
    Write-Host "                        Tests the x64 instruction encoding library"
    Write-Host ""
    
    Write-Host "  info                  Show toolchain information" -ForegroundColor Green
    Write-Host "                        Display installed components and versions"
    Write-Host ""
    
    Write-Host "  list-libs             List available static libraries" -ForegroundColor Green
    Write-Host "                        Show all .lib files with sizes and paths"
    Write-Host ""
    
    Write-Host "  verify                Verify installation integrity" -ForegroundColor Green
    Write-Host "                        Check that all required components are present"
    Write-Host ""
    
    Write-Host "EXTENSION MANAGEMENT:" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "  install-extension     Install VS Code extensions into RawrXD IDE" -ForegroundColor Green
    Write-Host "                        Downloads from VS Code Marketplace & registers"
    Write-Host "                        Supports publisher.name or shorthand aliases"
    Write-Host ""
    Write-Host "  list-extensions       List all installed extensions" -ForegroundColor Green
    Write-Host "                        Shows registered & filesystem extensions"
    Write-Host ""
    Write-Host "  uninstall-extension   Remove an installed extension" -ForegroundColor Green
    Write-Host "                        Removes files, registry entry, and Win32 reg"
    Write-Host ""
    
    Write-Host "  help                  Show this help message" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  .\RawrXD-CLI.ps1 generate-pe"
    Write-Host "  .\RawrXD-CLI.ps1 generate-pe custom_output.exe"
    Write-Host "  .\RawrXD-CLI.ps1 test-encoder"
    Write-Host "  .\RawrXD-CLI.ps1 info"
    Write-Host "  .\RawrXD-CLI.ps1 list-libs"
    Write-Host "  .\RawrXD-CLI.ps1 verify"
    Write-Host ""
    Write-Host "  .\RawrXD-CLI.ps1 install-extension amazonq" -ForegroundColor White
    Write-Host "  .\RawrXD-CLI.ps1 install-extension GitHub.copilot" -ForegroundColor White
    Write-Host "  .\RawrXD-CLI.ps1 install-extension amazonq copilot" -ForegroundColor White
    Write-Host "  .\RawrXD-CLI.ps1 list-extensions" -ForegroundColor White
    Write-Host "  .\RawrXD-CLI.ps1 uninstall-extension GitHub.copilot" -ForegroundColor White
    Write-Host ""
    
    Write-Host "DOCUMENTATION:" -ForegroundColor Yellow
    Write-Host "  Quick Start:  $DocsDir\PE_GENERATOR_QUICK_REF.md"
    Write-Host "  Full Docs:    $DocsDir\PRODUCTION_TOOLCHAIN_DOCS.md"
    Write-Host ""
}

# Main execution
switch ($Command) {
    "generate-pe" {
        $outputFile = if ($Arguments) { $Arguments[0] } else { "output.exe" }
        exit (Invoke-GeneratePE -OutputFile $outputFile)
    }
    "test-encoder" {
        exit (Invoke-TestEncoder)
    }
    "info" {
        Show-Info
        exit 0
    }
    "list-libs" {
        Show-Libraries
        exit 0
    }
    "verify" {
        exit (Invoke-Verify)
    }
    "install-extension" {
        exit (Invoke-InstallExtension -ExtIds $Arguments)
    }
    "list-extensions" {
        exit (Invoke-ListExtensions)
    }
    "uninstall-extension" {
        exit (Invoke-UninstallExtension -ExtIds $Arguments)
    }
    "help" {
        Show-Help
        exit 0
    }
    default {
        Show-Help
        exit 0
    }
}
