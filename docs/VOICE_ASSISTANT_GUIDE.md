# 🎤 RawrXD - Voice AI Assistant Complete Guide

## Quick Start

### Launch Modes

```powershell
# 1️⃣ Voice Command Mode (CLI - type commands)
.\voice_assistant_full.ps1

# 2️⃣ GUI Control Panel (Full interactive interface)
.\voice_assistant_full.ps1 -GUI

# 3️⃣ Test/Demo Mode (Run sample commands automatically)
.\voice_assistant_full.ps1 -TestMode

# 4️⃣ Custom Voice Name
.\voice_assistant_full.ps1 -VoiceName "Alexa"
.\voice_assistant_full.ps1 -GUI -VoiceName "JARVIS"
```

---

## 🎵 Music Player Commands

### Play Genres

```
RawrXD, play punk rock        → Classic punk artists: Sex Pistols, Ramones, Dead Kennedys
RawrXD, play rock             → Pink Floyd, Led Zeppelin, Queen, The Beatles
RawrXD, play metal            → Iron Maiden, Metallica, Black Sabbath, Judas Priest
RawrXD, play pop              → Taylor Swift, Billie Eilish, The Weeknd, Ariana Grande
RawrXD, play hiphop           → Eminem, Dr. Dre, Jay-Z, Kendrick Lamar
RawrXD, play jazz             → Miles Davis, John Coltrane, Duke Ellington, Chet Baker
RawrXD, play edm              → Deadmau5, Skrillex, Calvin Harris, David Guetta
RawrXD, play lofi             → Chill Lo-Fi Beats, Study Music, Ambient Beats
RawrXD, play classical        → Beethoven, Mozart, Bach, Chopin
```

### Play Specific Songs/Artists

```
RawrXD, play shape of you by ed sheeran
RawrXD, play bohemian rhapsody by queen
RawrXD, play blinding lights by the weeknd
```

### Music Controls

```
RawrXD, pause              → Pause current playback
RawrXD, resume             → Resume paused music
RawrXD, continue           → Resume paused music
RawrXD, next               → Skip to next track
RawrXD, skip               → Skip to next track
```

### Music Status

```
RawrXD, status             → Show current playing status
RawrXD, what are you playing?
RawrXD, what's playing
```

---

## 🌐 Web Browsing Commands

### Go to Websites

```
RawrXD, go to amazon            → Amazon.com
RawrXD, go to amazon deals      → Amazon Gold Box (Deals)
RawrXD, go to netflix           → Netflix.com
RawrXD, go to youtube           → YouTube.com
RawrXD, go to weather           → Weather.com
RawrXD, go to news              → Google News
RawrXD, go to reddit            → Reddit.com
RawrXD, go to github            → GitHub.com
RawrXD, go to stackoverflow     → Stack Overflow
RawrXD, go to twitter           → Twitter.com
RawrXD, go to facebook          → Facebook.com
RawrXD, go to gmail             → Gmail inbox
```

### Browse/Visit

```
RawrXD, browse netflix          → Same as "go to"
RawrXD, visit amazon            → Same as "go to"
RawrXD, open gmail              → Same as "go to"
```

### Search Commands

```
RawrXD, search python tutorials     → Google search
RawrXD, search how to cook steak    → Google search
RawrXD, find python documentation  → Google search
RawrXD, find best coffee shops near me
```

---

## 🎮 GUI Mode Features

### Visual Interface

The GUI provides a professional control panel with:

- **📊 Status Display** - Shows current playback status, genre, and URL
- **🎵 Music Player** - One-click buttons for all genres
  - Punk, Rock, Metal, Pop, HipHop, Jazz, EDM, LoFi
  
- **🌐 Web Browser** - Quick access buttons for popular sites
  - Amazon, Amazon Deals, Netflix, YouTube
  - Weather, News, Reddit, GitHub
  - StackOverflow, Twitter, Gmail, Google
  
- **📝 Command Input** - Type or paste voice commands
  - Execute button to process
  - Real-time feedback in status display
  
- **📜 Command History** - Track all executed commands
  - Scroll through previous commands

### Keyboard Shortcuts (in CLI mode)

```
Type 'help'      → Show command examples
Type 'quit'      → Exit the program
Type 'exit'      → Exit the program
Ctrl+C           → Force exit
```

---

## 💡 Advanced Features

### Command Recognition

The system understands natural language:

```
✅ "RawrXD, play punk rock"
✅ "play punk rock"              (automatic name detection)
✅ "RawrXD play punk rock"       (without comma)
✅ "go to amazon"
✅ "RAWR, go to amazon"          (name variations work)
```

### Batch Commands (in CLI mode)

You can pipe multiple commands:

```powershell
echo "play jazz`nstatus`nquit" | .\voice_assistant_full.ps1
```

### Integration with Other Tools

The system opens:
- **YouTube** for music playback (opens in browser)
- **Web URLs** in default browser
- **Search queries** in Google

---

## 🔧 Customization

### Change Voice Name

Use a different name for your assistant:

```powershell
# Change to Alexa
.\voice_assistant_full.ps1 -VoiceName "Alexa"

# Change to JARVIS
.\voice_assistant_full.ps1 -GUI -VoiceName "JARVIS"

# Change to Any Name
.\voice_assistant_full.ps1 -TestMode -VoiceName "Friday"
```

### Add Custom Genres

Edit the `InitializeGenres()` method to add:

```powershell
"reggae" = @("Bob Marley", "Peter Tosh", "Sean Paul")
"country" = @("Johnny Cash", "Dolly Parton", "Garth Brooks")
"funk" = @("Earth Wind & Fire", "Parliament-Funkadelic", "James Brown")
```

### Add Custom Websites

Edit the `InitializeWebsites()` hashtable:

```powershell
"discord" = "https://discord.com"
"twitch" = "https://twitch.tv"
"linkedin" = "https://linkedin.com"
```

---

## 📊 Command History & Statistics

### In CLI Mode

- All commands are logged in `$assistant.CommandHistory`
- All searches are logged in `$assistant.SearchHistory`
- View history anytime

### In GUI Mode

- Command history appears in the lower panel
- Scroll to see all previous commands
- See execution timestamps

---

## 🚀 Real-World Examples

### Music Discovery Session

```
You: RawrXD, play punk rock
[Opens YouTube with punk rock music search]

You: RawrXD, status
[Shows: ▶️ Playing | Genre: punk | YouTube Music]

You: RawrXD, next
[Skips to next song]

You: RawrXD, search punk rock history
[Opens Google search]
```

### Weekend Planning Session

```
You: RawrXD, go to amazon deals
[Opens Amazon deals page]

You: RawrXD, search best restaurants near me
[Google search for restaurants]

You: RawrXD, go to weather
[Opens weather.com]

You: RawrXD, play jazz
[Plays background jazz music]
```

### Work Session

```
You: RawrXD, play lofi
[Lo-fi ambient music starts]

You: RawrXD, go to github
[Opens GitHub]

You: RawrXD, search python asyncio
[Google search for asyncio]

You: RawrXD, go to stackoverflow
[Opens Stack Overflow]
```

---

## 🎯 Pro Tips

### 1. **Use GUI for Browsing, CLI for Music**
   - GUI has dedicated buttons for quick access
   - CLI is faster if you know exact command

### 2. **Command Names Are Flexible**
   - "RawrXD" or custom name required for music/web commands
   - Can omit for simple commands like "status", "pause"

### 3. **Chaining Commands**
   - Run multiple commands in sequence with test mode or batch input
   - Each command executes independently

### 4. **Voice Feedback**
   - System speaks confirmation for each action
   - Visual feedback in terminal/GUI status panel

### 5. **Browser Integration**
   - Music opens YouTube automatically
   - Websites open in default browser
   - Search queries use Google as default engine

---

## 🐛 Troubleshooting

### Issue: Commands not executing

**Solution:** Make sure command starts with your voice name
```
❌ "play punk rock"
✅ "RawrXD, play punk rock"
```

### Issue: GUI doesn't show

**Solution:** Use explicit `-GUI` flag
```powershell
.\voice_assistant_full.ps1 -GUI
```

### Issue: Browser not opening

**Solution:** Check default browser is set
- Windows → Settings → Apps → Default apps → Browser

### Issue: Voice synthesis not working

**Solution:** Check Windows Speech settings
- Windows → Settings → Ease of Access → Speech

---

## 📋 Command Reference Quick Table

| Category | Command | Example |
|----------|---------|---------|
| **Music - Play** | play [genre] | `play punk rock` |
| **Music - Control** | pause\|resume\|next | `pause` |
| **Music - Info** | status | `status` |
| **Web - Go** | go to [site] | `go to amazon` |
| **Web - Browse** | browse [site] | `browse netflix` |
| **Web - Search** | search [query] | `search python` |
| **System** | help\|quit | `help` |

---

## 🎉 You Now Have

✅ **Voice-Controlled Music Player**
- Support for 9 genres with famous artists
- Play/Pause/Next/Status controls
- YouTube integration for music playback

✅ **Agentic Web Browser**
- Voice commands to visit popular websites
- Google search integration
- Quick access to 12+ websites

✅ **Dual Interface**
- **CLI Mode** - Type commands in terminal
- **GUI Mode** - Professional control panel with buttons

✅ **Customizable**
- Change voice name (Alexa, JARVIS, etc.)
- Add custom genres
- Add custom websites

✅ **Smart Command Recognition**
- Natural language understanding
- Flexible command syntax
- Automatic name detection

---

## 🔗 Integration Points

This system integrates with:
- **System.Speech.Synthesis** - Voice feedback
- **Windows.Forms** - GUI controls
- **System.Diagnostics** - Browser launching
- **YouTube** - Music streaming
- **Google** - Web search
- **Browser** - Website access

---

## 📝 Notes

- All music opens in YouTube (requires internet)
- All websites open in default browser
- Voice synthesis requires Windows Speech engine
- GUI requires .NET Framework

---

**Last Updated:** January 25, 2026  
**Version:** 1.0 - Full Release  
**Status:** Production Ready ✅
