# 🎤 RawrXD Voice Assistant - Quick Reference

## 🚀 Launch (Pick One)

```powershell
.\voice_assistant_full.ps1              # CLI mode - type commands
.\voice_assistant_full.ps1 -GUI         # GUI mode - click buttons  
.\voice_assistant_full.ps1 -TestMode    # Demo mode - auto-run samples
```

---

## 🎵 Music Commands

```
"RawrXD, play punk rock"          → Opens punk music
"RawrXD, play jazz"               → Opens jazz music
"RawrXD, play [genre]"            → Genre options:
                                     rock, metal, pop, hiphop,
                                     edm, lofi, classical
"RawrXD, pause"                   → Pause music
"RawrXD, resume"                  → Resume music
"RawrXD, next"                    → Skip to next
"RawrXD, status"                  → Show what's playing
```

---

## 🌐 Web Commands

```
"RawrXD, go to amazon"            → Opens Amazon.com
"RawrXD, go to amazon deals"      → Amazon deals
"RawrXD, go to netflix"           → Netflix
"RawrXD, go to youtube"           → YouTube
"RawrXD, go to weather"           → Weather.com
"RawrXD, go to news"              → Google News
"RawrXD, go to reddit"            → Reddit
"RawrXD, go to github"            → GitHub
"RawrXD, search python"           → Google search for python
"RawrXD, find best restaurants"   → Google search
```

---

## 🎮 GUI Features

| Button | Action |
|--------|--------|
| **Genre Buttons** | Punk, Rock, Metal, Pop, HipHop, Jazz, EDM, LoFi |
| **Website Buttons** | Amazon, Netflix, YouTube, Weather, Reddit, GitHub, etc. |
| **Execute Button** | Run typed command |
| **Status Panel** | Shows current playing status |
| **History Panel** | Lists all commands executed |

---

## ⌨️ CLI Mode Tips

```
Type: help           → Show all command examples
Type: quit/exit      → Exit program
Ctrl+C              → Force exit
```

---

## 💡 Example Sentences

✅ Works:
- "RawrXD, play punk rock"
- "RawrXD, go to amazon deals"
- "play rock" (auto-detects name)
- "go to netflix"

---

## 🔧 Custom Names

```powershell
.\voice_assistant_full.ps1 -VoiceName "Alexa"    # Use "Alexa" instead
.\voice_assistant_full.ps1 -VoiceName "JARVIS"   # Use "JARVIS" instead
```

---

## 📊 Available Genres

| Genre | Artists Included |
|-------|------------------|
| **Punk** | Sex Pistols, Ramones, Dead Kennedys, Blink-182 |
| **Rock** | Pink Floyd, Led Zeppelin, Queen, Beatles |
| **Metal** | Iron Maiden, Metallica, Black Sabbath |
| **Pop** | Taylor Swift, Billie Eilish, The Weeknd |
| **HipHop** | Eminem, Dr. Dre, Jay-Z, Kendrick Lamar |
| **Jazz** | Miles Davis, John Coltrane, Duke Ellington |
| **EDM** | Deadmau5, Skrillex, Calvin Harris |
| **LoFi** | Chill Beats, Study Music, Ambient |
| **Classical** | Beethoven, Mozart, Bach, Chopin |

---

## 📱 Available Websites

Amazon • Amazon Deals • Netflix • YouTube • Weather • News • Reddit • GitHub • StackOverflow • Twitter • Facebook • Gmail • Google

---

## 🎯 Pro Commands

```
"RawrXD, play jazz"        then "RawrXD, go to github"        → Music + Work
"RawrXD, play lofi"        then "RawrXD, search python"       → Study Session
"RawrXD, play rock"        then "RawrXD, go to amazon deals"  → Shopping
```

---

## 🚨 What If It Doesn't Work?

1. **Command not recognized?** → Add "RawrXD," prefix: `RawrXD, play rock`
2. **Website not opening?** → Check browser is set as default
3. **No sound?** → Check Windows volume and speech settings
4. **GUI not showing?** → Use explicit flag: `-GUI`

---

## ✨ What You Can Now Do

✅ Play any music genre with one voice command  
✅ Open any website hands-free  
✅ Search the web by voice  
✅ Control music playback (pause/resume/skip)  
✅ Use GUI or CLI interface  
✅ Get real-time voice feedback  
✅ Track command history  
✅ Customize the voice assistant name  

---

## 🎉 One-Liner Quick Start

```powershell
# Copy and paste ONE of these:

# Option 1: CLI - Type commands
cd D:\lazy init ide\desktop; .\voice_assistant_full.ps1

# Option 2: GUI - Click buttons
cd D:\lazy init ide\desktop; .\voice_assistant_full.ps1 -GUI

# Option 3: Test - See demo
cd D:\lazy init ide\desktop; .\voice_assistant_full.ps1 -TestMode
```

---

**Status:** ✅ Production Ready | **Version:** 1.0
