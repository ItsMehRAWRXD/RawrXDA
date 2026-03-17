# 🎬 AGENTIC VIDEO ENGINE & BROWSER AUTOMATION ARCHITECTURE

**Date:** November 25, 2025  
**Status:** 🏗️ **DESIGN PHASE - IMPLEMENTATION READY**  
**Scope:** Custom video engine + AI-controlled browser + file downloads

---

## 🎯 Executive Summary

Building a **professional-grade video streaming engine** with **full agentic AI control** of the browser. The system will:

✅ **Video Playback:**
- H.264, VP9, AV1 codec support
- HLS/DASH adaptive streaming
- Hardware acceleration (where available)
- Subtitle support (SRT, VTT, ASS)
- 4K resolution streaming

✅ **Browser Automation:**
- JavaScript injection into WebView2
- Search automation (YouTube, Google)
- Form filling & button clicking
- Link extraction & DOM parsing
- Workflow automation

✅ **File Management:**
- Multi-threaded downloads
- Resumable downloads with ranges
- SHA-256 verification
- Progress tracking & throttling
- Auto-retry on failure

✅ **AI Integration:**
- Chat commands: `/search`, `/download`, `/play`, `/stream`
- Agent-controlled video selection
- Automated playlist creation
- Smart recommendations

---

## 🏗️ ARCHITECTURE OVERVIEW

```
┌─────────────────────────────────────────────────────────────┐
│                  RawrXD AI IDE                               │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────┐ │
│  │   Chat Input    │  │ Browser Tab      │  │ Video Pane │ │
│  │ (Agentic Cmds)  │  │ (WebView2)       │  │ (Playback) │ │
│  └────────┬────────┘  └────────┬─────────┘  └─────┬──────┘ │
│           │                    │                   │        │
│           ▼                    ▼                   ▼        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │        Agentic Control Layer                        │   │
│  │  • Command Parser                                   │   │
│  │  • Workflow Orchestrator                            │   │
│  │  • State Management                                 │   │
│  └────┬────────────────────────────────────────────┬───┘   │
│       │                                            │        │
│       ▼                                            ▼        │
│  ┌──────────────────┐  ┌──────────────────────┐  ┌────────┐│
│  │ Browser Ctrl     │  │ Video Engine         │  │Download││
│  │ • JavaScript Inj │  │ • Codec Framework    │  │Manager ││
│  │ • Link Extract   │  │ • Streaming         │  │ • HTTP ││
│  │ • Search Auto    │  │ • Playback Control  │  │ • Chunks
│  │ • Form Fill      │  │ • Subtitles         │  │ • Resume
│  └────┬─────────────┘  └──────────┬───────────┘  └────┬───┘│
│       │                           │                    │    │
└───────┼───────────────────────────┼────────────────────┼────┘
        │                           │                    │
        ▼                           ▼                    ▼
┌──────────────────┐ ┌──────────────────────┐ ┌──────────────┐
│ WebView2 Runtime │ │ Media Foundation     │ │ WinSock/HTTP │
│ • Core Rendering │ │ • H.264 Decoding     │ │ • TCP Sockets│
│ • JavaScript     │ │ • VP9/AV1 (SW)      │ │ • Range Req  │
│ • DOM Access     │ │ • Audio Processing   │ │ • TLS/SSL    │
└──────────────────┘ └──────────────────────┘ └──────────────┘
```

---

## 📋 DETAILED COMPONENT SPECIFICATIONS

### 1. AGENTIC CONTROL LAYER

**Location:** New functions in RawrXD.ps1

#### 1.1 Command Parser
```powershell
function Parse-AgentCommand {
    param([string]$Command)
    
    # Patterns:
    # /search [query]           - Search YouTube/Google
    # /play [url|id]            - Play video
    # /download [url] [path]    - Download video
    # /stream [url] [quality]   - Stream with specified quality
    # /recommend [type]         - Get recommendations
    # /list [type]              - List videos/playlists
    # /navigate [url]           - Navigate browser to URL
    # /click [selector]         - Click DOM element
    # /fill [selector] [text]   - Fill form field
    # /screenshot               - Capture browser screenshot
}
```

#### 1.2 Workflow Orchestrator
```powershell
function Invoke-AgentWorkflow {
    param(
        [string]$WorkflowName,
        [hashtable]$Parameters
    )
    
    # Workflows:
    # PlayYouTubePlaylist    - Download & play entire playlist
    # SearchAndDownload      - Search, select, download video
    # StreamAndRecord        - Stream video with simultaneous recording
    # CreatePlaylist         - Build custom playlist from search results
    # AutoQualityStream      - Adaptive quality based on network
}
```

### 2. BROWSER AUTOMATION ENGINE

**Technology:** JavaScript injection + WebView2 ExecuteScriptAsync

#### 2.1 Search Automation
```javascript
// YouTube Search
async function searchYouTube(query, pageToken = null) {
    // Inject search query
    // Get results: title, channelName, videoId, duration, views
    // Return array of video metadata
}

// Google Search
async function searchGoogle(query) {
    // Similar pattern for Google
}
```

#### 2.2 DOM Manipulation
```javascript
// Click button by selector
async function clickElement(selector) {
    const element = document.querySelector(selector);
    if (element) {
        element.click();
        return true;
    }
    return false;
}

// Fill form field
async function fillFormField(selector, text) {
    const field = document.querySelector(selector);
    if (field) {
        field.value = text;
        field.dispatchEvent(new Event('change', { bubbles: true }));
        return true;
    }
    return false;
}

// Extract links from page
async function extractLinks(selector = 'a') {
    return Array.from(document.querySelectorAll(selector))
        .map(a => ({ href: a.href, text: a.textContent }));
}
```

#### 2.3 Video Metadata Extraction
```javascript
async function getVideoMetadata(videoId) {
    // Get: title, duration, views, likes, channel, description
    // Format: { id, title, duration, views, channel, description }
}

async function getPlaylistVideos(playlistId) {
    // Get all videos in playlist with metadata
    // Handle pagination
}
```

### 3. VIDEO ENGINE FRAMEWORK

**Codecs Supported:**
| Codec | Type | Hardware Accel | Quality | Use Case |
|-------|------|---|----------|----------|
| **H.264** | Video | ✅ Yes (DirectX) | 720p-1080p | Most compatible |
| **VP9** | Video | ❌ Software | 1080p-4K | YouTube, high quality |
| **AV1** | Video | ❌ Software | 1080p-4K | Modern, efficient |
| **AAC** | Audio | N/A | 128-256kbps | MP4, MP3 |
| **Opus** | Audio | N/A | 64-256kbps | WebM, modern |
| **Vorbis** | Audio | N/A | 128-256kbps | OGG containers |

#### 3.1 Streaming Formats
```powershell
# HLS (HTTP Live Streaming)
# - m3u8 playlist format
# - Variable bitrate chunks
# - Adaptive quality selection
# - 6-second segments (typical)

# DASH (Dynamic Adaptive Streaming over HTTP)
# - MPD (Media Presentation Description) XML
# - Multiple quality profiles
# - Client-side bitrate adaptation
# - Smoother quality transitions

# Progressive Download
# - Direct MP4/WebM file
# - Full seeking capability
# - Requires full file size
```

#### 3.2 Playback Engine Functions
```powershell
function Initialize-VideoEngine {
    # Load Media Foundation codecs
    # Initialize audio subsystem
    # Setup frame buffer (512MB for 4K)
    # Initialize subtitle renderer
}

function Stream-VideoFromURL {
    param(
        [string]$URL,
        [string]$Quality = "auto",  # auto, 480p, 720p, 1080p, 4K
        [bool]$HardwareAccel = $true
    )
    
    # Detect format (HLS/DASH/Progressive)
    # Select quality based on network bandwidth
    # Initialize decoder
    # Start playback
    # Handle adaptive bitrate transitions
}

function Seek-VideoToPosition {
    param(
        [double]$TimeSeconds,
        [bool]$PreloadAround = $true
    )
    
    # Find keyframe at position
    # Load chunks around seek point
    # Resume playback at new position
}

function Render-Subtitles {
    param(
        [string]$SubtitleFile,
        [string]$Format  # srt, vtt, ass
    )
    
    # Parse subtitle file
    # Sync with video playback
    # Render with proper styling
}
```

#### 3.3 Hardware Acceleration
```powershell
# DirectX 11 Video Processing
# - H.264 decoding via DXVA2
# - VP9 profile detection
# - Frame output to D3D11 texture
# - GPU-accelerated color space conversion

# Fallback: Software Decoding
# - Use libavcodec (via SharpAV or similar)
# - CPU-based H.264/VP9/AV1 decoding
# - Suitable for lower resolution streams
```

### 4. DOWNLOAD MANAGER

**Capabilities:**
- Multi-threaded downloads (4-8 threads)
- Resumable downloads (HTTP Range requests)
- Progress tracking (bytes/speed/ETA)
- SHA-256 integrity verification
- Auto-retry with exponential backoff
- Rate limiting (configurable)

#### 4.1 Download Architecture
```powershell
function Invoke-MultiThreadedDownload {
    param(
        [string]$URL,
        [string]$OutputPath,
        [int]$ThreadCount = 4,
        [int64]$ChunkSize = 1MB,
        [int]$MaxRetries = 3
    )
    
    # 1. HEAD request to get file size & range support
    # 2. Split file into chunks
    # 3. Create download jobs (one per thread)
    # 4. Download chunks in parallel
    # 5. Verify chunks (CRC32)
    # 6. Merge chunks into final file
    # 7. Verify SHA-256 of complete file
    # 8. Return: success, speed, duration
}

function Resume-Download {
    param(
        [string]$URL,
        [string]$PartialPath,
        [int64]$ResumePosition
    )
    
    # Send Range: bytes=ResumePosition- header
    # Append to existing file
    # Continue from last position
}

function Track-DownloadProgress {
    param([string]$DownloadID)
    
    # Return: {
    #   TotalBytes: 1000000000
    #   DownloadedBytes: 500000000
    #   Speed: "25 MB/s"
    #   ETA: "00:30:00"
    #   Threads: 4
    # }
}
```

#### 4.2 Video Download Handlers
```powershell
# YouTube Video Download
# - Extract video ID from URL
# - Query youtube-dl API or similar
# - Get available formats/qualities
# - Download best quality (with fallbacks)
# - Extract audio/video if needed
# - Mux into single container (MP4)

# Generic HTTP Download
# - Supports .mp4, .webm, .mkv
# - Direct streaming to file
# - Progress reporting
# - Automatic resume on interruption
```

### 5. SEARCH SYSTEM

#### 5.1 YouTube Search
```powershell
function Search-YouTube {
    param(
        [string]$Query,
        [int]$MaxResults = 10,
        [string]$Filter = "video"  # video, channel, playlist
    )
    
    # Execute JavaScript search in WebView2
    # Extract results:
    # - Video ID
    # - Title
    # - Channel Name
    # - Duration
    # - View Count
    # - Upload Date
    # - Thumbnail URL
    
    # Return array of metadata
}

function Get-YouTubeRecommendations {
    param([string]$VideoID)
    
    # Get similar/recommended videos
    # Based on: tags, channel, category
}
```

#### 5.2 Google Search
```powershell
function Search-Google {
    param([string]$Query)
    
    # Similar pattern to YouTube
    # Extract: title, URL, description, date
}
```

---

## 🔧 IMPLEMENTATION PHASES

### Phase 1: Browser Automation (Week 1)
✅ **Deliverables:**
1. JavaScript injection framework
2. YouTube search automation
3. Link extraction system
4. Form filling capability
5. Chat command routing

**Files to create/modify:**
- `functions/Invoke-BrowserAutomation.ps1`
- `javascript/YouTubeSearch.js`
- `javascript/FormFiller.js`
- `javascript/LinkExtractor.js`

### Phase 2: Video Engine (Week 2-3)
✅ **Deliverables:**
1. Codec framework
2. HLS/DASH playlist parser
3. Streaming playback
4. Hardware acceleration detection
5. Subtitle renderer

**Files to create/modify:**
- `video_engine/VideoCodecFramework.ps1`
- `video_engine/StreamingParser.ps1`
- `video_engine/PlaybackControl.ps1`
- `video_engine/SubtitleRenderer.ps1`

### Phase 3: Download Manager (Week 2)
✅ **Deliverables:**
1. Multi-threaded downloader
2. Resumable download support
3. Progress tracking
4. Integrity verification
5. Auto-retry logic

**Files to create/modify:**
- `download_manager/MultiThreadedDownloader.ps1`
- `download_manager/ProgressTracker.ps1`
- `download_manager/IntegrityVerifier.ps1`

### Phase 4: Agentic Integration (Week 3)
✅ **Deliverables:**
1. Command parser
2. Workflow orchestrator
3. Chat command handlers
4. Agent state management
5. Error recovery

**Files to create/modify:**
- `agentic/CommandParser.ps1`
- `agentic/WorkflowOrchestrator.ps1`
- `agentic/ChatCommandHandlers.ps1`

---

## 💬 AGENTIC CHAT COMMANDS

### Video Playback Commands
```
# Search & play
/search youtube "machine learning tutorials"
→ Lists 10 results, AI selects best match, plays in Browser tab

# Direct play
/play "https://youtube.com/watch?v=..."
→ Detects format, selects quality, plays video

# Stream with quality selection
/stream "url" 720p
→ Automatically adapts to 720p quality (or closest available)

# Recommendations
/recommend action movies
→ AI searches for action movies on YouTube
```

### Download Commands
```
# Download video
/download "https://youtube.com/watch?v=..." ~/Videos/MyVideo.mp4
→ Downloads best quality video with audio

# Download playlist
/download playlist "https://youtube.com/playlist?list=..." ~/Videos/Playlist/
→ Downloads all videos in playlist with numbering

# Download with specific quality
/download "url" 1080p ~/video.mp4
→ Downloads closest quality to 1080p
```

### Playlist Commands
```
# Create playlist from search
/playlist create "machine learning"
→ Searches, AI selects top 20 results, creates playlist

# Add to playlist
/playlist add "video_url" ~/playlists/MyList.m3u
→ Adds video to local M3U playlist

# List playlists
/playlist list
→ Shows all local playlists and their video counts
```

### Browser Control Commands
```
# Navigate to URL
/navigate "https://youtube.com"
→ Opens URL in browser tab

# Search in browser
/search web "latest AI news"
→ Opens Google, searches query

# Click element
/click "#search-button"
→ Clicks element by CSS selector

# Screenshot
/screenshot
→ Captures browser view, saves to ~/screenshots/
```

---

## 🎛️ UI INTEGRATION

### New Browser Tab Components

```
┌────────────────────────────────────────────────┐
│ Browser Tab (Existing)                         │
├────────────────────────────────────────────────┤
│ [◀] [▶] [🔄] [URL Box..................] [Go]  │  ← Existing
├────────────────────────────────────────────────┤
│                                                │
│ ┌──────────────────┐  ┌──────────────────┐    │
│ │ WebView2 Browser │  │ Video Playback   │    │ ← New Video Pane
│ │                  │  │ (Side by side)   │    │
│ │ (70% width)      │  │ (30% width)      │    │
│ │                  │  │                  │    │
│ │                  │  │ [►][⏸][⏹][Full]  │    │ ← Video Controls
│ │                  │  │ [████░░░░] 2:30  │    │
│ │                  │  │                  │    │
│ └──────────────────┘  └──────────────────┘    │
├────────────────────────────────────────────────┤
│ Status: Ready | Download: 0% | Video: Playing │  ← Status Bar
└────────────────────────────────────────────────┘
```

### New Chat Command Integration

```
Chat Input:
/search youtube "machine learning"

Agent Processing:
1. Parses command: action=search, source=youtube, query="machine learning"
2. Invokes JavaScript in WebView2
3. Extracts 10 results
4. Displays results in chat with thumbnails
5. Waits for selection or auto-selects best match
6. Starts playback in video pane

Chat Output:
Agent > Searching YouTube for "machine learning"...
Agent > Found 10 results:
  1. Machine Learning Fundamentals [3:45:20] - Dr. Andrew Ng
  2. Deep Learning Tutorial [2:30:15] - Fast.ai
  3. ... (8 more results)
Agent > Playing: Machine Learning Fundamentals
```

---

## 🔐 SECURITY CONSIDERATIONS

### Privacy & Safety
- ✅ HTTPS only for downloads
- ✅ No tracking/telemetry from video streams
- ✅ Local playback (no cloud streaming logs)
- ✅ User consent for each search
- ✅ Encrypted download resume data

### Performance
- ✅ Lazy loading of video engine (only when video plays)
- ✅ Memory-efficient streaming (buffer = 64MB max)
- ✅ Hardware acceleration fallback
- ✅ Automatic quality downgrade on network congestion

### Error Handling
- ✅ Network timeout recovery
- ✅ Corrupt file detection (SHA-256)
- ✅ Fallback codecs on decode failure
- ✅ Graceful degradation to lower quality
- ✅ Comprehensive error logging

---

## 📊 PERFORMANCE TARGETS

| Metric | Target | Notes |
|--------|--------|-------|
| **Search Time** | < 2 seconds | YouTube/Google search |
| **Play Start** | < 3 seconds | Buffer first 10 seconds |
| **Download Speed** | 80-90% line speed | With 4 parallel threads |
| **Memory Usage** | < 200MB | With HD video streaming |
| **CPU Usage** | < 15% | During HD playback (with HW accel) |
| **Quality Adapt** | < 1 second | Bitrate change detection |

---

## 🚀 DEPLOYMENT CHECKLIST

- [ ] Browser automation functions tested
- [ ] YouTube search working
- [ ] Video engine playback verified
- [ ] Download manager multi-threading confirmed
- [ ] Chat commands integrated
- [ ] Error handling comprehensive
- [ ] Performance benchmarked
- [ ] Security audit completed
- [ ] User documentation written
- [ ] Fallback mechanisms tested
- [ ] Memory leaks eliminated
- [ ] Logging system integrated

---

## 📚 REFERENCE IMPLEMENTATIONS

### Similar Systems
- **VLC Media Player** - Codec framework, playback control
- **yt-dlp** - YouTube metadata extraction, format selection
- **FFmpeg** - Streaming protocol support, codec handling
- **youtube-dl** - Search API integration, error recovery

### WebView2 Resources
- **Microsoft.Web.WebView2 API** - JavaScript execution, DOM access
- **ExecuteScriptAsync** - Async JavaScript injection
- **AddHostObjectToScript** - Bidirectional communication

---

## 🎯 SUCCESS CRITERIA

✅ **Phase 1 Complete When:**
- YouTube search returns results in < 2 seconds
- Chat commands parse correctly
- JavaScript injection works reliably

✅ **Phase 2 Complete When:**
- Video plays smoothly (60 FPS target)
- Quality auto-adapts to network
- Subtitles sync with video

✅ **Phase 3 Complete When:**
- Downloads resume after interruption
- Multi-threading achieves 80%+ efficiency
- Integrity verification works

✅ **Phase 4 Complete When:**
- All chat commands functional
- Workflows execute end-to-end
- Error recovery automated

---

**Next Step:** Begin Phase 1 implementation with browser automation functions!
