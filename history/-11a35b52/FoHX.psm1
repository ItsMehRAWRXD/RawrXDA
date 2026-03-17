# RawrXD-CopilotIntegration.psm1
# Copilot branding, legal compliance, and telemetry for RawrXD IDE

using namespace System.Windows.Forms
using namespace System.Drawing
using namespace System.Net.Http
using namespace System.Text.Json

class CopilotBrandingManager {
    static [void]ApplyCopilotStyling([System.Windows.Forms.Control]$control) {
        # Apply GitHub Copilot color scheme
        $copilotColors = @{
            Primary = [Color]::FromArgb(0, 122, 255)  # Copilot blue
            Secondary = [Color]::FromArgb(9, 105, 218)  # GitHub blue
            Accent = [Color]::FromArgb(163, 113, 247)  # Purple accent
            Background = [Color]::FromArgb(246, 248, 250)  # Light gray
            Text = [Color]::FromArgb(36, 41, 47)  # Dark gray
        }

        $control.BackColor = $copilotColors.Background
        $control.ForeColor = $copilotColors.Text

        if ($control -is [Button]) {
            $control.BackColor = $copilotColors.Primary
            $control.ForeColor = [Color]::White
            $control.FlatStyle = [FlatStyle]::Flat
            $control.FlatAppearance.BorderColor = $copilotColors.Primary
        }

        if ($control -is [TextBox] -or $control -is [RichTextBox]) {
            $control.BorderStyle = [BorderStyle]::FixedSingle
        }

        # Apply to child controls recursively
        foreach ($child in $control.Controls) {
            [CopilotBrandingManager]::ApplyCopilotStyling($child)
        }
    }

    static [Image]GetCopilotLogo() {
        # Create a simple Copilot logo programmatically
        $bitmap = New-Object Bitmap 32, 32
        $graphics = [Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

        # Draw Copilot icon (simplified)
        $brush = New-Object SolidBrush ([Color]::FromArgb(0, 122, 255))
        $graphics.FillEllipse($brush, 4, 4, 24, 24)

        # Draw sparkles
        $whiteBrush = New-Object SolidBrush ([Color]::White)
        $graphics.FillEllipse($whiteBrush, 8, 8, 4, 4)
        $graphics.FillEllipse($whiteBrush, 20, 12, 3, 3)
        $graphics.FillEllipse($whiteBrush, 12, 20, 2, 2)

        $graphics.Dispose()
        return $bitmap
    }

    static [Font]GetCopilotFont([float]$size = 9) {
        # Use a Copilot-like font (Segoe UI or similar)
        return New-Object Font ("Segoe UI", $size, [FontStyle]::Regular)
    }
}

class TelemetryManager {
    [string]$Endpoint
    [bool]$Enabled
    [hashtable]$UserPreferences
    [System.Collections.Concurrent.ConcurrentQueue[hashtable]]$EventQueue
    [System.Threading.Timer]$FlushTimer

    TelemetryManager([string]$endpoint) {
        $this.Endpoint = $endpoint
        $this.Enabled = $true
        $this.UserPreferences = @{
            UsageStats = $true
            ErrorReports = $true
            PerformanceMetrics = $true
            CodeSuggestions = $false  # Don't send code by default
        }
        $this.EventQueue = New-Object System.Collections.Concurrent.ConcurrentQueue[hashtable]
        $this.StartFlushTimer()
    }

    [void]SetUserConsent([hashtable]$preferences) {
        $this.UserPreferences = $preferences
        $this.Enabled = $preferences.Values -contains $true
    }

    [void]TrackEvent([string]$eventName, [hashtable]$properties = @{}) {
        if (-not $this.Enabled) { return }

        $event = @{
            EventName = $eventName
            Timestamp = Get-Date -Format "o"
            SessionId = [TelemetryManager]::GetSessionId()
            UserId = [TelemetryManager]::GetAnonymizedUserId()
            Properties = [TelemetryManager]::AnonymizeProperties($properties)
            Version = "1.0.0"
        }

        $this.EventQueue.Enqueue($event)
    }

    [void]TrackError([Exception]$exception, [string]$context = "") {
        if (-not $this.UserPreferences.ErrorReports) { return }

        $errorEvent = @{
            Message = $exception.Message
            StackTrace = [TelemetryManager]::AnonymizeStackTrace($exception.StackTrace)
            Context = $context
            ExceptionType = $exception.GetType().Name
        }

        $this.TrackEvent("Error", $errorEvent)
    }

    [void]TrackPerformance([string]$operation, [TimeSpan]$duration, [hashtable]$metadata = @{}) {
        if (-not $this.UserPreferences.PerformanceMetrics) { return }

        $perfEvent = @{
            Operation = $operation
            DurationMs = $duration.TotalMilliseconds
            Metadata = $metadata
        }

        $this.TrackEvent("Performance", $perfEvent)
    }

    [void]TrackCodeSuggestion([string]$suggestion, [bool]$accepted, [string]$language = "") {
        if (-not $this.UserPreferences.CodeSuggestions) { return }

        # Never send actual code - only metadata
        $codeEvent = @{
            Language = $language
            SuggestionLength = $suggestion.Length
            Accepted = $accepted
            Hash = [TelemetryManager]::GetStringHash($suggestion)
        }

        $this.TrackEvent("CodeSuggestion", $codeEvent)
    }

    static [string]GetSessionId() {
        $sessionFile = Join-Path $env:TEMP "rawrxd_session.id"
        if (Test-Path $sessionFile) {
            return Get-Content $sessionFile -Raw
        }

        $sessionId = [Guid]::NewGuid().ToString()
        $sessionId | Out-File $sessionFile -Encoding UTF8
        return $sessionId
    }

    static [string]GetAnonymizedUserId() {
        $userIdFile = Join-Path $env:APPDATA "RawrXD\user.id"
        if (Test-Path $userIdFile) {
            return Get-Content $userIdFile -Raw
        }

        # Create anonymized ID based on hardware hash
        $hardwareInfo = Get-WmiObject Win32_ComputerSystemProduct | Select-Object -ExpandProperty UUID
        $userId = [TelemetryManager]::GetStringHash($hardwareInfo)
        $userId | Out-File $userIdFile -Encoding UTF8
        return $userId
    }

    static [hashtable]AnonymizeProperties([hashtable]$properties) {
        $anonymized = @{}
        foreach ($key in $properties.Keys) {
            $value = $properties[$key]
            if ($value -is [string] -and $value.Length -gt 100) {
                # Truncate long strings
                $anonymized[$key] = $value.Substring(0, 100) + "..."
            }
            elseif ($key -match "password|token|key|secret") {
                $anonymized[$key] = "[REDACTED]"
            }
            else {
                $anonymized[$key] = $value
            }
        }
        return $anonymized
    }

    static [string]AnonymizeStackTrace([string]$stackTrace) {
        # Remove file paths and sensitive information
        $lines = $stackTrace -split "`n"
        $anonymizedLines = @()
        foreach ($line in $lines) {
            $anonymizedLine = $line -replace 'at .*? in [A-Za-z]:\\.*?\\', 'at <method> in <file>:'
            $anonymizedLines += $anonymizedLine
        }
        return $anonymizedLines -join "`n"
    }

    static [string]GetStringHash([string]$input) {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $bytes = [Encoding]::UTF8.GetBytes($input)
        $hash = $sha256.ComputeHash($bytes)
        return [BitConverter]::ToString($hash).Replace("-", "").ToLower()
    }

    [void]StartFlushTimer() {
        $this.FlushTimer = New-Object System.Threading.Timer(
            { $this.FlushEvents() },
            $null,
            30000,  # 30 seconds
            30000
        )
    }

    [void]FlushEvents() {
        $events = New-Object System.Collections.Generic.List[hashtable]
        $event = $null
        while ($this.EventQueue.TryDequeue([ref]$event)) {
            $events.Add($event)
        }

        if ($events.Count -gt 0) {
            $this.SendEvents($events)
        }
    }

    [void]SendEvents([System.Collections.Generic.List[hashtable]]$events) {
        try {
            $json = [JsonSerializer]::Serialize($events)
            $content = New-Object StringContent ($json, [Encoding]::UTF8, "application/json")

            $client = New-Object HttpClient
            $response = $client.PostAsync($this.Endpoint, $content).Result

            if (-not $response.IsSuccessStatusCode) {
                # Log failed telemetry send (without sensitive data)
                Write-Warning "Telemetry send failed: $($response.StatusCode)"
            }
        }
        catch {
            # Silently fail telemetry
        }
    }

    [void]Dispose() {
        $this.FlushTimer.Dispose()
        $this.FlushEvents()
    }
}

class ComplianceManager {
    static [bool]CheckGDPRCompliance([hashtable]$data) {
        # Check if data contains personal information
        $personalDataPatterns = @(
            '\b\d{3}-\d{2}-\d{4}\b',  # SSN
            '\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b',  # Email
            '\b\d{3}-\d{3}-\d{4}\b',  # Phone
            '\b\d{1,2}/\d{1,2}/\d{4}\b'  # Date of birth
        )

        foreach ($pattern in $personalDataPatterns) {
            if ($data.Values -match $pattern) {
                return $false
            }
        }

        return $true
    }

    static [void]LogComplianceEvent([string]$event, [hashtable]$data) {
        $complianceLog = Join-Path $env:APPDATA "RawrXD\compliance.log"
        $entry = @{
            Timestamp = Get-Date -Format "o"
            Event = $event
            DataProcessed = $data.Count
            Compliant = [ComplianceManager]::CheckGDPRCompliance($data)
        }

        $json = $entry | ConvertTo-Json
        Add-Content -Path $complianceLog -Value $json -Encoding UTF8
    }

    static [void]GenerateComplianceReport() {
        $complianceLog = Join-Path $env:APPDATA "RawrXD\compliance.log"
        if (-not (Test-Path $complianceLog)) { return }

        $entries = Get-Content $complianceLog | ConvertFrom-Json
        $report = @{
            ReportDate = Get-Date -Format "o"
            TotalEvents = $entries.Count
            CompliantEvents = ($entries | Where-Object Compliant).Count
            NonCompliantEvents = ($entries | Where-Object -not Compliant).Count
            DataRetentionDays = 2555  # 7 years for GDPR
            LastCleanup = Get-Date
        }

        $reportPath = Join-Path $env:APPDATA "RawrXD\compliance_report.json"
        $report | ConvertTo-Json | Out-File $reportPath -Encoding UTF8
    }
}

class PrivacyControls {
    [hashtable]$Settings

    PrivacyControls() {
        $this.LoadSettings()
    }

    [void]LoadSettings() {
        $settingsFile = Join-Path $env:APPDATA "RawrXD\privacy_settings.json"
        if (Test-Path $settingsFile) {
            $this.Settings = Get-Content $settingsFile | ConvertFrom-Json -AsHashtable
        }
        else {
            $this.Settings = @{
                TelemetryEnabled = $true
                DataAnonymization = $true
                LocalProcessingOnly = $false
                DataExportEnabled = $true
                DataDeletionEnabled = $true
                NetworkMonitoring = $true
            }
            $this.SaveSettings()
        }
    }

    [void]SaveSettings() {
        $settingsFile = Join-Path $env:APPDATA "RawrXD\privacy_settings.json"
        $this.Settings | ConvertTo-Json | Out-File $settingsFile -Encoding UTF8
    }

    [void]UpdateSetting([string]$key, [bool]$value) {
        $this.Settings[$key] = $value
        $this.SaveSettings()
    }

    [bool]IsAllowed([string]$feature) {
        return $this.Settings.ContainsKey($feature) -and $this.Settings[$feature]
    }

    [void]ExportUserData([string]$exportPath) {
        if (-not $this.IsAllowed("DataExportEnabled")) {
            throw "Data export not enabled"
        }

        $userData = @{
            Settings = $this.Settings
            SessionHistory = Get-Content (Join-Path $env:APPDATA "RawrXD\session.log") -ErrorAction SilentlyContinue
            Preferences = Get-Content (Join-Path $env:APPDATA "RawrXD\preferences.json") -ErrorAction SilentlyContinue
        }

        $userData | ConvertTo-Json -Depth 10 | Out-File $exportPath -Encoding UTF8
    }

    [void]DeleteUserData() {
        if (-not $this.IsAllowed("DataDeletionEnabled")) {
            throw "Data deletion not enabled"
        }

        $dataPaths = @(
            Join-Path $env:APPDATA "RawrXD\*.log"
            Join-Path $env:APPDATA "RawrXD\*.json"
            Join-Path $env:APPDATA "RawrXD\credentials.vault*"
        )

        foreach ($path in $dataPaths) {
            Remove-Item $path -Force -ErrorAction SilentlyContinue
        }

        # Reset settings
        $this.Settings = @{
            TelemetryEnabled = $false
            DataAnonymization = $true
            LocalProcessingOnly = $true
            DataExportEnabled = $false
            DataDeletionEnabled = $false
            NetworkMonitoring = $false
        }
        $this.SaveSettings()
    }

    [void]ShowPrivacyDialog() {
        $form = New-Object Form
        $form.Text = "Privacy Settings - RawrXD IDE"
        $form.Size = New-Object Drawing.Size(400, 500)
        $form.StartPosition = "CenterScreen"

        $y = 20
        foreach ($setting in $this.Settings.GetEnumerator()) {
            $checkbox = New-Object CheckBox
            $checkbox.Text = $setting.Key -replace "([A-Z])", " $1"
            $checkbox.Checked = $setting.Value
            $checkbox.Location = New-Object Drawing.Point(20, $y)
            $checkbox.Size = New-Object Drawing.Size(350, 20)
            $checkbox.Add_CheckedChanged({
                $key = $this.Text -replace " ", ""
                $this.Parent.Tag.Settings[$key] = $this.Checked
            })
            $form.Controls.Add($checkbox)
            $y += 30
        }

        $saveButton = New-Object Button
        $saveButton.Text = "Save Settings"
        $saveButton.Location = New-Object Drawing.Point(20, $y)
        $saveButton.Size = New-Object Drawing.Size(100, 30)
        $saveButton.Add_Click({
            $this.Parent.Tag.SaveSettings()
            $this.Parent.Close()
        })
        $form.Controls.Add($saveButton)

        $exportButton = New-Object Button
        $exportButton.Text = "Export Data"
        $exportButton.Location = New-Object Drawing.Point(130, $y)
        $exportButton.Size = New-Object Drawing.Size(100, 30)
        $exportButton.Add_Click({
            $saveDialog = New-Object SaveFileDialog
            $saveDialog.Filter = "JSON files (*.json)|*.json"
            if ($saveDialog.ShowDialog() -eq [DialogResult]::OK) {
                try {
                    $this.Parent.Tag.ExportUserData($saveDialog.FileName)
                    [MessageBox]::Show("Data exported successfully", "Success")
                }
                catch {
                    [MessageBox]::Show("Export failed: $_", "Error")
                }
            }
        })
        $form.Controls.Add($exportButton)

        $deleteButton = New-Object Button
        $deleteButton.Text = "Delete All Data"
        $deleteButton.Location = New-Object Drawing.Point(240, $y)
        $deleteButton.Size = New-Object Drawing.Size(120, 30)
        $deleteButton.BackColor = [Color]::Red
        $deleteButton.ForeColor = [Color]::White
        $deleteButton.Add_Click({
            $result = [MessageBox]::Show("This will permanently delete all your data. Are you sure?", "Confirm Deletion", [MessageBoxButtons]::YesNo, [MessageBoxIcon]::Warning)
            if ($result -eq [DialogResult]::Yes) {
                try {
                    $this.Parent.Tag.DeleteUserData()
                    [MessageBox]::Show("All data deleted", "Success")
                    $this.Parent.Close()
                }
                catch {
                    [MessageBox]::Show("Deletion failed: $_", "Error")
                }
            }
        })
        $form.Controls.Add($deleteButton)

        $form.Tag = $this
        $form.ShowDialog()
    }
}

# Export functions
function Initialize-CopilotBranding {
    param([System.Windows.Forms.Control]$control)
    [CopilotBrandingManager]::ApplyCopilotStyling($control)
}

function Get-CopilotLogo {
    return [CopilotBrandingManager]::GetCopilotLogo()
}

function Get-CopilotFont {
    param([float]$size = 9)
    return [CopilotBrandingManager]::GetCopilotFont($size)
}

function New-TelemetryManager {
    param([string]$endpoint = "https://api.github.com/telemetry")  # Placeholder endpoint
    return [TelemetryManager]::new($endpoint)
}

function Test-GDPRCompliance {
    param([hashtable]$data)
    return [ComplianceManager]::CheckGDPRCompliance($data)
}

function New-PrivacyControls {
    return [PrivacyControls]::new()
}

function Show-PrivacySettings {
    $privacy = New-PrivacyControls
    $privacy.ShowPrivacyDialog()
}

Export-ModuleMember -Function * -Variable *