Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Enhanced error logging with file output
$ErrorActionPreference = "Continue"
$logPath = Join-Path $env:TEMP "RawrBrowser_DetailedLog.txt"
$debugMode = $true

function Write-DetailedLog {
  param(
    [string]$Message,
    [string]$Level = "INFO",
    [System.Exception]$Exception = $null
  )
    
  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
  $logEntry = "[$timestamp] [$Level] $Message"
    
  if ($Exception) {
    $logEntry += "`r`n    Exception: $($Exception.Message)"
    $logEntry += "`r`n    Type: $($Exception.GetType().FullName)"
    if ($Exception.InnerException) {
      $logEntry += "`r`n    Inner: $($Exception.InnerException.Message)"
    }
  }
    
  # Write to file
  try {
    Add-Content -Path $logPath -Value $logEntry -Encoding UTF8 -ErrorAction Stop
  }
  catch {
    Write-Host "Failed to write to log: $_" -ForegroundColor Red
  }
    
  # Console output
  $color = switch ($Level) {
    "ERROR" { "Red" }
    "WARNING" { "Yellow" }
    "SUCCESS" { "Green" }
    "DEBUG" { "Magenta" }
    default { "Cyan" }
  }
  Write-Host $logEntry -ForegroundColor $color
    
  if ($Exception -and $debugMode) {
    Write-Host "Stack Trace:" -ForegroundColor Gray
    Write-Host $Exception.StackTrace -ForegroundColor Gray
  }
}

# Clear previous log and start fresh
if (Test-Path $logPath) { Remove-Item $logPath -Force }
Write-DetailedLog "=== RAWR BROWSER DEBUG SESSION START ===" "INFO"
Write-DetailedLog "Log file: $logPath" "INFO"
Write-DetailedLog "PowerShell: $($PSVersionTable.PSVersion)" "INFO"
Write-DetailedLog "CLR Version: $([System.Environment]::Version)" "INFO"

# Check system WebView2 installation
Write-DetailedLog "Checking system WebView2 installation..." "INFO"
try {
  $regPaths = @(
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
    "HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
  )
    
  $webView2Found = $false
  foreach ($regPath in $regPaths) {
    if (Test-Path $regPath) {
      $version = (Get-ItemProperty $regPath -ErrorAction SilentlyContinue).pv
      Write-DetailedLog "Found WebView2 Runtime: $version at $regPath" "SUCCESS"
      $webView2Found = $true
      break
    }
  }
    
  if (-not $webView2Found) {
    Write-DetailedLog "No WebView2 Runtime found in registry" "WARNING"
        
    # Check Edge installation
    $edgePath = "C:\Program Files (x86)\Microsoft\Edge\Application"
    if (Test-Path $edgePath) {
      $edgeVersions = Get-ChildItem $edgePath | Where-Object { $_.Name -match "^\d+\." }
      if ($edgeVersions) {
        Write-DetailedLog "Microsoft Edge found: $($edgeVersions[0].Name)" "INFO"
      }
    }
  }
}
catch {
  Write-DetailedLog "Failed to check WebView2 installation" "ERROR" $_
}

# WebView2 SDK setup
$wvDir = "$env:TEMP\WVLibs"
$useWebView2 = $false

Write-DetailedLog "WebView2 SDK directory: $wvDir" "DEBUG"

if (!(Test-Path $wvDir)) {
  Write-DetailedLog "Downloading WebView2 SDK..." "INFO"
  try {
    New-Item -ItemType Directory -Path $wvDir -Force | Out-Null
        
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Write-DetailedLog "Downloading from NuGet..." "DEBUG"
        
    $downloadUrl = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2"
    Invoke-WebRequest $downloadUrl -OutFile "$wvDir\wv.zip" -TimeoutSec 30
    Write-DetailedLog "Download completed: $((Get-Item "$wvDir\wv.zip").Length) bytes" "DEBUG"
        
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory("$wvDir\wv.zip", "$wvDir\ex")
    Write-DetailedLog "Package extracted successfully" "DEBUG"
        
    $net462Path = "$wvDir\ex\lib\net462"
    if (Test-Path $net462Path) {
      Copy-Item "$net462Path\Microsoft.Web.WebView2.Core.dll" -Destination $wvDir -Force
      Copy-Item "$net462Path\Microsoft.Web.WebView2.WinForms.dll" -Destination $wvDir -Force
            
      $loaderPath = "$wvDir\ex\runtimes\win-x64\native\WebView2Loader.dll"
      if (Test-Path $loaderPath) {
        Copy-Item $loaderPath -Destination $wvDir -Force
        Write-DetailedLog "Native WebView2Loader.dll copied" "DEBUG"
      }
            
      Write-DetailedLog "WebView2 SDK installed successfully" "SUCCESS"
    }
  }
  catch {
    Write-DetailedLog "WebView2 SDK download failed" "ERROR" $_
  }
}

# Load WebView2 assemblies with detailed error checking
Write-DetailedLog "Loading WebView2 assemblies..." "INFO"
if (Test-Path "$wvDir\Microsoft.Web.WebView2.Core.dll") {
  try {
    Write-DetailedLog "Core.dll size: $((Get-Item "$wvDir\Microsoft.Web.WebView2.Core.dll").Length) bytes" "DEBUG"
    Write-DetailedLog "WinForms.dll size: $((Get-Item "$wvDir\Microsoft.Web.WebView2.WinForms.dll").Length) bytes" "DEBUG"
        
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.Core.dll" -ErrorAction Stop
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
        
    $useWebView2 = $true
    Write-DetailedLog "WebView2 assemblies loaded successfully" "SUCCESS"
  }
  catch {
    Write-DetailedLog "Failed to load WebView2 assemblies" "ERROR" $_
    $useWebView2 = $false
  }
}
else {
  Write-DetailedLog "WebView2.Core.dll not found at expected path" "WARNING"
}

# Create main form
Write-DetailedLog "Creating main form..." "DEBUG"
$form = New-Object System.Windows.Forms.Form
$form.Width = 1400
$form.Height = 900
$form.Text = "🦖 Rawr Browser - Debug Mode"
$form.StartPosition = "CenterScreen"

# Status bar with detailed info
$statusBar = New-Object System.Windows.Forms.StatusStrip
$statusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusLabel.Text = "Initializing..."
$statusLabel.Spring = $true
$statusBar.Items.Add($statusLabel)

$logButton = New-Object System.Windows.Forms.ToolStripButton
$logButton.Text = "View Log"
$logButton.add_Click({
    try {
      Start-Process notepad.exe -ArgumentList $logPath
    }
    catch {
      Write-DetailedLog "Failed to open log file" "ERROR" $_
    }
  })
$statusBar.Items.Add($logButton)

$form.Controls.Add($statusBar)

if ($useWebView2) {
  Write-DetailedLog "Attempting WebView2 initialization..." "INFO"
  try {
    # Create WebView2 control
    Write-DetailedLog "Creating WebView2 control..." "DEBUG"
    $browser = New-Object Microsoft.Web.WebView2.WinForms.WebView2 -ErrorAction Stop
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
        
    # Set user data folder
    $userDataFolder = "$env:TEMP\RawrBrowserUserData"
    if (!(Test-Path $userDataFolder)) {
      New-Item -ItemType Directory -Path $userDataFolder -Force | Out-Null
      Write-DetailedLog "Created user data folder: $userDataFolder" "DEBUG"
    }
        
    # Add to form
    $form.Controls.Add($browser)
    Write-DetailedLog "WebView2 control added to form" "DEBUG"
        
    $statusLabel.Text = "WebView2 control created, initializing..."
        
    # Track initialization state
    $script:initStarted = $false
    $script:initCompleted = $false
    $script:navigationCompleted = $false
        
    # Core initialization event with comprehensive error handling
    $browser.add_CoreWebView2InitializationCompleted({
        param($browserObj, $initEvent)
        try {
          Write-DetailedLog "CoreWebView2InitializationCompleted fired" "DEBUG"
          Write-DetailedLog "IsSuccess: $($initEvent.IsSuccess)" "DEBUG"
                
          if ($initEvent.IsSuccess) {
            Write-DetailedLog "WebView2 core initialized successfully" "SUCCESS"
            $script:initCompleted = $true
            $statusLabel.Text = "Loading interface..."
                    
            # Enable dev tools
            $browser.CoreWebView2.Settings.AreDevToolsEnabled = $true
            $browser.CoreWebView2.Settings.AreDefaultContextMenusEnabled = $true
            Write-DetailedLog "Dev tools enabled" "DEBUG"            # Load HTML content
            $htmlContent = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Browser Debug</title>
    <style>
        body { 
            font-family: 'Consolas', monospace; 
            background: linear-gradient(135deg, #000428, #004e92);
            color: #00ff00; 
            margin: 20px; 
            line-height: 1.6; 
        }
        .header { 
            text-align: center; 
            background: rgba(0,0,0,0.8); 
            padding: 20px; 
            border-radius: 10px; 
            margin-bottom: 20px;
        }
        .debug-info {
            background: rgba(255,255,255,0.1);
            border: 1px solid #00ff00;
            border-radius: 5px;
            padding: 15px;
            margin: 10px 0;
            white-space: pre-wrap;
        }
        button {
            background: #004400;
            color: #00ff00;
            border: 2px solid #00ff00;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
            font-family: 'Consolas', monospace;
        }
        button:hover {
            background: #006600;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🦖 RAWR BROWSER - DEBUG MODE</h1>
        <h2>WebView2 Successfully Initialized!</h2>
    </div>
    
    <div class="debug-info" id="systemInfo">
        Loading system information...
    </div>
    
    <div>
        <button onclick="openDevTools()">🔧 Open Dev Tools</button>
        <button onclick="testFeatures()">🧪 Test Features</button>
        <button onclick="showFileDialog()">📁 File Dialog Test</button>
    </div>
    
    <div class="debug-info" id="testOutput">
        Click buttons above to test functionality...
    </div>

    <script>
        function log(msg) {
            const output = document.getElementById('testOutput');
            const timestamp = new Date().toLocaleTimeString();
            output.innerHTML += '[' + timestamp + '] ' + msg + '\n';
        }
        
        function openDevTools() {
            log('Opening developer tools...');
            // Dev tools should be accessible via F12 or right-click -> Inspect
        }
        
        function testFeatures() {
            log('Testing WebView2 features...');
            log('User Agent: ' + navigator.userAgent);
            log('Window size: ' + window.innerWidth + 'x' + window.innerHeight);
            log('Document ready state: ' + document.readyState);
            log('Local storage available: ' + (typeof(Storage) !== 'undefined'));
        }
        
        function showFileDialog() {
            const input = document.createElement('input');
            input.type = 'file';
            input.multiple = true;
            input.onchange = function(e) {
                log('Selected ' + e.target.files.length + ' file(s):');
                for (let i = 0; i < e.target.files.length; i++) {
                    const f = e.target.files[i];
                    log('  - ' + f.name + ' (' + f.size + ' bytes)');
                }
            };
            input.click();
        }
        
        // System info
        window.addEventListener('load', function() {
            const info = document.getElementById('systemInfo');
            info.innerHTML = 
                'Browser: ' + navigator.userAgent + '\n' +
                'Platform: ' + navigator.platform + '\n' +
                'Language: ' + navigator.language + '\n' +
                'Cookies enabled: ' + navigator.cookieEnabled + '\n' +
                'Online: ' + navigator.onLine + '\n' +
                'Screen: ' + screen.width + 'x' + screen.height + '\n' +
                'Available: ' + screen.availWidth + 'x' + screen.availHeight;
            
            log('WebView2 interface loaded successfully!');
            log('Right-click anywhere for context menu');
            log('Press F12 to open developer tools');
        });
        
        // Error handling
        window.onerror = function(msg, url, line) {
            log('JavaScript Error: ' + msg + ' at line ' + line);
        };
    </script>
</body>
</html>
"@
                    
            $browser.CoreWebView2.NavigateToString($htmlContent)
            Write-DetailedLog "HTML content loaded into WebView2" "DEBUG"
                    
          }
        }
        else {
          # Get detailed error information
          $errorInfo = "Initialization failed"
          $errorDetails = ""
            
          if ($initEvent.Exception) {
            $errorInfo = $initEvent.Exception.Message
            $errorDetails = $initEvent.Exception.ToString()
            Write-DetailedLog "Exception details: $errorDetails" "DEBUG"
          }
            
          if ($initEvent.InitializationException) {
            $errorInfo = $initEvent.InitializationException.Message
            $errorDetails = $initEvent.InitializationException.ToString()
            Write-DetailedLog "Initialization exception details: $errorDetails" "DEBUG"
          }
            
          # Check for common WebView2 issues
          if ($errorInfo -like "*access*denied*" -or $errorInfo -like "*permission*") {
            $errorInfo += " (Try running as administrator)"
          }
          elseif ($errorInfo -like "*file*not*found*" -or $errorInfo -like "*path*") {
            $errorInfo += " (WebView2 runtime path issue)"
          }
          elseif ($errorInfo -like "*version*" -or $errorInfo -like "*compatibility*") {
            $errorInfo += " (Runtime version mismatch)"
          }
            
          Write-DetailedLog "WebView2 initialization failed: $errorInfo" "ERROR"
          $statusLabel.Text = "WebView2 init failed: $errorInfo"
        }
      } catch {
        Write-DetailedLog "Exception in initialization handler: $($_.Exception.Message)" "ERROR"
        $statusLabel.Text = "Exception in init handler: $($_.Exception.Message)"
      }
    })  # Navigation events
  $browser.add_NavigationCompleted({
      param($sender, $e)
      try {
        Write-DetailedLog "Navigation completed - Success: $($e.IsSuccess)" "DEBUG"
        $script:navigationCompleted = $true
        if ($e.IsSuccess) {
          $statusLabel.Text = "🦖 Rawr Browser Ready - Debug Mode Active"
        }
        else {
          Write-DetailedLog "Navigation failed" "ERROR"
          $statusLabel.Text = "Navigation failed"
        }
      }
      catch {
        Write-DetailedLog "Exception in navigation handler" "ERROR" $_
      }
    })
        
  # Start initialization with timeout
  Write-DetailedLog "Starting WebView2 core initialization..." "INFO"
  $script:initStarted = $true
        
  # Use the correct overload - no parameters for default initialization
  try {
    $initTask = $browser.EnsureCoreWebView2Async()
    Write-DetailedLog "EnsureCoreWebView2Async() called successfully" "DEBUG"
  }
  catch {
    Write-DetailedLog "EnsureCoreWebView2Async() failed: $($_.Exception.Message)" "ERROR"
    $statusLabel.Text = "Init call failed: $($_.Exception.Message)"
  }    # Timeout checker
  $timer = New-Object System.Windows.Forms.Timer
  $timer.Interval = 10000  # 10 second timeout
  $timer.add_Tick({
      if (-not $script:initCompleted) {
        Write-DetailedLog "WebView2 initialization timeout after 10 seconds" "ERROR"
        $statusLabel.Text = "Initialization timeout - check log for details"
        $timer.Stop()
      }
      else {
        $timer.Stop()
      }
    })
  $timer.Start()
        
  Write-DetailedLog "WebView2 setup completed, showing form..." "INFO"
        
}
catch {
  Write-DetailedLog "WebView2 setup failed: $($_.Exception.Message)" "ERROR"
  $statusLabel.Text = "WebView2 setup failed - see log"
  $useWebView2 = $false
}
}

# Fallback if WebView2 fails
if (-not $useWebView2) {
  Write-DetailedLog "Creating fallback browser..." "INFO"
    
  try {
    $webBrowser = New-Object System.Windows.Forms.WebBrowser
    $webBrowser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($webBrowser)
        
    $statusLabel.Text = "Using WebBrowser fallback"
        
    $webBrowser.DocumentText = @"
<!DOCTYPE html>
<html>
<head><title>Fallback Mode</title></head>
<body style='font-family:Consolas; background:#111; color:#0f0; padding:20px;'>
    <h1>🦖 Rawr Browser - Fallback Mode</h1>
    <p>WebView2 is not available. Using IE WebBrowser control.</p>
    <p>Check the log file for details: <code>$logPath</code></p>
    <button onclick="alert('Basic functionality only')">Test Alert</button>
</body>
</html>
"@
        
    Write-DetailedLog "Fallback browser created" "SUCCESS"
        
  }
  catch {
    Write-DetailedLog "Failed to create fallback browser" "ERROR" $_
        
    # Last resort - just show error info
    $textBox = New-Object System.Windows.Forms.TextBox
    $textBox.Multiline = $true
    $textBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $textBox.Text = "Rawr Browser failed to initialize.`r`n`r`nCheck log file: $logPath`r`n`r`nPress Ctrl+A to select all text and copy for debugging."
    $form.Controls.Add($textBox)
        
    $statusLabel.Text = "All browser controls failed - text mode"
  }
}

# Form events
$form.add_FormClosing({
    Write-DetailedLog "Form closing - cleaning up..." "INFO"
    Write-DetailedLog "=== RAWR BROWSER DEBUG SESSION END ===" "INFO"
  })

# Show the form
Write-DetailedLog "Displaying main form..." "INFO"
$form.ShowDialog() | Out-Null

Write-DetailedLog "Application ended normally" "INFO"
Write-Host "`nDetailed log saved to: $logPath" -ForegroundColor Green
Write-Host "Open the log file to see full debug information." -ForegroundColor Yellow