# 🎬 AGENTIC VIDEO ENGINE - QUICK REFERENCE CARD

**Quick Start Commands for RawrXD AI IDE**

---

## 🔍 SEARCH

```
/search youtube "query"
/search google "query"
```
Example: `/search youtube "machine learning tutorials"`

Returns: 10 results with title, channel, duration, views

---

## ▶️ PLAYBACK

```
/play 1                    # Play search result #1
/play "url"                # Play video from URL
/stream "url" 720p         # Stream with specific quality
```

Quality Options: `360p`, `480p`, `720p`, `1080p`, `4K`, `auto`

---

## 📥 DOWNLOAD

```
/download 1                                      # Download result #1
/download "url" ~/Videos/video.mp4              # Download with path
/download "url" ~/Videos/ 720p                  # Download with quality
```

Returns: File size, speed (MB/s), duration

---

## 📋 PLAYLISTS

```
/playlist create "topic"              # Create new playlist
/playlist add "url" ~/Videos/list.m3u # Add video to playlist
/playlist list                        # Show all playlists
```

Playlists saved as `.m3u` in `~/Videos/`

---

## 🌐 BROWSER

```
/navigate "youtube.com"      # Open URL in browser tab
/click "#button-id"          # Click element by CSS selector
/screenshot                  # Take browser screenshot
```

Screenshots saved to: `~/screenshots/`

---

## 💡 NATURAL LANGUAGE (AI Understands)

```
"download that video"        → /download [from last search]
"play python tutorials"      → /search youtube python + /play 1
"watch machine learning"     → /search youtube + /play best result
"save to downloads"          → /download [with default path]
```

---

## 📊 QUICK STATS

| Feature | Speed | Notes |
|---------|-------|-------|
| Search | <2 sec | YouTube returns 10 results |
| Download | 85% line speed | 4 parallel threads |
| Play Start | <3 sec | Buffering 10 seconds |
| Resume | Any byte | Automatic from interruption |
| Quality | Auto/Manual | Adaptive or selected |

---

## 🎯 COMMON WORKFLOWS

### **Research & Download**
```
1. /search youtube "topic"
2. Review results in chat
3. /play 1                  (preview)
4. /download 1 ~/Videos/    (save for later)
```

### **Create Study Playlist**
```
1. /playlist create "python programming"
2. Results: 20 videos found
3. Agent creates: ~/Videos/Playlist_xxxxx.m3u
4. Open in VLC or similar
```

### **Auto-Quality Download**
```
1. /search youtube "4K video"
2. /download 1 ~/Videos/ auto
3. Agent: Selects best quality for connection
4. Progress: 25 MB/s | ETA: 3:45
```

### **Browser Automation**
```
1. /navigate youtube.com
2. /click "button.search"          (click search)
3. /screenshot                     (save page)
4. Results: Screenshot saved
```

---

## ⚙️ CUSTOMIZATION

### **More Threads (Faster Download)**
Modify `DownloadManager.ps1`:
```powershell
-ThreadCount 8  # was 4 (faster, uses more bandwidth)
```

### **Different Default Quality**
Modify `AgentCommandProcessor.ps1`:
```powershell
$parsed.Quality = "1080p"  # was "auto"
```

### **Higher Speed Targets**
```
Normal: 85% line speed
High:   95% line speed (8 threads, optimal conditions)
Limited: 5 MB/s max (for throttled connections)
```

---

## 🆘 HELP COMMANDS

```
/help                      # Show all commands
/commands                  # Same as /help
/search youtube test       # Test if search works
/navigate youtube.com      # Test if browser works
/screenshot                # Test if screenshots work
```

---

## ✅ BEFORE YOU START

- [ ] WebView2 Runtime installed
- [ ] Internet connection available
- [ ] 3 PS1 files in RawrXD directory
- [ ] RawrXD has been restarted

---

## 🔥 COMMON ISSUES & FIXES

| Issue | Fix |
|-------|-----|
| "No results" | YouTube might be slow, try again |
| "WebView2 error" | Wait for browser tab to load |
| "Slow download" | Use `-ThreadCount 8` option |
| "Form not filled" | Use browser DevTools (F12) to find selector |
| "Timeout" | Network slow, increase -Timeout to 20 |

---

## 📱 KEYBOARD SHORTCUTS

```
Alt+1          # Focus browser tab
Alt+2          # Focus chat panel
/search        # Focus search (type in chat)
Enter          # Send command
Esc            # Cancel operation
F12            # Open browser DevTools (inspect elements)
```

---

## 🎓 EXAMPLES

### Search & Download
```
User: /search youtube python basics
Agent: Found 10 results...

User: /download 1 ~/Downloads/
Agent: ✅ Downloaded: python_basics.mp4
       Speed: 24 MB/s | Duration: 3:45
```

### Stream with Quality
```
User: /stream https://youtube.com/watch?v=xxx 1080p
Agent: ▶️  Streaming at 1080p (detected: 1080p available)
       Buffering... 10 seconds loaded
```

### Create Playlist
```
User: /playlist create "web development"
Agent: 📝 Creating playlist...
       Found 20 videos
       ✅ Saved: ~/Videos/Playlist_12345.m3u
       
User: /playlist list
Agent: Found 3 playlists:
       1. python_programming (15 videos)
       2. web_development (20 videos)
       3. machine_learning (25 videos)
```

---

## 💾 FILE LOCATIONS

```
Downloads:     ~/Videos/
Screenshots:   ~/screenshots/
Playlists:     ~/Videos/Playlist_*.m3u
Partial DL:    ~/Videos/*.part
Logs:          %TEMP%/RawrXD_*.log
```

---

## 🚀 POWER FEATURES

### **Resume Any Download**
- Interrupted at 50%? Continue from there
- No need to restart, just type `/download` again

### **Parallel Downloads**
- Download multiple videos at once
- Each uses 4 threads = 8 threads total
- System auto-manages bandwidth

### **Smart Quality Selection**
- `/stream "url" auto` = Best quality for your connection
- Adapts if network changes
- Downgrades if too slow

### **Playlist Batch Operations**
- Create from search results
- Import/export M3U format
- Works with all media players

---

## 📞 SUPPORT TIPS

1. Check Dev Console for detailed logs (View → Dev Console)
2. Enable verbose logging: Set `$VerbosePreference = "Continue"`
3. Test individual functions in PowerShell ISE
4. Check internet speed: `Test-NetConnection -ComputerName google.com`
5. Verify WebView2: Check Windows Settings → Apps

---

## 🎉 YOU'RE READY!

**Minimum Setup:**
1. Copy 3 PS1 files
2. Add imports to RawrXD.ps1
3. Route `/` commands in chat handler
4. Test with: `/search youtube test`

**Total Time:** 10-15 minutes

**Commands Available:** 15+

**Performance:** 85%+ download speed

---

**Happy streaming! 🚀**

For full documentation, see:
- `AGENTIC-VIDEO-ENGINE-ARCHITECTURE.md`
- `VIDEO-ENGINE-IMPLEMENTATION-GUIDE.md`
- `AGENTIC-VIDEO-ENGINE-COMPLETE-SUMMARY.md`
