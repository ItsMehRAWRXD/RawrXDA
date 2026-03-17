# Production-ready feature: SecurityVulnerabilityScanner
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error')][string]$Level = 'Info'
        )
        Write-Host "[$Level] $Message"
    }
}

function Invoke-SecurityVulnerabilityScanner {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)][ValidateScript({Test-Path $_ -PathType 'Container'})][string]$SourceDirectory = "D:/lazy init ide/src",
        [string]$ReportPath = "D:/lazy init ide/reports/security_report.json"
    )

    Write-StructuredLog -Message "Starting SecurityVulnerabilityScanner..." -Level Info

    try {
        # Ensure report directory exists
        $reportDir = Split-Path $ReportPath -Parent
        if (-not (Test-Path $reportDir)) {
            New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
        }

        # Scan for vulnerabilities
        $files = Get-ChildItem -Path $SourceDirectory -Recurse -Filter "*.ps1" -ErrorAction Stop
        $vulnerabilities = @()

        foreach ($file in $files) {
            Write-StructuredLog -Message "Scanning file: $($file.Name)" -Level Info
            try {
                $content = Get-Content -Path $file.FullName -Raw -ErrorAction Stop
                # Check for common vulnerabilities
                if ($content -match "Invoke-Expression") {
                    Write-StructuredLog -Message "Potential vulnerability found in $($file.Name): Use of Invoke-Expression" -Level Warning
                    $vulnerabilities += [PSCustomObject]@{
                        FileName = $file.Name
                        FullPath = $file.FullName
                        Issue = "Use of Invoke-Expression"
                        Severity = "High"
                        Timestamp = (Get-Date).ToString('o')
                    }
                }
                if ($content -match "ConvertTo-SecureString.*AsPlainText") {
                    Write-StructuredLog -Message "Potential vulnerability found in $($file.Name): Plain text password conversion" -Level Warning
                    $vulnerabilities += [PSCustomObject]@{
                        FileName = $file.Name
                        FullPath = $file.FullName
                        Issue = "Plain text password conversion"
                        Severity = "High"
                        Timestamp = (Get-Date).ToString('o')
                    }
                }
            } catch {
                Write-StructuredLog -Message "Error scanning file $($file.Name): $_" -Level Error
            }
        }

        # Save results to a report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            SourceDirectory = $SourceDirectory
            TotalFilesScanned = $files.Count
            VulnerabilitiesFound = $vulnerabilities.Count
            Vulnerabilities = $vulnerabilities
        }
        $report | ConvertTo-Json -Depth 10 | Set-Content -Path $ReportPath -ErrorAction Stop
        Write-StructuredLog -Message "Security report saved to $ReportPath" -Level Info

        Write-StructuredLog -Message "SecurityVulnerabilityScanner completed successfully." -Level Info
        return "success"
    } catch {
        Write-StructuredLog -Message "SecurityVulnerabilityScanner encountered an error: $_" -Level Error
        return "error"
    }
}

