Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Create log file path
$logPath = Join-Path $env:TEMP "RawrBrowser_Startup.log"
$logDir = Split-Path $logPath
if (-not (Test-Path $logDir)) {
  New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}

# Startup logger function
function Write-StartupLog {
  param(
    [string]$Message,
    [string]$Level = "INFO"
  )
    
  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
  $logEntry = "[$timestamp] [$Level] $Message"
    
  # Write to log file
  Add-Content -Path $logPath -Value $logEntry -Encoding UTF8
    
  # Also output to console with colors for immediate feedback
  $color = switch ($Level) {
    "ERROR" { "Red" }
    "WARNING" { "Yellow" }
    "SUCCESS" { "Green" }
    "INFO" { "Cyan" }
    default { "White" }
  }
  Write-Host $logEntry -ForegroundColor $color
}

# Initialize log file
$startupTime = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
Write-StartupLog "═══════════════════════════════════════════════" "INFO"
Write-StartupLog "RawrBrowser Startup Log - Session Started: $startupTime" "INFO"
Write-StartupLog "═══════════════════════════════════════════════" "INFO"
Write-StartupLog "PowerShell Version: $($PSVersionTable.PSVersion)" "INFO"
Write-StartupLog "Operating System: $([System.Environment]::OSVersion)" "INFO"
Write-StartupLog "Log Path: $logPath" "INFO"

# WebView2 Setup - Only load managed assemblies, not native DLLs
$wvDir = "$env:TEMP\WVLibs"
$useWebView2 = $false

Write-StartupLog "Checking WebView2 components..." "INFO"

if (!(Test-Path "$wvDir")) {
  Write-StartupLog "WebView2 components not found, downloading..." "WARNING"
  try {
    New-Item -ItemType Directory -Path "$wvDir" -Force | Out-Null
    Invoke-WebRequest "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" -OutFile "$wvDir\wv.zip"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory("$wvDir\wv.zip", "$wvDir\ex")
        
    $net462Path = "$wvDir\ex\lib\net462"
    if (Test-Path $net462Path) {
      Copy-Item "$net462Path\Microsoft.Web.WebView2.Core.dll" -Destination $wvDir -Force -ErrorAction SilentlyContinue
      Copy-Item "$net462Path\Microsoft.Web.WebView2.WinForms.dll" -Destination $wvDir -Force -ErrorAction SilentlyContinue
      
      # Copy the native WebView2Loader.dll for x64
      $loaderPath = "$wvDir\ex\runtimes\win-x64\native\WebView2Loader.dll"
      if (Test-Path $loaderPath) {
        Copy-Item $loaderPath -Destination $wvDir -Force -ErrorAction SilentlyContinue
      }
      
      Write-StartupLog "WebView2 components downloaded successfully" "SUCCESS"
    }
  }
  catch {
    Write-StartupLog "WebView2 download failed: $_" "ERROR"
    Write-StartupLog "Will use fallback WebBrowser control" "WARNING"
  }
}

# Try to load WebView2 assemblies
if (Test-Path "$wvDir\Microsoft.Web.WebView2.Core.dll") {
  try {
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.Core.dll" -ErrorAction Stop
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
    $useWebView2 = $true
    Write-StartupLog "WebView2 assemblies loaded successfully" "SUCCESS"
  }
  catch {
    Write-StartupLog "WebView2 assembly load failed: $($_.Exception.Message)" "ERROR"
    Write-StartupLog "Will use fallback WebBrowser control" "WARNING"
    $useWebView2 = $false
  }
}

# Create the main form
$form = New-Object System.Windows.Forms.Form
$form.Width = 1400
$form.Height = 900
$form.Text = "Rawr Browser – Byte Engine"
$form.StartPosition = "CenterScreen"

# Add status label for better feedback
$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Initializing..."
$statusLabel.Dock = "Bottom"
$statusLabel.Height = 25
$statusLabel.BackColor = [System.Drawing.Color]::DarkGray
$statusLabel.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($statusLabel)

if ($useWebView2) {
  Write-StartupLog "Creating WebView2 browser control..." "INFO"
  try {
    $browser = New-Object Microsoft.Web.WebView2.WinForms.WebView2 -ErrorAction Stop
    Write-StartupLog "WebView2 control created successfully" "SUCCESS"
    
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($browser)
    $statusLabel.Text = "WebView2 control created, initializing runtime..."

    Write-StartupLog "Initializing WebView2 runtime..." "INFO"
    
    # Use event-based initialization instead of polling
    $browser.add_NavigationCompleted({
        param($browserSender, $navEventArgs)
        Write-StartupLog "WebView2 navigation completed successfully" "SUCCESS"
        $statusLabel.Text = "Ready - Rawr Byte Engine"
      })

    # Handle core initialization properly
    $browser.add_CoreWebView2InitializationCompleted({
        param($browserSender, $initEventArgs)
        if ($initEventArgs.IsSuccess) {
          Write-StartupLog "WebView2 core initialization completed successfully" "SUCCESS"
          $statusLabel.Text = "Loading Rawr interface..."
            
          # Now set up the page content with improved JavaScript
          $browser.CoreWebView2.NavigateToString(@"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Byte Engine</title>
    <style>
        body { 
            font-family: 'Consolas', monospace; 
            background: #111; 
            color: #0f0; 
            margin: 20px;
            line-height: 1.6;
        }
        h2 { color: #ff0; }
        input[type='file'] { 
            background: #333; 
            color: #0f0; 
            border: 1px solid #0f0; 
            padding: 10px; 
            margin: 10px 0;
        }
        #log { 
            background: #000; 
            border: 1px solid #0f0; 
            padding: 15px; 
            height: 300px; 
            overflow-y: auto;
            white-space: pre-wrap;
        }
        .status { color: #ff0; }
        .success { color: #0f0; }
        .error { color: #f00; }
    </style>
</head>
<body>
    <h2>🦖 Rawr Byte Engine v2.0</h2>
    <p class='status'>Advanced file compression and analysis tool</p>
    
    <input type='file' id='fileInput' multiple accept='*/*' />
    <button onclick='clearLog()' style='margin-left: 10px; background: #333; color: #0f0; border: 1px solid #0f0; padding: 5px 10px;'>Clear Log</button>
    
    <pre id='log'>Ready for file processing...</pre>

    <script>
        const log = (msg, type = 'normal') => {
            const logEl = document.getElementById('log');
            const timestamp = new Date().toLocaleTimeString();
            const className = type === 'error' ? 'error' : type === 'success' ? 'success' : '';
            logEl.innerHTML += '<span class="' + className + '">[' + timestamp + '] ' + msg + '</span>\n';
            logEl.scrollTop = logEl.scrollHeight;
        };

        const clearLog = () => {
            document.getElementById('log').innerHTML = 'Log cleared...\n';
        };

        // Simplified file processing without WebAssembly dependencies
        function processFile(file) {
            log('Processing: ' + file.name + ' (' + file.size + ' bytes)');
            
            const reader = new FileReader();
            reader.onload = function(e) {
                const arrayBuffer = e.target.result;
                const uint8Array = new Uint8Array(arrayBuffer);
                
                // Basic analysis
                let zeros = 0, ones = 0, entropy = 0;
                const freq = new Array(256).fill(0);
                
                for (let i = 0; i < uint8Array.length; i++) {
                    const byte = uint8Array[i];
                    freq[byte]++;
                    if (byte === 0) zeros++;
                    if (byte === 255) ones++;
                }
                
                // Calculate basic entropy
                for (let i = 0; i < 256; i++) {
                    if (freq[i] > 0) {
                        const p = freq[i] / uint8Array.length;
                        entropy -= p * Math.log2(p);
                    }
                }
                
                log('Analysis complete:', 'success');
                log('  - Null bytes: ' + zeros + ' (' + (zeros/uint8Array.length*100).toFixed(1) + '%)');
                log('  - FF bytes: ' + ones + ' (' + (ones/uint8Array.length*100).toFixed(1) + '%)');
                log('  - Entropy: ' + entropy.toFixed(2) + ' bits/byte');
                log('  - Estimated compression: ' + Math.max(0, 100 - entropy/8*100).toFixed(1) + '%');
                log('Raw analysis complete for ' + file.name, 'success');
            };
            
            reader.onerror = () => log('Error reading file: ' + file.name, 'error');
            reader.readAsArrayBuffer(file);
        }

        // File input handler
        document.getElementById('fileInput').onchange = function(e) {
            const files = e.target.files;
            if (files.length === 0) return;
            
            log('=== Starting batch processing ===', 'success');
            log('Selected ' + files.length + ' file(s)');
            
            for (let i = 0; i < files.length; i++) {
                setTimeout(() => processFile(files[i]), i * 100); // Stagger processing
            }
        };

        // Initial ready message
        setTimeout(() => {
            log('🦖 Rawr Engine ready for file analysis!', 'success');
            log('Select files to analyze their compression potential...');
        }, 500);
    </script>
</body>
</html>
"@)
        }
        else {
          Write-StartupLog "WebView2 initialization failed: $($initEventArgs.Exception.Message)" "ERROR"
          $statusLabel.Text = "WebView2 initialization failed - using fallback"
          # Don't set useWebView2 to false here as we're already in the WebView2 path
        }
      })
    
    # Start the initialization
    $browser.EnsureCoreWebView2Async($null) | Out-Null
    
    # Show the form
    $statusLabel.Text = "Starting WebView2..."
    $form.ShowDialog() | Out-Null
    
    $form.ShowDialog() | Out-Null
  }
  catch {
    Write-StartupLog "WebView2 creation failed: $($_.Exception.Message)" "ERROR"
    Write-StartupLog "Falling back to IE WebBrowser control" "WARNING"
    
    # Create fallback WebBrowser control
    $browser = New-Object System.Windows.Forms.WebBrowser
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($browser)
    
    $statusLabel.Text = "Using IE WebBrowser fallback"
    
    # Simple HTML for fallback
    $browser.DocumentText = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Browser - Fallback Mode</title>
</head>
<body style='font-family:consolas; background:#111; color:#0f0; margin:20px;'>
    <h2>🦖 Rawr Browser - Fallback Mode</h2>
    <p style='color:#ff0;'>WebView2 unavailable - using Internet Explorer engine</p>
    <p>Limited functionality in this mode.</p>
    <input type='file' multiple />
    <pre id='log' style='background:#000; border:1px solid #0f0; padding:15px; height:200px; overflow:auto;'>
Fallback mode active...
WebView2 features disabled.
    </pre>
</body>
</html>
"@
    
    $form.ShowDialog() | Out-Null
  }
}

if (!$useWebView2) {
  Write-StartupLog "Using fallback IE WebBrowser control (WebView2 not available)" "INFO"
  
  # Create fallback WebBrowser control
  $browser = New-Object System.Windows.Forms.WebBrowser
  $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
  $form.Controls.Add($browser)
  
  $statusLabel.Text = "Using IE WebBrowser fallback"
  
  # Simple HTML for fallback
  $browser.DocumentText = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Browser - Fallback Mode</title>
</head>
<body style='font-family:consolas; background:#111; color:#0f0; margin:20px;'>
    <h2>🦖 Rawr Browser - Fallback Mode</h2>
    <p style='color:#ff0;'>WebView2 unavailable - using Internet Explorer engine</p>
    <p>Limited functionality in this mode.</p>
    <input type='file' multiple />
    <pre id='log' style='background:#000; border:1px solid #0f0; padding:15px; height:200px; overflow:auto;'>
Fallback mode active...
WebView2 features disabled.
    </pre>
</body>
</html>
"@
  
  Write-StartupLog "Application startup completed - Showing main window" "SUCCESS"
  $form.ShowDialog() | Out-Null
}

# Log session end when application closes
Write-StartupLog "Application session ended" "INFO"
Write-StartupLog "═══════════════════════════════════════════════" "INFO"