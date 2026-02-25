# RawrXD.Config.psm1
# Production-ready, dependency-free config loader for RawrXD

function Get-RawrXDRootPath {
    $envRoot = $env:LAZY_INIT_IDE_ROOT
    if ($envRoot -and (Test-Path $envRoot)) {
        return $envRoot
    }

    $moduleRoot = $PSScriptRoot
    $envPathsFile = Join-Path $moduleRoot "config/env.paths.json"
    if (Test-Path $envPathsFile) {
        try {
            $envPaths = Get-Content $envPathsFile -Raw | ConvertFrom-Json
            if ($envPaths.LAZY_INIT_IDE_ROOT -and (Test-Path $envPaths.LAZY_INIT_IDE_ROOT)) {
                return $envPaths.LAZY_INIT_IDE_ROOT
            }
        } catch {
        }
    }

    return $moduleRoot
}

function Get-RawrXDConfigPath {
    $root = Get-RawrXDRootPath
    $envConfig = $env:RAWRXD_CONFIG_PATH

    if ($envConfig -and (Test-Path $envConfig)) {
        return $envConfig
    }

    $primary = Join-Path $root "config/RawrXD.config.json"
    if (Test-Path $primary) {
        return $primary
    }

    $fallback = Join-Path $root "config.example.json"
    if (Test-Path $fallback) {
        return $fallback
    }

    return $primary
}

function Import-RawrXDConfig {
    param(
        [string]$Path = $(Get-RawrXDConfigPath)
    )
    if (-Not (Test-Path $Path)) {
        Write-Host "[RawrXD][CONFIG] Config file not found at $Path. Using defaults."
        return @{}
    }
    try {
        $json = Get-Content $Path -Raw | ConvertFrom-Json
        return $json
    } catch {
        Write-Host "[RawrXD][CONFIG][ERROR] Failed to load config: $_" -ForegroundColor Red
        return @{}
    }
}

Export-ModuleMember -Function Import-RawrXDConfig, Get-RawrXDRootPath, Get-RawrXDConfigPath
