# RawrXD Agentic Browser & Video Streaming System

## Overview

The RawrXD Agentic Browser & Video Streaming System is a pure PowerShell implementation that provides autonomous web browsing and video streaming capabilities. Built as modular extensions to the RawrXD ecosystem, this system enables AI-powered automation, video playback, and intelligent web interaction without external dependencies.

## Architecture

### Core Components

```
RawrXD Ecosystem
├── RawrXD.Browser.psm1      # Agentic browser engine
├── RawrXD.Video.psm1        # Video streaming & playback
├── RawrXD.Logging.psm1      # Integrated logging
└── Browser-Video-Integration.ps1  # Demo & integration script
```

### Key Features

#### 🤖 Agentic Browser Engine (`RawrXD.Browser.psm1`)

- **Autonomous Navigation**: Intelligent web browsing with task queuing
- **Element Interaction**: Click, fill forms, execute JavaScript
- **Video Control**: Native video playback control in web pages
- **Screenshot Capture**: Automated screenshot functionality
- **Task Automation**: Create and execute sequences of browser actions

**Supported Browsers:**
- ✅ WebView2 (preferred - modern Chromium engine)
- ✅ Internet Explorer (fallback for older systems)

#### 🎬 Video Streaming Engine (`RawrXD.Video.psm1`)

- **Multi-Format Support**: MP4, WebM, AVI, MKV, MOV
- **Streaming Protocols**: YouTube, Vimeo, HLS, DASH
- **Playback Control**: Play, pause, stop, seek, volume
- **Metadata Extraction**: Video duration, format detection
- **Playlist Management**: Queue and manage video collections

**Player Technologies:**
- ✅ WPF MediaElement (preferred - native .NET)
- ✅ HTML5 Video (fallback via WebBrowser control)

## Quick Start

### Installation

1. **Ensure PowerShell 7+** with GUI support
2. **Copy modules** to your project:
   ```powershell
   # From RawrXD project directory
   Copy-Item "Modules/Modules/RawrXD.Browser.psm1" -Destination "YourProject/Modules/"
   Copy-Item "Modules/Modules/RawrXD.Video.psm1" -Destination "YourProject/Modules/"
   ```

3. **Import modules**:
   ```powershell
   Import-Module ".\Modules\RawrXD.Browser.psm1" -Force
   Import-Module ".\Modules\RawrXD.Video.psm1" -Force
   ```

### Basic Usage

#### Browser Agent
```powershell
# Create browser instance
$form = New-Object System.Windows.Forms.Form
$panel = New-Object System.Windows.Forms.Panel
$form.Controls.Add($panel)

$browser = New-BrowserAgent -Id "my-browser" -ParentControl $panel

# Navigate and interact
Invoke-BrowserNavigation -AgentId "my-browser" -Url "https://youtube.com"
Invoke-BrowserElementInteraction -AgentId "my-browser" -Action "Click" -Selector "#play-button"
Invoke-BrowserVideoControl -AgentId "my-browser" -Action "Play"
```

#### Video Streaming
```powershell
# Create video player
$form = New-Object System.Windows.Forms.Form
$panel = New-Object System.Windows.Forms.Panel
$form.Controls.Add($panel)

$video = New-VideoStream -Id "my-video" -Url "https://example.com/video.mp4" -ParentControl $panel

# Control playback
Invoke-VideoControl -StreamId "my-video" -Action "Play"
Set-VideoVolume -StreamId "my-video" -Volume 0.7
Set-VideoPosition -StreamId "my-video" -Seconds 30
```

#### Agent Automation
```powershell
# Create automated tasks
New-AgentTask -Id "search-youtube" -Type "Navigate" -Parameters @{ Url = "https://youtube.com" }
New-AgentTask -Id "fill-search" -Type "FillForm" -Parameters @{ Selector = "#search-input"; Value = "PowerShell" }
New-AgentTask -Id "click-search" -Type "Click" -Parameters @{ Selector = "#search-button" }
New-AgentTask -Id "play-video" -Type "PlayVideo" -Parameters @{}

# Execute automation sequence
Start-AgentAutomation -AgentId "my-browser" -TaskSequence @("search-youtube", "fill-search", "click-search", "play-video") -Interval 2
```

## Advanced Features

### Autonomous Task Execution

The system supports complex automation sequences with safety controls:

```powershell
# Define automation with safety constraints
$automationConfig = @{
    AgentId = "research-browser"
    TaskSequence = @(
        @{ Id = "nav-google"; Type = "Navigate"; Url = "https://google.com" }
        @{ Id = "search-topic"; Type = "FillForm"; Selector = "#search-input"; Value = "PowerShell automation" }
        @{ Id = "click-search"; Type = "Click"; Selector = "#search-button" }
        @{ Id = "extract-results"; Type = "CustomScript"; Script = "extractSearchResults()" }
    )
    SafetyLimits = @{
        MaxExecutionTime = 300  # 5 minutes
        MaxActions = 50
        AllowedDomains = @("google.com", "youtube.com")
    }
    ErrorHandling = @{
        RetryCount = 3
        FallbackActions = @("take-screenshot", "log-error")
    }
}

Start-AgentAutomation @automationConfig
```

### Video Playlist Management

```powershell
# Create and manage video playlists
$playlist = New-VideoPlaylist -Id "learning-playlist"

# Add videos from various sources
Add-VideoToPlaylist -PlaylistId "learning-playlist" -StreamId "powershell-intro"
Add-VideoToPlaylist -PlaylistId "learning-playlist" -StreamId "advanced-automation"

# Playback controls
$playlist.PlayNext()
$playlist.PlayPrevious()
$playlist.IsLooping = $true
$playlist.IsShuffling = $true
```

### Cross-Component Integration

The browser and video modules can work together seamlessly:

```powershell
# Integrated browser + video experience
$form = New-Object System.Windows.Forms.Form
$splitContainer = New-Object System.Windows.Forms.SplitContainer
$form.Controls.Add($splitContainer)

# Browser in top panel
$browser = New-BrowserAgent -Id "web-browser" -ParentControl $splitContainer.Panel1

# Video in bottom panel
$video = New-VideoStream -Id "media-player" -Url "video.mp4" -ParentControl $splitContainer.Panel2

# Synchronized control
Invoke-BrowserNavigation -AgentId "web-browser" -Url "https://youtube.com"
Invoke-VideoControl -StreamId "media-player" -Action "Play"
```

## API Reference

### Browser Module Functions

#### Agent Management
- `New-BrowserAgent -Id <string> -ParentControl <object>`
- `Get-BrowserAgent -Id <string>`
- `Get-BrowserInfo`

#### Navigation & Interaction
- `Invoke-BrowserNavigation -AgentId <string> -Url <string>`
- `Invoke-BrowserElementInteraction -AgentId <string> -Action <Click|Fill> -Selector <string> [-Value <string>]`
- `Invoke-BrowserScreenshot -AgentId <string> -OutputPath <string>`

#### Video Control (Web Pages)
- `Invoke-BrowserVideoControl -AgentId <string> -Action <Play|Pause|Stop>`

#### Automation
- `New-AgentTask -Id <string> -Type <string> -Parameters <hashtable>`
- `Invoke-AgentTask -TaskId <string> -AgentId <string>`
- `Start-AgentAutomation -AgentId <string> -TaskSequence <string[]> [-Interval <int>]`

### Video Module Functions

#### Stream Management
- `New-VideoStream -Id <string> -Url <string> -ParentControl <object>`
- `Get-VideoStream -Id <string>`
- `Get-VideoStreams`
- `Get-VideoCapabilities`

#### Playback Control
- `Invoke-VideoControl -StreamId <string> -Action <Play|Pause|Stop>`
- `Set-VideoPosition -StreamId <string> -Seconds <double>`
- `Set-VideoVolume -StreamId <string> -Volume <double>`

#### Metadata & Info
- `Get-VideoMetadata -StreamId <string>`
- `Stop-AllVideos`

#### Playlist Management
- `New-VideoPlaylist -Id <string>`
- `Add-VideoToPlaylist -PlaylistId <string> -StreamId <string>`

## Integration with RawrXD

### Module Loading
```powershell
# In RawrXD main script, add to module imports:
$modules = @(
    'RawrXD.Agent',
    'RawrXD.AI',
    'RawrXD.Core',
    'RawrXD.Logging',
    'RawrXD.Browser',      # NEW
    'RawrXD.Video',        # NEW
    'RawrXD.Utilities'
)
```

### UI Integration
```powershell
# Add browser panel to RawrXD interface
function Initialize-BrowserPanel {
    $browserPanel = New-Object System.Windows.Forms.Panel
    $browserPanel.Dock = [System.Windows.Forms.DockStyle]::Fill

    # Create browser agent
    $browser = New-BrowserAgent -Id "rawrxd-browser" -ParentControl $browserPanel

    # Add to main form
    $mainForm.Controls.Add($browserPanel)
}
```

### Command Integration
```powershell
# Add browser commands to CLI
"browser-navigate" {
    param($url)
    Invoke-BrowserNavigation -AgentId "main-browser" -Url $url
    "Browser navigated to: $url" | Write-Host -ForegroundColor Green
}
"browser-video-play" {
    Invoke-BrowserVideoControl -AgentId "main-browser" -Action "Play"
    "Video playback started" | Write-Host -ForegroundColor Green
}
```

## Demo Scripts

### Interactive Demo
```powershell
# Run full interactive demo
.\Browser-Video-Integration.ps1
```

### Automated Demo
```powershell
# Run demos automatically
.\Browser-Video-Integration.ps1 -DemoMode
```

### Specific Component Demo
```powershell
# Browser-only demo
.\Browser-Video-Integration.ps1 -DemoMode
# Select option 3 from menu
```

## Security Considerations

### Safe Automation
- **Domain Whitelisting**: Restrict navigation to approved domains
- **Action Limits**: Prevent infinite loops with execution timeouts
- **Input Validation**: Sanitize all user inputs and form data
- **Audit Logging**: Track all automation actions for review

### Video Security
- **URL Validation**: Only allow trusted video sources
- **Content Filtering**: Block malicious or inappropriate content
- **Bandwidth Limits**: Prevent excessive data usage
- **Access Controls**: Restrict video access based on user permissions

## Troubleshooting

### Common Issues

**WebView2 Not Available:**
```
Error: WebView2 runtime not found
Solution: Install WebView2 runtime from Microsoft
```

**Video Won't Play:**
```
Check: Is WPF MediaElement available?
Check: Does the video URL support HTML5 playback?
Check: Are necessary codecs installed?
```

**Browser Navigation Fails:**
```
Check: Is the URL accessible?
Check: Are there network connectivity issues?
Check: Is the domain blocked by security policies?
```

### Debug Mode
```powershell
# Enable verbose logging
$VerbosePreference = "Continue"

# Check module status
Get-BrowserInfo
Get-VideoCapabilities

# Test basic functionality
$browser = New-BrowserAgent -Id "debug" -ParentControl $panel -Verbose
$video = New-VideoStream -Id "debug-video" -Url "test.mp4" -ParentControl $panel -Verbose
```

## Performance Optimization

### Browser Optimization
- **WebView2 Preferred**: Faster, more secure than WebBrowser
- **Lazy Loading**: Only load pages when needed
- **Resource Caching**: Cache frequently accessed resources
- **Background Processing**: Run intensive operations asynchronously

### Video Optimization
- **Format Selection**: Use MP4 for best compatibility
- **Resolution Adaptation**: Adjust quality based on bandwidth
- **Buffering Management**: Optimize buffer sizes for smooth playback
- **Hardware Acceleration**: Leverage GPU for video decoding

## Future Enhancements

### Planned Features
- [ ] **AI-Powered Automation**: Use RawrXD AI for intelligent web interaction
- [ ] **Multi-Browser Support**: Chrome, Firefox, Edge integration
- [ ] **Advanced Video Processing**: Transcoding, effects, thumbnails
- [ ] **Recording Capabilities**: Screen recording and video capture
- [ ] **Cloud Integration**: Stream from and to cloud storage
- [ ] **Collaborative Features**: Multi-user browser sessions

### Research Areas
- **Computer Vision**: Visual element detection and interaction
- **Natural Language Processing**: Voice-controlled browsing
- **Machine Learning**: Predictive browsing patterns
- **Blockchain Integration**: Decentralized video distribution

## Contributing

### Development Guidelines
1. **Pure PowerShell**: No external dependencies without approval
2. **Modular Design**: Keep functions focused and reusable
3. **Comprehensive Testing**: Include unit and integration tests
4. **Security First**: Validate all inputs and actions
5. **Performance Conscious**: Optimize for responsiveness

### Testing
```powershell
# Run module tests
Invoke-Pester -Path "Tests/RawrXD.Browser.Tests.ps1"
Invoke-Pester -Path "Tests/RawrXD.Video.Tests.ps1"
```

### Code Style
- Use PowerShell approved verbs
- Include comprehensive help documentation
- Handle errors gracefully
- Follow consistent naming conventions

## Support & Documentation

### Getting Help
- **Interactive Demo**: `.\Browser-Video-Integration.ps1`
- **Function Help**: `Get-Help FunctionName -Full`
- **Module Info**: `Get-Module RawrXD.Browser`
- **System Status**: `Get-BrowserInfo; Get-VideoCapabilities`

### Documentation Links
- [WebView2 Documentation](https://docs.microsoft.com/en-us/microsoft-edge/webview2/)
- [PowerShell GUI Development](https://docs.microsoft.com/en-us/powershell/scripting/samples/creating-a-custom-input-box)
- [WPF MediaElement](https://docs.microsoft.com/en-us/dotnet/api/system.windows.controls.mediaelement)

---

**Version**: 1.0.0  
**Last Updated**: 2024-11-25  
**Compatibility**: PowerShell 7.0+, .NET Framework 4.8+  
**License**: MIT
